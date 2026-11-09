// Real recordings through the sample build: open each take (a folder of one-mic files), Auto Prep, then render
// every mic through Tracktion with the processing off (A) and on (B). Checks the hit count against the file names,
// that A comes back as recorded (the timeline holds), and that B lines each touched mic up with the lead at least
// as well as A. LD_CORPUS = a folder of take folders; LD_CORPUS_REPORT = where to write the table (optional).
// Without LD_CORPUS the test says so loudly and does nothing (set LD_CORPUS_REQUIRED=1 to make that a failure);
// LD_CORPUS set but no takes run is always a failure.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "samples/SampleModel.h"
#include "core/Session.h"
#include "hl/Session.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <regex>
#include <map>

namespace te = tracktion;

namespace
{
using Chans = std::vector<std::vector<float>>;

/** One track rendered by Tracktion, mono or stereo as the track is (as the export renders it), the edit's lead-in
    removed. Fails the test if the render comes back empty or the wrong length. */
Chans renderTrack (te::Edit& edit, int idx, double sr, bool stereo)
{
    juce::TemporaryFile temp (".wav");
    auto all = te::getAllTracks (edit);
    auto at = te::getAudioTracks (edit);
    juce::BigInteger which; which.setBit (all.indexOf (at[idx]));
    te::Renderer::Parameters p (edit);
    p.destFile = temp.getFile();
    p.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32;
    p.sampleRateForAudio = sr;
    p.blockSizeForAudio = 512;
    p.time = te::TimeRange (te::TimePosition(), edit.getLength());
    p.tracksToDo = which;
    p.usePlugins = true;
    p.useMasterPlugins = false;
    p.mustRenderInMono = ! stereo;
    p.ditheringEnabled = false;
    REQUIRE (te::Renderer::renderToFile ("Render", p) != juce::File());
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (temp.getFile()));
    REQUIRE (r != nullptr);
    REQUIRE (std::abs ((double) r->lengthInSamples - edit.getLength().inSeconds() * sr) <= 1.0);
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    const int pad = (int) std::lround (ld::samples::SampleModel::kPad * sr);   // the take sits this far into the edit
    Chans out;
    for (int c = 0; c < (stereo ? 2 : 1); ++c)
    {
        REQUIRE (c < b.getNumChannels());
        out.emplace_back (b.getReadPointer (c) + std::min (pad, b.getNumSamples()), b.getReadPointer (c) + b.getNumSamples());
    }
    return out;
}

// how well two mics add up over the first 25 ms of every hit: 1 = in step, 0 = unrelated, below 0 = fighting
double coherence (const std::vector<float>& a, const std::vector<float>& b, const std::vector<long>& on, double sr)
{
    double ab = 0, aa = 0, bb = 0;
    const long w = (long) (0.025 * sr);
    for (auto o : on)
        for (long n = o; n < o + w && n < (long) std::min (a.size(), b.size()); ++n)
        { ab += a[(size_t) n] * b[(size_t) n]; aa += a[(size_t) n] * a[(size_t) n]; bb += b[(size_t) n] * b[(size_t) n]; }
    return aa > 0 && bb > 0 ? ab / std::sqrt (aa * bb) : 0;
}

// rooms keep their own arrival: the strongest cross-correlation with the lead over 0..40 ms later, signed
double arrivalPeak (const std::vector<float>& a, const std::vector<float>& b, const std::vector<long>& on, double sr)
{
    const long w = (long) (0.03 * sr), maxLag = (long) (0.04 * sr), step = std::max (1L, (long) (sr / 48000));
    double best = 0;
    double aa = 0, bb = 0;
    for (auto o : on)
        for (long n = o; n < o + w + maxLag && n < (long) std::min (a.size(), b.size()); ++n) { if (n < o + w) aa += a[(size_t) n] * a[(size_t) n]; bb += b[(size_t) n] * b[(size_t) n]; }
    if (aa <= 0 || bb <= 0) return 0;
    for (long k = 0; k <= maxLag; k += step)
    {
        double ab = 0;
        for (auto o : on)
            for (long n = o; n < o + w && n + k < (long) std::min (a.size(), b.size()); ++n) ab += a[(size_t) n] * b[(size_t) (n + k)];
        const double c = ab / std::sqrt (aa * bb);
        if (std::abs (c) > std::abs (best)) best = c;
    }
    return best;
}

double maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0;
    for (size_t n = 0; n < std::min (a.size(), b.size()); ++n) m = std::max (m, (double) std::abs (a[n] - b[n]));
    return m;
}
} // namespace

TEST_CASE ("Sample corpus: real takes through Auto Prep and Tracktion")
{
    const char* dirEnv = std::getenv ("LD_CORPUS");
    if (dirEnv == nullptr)
    {
        WARN ("LD_CORPUS not set: the real-recording corpus was NOT tested");
        CHECK (std::getenv ("LD_CORPUS_REQUIRED") == nullptr);
        return;
    }
    juce::File root (dirEnv);
    REQUIRE (root.isDirectory());
    juce::String report = "take | mics | expect | found | V x RR | open s | prep s | A error | per mic (shift ms, flip, all-pass, coherence A -> B)\n";
    auto folders = root.findChildFiles (juce::File::findDirectories, false);
    folders.sort();
    REQUIRE (! folders.isEmpty());
    int tested = 0;
    for (auto& f : folders)
    {
        if (const char* only = std::getenv ("LD_CORPUS_ONLY")) if (! f.getFileName().containsIgnoreCase (only)) continue;
        ++tested;
        INFO ("take: " << f.getFileName());
        [&]
        {
            auto wavs = f.findChildFiles (juce::File::findFiles, false, "*.wav");
            CHECK (! wavs.isEmpty()); if (wavs.isEmpty()) return;
            std::smatch mv, mr;
            const std::string fn = wavs[0].getFileName().toStdString();
            const bool named = std::regex_search (fn, mv, std::regex ("_V(\\d+)")) && std::regex_search (fn, mr, std::regex ("_RR(\\d+)"));
            CHECK (named);
            if (! named) return;
            const int V = std::stoi (mv[1]), RR = std::stoi (mr[1]);

            ld::samples::SampleModel model (ldtest::engine());
            auto t0 = std::chrono::steady_clock::now();
            const auto openErr = model.open ({ f }); CHECK (openErr.toStdString() == ""); if (openErr.isNotEmpty()) return;
            auto t1 = std::chrono::steady_clock::now();
            const auto err = model.autoPrepNow();
            auto t2 = std::chrono::steady_clock::now();
            // LD_CORPUS_TUNE=1: tune every pitched take (each hit to the drum's note), so the tuned path is checked too
            if (std::getenv ("LD_CORPUS_TUNE") && model.session()->tuneAnalyzed)
            {
                model.change ([] (ld::hl::Session& s) { s.tune.on = true; s.tune.mode = "rr"; }, ld::samples::SampleModel::Redo::tune);
                const auto w0 = juce::Time::getMillisecondCounter();
                do juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                while (model.isBusy() && juce::Time::getMillisecondCounter() - w0 < 120000);
            }
            const auto& S = model.engineState()->session;
            const int found = (int) S.onsets.size();
            CHECK (err.toStdString() == "");
            CHECK (found == V * RR);
            CHECK (S.det.V == V);
            CHECK (S.det.RR == RR);

            auto* edit = model.getEdit();
            REQUIRE (edit != nullptr);
            const double sr = S.sr;
            juce::String micLine;
            double worstA = 0;
            model.setProcessed (true);
            std::vector<Chans> B;
            for (int t = 0; t < (int) S.tracks.size(); ++t) B.push_back (renderTrack (*edit, t, sr, S.tracks[(size_t) t].ch.size() > 1));
            model.setProcessed (false);
            model.setLevelMatch (false);   // A exactly as recorded
            std::vector<Chans> A;
            for (int t = 0; t < (int) S.tracks.size(); ++t)
            {
                const auto& tr = S.tracks[(size_t) t];
                A.push_back (renderTrack (*edit, (int) S.tracks.size() + t, sr, tr.ch.size() > 1));
                // A is the recording: every channel, every sample, on the same timeline, and nothing after it
                for (size_t c = 0; c < tr.ch.size(); ++c)
                {
                    const auto& orig = tr.ch[c];
                    const auto& got = A.back()[c];
                    REQUIRE (got.size() >= orig.size());
                    for (size_t n = 0; n < got.size(); ++n)
                        worstA = std::max (worstA, (double) std::abs (got[n] - (n < orig.size() ? orig[n] : 0.0f)));
                }
                for (auto& ch : B[(size_t) t]) for (auto v : ch) REQUIRE (std::isfinite (v));
            }
            CHECK (worstA < 1.0e-4);

            // B against the reference renderer (Hitline's export path): every hit of every mic
            juce::String nullLine;
            const auto* st = model.engineState();
            auto Sx = S;
            Sx.tuneTaps = 64;   // the reference at the filter length the tuned hits are rendered with
            if (const char* tp = std::getenv ("LD_CORPUS_REF_TAPS")) Sx.tuneTaps = std::atoi (tp);
            if (st->mix && st->levels && st->levels->valid)
            {
                const auto& Rg = st->levels->regions;
                for (int t = 0; t < (int) S.tracks.size(); ++t)
                {
                    // every channel of every hit; the last 12 ms before the next hit is left out (the next hit's gain
                    // crossfade starts there by design)
                    double e = 0, d = 0, worstHit = -300;
                    for (size_t i = 0; i < Rg.size(); ++i)
                    {
                        const auto ref = ld::hl::renderHitMic (Sx, *st->mix, t, i, Rg[i], sr, std::pow (10.0, model.hitGainDb (t, i) / 20));
                        const long n0 = Rg[i].start;
                        double eh = 0, dh = 0;
                        for (size_t c = 0; c < ref.size() && c < B[(size_t) t].size(); ++c)
                        {
                            const auto& b = B[(size_t) t][c];
                            const long cap = (i + 1 < Rg.size() ? Rg[i + 1].start : (long) b.size()) - n0 - (long) (0.012 * sr);
                            for (long n = 0; n < std::min ((long) ref[c].size(), cap) && n0 + n < (long) b.size(); ++n)
                            {
                                const double r = ref[c][(size_t) n], x = b[(size_t) (n0 + n)];
                                eh += r * r; dh += (r - x) * (r - x);
                            }
                        }
                        e += eh; d += dh;
                        if (eh > 0) worstHit = std::max (worstHit, 10 * std::log10 (std::max (dh, 1e-30) / eh));
                    }
                    const double nullDb = 10 * std::log10 (std::max (d, 1e-30) / std::max (e, 1e-30));
                    nullLine << S.tracks[(size_t) t].mic << " " << juce::String (nullDb, 1) << " (worst hit " << juce::String (worstHit, 1) << ") dB  ";
                    INFO (S.tracks[(size_t) t].name << ": B vs the reference export " << nullDb << " dB, worst single hit " << worstHit << " dB");
                    CHECK (nullDb < -30);
                    CHECK (worstHit < -25);
                }
            }
            fprintf (stderr, "[corpus] %s null vs reference: %s\n", f.getFileName().toRawUTF8(), nullLine.toRawUTF8());

            // the export, rendered by Tracktion, against the reference export file by file
            if (st->mix && st->levels && st->levels->valid)
            {
                auto outDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_corpus_export");
                outDir.deleteRecursively(); outDir.createDirectory();
                int count = 0;
                const auto te0 = std::chrono::steady_clock::now();
                const auto exErr = model.exportTo (outDir, count);
                const double exS = std::chrono::duration<double> (std::chrono::steady_clock::now() - te0).count();
                CHECK (exErr.toStdString() == "");
                std::map<std::string, juce::MemoryBlock> refFiles;
                const int refCount = ld::hl::writeSession (Sx, *st->mix, *st->levels, [&] (const std::string& path, const juce::MemoryBlock& mb) { refFiles[path] = mb; });
                CHECK (count == refCount);
                juce::AudioFormatManager fm; fm.registerBasicFormats();
                double worst = -300, worstLen = 0; std::string worstName;
                for (auto& [path, mb] : refFiles)
                {
                    auto mine = outDir.getChildFile (juce::String (path));
                    INFO ("file " << path);
                    CHECK (mine.existsAsFile());
                    if (! mine.existsAsFile()) continue;
                    std::unique_ptr<juce::AudioFormatReader> ra (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, false)));
                    std::unique_ptr<juce::AudioFormatReader> rb (fm.createReaderFor (mine));
                    REQUIRE (ra != nullptr); REQUIRE (rb != nullptr);
                    CHECK (ra->numChannels == rb->numChannels);
                    CHECK (ra->sampleRate == rb->sampleRate);
                    CHECK (ra->bitsPerSample == rb->bitsPerSample);
                    const double lenDiff = std::abs ((double) ra->lengthInSamples - (double) rb->lengthInSamples) / ra->sampleRate;
                    worstLen = std::max (worstLen, lenDiff);
                    CHECK (ra->lengthInSamples == rb->lengthInSamples);   // to the frame
                    juce::AudioBuffer<float> a ((int) ra->numChannels, (int) ra->lengthInSamples), b ((int) rb->numChannels, (int) rb->lengthInSamples);
                    ra->read (&a, 0, a.getNumSamples(), 0, true, true); rb->read (&b, 0, b.getNumSamples(), 0, true, true);
                    double e = 0, d = 0;
                    for (int c = 0; c < std::min (a.getNumChannels(), b.getNumChannels()); ++c)
                        for (int n = 0; n < std::min (a.getNumSamples(), b.getNumSamples()); ++n)
                        { const double x = a.getSample (c, n), y = b.getSample (c, n); e += x * x; d += (x - y) * (x - y); }
                    const double nd = 10 * std::log10 (std::max (d, 1e-30) / std::max (e, 1e-30));
                    if (nd > worst) { worst = nd; worstName = path; }
                }
                fprintf (stderr, "[corpus] %s export: %d files (reference %d) in %.2f s, worst null %.1f dB (%s), worst length difference %.1f ms\n", f.getFileName().toRawUTF8(), count, refCount, exS, worst, worstName.c_str(), worstLen * 1000);
                CHECK (worst < -30);
                report << "  export: " << count << " files in " << juce::String (exS, 2) << " s, worst null vs reference " << juce::String (worst, 1) << " dB, length within " << juce::String (worstLen * 1000, 1) << " ms\n";
            }
            const int L = S.refIdx;
            for (int t = 0; t < (int) S.tracks.size(); ++t)
            {
                const auto& tr = S.tracks[(size_t) t];
                const double cA = t == L ? 1 : coherence (A[(size_t) L][0], A[(size_t) t][0], S.onsets, sr);
                const double cB = t == L ? 1 : coherence (B[(size_t) L][0], B[(size_t) t][0], S.onsets, sr);
                const bool timed = std::abs (tr.lag) > 0.01 || ! tr.ap.empty();
                double pA = 0, pB = 0;
                if (t != L && timed)
                {
                    INFO (tr.name << ": coherence with the lead " << cA << " -> " << cB);
                    CHECK (cB >= cA - 0.02);
                }
                else if (t != L)
                {
                    // kept on its own arrival (rooms): judge polarity at that arrival
                    pA = arrivalPeak (A[(size_t) L][0], A[(size_t) t][0], S.onsets, sr);
                    pB = arrivalPeak (B[(size_t) L][0], B[(size_t) t][0], S.onsets, sr);
                    INFO (tr.name << ": strongest arrival " << pA << " -> " << pB);
                    // reported, not checked: Auto Prep judges a room's polarity on the whole 60 ms window band by band
                    // (as Hitline), which can disagree with the first arrival on its own
                }
                micLine << tr.mic << (t == L ? "*" : "") << " (" << juce::String (model.shiftMs (t), 3) << ", " << (tr.pol < 0 ? "flip" : "-")
                        << ", " << (int) tr.ap.size() << ", " << juce::String (cA, 2) << "->" << juce::String (cB, 2)
                        << (pA != 0 ? ", arrival " + juce::String (pA, 2) + "->" + juce::String (pB, 2) : juce::String()) << ")  ";
            }
            const double so = std::chrono::duration<double> (t1 - t0).count(), sp = std::chrono::duration<double> (t2 - t1).count();
            report << f.getFileName() << " | " << (int) S.tracks.size() << " | " << V * RR << " | " << found << " | " << S.det.V << "x" << S.det.RR
                   << " | " << juce::String (so, 2) << " | " << juce::String (sp, 2) << " | " << juce::String (worstA, 7) << " | " << micLine << "\n";
            if (S.tinfo) { double lo = 1e9, hi = -1e9; for (double r : S.tinfo->ratios) { lo = std::min (lo, r); hi = std::max (hi, r); }
                           fprintf (stderr, "[corpus] %s tuning: %.1f .. %.1f cents\n", f.getFileName().toRawUTF8(), 1200 * std::log2 (lo), 1200 * std::log2 (hi)); }
            fprintf (stderr, "[corpus] %s: %d/%d hits, %dx%d, open %.2fs, prep %.2fs, A err %.2g\n  %s\n", f.getFileName().toRawUTF8(), found, V * RR,
                     S.det.V, S.det.RR, so, sp, worstA, micLine.toRawUTF8());
        }();
    }
    CHECK (tested > 0);   // a corpus filtered down to nothing tests nothing: that's a failure, not a pass
    if (const char* rep = std::getenv ("LD_CORPUS_REPORT")) juce::File (rep).appendText (report);
}

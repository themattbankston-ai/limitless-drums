// Real recordings through the sample build: open each take (a folder of one-mic files), Auto Prep, then render
// every mic through Tracktion with the processing off (A) and on (B). Checks the hit count against the file names,
// that A comes back as recorded (the timeline holds), and that B lines each touched mic up with the lead at least
// as well as A. LD_CORPUS = a folder of take folders; LD_CORPUS_REPORT = where to write the table (optional).
// Without LD_CORPUS the test says so loudly and does nothing.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "samples/SampleModel.h"
#include "core/Session.h"
#include "hl/Session.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <regex>

namespace te = tracktion;

namespace
{
std::vector<float> renderTrack (te::Edit& edit, int idx, double sr)
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
    p.mustRenderInMono = true;
    p.ditheringEnabled = false;
    te::Renderer::renderToFile ("Render", p);
    auto b = ld::readMono (temp.getFile());
    return std::vector<float> (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
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
        return;
    }
    juce::File root (dirEnv);
    REQUIRE (root.isDirectory());
    juce::String report = "take | mics | expect | found | V x RR | open s | prep s | A error | per mic (shift ms, flip, all-pass, coherence A -> B)\n";
    auto folders = root.findChildFiles (juce::File::findDirectories, false);
    folders.sort();
    REQUIRE (! folders.isEmpty());
    for (auto& f : folders)
    {
        if (const char* only = std::getenv ("LD_CORPUS_ONLY")) if (! f.getFileName().containsIgnoreCase (only)) continue;
        INFO ("take: " << f.getFileName());
        [&]
        {
            auto wavs = f.findChildFiles (juce::File::findFiles, false, "*.wav");
            CHECK (! wavs.isEmpty()); if (wavs.isEmpty()) return;
            std::smatch m;
            const std::string fn = wavs[0].getFileName().toStdString();
            const bool named = std::regex_search (fn, m, std::regex ("_V(\\d+)_RR(\\d+)")); CHECK (named); if (! named) return;
            const int V = std::stoi (m[1]), RR = std::stoi (m[2]);

            ld::samples::SampleModel model (ldtest::engine());
            auto t0 = std::chrono::steady_clock::now();
            const auto openErr = model.open ({ f }); CHECK (openErr.toStdString() == ""); if (openErr.isNotEmpty()) return;
            auto t1 = std::chrono::steady_clock::now();
            const auto err = model.autoPrepNow();
            auto t2 = std::chrono::steady_clock::now();
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
            std::vector<std::vector<float>> B;
            for (int t = 0; t < (int) S.tracks.size(); ++t) B.push_back (renderTrack (*edit, t, sr));
            model.setProcessed (false);
            model.setLevelMatch (false);   // A exactly as recorded
            std::vector<std::vector<float>> A;
            for (int t = 0; t < (int) S.tracks.size(); ++t)
            {
                A.push_back (renderTrack (*edit, (int) S.tracks.size() + t, sr));
                // A is the recording: same samples on the same timeline (mono: channel 0 of the take)
                const auto& orig = S.tracks[(size_t) t].ch[0];
                std::vector<float> o (orig.begin(), orig.end());
                worstA = std::max (worstA, maxDiff (A.back(), o));
                for (auto v : B[(size_t) t]) REQUIRE (std::isfinite (v));
            }
            CHECK (worstA < 1.0e-4);

            // B against the reference renderer (Hitline's export path): every hit of every mic
            juce::String nullLine;
            const auto* st = model.engineState();
            auto Sx = S;
            if (const char* tp = std::getenv ("LD_CORPUS_REF_TAPS")) Sx.tuneTaps = std::atoi (tp);
            if (st->mix && st->levels && st->levels->valid)
            {
                const auto& Rg = st->levels->regions;
                for (int t = 0; t < (int) S.tracks.size(); ++t)
                {
                    double e = 0, d = 0;
                    for (size_t i = 0; i < Rg.size(); ++i)
                    {
                        const auto ref = ld::hl::renderHitMic (Sx, *st->mix, t, i, Rg[i], sr, std::pow (10.0, model.hitGainDb (t, i) / 20));
                        const long n0 = Rg[i].start;
                        const long cap = (i + 1 < Rg.size() ? Rg[i + 1].start : (long) B[(size_t) t].size()) - n0 - (long) (0.012 * sr);
                        for (long n = 0; n < std::min ((long) ref[0].size(), cap); ++n)
                        {
                            if (n0 + n >= (long) B[(size_t) t].size()) break;
                            const double r = ref[0][(size_t) n], b = B[(size_t) t][(size_t) (n0 + n)];
                            e += r * r; d += (r - b) * (r - b);
                        }
                    }
                    if (std::getenv ("LD_CORPUS_DEBUG"))
                        for (size_t i = 0; i < std::min<size_t> (3, Rg.size()); ++i)
                        {
                            const auto ref = ld::hl::renderHitMic (S, *st->mix, t, i, Rg[i], sr, std::pow (10.0, model.hitGainDb (t, i) / 20));
                            const long n0 = Rg[i].start, W = std::min<long> ((long) ref[0].size(), (long) (0.1 * sr));
                            double best = -1e9; int bl = 0; double rr = 0, bb = 0;
                            for (int l = -300; l <= 300; ++l)
                            {
                                double c = 0;
                                for (long n = 0; n < W; ++n) { const long k = n0 + n + l; if (k >= 0 && k < (long) B[(size_t) t].size()) c += ref[0][(size_t) n] * B[(size_t) t][(size_t) k]; }
                                if (c > best) { best = c; bl = l; }
                            }
                            for (long n = 0; n < W; ++n) { rr += ref[0][(size_t) n] * ref[0][(size_t) n]; const long k = n0 + n + bl; if (k < (long) B[(size_t) t].size()) bb += B[(size_t) t][(size_t) k] * B[(size_t) t][(size_t) k]; }
                            {
                                juce::String segs;
                                const long seg = (long) (0.5 * sr);
                                for (long s0 = 0; s0 < (long) ref[0].size(); s0 += seg)
                                {
                                    double ee = 0, dd = 0;
                                    for (long n = s0; n < std::min (s0 + seg, (long) ref[0].size()) && n0 + n < (long) B[(size_t) t].size(); ++n)
                                    { const double r = ref[0][(size_t) n], b = B[(size_t) t][(size_t) (n0 + n)]; ee += r * r; dd += (r - b) * (r - b); }
                                    segs << juce::String (10 * std::log10 (std::max (dd, 1e-30) / std::max (ee, 1e-30)), 0) << " ";
                                }
                                fprintf (stderr, "  [dbg] null per 0.5 s: %s\n", segs.toRawUTF8());
                            }
                            fprintf (stderr, "  [dbg] %s hit %zu: region %ld..%ld ratio %.4f gain %.2f dB, best lag %d, level B/ref %.2f dB, corr %.3f, Arender peak %.3f orig peak %.3f\n", S.tracks[(size_t) t].mic.c_str(), i, Rg[i].start, Rg[i].end,
                                     model.hitRatio (i), model.hitGainDb (t, i), bl, 10 * std::log10 (bb / rr), best / std::sqrt (rr * bb),
                                     (double) *std::max_element (A[(size_t) t].begin(), A[(size_t) t].end()), (double) *std::max_element (S.tracks[(size_t) t].ch[0].begin(), S.tracks[(size_t) t].ch[0].end()));
                        }
                    if (const char* dumpDir = std::getenv ("LD_CORPUS_DUMP"))
                    {
                        const auto ref = ld::hl::renderHitMic (Sx, *st->mix, t, 0, Rg[0], sr, std::pow (10.0, model.hitGainDb (t, 0) / 20));
                        const auto& M = *st->mix;
                        const auto& pre = M.bus[(size_t) t].empty() ? M.proc[(size_t) t] : M.bus[(size_t) t];
                        auto dump = [&] (const juce::String& nm, const float* p, size_t n) { juce::File (dumpDir).getChildFile (nm).replaceWithData (p, n * sizeof (float)); };
                        const juce::String base = juce::String (t) + "_";
                        dump (base + "src.f32", pre[0].data(), pre[0].size());
                        dump (base + "ref.f32", ref[0].data(), ref[0].size());
                        dump (base + "b.f32", B[(size_t) t].data() + Rg[0].start, std::min<size_t> (ref[0].size(), B[(size_t) t].size() - (size_t) Rg[0].start));
                        juce::File (dumpDir).getChildFile (base + "meta.txt").replaceWithText (juce::String (Rg[0].start) + " " + juce::String (S.tracks[(size_t) t].lag, 6) + " " + juce::String (S.tracks[(size_t) t].pol) + " " + juce::String (model.hitGainDb (t, 0), 6) + " " + juce::String ((int) S.tracks[(size_t) t].ap.size()) + " " + juce::String (sr));
                    }
                    const double nullDb = 10 * std::log10 (std::max (d, 1e-30) / std::max (e, 1e-30));
                    nullLine << S.tracks[(size_t) t].mic << " " << juce::String (nullDb, 1) << " dB  ";
                    INFO (S.tracks[(size_t) t].name << ": B vs the reference export " << nullDb << " dB");
                    CHECK (nullDb < -30);
                }
            }
            fprintf (stderr, "[corpus] %s null vs reference: %s\n", f.getFileName().toRawUTF8(), nullLine.toRawUTF8());
            const int L = S.refIdx;
            for (int t = 0; t < (int) S.tracks.size(); ++t)
            {
                const auto& tr = S.tracks[(size_t) t];
                const double cA = t == L ? 1 : coherence (A[(size_t) L], A[(size_t) t], S.onsets, sr);
                const double cB = t == L ? 1 : coherence (B[(size_t) L], B[(size_t) t], S.onsets, sr);
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
                    pA = arrivalPeak (A[(size_t) L], A[(size_t) t], S.onsets, sr);
                    pB = arrivalPeak (B[(size_t) L], B[(size_t) t], S.onsets, sr);
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
            fprintf (stderr, "[corpus] %s: %d/%d hits, %dx%d, open %.2fs, prep %.2fs, A err %.2g\n  %s\n", f.getFileName().toRawUTF8(), found, V * RR,
                     S.det.V, S.det.RR, so, sp, worstA, micLine.toRawUTF8());
        }();
    }
    if (const char* rep = std::getenv ("LD_CORPUS_REPORT")) juce::File (rep).appendText (report);
}

// Real recordings through the sample build: open each take (a folder of one-mic files), Auto Prep, then render
// every mic through Tracktion with the processing off (A) and on (B). Checks the hit count against the file names,
// that A comes back as recorded (the timeline holds), and that B lines each touched mic up with the lead at least
// as well as A. LD_CORPUS = a folder of take folders; LD_CORPUS_REPORT = where to write the table (optional).
// Without LD_CORPUS the test says so loudly and does nothing.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "samples/SampleModel.h"
#include "core/Session.h"
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
            std::vector<std::vector<float>> A;
            for (int t = 0; t < (int) S.tracks.size(); ++t)
            {
                A.push_back (renderTrack (*edit, t, sr));
                // A is the recording: same samples on the same timeline (mono: channel 0 of the take)
                const auto& orig = S.tracks[(size_t) t].ch[0];
                std::vector<float> o (orig.begin(), orig.end());
                worstA = std::max (worstA, maxDiff (A.back(), o));
                for (auto v : B[(size_t) t]) REQUIRE (std::isfinite (v));
            }
            CHECK (worstA < 1.0e-4);
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

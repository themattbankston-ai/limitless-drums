// The rule book: off, every result is Hitline's; on, it reports what changed on real takes.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/Rules.h"
#include "../src/hl/Session.h"
#include <cstdio>

namespace
{
    /** How the lead and this mic add up, per band (lows < 150 Hz, mids, highs > 2 kHz), in dB against
        their powers added: above 0 they reinforce, below they cancel. The mic is level-matched first, so
        only the timing, polarity and phase EQ count. */
    std::array<double, 3> sumDb (const ld::hl::Session& S, const ld::hl::SessionTrack& tr, ld::hl::F32* pairOut = nullptr)
    {
        const auto& L = S.tracks[(size_t) S.refIdx].mono;
        auto x = tr.ap.empty() ? tr.mono : ld::hl::applyAllpass (tr.mono, S.sr, tr.ap);
        const long n = (long) std::min (L.size(), x.size());
        std::vector<float> y ((size_t) n, 0.0f);
        for (long i = 0; i < n; ++i)
        {
            const double p = i + tr.lag; const long a = (long) std::floor (p); const double f = p - a;
            if (a >= 0 && a + 1 < (long) x.size()) y[(size_t) i] = (float) (tr.pol * (x[(size_t) a] * (1 - f) + x[(size_t) a + 1] * f));
        }
        const long W = (long) (0.3 * S.sr);
        double eL = 0, eY = 0;
        for (long o : S.onsets) for (long i = o; i < o + W && i < n; ++i) { eL += L[(size_t) i] * L[(size_t) i]; eY += y[(size_t) i] * y[(size_t) i]; }
        const double g = eY > 0 ? std::sqrt (eL / eY) : 0;
        if (pairOut)
        {
            // the lead and this mic at equal level, the first eight hits, peak -3 dB
            const long end = std::min (n, S.onsets.size() > 8 ? S.onsets[8] : n);
            const long start = std::max (0L, S.onsets.empty() ? 0L : S.onsets[0] - (long) (0.05 * S.sr));
            pairOut->assign ((size_t) (end - start), 0.0f);
            float pk = 0;
            for (long i = start; i < end; ++i) { const float v = (float) (L[(size_t) i] + g * y[(size_t) i]); (*pairOut)[(size_t) (i - start)] = v; pk = std::max (pk, std::abs (v)); }
            if (pk > 0) for (auto& v : *pairOut) v *= 0.708f / pk;
        }
        std::array<double, 3> out {};
        const double fcs[2] = { 150, 2000 };
        for (int b = 0; b < 3; ++b)
        {
            double es = 0, ep = 0;
            auto filt = [&] (const float* v, std::vector<double>& o2)
            {
                o2.assign ((size_t) n, 0);
                const double k1 = std::exp (-2 * M_PI * fcs[0] / S.sr), k2 = std::exp (-2 * M_PI * fcs[1] / S.sr);
                double a1 = 0, a2 = 0, c1 = 0, c2 = 0;
                for (long i = 0; i < n; ++i)
                {
                    a1 = (1 - k1) * v[i] + k1 * a1; a2 = (1 - k1) * a1 + k1 * a2;   // < 150
                    c1 = (1 - k2) * v[i] + k2 * c1; c2 = (1 - k2) * c1 + k2 * c2;   // < 2k
                    o2[(size_t) i] = b == 0 ? a2 : b == 1 ? c2 - a2 : v[i] - c2;
                }
            };
            std::vector<double> fl, fy; filt (L.data(), fl); filt (y.data(), fy);
            for (long o : S.onsets)
                for (long i = o; i < o + W && i < n; ++i)
                {
                    const double a = fl[(size_t) i], c = g * fy[(size_t) i];
                    es += (a + c) * (a + c); ep += a * a + c * c;
                }
            out[(size_t) b] = ep > 0 ? 10 * std::log10 (std::max (1e-12, es / ep)) : 0;
        }
        return out;
    }

    void report (const juce::File& folder, const juce::String& artic)
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
        std::vector<juce::File> fv (files.begin(), files.end());
        auto st = ld::openTake (fv, artic);
        if (st->session.tracks.empty()) return;
        ld::hl::runDetect (st->session);
        auto off = st->session, on = st->session;
        on.rules = true;
        ld::hl::runAnalyze (off); ld::hl::runAnalyze (on);
        std::printf ("\n== %s  (lead: %s, %d hits)\n", folder.getFileName().toRawUTF8(), off.tracks[(size_t) off.refIdx].mic.c_str(), (int) off.onsets.size());
        for (size_t i = 0; i < on.tracks.size(); ++i)
        {
            if ((int) i == on.refIdx) continue;
            const auto& a = off.tracks[i]; const auto& b = on.tracks[i];
            const double ms = 1000 / on.sr;
            const bool changed = std::abs (a.lag - b.lag) > 0.5 || a.pol != b.pol || a.alignTo != b.alignTo || a.ap.size() != b.ap.size();
            if (changed)
            {
                ld::hl::F32 pa, pb;
                const auto x = sumDb (off, a, &pa), y = sumDb (on, b, &pb);
                const auto ab = juce::SystemStats::getEnvironmentVariable ("LD_RULES_AB", {});
                if (ab.isNotEmpty())
                {
                    auto dir = juce::File (ab); dir.createDirectory();
                    const auto stem = folder.getParentDirectory().getFileName() + " " + folder.getFileName() + " - " + juce::String (b.mic);
                    dir.getChildFile (stem + " - A as Hitline.wav").replaceWithData (ld::hl::encodeWav ({ pa }, off.sr, 24).getData(), ld::hl::encodeWav ({ pa }, off.sr, 24).getSize());
                    dir.getChildFile (stem + " - B rule book.wav").replaceWithData (ld::hl::encodeWav ({ pb }, on.sr, 24).getData(), ld::hl::encodeWav ({ pb }, on.sr, 24).getSize());
                }
                std::printf ("      adds up (low/mid/high dB)  Hitline %+5.1f %+5.1f %+5.1f   rules %+5.1f %+5.1f %+5.1f\n", x[0], x[1], x[2], y[0], y[1], y[2]);
            }
            std::printf ("  %-14s heard as %-6s  Hitline %+7.2f ms %s %s%s  ->  rules %+7.2f ms %s %s%s  %s\n", b.mic.c_str(), ld::hl::effectiveRole (on, (int) i).c_str(),
                         a.lag * ms, a.pol < 0 ? "flip" : "    ", a.alignTo.c_str(), a.ap.empty() ? "" : " +EQ",
                         b.lag * ms, b.pol < 0 ? "flip" : "    ", b.alignTo.c_str(), b.ap.empty() ? "" : " +EQ", changed ? "CHANGED" : "");
        }
    }
}

TEST_CASE ("Rule book report on real takes", "[.report]")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_RULES_TAKES", {});
    if (root.isEmpty()) return;
    for (auto& d : juce::File (root).findChildFiles (juce::File::findDirectories, false))
        report (d, "Center");
    const auto ps = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (ps.isNotEmpty())
        for (auto& d : juce::File (ps).findChildFiles (juce::File::findDirectories, false))
            for (auto& a : d.findChildFiles (juce::File::findDirectories, false)) report (a, a.getFileName());
}

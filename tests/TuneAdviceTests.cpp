// Tuning advice: cymbals are recognised by their sound, and a drum's pitch reading is judged before tuning is
// recommended; the attack-keeping varispeed keeps every mic's timing.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/Tune.h"
#include <cstdio>

static std::shared_ptr<ld::EngineState> openFolder (const juce::File& d, const std::string& drum)
{
    auto files = d.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    ld::TakeOptions o; o.drum = drum;
    auto st = ld::openTake (fv, "Center", o);
    ld::hl::runDetect (st->session);
    return st;
}

TEST_CASE ("Tuning advice")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_TUNE_TAKES", {});
    if (root.isEmpty()) { WARN ("LD_TUNE_TAKES not set; skipped"); return; }
    const juce::File R (root);
    {
        // a hat, even when it's been called a snare
        auto st = openFolder (R.getChildFile ("PANDA_HAT"), "snare");
        CHECK (ld::hl::soundsLikeCymbal (st->session));
        CHECK (ld::hl::tuneAdvice (st->session) == "cymbal");
    }
    for (auto [folder, drum] : { std::pair { "PANDA_TOM", "tom" }, std::pair { "PANDA_KICK", "kick" }, std::pair { "GGD_KICK", "kick" }, std::pair { "GGD_SNARE", "snare" } })
    {
        auto f = R.getChildFile (folder);
        if (! f.isDirectory()) f = juce::File (juce::SystemStats::getEnvironmentVariable ("LD_RULES_TAKES", {})).getChildFile (folder);
        if (! f.isDirectory()) continue;
        auto st = openFolder (f, drum);
        auto& S = st->session;
        CHECK (! ld::hl::soundsLikeCymbal (S));
        ld::hl::runPitch (S);
        int n = 0; for (auto& h : S.pitch.hits) if (h) ++n;
        std::printf ("%s: %d/%d hits pitched, est %.1f Hz, advice '%s'\n", folder, n, (int) S.pitch.hits.size(), S.pitch.est, ld::hl::tuneAdvice (S).c_str());
        if (std::string (drum) != "kick") CHECK (ld::hl::tuneAdvice (S) == "");   // kicks may honestly have no steady pitch
    }
    // noise has no pitch
    {
        auto st = openFolder (R.getChildFile ("PANDA_TOM"), "tom");
        auto& S = st->session;
        juce::Random rnd (7);
        for (auto& tr : S.tracks)
        {
            // the drum's own envelope on low-passed noise: a dead thud with no note in it
            for (auto& c : tr.ch.mut())
            {
                float lp = 0;
                for (size_t i = 0; i < c.size(); ++i) { lp += 0.08f * ((rnd.nextFloat() * 2 - 1) - lp); c[i] = std::abs (c[i]) > 1e-4f ? lp * 4 * std::abs (c[i]) : 0.0f; }
            }
            for (size_t i = 0; i < tr.mono.size(); ++i) tr.mono[i] = tr.ch[0][i];
        }
        ld::hl::runPitch (S);
        int n = 0; for (auto& h : S.pitch.hits) if (h) ++n;
        std::printf ("noise: %d/%d hits pitched, advice '%s'\n", n, (int) S.pitch.hits.size(), ld::hl::tuneAdvice (S).c_str());
        CHECK (ld::hl::tuneAdvice (S) == "nopitch");
    }
}

TEST_CASE ("Keeping the attack: the first 3 ms as recorded, the body tuned, every mic's timing held")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_TUNE_TAKES", {});
    if (root.isEmpty()) { WARN ("LD_TUNE_TAKES not set; skipped"); return; }
    auto st = openFolder (juce::File (root).getChildFile ("PANDA_TOM"), "tom");
    auto& S = st->session;
    const auto& x = S.tracks[(size_t) S.refIdx].mono;
    const double sr = S.sr, on = (double) S.onsets[2], step = std::pow (2.0, 2.0 / 12);   // up two semitones
    const long pre = 200, len = (long) (0.3 * sr);
    const auto plain = ld::hl::renderTuned (x, on, pre, len, 0, 1, 1, step, 0, (long) x.size(), 0, 64);
    const auto flat = ld::hl::renderTuned (x, on, pre, len, 0, 1, 1, 1.0, 0, (long) x.size(), 0, 64);
    ld::hl::AttackKeep k { (long) (0.003 * sr), (long) (0.002 * sr), 1.0 };
    const auto kept = ld::hl::renderTuned (x, on, pre, len, 0, 1, 1, step, 0, (long) x.size(), 0, 64, k);
    // the attack is the recording (up to the filter); the body is the tuned body, a constant offset later
    double d = 0, e = 0;
    for (long n = pre + 10; n < pre + k.keepN - 10; ++n) { d += std::pow (kept[(size_t) n] - flat[(size_t) n], 2); e += std::pow (flat[(size_t) n], 2); }
    CHECK (d / e < 1e-3);
    const double off = k.keepN * (1 - step) + k.rampN * (1 - step) / 2;   // source samples the attack held back
    // kept at output n reads source (on + u*step + off): the plain tuned render at n + off/step
    double d2 = 0, e2 = 0;
    for (long n = pre + k.keepN + k.rampN + 50; n < pre + k.keepN + k.rampN + 3000; ++n)
    {
        const double q = n + off / step; const long q0 = (long) std::floor (q); const double f = q - q0;
        const double want = plain[(size_t) q0] * (1 - f) + plain[(size_t) q0 + 1] * f;
        d2 += std::pow (kept[(size_t) n] - want, 2); e2 += want * want;
    }
    CHECK (d2 / e2 < 1e-3);

    // A/B files for listening, when asked for
    const auto ab = juce::SystemStats::getEnvironmentVariable ("LD_TUNE_AB", {});
    if (ab.isNotEmpty())
    {
        ld::hl::runAnalyze (S);
        ld::hl::runPitch (S);
        REQUIRE (S.tuneInfo);
        for (int semis : { 1, 2, 3 })
        {
            S.tune.on = true; S.tune.target = std::to_string (S.tuneInfo->nearest + semis);
            ld::hl::computeTune (S);
            const auto M = ld::hl::renderMix (S);
            const auto L = ld::hl::measureLevels (S, M);
            for (bool keepIt : { false, true })
            {
                S.keepAttack = keepIt; S.tuneTaps = 64;
                ld::hl::Chs mix (1);
                for (size_t i = 0; i < S.onsets.size(); ++i)
                {
                    ld::hl::StemOptions o; o.mono = true; o.taps = 64;
                    const auto h = ld::hl::renderHitStems (S, M, L, i, o);
                    ld::hl::F32 m ((size_t) h.len, 0.0f);
                    for (auto& stc : h.stems) for (auto& c : stc) for (size_t q = 0; q < c.size() && q < m.size(); ++q) m[q] += c[q];
                    m.resize ((size_t) (sr * 1.2), 0.0f);
                    mix[0].insert (mix[0].end(), m.begin(), m.end());
                }
                float pk = 1e-9f; for (float v : mix[0]) pk = std::max (pk, std::abs (v));
                for (auto& v : mix[0]) v *= 0.7f / pk;
                const auto mb = ld::hl::encodeWav (mix, sr, 24);
                juce::File (ab).getChildFile (juce::String ("Tom up ") + juce::String (semis) + (semis == 1 ? " semitone" : " semitones") + (keepIt ? " - B attack kept.wav" : " - A plain varispeed.wav"))
                    .replaceWithData (mb.getData(), mb.getSize());
            }
        }
    }
}

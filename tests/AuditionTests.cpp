// The audition player on a real session: every play mode renders, plays to its end, stays under the
// preview ceiling, and the heart monitor gets fed.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Audition.h"

TEST_CASE ("Every play mode plays a real session to its end")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    const auto folder = juce::File (root).getChildFile ("SNARE 1 - MH/Center");
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    auto st = ld::prepareTake (fv, "Center");
    REQUIRE (st->prepared);
    ld::renderAndMeasure (*st);
    REQUIRE (st->levels);
    juce::AudioDeviceManager dm;
    ld::Audition au (dm);
    au.setEngine (st);
    const auto& S = st->session;
    struct M { const char* name; std::vector<ld::PlayEvent> ev; bool mono, chain; };
    std::vector<M> ms { { "hit", ld::modes::hit (S, 20), false, false }, { "layer", ld::modes::layer (S, 20), false, false },
                        { "sweep", ld::modes::sweep (S, 20), false, true }, { "groove", ld::modes::groove (S), false, false },
                        { "mono", ld::modes::hit (S, 20), true, false } };
    for (auto& m : ms)
    {
        INFO (m.name);
        REQUIRE (! m.ev.empty());
        ld::Audition::Options o; o.mode = m.name; o.mono = m.mono; o.chain = m.chain;
        au.play (m.ev, o);
        REQUIRE (au.isPlaying());
        std::vector<float> L (512), R (512);
        double peak = 0, energy = 0;
        int guard = 0, monitor = 0;
        ld::Audition::MonitorPoint pts[512];
        while (au.position() < au.phraseEnd() + 0.1 && ++guard < 100000)
        {
            au.renderBlock (L.data(), R.data(), 512, 48000);
            for (int q = 0; q < 512; ++q) { peak = std::max ({ peak, (double) std::abs (L[(size_t) q]), (double) std::abs (R[(size_t) q]) }); energy += (double) L[(size_t) q] * L[(size_t) q]; }
            monitor += au.drainMonitor (pts, 512);
        }
        CHECK (guard < 100000);
        CHECK (energy > 1);
        CHECK (peak <= 0.8915);
        CHECK (monitor > 100);
        UNSCOPED_INFO (m.name << ": " << m.ev.size() << " hits, " << au.phraseEnd() << " s, peak " << peak);
        au.stop();
    }
    CHECK (ld::modes::groove (S).size() == 36 + 8);
    CHECK (ld::modes::rudiments (S).size() > 70);
}

TEST_CASE ("The pair plays as recorded or aligned whichever side the A/B switch is on, and the gain is B minus A")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    const auto folder = juce::File (root).getChildFile ("SNARE 1 - MH/Center");
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    auto st = ld::prepareTake (fv, "Center");
    REQUIRE (st->prepared);
    ld::renderAndMeasure (*st);
    juce::AudioDeviceManager dm;
    ld::Audition au (dm);
    au.setEngine (st);
    const auto& S = st->session;
    int mic = -1;
    for (int i = 0; i < (int) S.tracks.size(); ++i) if (i != S.refIdx && S.tracks[(size_t) i].mic == "Bottom") mic = i;
    REQUIRE (mic >= 0);
    // a console that would hide the pair: the mic muted, the lead soloed away
    std::vector<float> g (S.tracks.size(), 0.0f);
    au.setConsole (g, std::vector<float> (S.tracks.size(), 0.0f), std::vector<bool> (S.tracks.size(), false), 1, 0);
    auto take = [&] (int side, bool processedSwitch, bool gain)
    {
        au.setProcessed (processedSwitch);
        ld::Audition::Options o; o.mono = true; o.only = { S.refIdx, mic }; o.side = side; o.console = false; o.gain = gain; o.loudnessMatch = false;
        au.play (ld::modes::hit (S, 20), o);
        std::vector<float> out, L (512), R (512);
        int guard = 0;
        while (au.position() < au.phraseEnd() + 0.05 && ++guard < 20000)
        {
            au.renderBlock (L.data(), R.data(), 512, S.sr);
            out.insert (out.end(), L.begin(), L.end());
        }
        au.stop();
        return out;
    };
    auto same = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double d = 0, e = 0;
        for (size_t q = 0; q < std::min (a.size(), b.size()); ++q) { d += (a[q] - b[q]) * (a[q] - b[q]); e += a[q] * a[q]; }
        return e > 1e-6 && d / e < 1e-9;
    };
    const auto recA = take (0, false, false), recB = take (0, true, false);   // "Pair as recorded" with the switch on A, then on B
    const auto alA = take (1, false, false), alB = take (1, true, false);     // "Pair aligned"
    CHECK (same (recA, recB));
    CHECK (same (alA, alB));
    CHECK (! same (recA, alA));
    // hear what you gain: exactly aligned minus as recorded, at the same scale
    const auto gain = take (-1, true, true);
    double d = 0, e = 0;
    for (size_t q = 0; q < std::min ({ gain.size(), alA.size(), recA.size() }); ++q)
    {
        const double want = alA[q] - recA[q];
        d += (gain[q] - want) * (gain[q] - want); e += want * want;
    }
    CHECK (e > 1e-6);
    CHECK (d / e < 1e-3);
}

TEST_CASE ("Song mode playback: the performance from the playhead, with the reverb return")
{
    const auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("LD_SONG", "/home/claude/songtest/song60"));
    if (! dir.isDirectory()) { WARN ("no test song; skipped"); return; }
    ld::hl::setReverbLibrary (juce::JSON::parse (juce::File (LD_SOURCE_DIR).getChildFile ("assets/irlib.json")));
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    ld::TakeOptions o; o.drum = "kit";
    auto st = ld::prepareTake (fv, dir.getFileName(), o);
    REQUIRE (st->prepared);
    st->session.song.set.tightness = 0.5;
    for (auto& t : st->session.tracks) t.strip.r1 = -6;
    ld::renderAndMeasure (*st);
    juce::AudioDeviceManager dm;
    ld::Audition au (dm);
    au.setEngine (st);
    std::vector<float> L (1024), R (1024);
    auto energyOf = [&] (double seconds)
    {
        double e = 0;
        for (int b = 0; b < (int) (seconds * 48000 / 1024); ++b) { au.renderBlock (L.data(), R.data(), 1024, 48000); for (float v : L) e += (double) v * v; }
        return e;
    };
    au.playSong (10, 4);
    REQUIRE (au.isPlaying());
    const double e1 = energyOf (2);
    CHECK (e1 > 1);
    CHECK (au.position() > 1.9);
    CHECK (au.phraseEnd() > 3.9); CHECK (au.phraseEnd() < 4.1);
    // the reverb return is in it: the same stretch with every send off sounds different
    au.stop();
    for (auto& t : st->session.tracks) t.strip.r1 = -60;
    ld::renderAndMeasure (*st);
    au.setEngine (st);
    au.playSong (10, 4);
    const double e2 = energyOf (2);
    CHECK (std::abs (e1 - e2) > e2 * 0.01);   // the returns changed what you hear
}

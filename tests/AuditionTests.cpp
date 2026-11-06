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
    CHECK (ld::modes::groove (S).size() > 36 + 8);
}

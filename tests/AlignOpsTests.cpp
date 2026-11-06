// The Align step's controls on a real session: nudges, flips, back to measured, timing modes, the overlay heat.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"

TEST_CASE ("Align controls")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    auto files = juce::File (root).getChildFile ("SNARE 1 - MH/Center").findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    auto st = ld::prepareTake (fv, "Center");
    REQUIRE (st->prepared);
    auto& S = st->session;
    int b = -1, room = -1;
    for (int i = 0; i < (int) S.tracks.size(); ++i)
    {
        if (S.tracks[(size_t) i].mic == "Bottom") b = i;
        if (S.tracks[(size_t) i].mic == "Room Far") room = i;
    }
    REQUIRE (b >= 0); REQUIRE (room >= 0);
    const double lag0 = S.tracks[(size_t) b].lag; const int pol0 = S.tracks[(size_t) b].pol;
    ld::hl::nudgeLag (S, b, -4);   // earlier by one sample (Hitline: lag grows)
    CHECK (std::abs (S.tracks[(size_t) b].lag - (std::round ((lag0 + 1) * 4) / 4)) < 1e-9);
    CHECK (S.tracks[(size_t) b].manualLag);
    ld::hl::flipMic (S, b);
    CHECK (S.tracks[(size_t) b].pol == -pol0);
    ld::hl::resetMic (S, b);
    CHECK (S.tracks[(size_t) b].lag == lag0);
    CHECK (S.tracks[(size_t) b].pol == pol0);
    // the heat peaks at or near where the aligner put it
    const auto H = ld::hl::overlayHeat (S, b, 20, true);
    REQUIRE (! H.curve.empty());
    CHECK (std::abs (H.best - H.cur) <= 0.0015 * S.sr);
    CHECK (H.curC <= H.bestC + 1e-12);
    // timing: as recorded leaves every mic where it was recorded; tight lines them up again
    ld::hl::setTiming (S, "natural");
    for (int i = 0; i < (int) S.tracks.size(); ++i) if (i != S.refIdx) { CHECK (S.tracks[(size_t) i].alignTo == "none"); CHECK (S.tracks[(size_t) i].lag == 0); }
    ld::hl::setTiming (S, "tight");
    CHECK (S.tracks[(size_t) b].lag == lag0);
    CHECK (S.tracks[(size_t) room].alignTo == "lead");
    ld::hl::setAlignTo (S, room, "none");
    CHECK (S.tracks[(size_t) room].lag == 0);
}

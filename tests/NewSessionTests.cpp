// New Session: libraries persist, settings carry from one session to the next, and opening a take with the
// window's choices loads it (named, the lead first) without running Auto prep.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/app/Library.h"

TEST_CASE ("Libraries save and load")
{
    const auto f = juce::File::createTempFile (".json");
    {
        ld::LibraryStore s (f);
        REQUIRE (s.libraries.size() == 1);
        CHECK (s.add ("Limitless Snares"));
        CHECK (! s.add ("Limitless Snares"));
        CHECK (! s.add ("  "));
        ld::LibrarySession x; x.id = "a1"; x.name = "Snare 1"; x.drum = "snare"; x.artic = "Center"; x.vel = 6; x.rr = 5;
        auto o = juce::var (new juce::DynamicObject()); o.getDynamicObject()->setProperty ("k", 3); x.settings = o;
        s.put ("Limitless Snares", x);
        x.name = "Snare 1 renamed"; s.put ("Limitless Snares", x);   // same id replaces
        s.current = "Limitless Snares";
        REQUIRE (s.save());
    }
    ld::LibraryStore s (f);
    REQUIRE (s.libraries.size() == 2);
    CHECK (s.current == "Limitless Snares");
    REQUIRE (s.find ("Limitless Snares")->sessions.size() == 1);
    const auto* x = s.findSession ("a1");
    REQUIRE (x != nullptr);
    CHECK (x->name == "Snare 1 renamed");
    CHECK (x->rr == 5);
    CHECK ((int) x->settings["k"] == 3);
    CHECK (s.libraries[0].colour != s.libraries[1].colour);
    f.deleteFile();
}

TEST_CASE ("Settings carry from one session to a new one")
{
    ld::hl::Session a;
    a.drum = "snare"; a.lev.offsetDb = -3.5; a.lev.curve = "even"; a.exp.bits = 24; a.exp.rate = "48000"; a.dnOn = false;
    a.tone.bright = 2; a.shape.atk = 1.5; a.busFader = -2; a.room.on = true; a.room.ret = -9; a.minGapMs = 220;
    ld::hl::SessionTrack t; t.mic = "Top"; t.fader = -4; t.strip.pan = 0.25; t.strip.tone.crack = 1; a.tracks.push_back (t);
    t.mic = "Bottom"; t.fader = -7; a.tracks.push_back (t);
    const auto v = juce::JSON::parse (juce::JSON::toString (ld::hl::settingsToVar (a)));   // through text, as saved

    ld::hl::Session b;
    ld::hl::SessionTrack u; u.mic = "top"; b.tracks.push_back (u); u.mic = "Room"; u.fader = 1; b.tracks.push_back (u);
    ld::hl::applySettings (b, v);
    CHECK (b.lev.offsetDb == -3.5);
    CHECK (b.lev.curve == "even");
    CHECK (b.exp.bits == 24);
    CHECK (b.exp.rate == "48000");
    CHECK (! b.dnOn);
    CHECK (b.tone.bright == 2);
    CHECK (b.shape.atk == 1.5);
    CHECK (b.busFader == -2);
    CHECK (b.room.on);
    CHECK (b.room.ret == -9);
    CHECK (b.minGapMs == 220);
    CHECK (b.tracks[0].fader == -4);          // matched by mic name, any case
    CHECK (b.tracks[0].strip.pan == 0.25);
    CHECK (b.tracks[0].strip.tone.crack == 1);
    CHECK (b.tracks[1].fader == 1);           // a mic the settings don't name keeps its own
    ld::hl::applySettings (b, {});            // nothing to apply: unchanged
    CHECK (b.lev.offsetDb == -3.5);
}

TEST_CASE ("A new session loads the take without Auto prep, with the window's choices")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    const auto folder = juce::File (root).getChildFile ("SNARE 1 - MH/Center");
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());

    auto plain = ld::openTake (fv, "Center");
    REQUIRE (! plain->session.tracks.empty());
    CHECK (! plain->prepared);
    CHECK (plain->session.tracks[0].mic == plain->session.tracks[(size_t) plain->session.refIdx].mic);

    ld::TakeOptions o; o.drum = "snare"; o.artic = "Rimshot"; o.vel = 3; o.rr = 4;
    ld::hl::Session withOffset = plain->session; withOffset.lev.offsetDb = -2;
    o.settings = ld::hl::settingsToVar (withOffset);
    auto st = ld::openTake (fv, "Center", o);
    CHECK (st->session.artic == "Rimshot");
    CHECK (st->session.det.V == 3);
    CHECK (st->session.det.RR == 4);
    CHECK (st->session.lev.offsetDb == -2);
    CHECK (! st->prepared);

    // the same take with the counts the names give prepares as before
    auto full = ld::prepareTake (fv, "Center");
    CHECK (full->prepared);
}

TEST_CASE ("A stereo mic's two pans: hard left/right is Hitline's stereo pan, otherwise each channel pans on its own")
{
    using ld::hl::trackPanGains; using ld::hl::panGains;
    for (double p : { -1.0, -0.4, 0.0, 0.3, 1.0 })
    {
        const auto a = trackPanGains (p, -1, 1, true), b = panGains (p, true);
        for (int k = 0; k < 4; ++k) CHECK (a[(size_t) k] == b[(size_t) k]);   // bit-identical: exports and parity unchanged
    }
    // both channels to the centre: equal-power mono of each
    const auto c = trackPanGains (0, 0, 0, true);
    CHECK (std::abs (c[0] - std::sqrt (0.5)) < 1e-12); CHECK (std::abs (c[3] - std::sqrt (0.5)) < 1e-12);
    // channels swapped: left channel hard right, right channel hard left
    const auto s = trackPanGains (0, 1, -1, true);
    CHECK (std::abs (s[0]) < 1e-12); CHECK (std::abs (s[2] - 1) < 1e-12); CHECK (std::abs (s[1] - 1) < 1e-12); CHECK (std::abs (s[3]) < 1e-12);
    // mono tracks ignore the stereo pans
    const auto m = trackPanGains (0.5, 0, 0, false), n = panGains (0.5, false);
    for (int k = 0; k < 4; ++k) CHECK (m[(size_t) k] == n[(size_t) k]);
}

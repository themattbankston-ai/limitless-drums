// The two reverb units (R1, R2): post-fader sends, the drum bus send, nothing returned when nothing sends,
// a send doubling the return, mute silencing a channel's send, settings saved and restored.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/FxSat.h"

static double rms (const ld::hl::Chs& c)
{
    double s = 0; size_t n = 0;
    for (auto& x : c) for (float v : x) { s += (double) v * v; ++n; }
    return n ? std::sqrt (s / (double) n) : 0;
}

TEST_CASE ("Reverb sends R1 and R2")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    ld::hl::setReverbLibrary (juce::JSON::parse (juce::File (LD_SOURCE_DIR).getChildFile ("assets/irlib.json")));
    auto files = juce::File (root).getChildFile ("SNARE 1 - MH/Center").findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    auto st = ld::prepareTake (fv, "Center");
    REQUIRE (st->prepared);
    auto S = st->session;
    const int t = S.refIdx;

    // nothing sends: no returns
    auto M = ld::hl::renderMix (S);
    CHECK (M.rev[0].empty()); CHECK (M.rev[1].empty());

    // one channel at -12 into R1: a stereo return on R1 only
    S.tracks[(size_t) t].strip.r1 = -12;
    M = ld::hl::renderMix (S);
    REQUIRE (M.rev[0].size() == 2); CHECK (M.rev[1].empty());
    const double a = rms (M.rev[0]);
    CHECK (a > 0);

    // +6 dB more send: the return is twice as loud (linear reverb)
    S.tracks[(size_t) t].strip.r1 = -6;
    const double b = rms (ld::hl::renderMix (S).rev[0]);
    CHECK (std::abs (b / a - 2.0) < 0.02);

    // post-fader: the fader down 6 dB halves it again
    S.tracks[(size_t) t].fader = -6;
    const double c = rms (ld::hl::renderMix (S).rev[0]);
    CHECK (std::abs (c / a - 1.0) < 0.02);

    // muted: no send
    S.tracks[(size_t) t].mute = true;
    for (auto& tr : S.tracks) if (&tr != &S.tracks[(size_t) t]) tr.strip.r1 = -60;
    CHECK (rms (ld::hl::renderMix (S).rev[0]) == 0);
    S.tracks[(size_t) t].mute = false; S.tracks[(size_t) t].fader = 0; S.tracks[(size_t) t].strip.r1 = -60;

    // the drum bus send alone feeds R2 from the whole kit
    S.busR2 = 0;
    M = ld::hl::renderMix (S);
    CHECK (M.rev[0].empty()); REQUIRE (M.rev[1].size() == 2); CHECK (rms (M.rev[1]) > 0);

    // the types map to the measured models
    ld::hl::VerbUnit v; v.type = "plate"; CHECK (ld::hl::verbParams (v)["model"].toString() == "emt");
    v.type = "hall"; CHECK (ld::hl::verbParams (v)["prog"].toString() == "Small Hall A");

    // saved and restored
    S.verbs[0].type = "chamber"; S.verbs[0].decay = 1.7; S.tracks[(size_t) t].strip.r2 = -9;
    const auto saved = ld::hl::settingsToVar (S);
    auto S2 = st->session;
    ld::hl::applySettings (S2, saved);
    CHECK (S2.verbs[0].type == "chamber"); CHECK (S2.verbs[0].decay == 1.7);
    CHECK (S2.busR2 == 0); CHECK (S2.tracks[(size_t) t].strip.r2 == -9);
}

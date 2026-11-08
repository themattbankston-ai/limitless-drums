// Standard: the knobs land the tone close to the pro target (closer than flat); Match envelope moves the shape.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/Shape.h"

TEST_CASE ("Standard and Match envelope")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    ld::hl::setLibraryReference (juce::JSON::parse (juce::File (LD_SOURCE_DIR).getChildFile ("assets/libref.json")));
    auto files = juce::File (root).getChildFile ("SNARE 1 - MH/Center").findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    auto st = ld::prepareTake (fv, "Center");
    REQUIRE (st->prepared);
    auto S = st->session;
    // a mix with the tone pass listening (scans), as the app has it
    S.tone.match = 0.01;
    const auto M = ld::hl::renderMix (S);
    S.tone.match = 0;
    const auto [T, H] = ld::hl::standardMoves (S, M, -1);
    INFO ("bright " << T.bright << " crack " << T.crack << " dec " << H.dec << " sus " << H.sus << " rel " << H.rel);
    CHECK (T.match == 0);
    CHECK (std::abs (T.bright) <= 1); CHECK (std::abs (T.crack) <= 1);
    CHECK (T.smooth > 0); CHECK (T.tame > 0);

    // the fitted knobs land closer to the pro curve than flat does
    auto Starget = S; Starget.tone.match = 0.6;
    ld::hl::ToneEngine Et (Starget, M.scan);
    const int v = S.det.V - 1;
    ld::hl::F64 target (30, 0.0); int n = 0;
    for (int t = 0; t < (int) S.tracks.size(); ++t) if (M.scan[(size_t) t]) { auto mb = Et.matchBands (t, v); for (int j = 0; j < 30; ++j) target[(size_t) j] += mb[(size_t) j]; ++n; }
    REQUIRE (n > 0);
    for (auto& x : target) x /= n;
    auto err = [&] (double b, double c)
    {
        auto Q = S; Q.tone.bright = b; Q.tone.crack = c;
        ld::hl::ToneEngine E (Q, M.scan);
        const auto& B = ld::hl::bands3(); double e = 0;
        for (int j = 0; j < 30; ++j) if (B[(size_t) j] >= 100 && B[(size_t) j] <= 12000) { const double d = E.toneAt (0, nullptr, B[(size_t) j]) - target[(size_t) j]; e += d * d; }
        return e;
    };
    CHECK (err (T.bright, T.crack) <= err (0, 0));

    // Match envelope: a reference envelope with more tail changes the shape table; zero amount leaves it alone
    S.tone.hasRefEnv = true; S.tone.refEnv = { 0, 2, 5 }; S.tone.refEnvAmt = 0;
    ld::hl::ToneEngine E0 (S, M.scan);
    CHECK (E0.shapeTable (0).empty());
    S.tone.refEnvAmt = 1;
    ld::hl::ToneEngine E1 (S, M.scan);
    const auto tab = E1.shapeTable (0);
    REQUIRE (! tab.empty());
    CHECK (tab.back() > 0.5f);   // the tail comes up (dB)
}

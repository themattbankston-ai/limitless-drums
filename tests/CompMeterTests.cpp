// The Vise's meter: every compressor reports its gain reduction per millisecond, harder settings reduce more,
// and the bus compressor reports too.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/FxSat.h"
#include "../src/hl/FxInserts.h"

static juce::var ins (const char* type, const char* p)
{
    auto* o = new juce::DynamicObject(); o->setProperty ("type", type); o->setProperty ("on", true); o->setProperty ("p", juce::JSON::parse (p));
    return juce::var (o);
}
static const char* COMP = R"({"model":"dist","mix":1,"scHpf":20,"out":0,"dist":{"input":5,"ratio":"4","attack":5,"release":5,"dist":"","hp":false}})";

static float peakOf (const ld::hl::F32& c) { float m = 0; for (float v : c) m = std::max (m, v); return m; }

TEST_CASE ("Compressor gain-reduction curves")
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
    auto withComp = [&] (double input)
    {
        auto S1 = S;
        auto c = ins ("comp", COMP);
        c["p"]["dist"].getDynamicObject()->setProperty ("input", input);
        juce::Array<juce::var> a; a.add (ins ("eq", R"({"phase":"min","out":0,"bands":[]})")); a.add (c);
        S1.tracks[(size_t) t].strip.inserts = juce::var (a);
        return S1;
    };
    const auto M1 = ld::hl::renderMix (withComp (3));
    const auto M2 = ld::hl::renderMix (withComp (9));
    const std::string key = std::to_string (t) + ":1";   // slot 1: after the EQ
    REQUIRE (M1.gr.count (key)); REQUIRE (M2.gr.count (key));
    const auto& g1 = M1.gr.at (key);
    CHECK (std::abs ((double) g1.size() - (double) M1.proc[(size_t) t][0].size() / S.sr * 1000) < 500);   // proc carries the alignment padding
    CHECK (peakOf (g1) > 0.5f);
    CHECK (peakOf (M2.gr.at (key)) > peakOf (g1) + 3);

    // the bus compressor
    auto S3 = S;
    juce::Array<juce::var> b; b.add (ins ("comp", COMP));
    S3.busIns = juce::var (b);
    const auto M3 = ld::hl::renderMix (S3);
    REQUIRE (M3.gr.count ("bus:0"));
    CHECK (peakOf (M3.gr.at ("bus:0")) > 0.5f);
}

TEST_CASE ("Gate: attenuation curve, flip and listen")
{
    using namespace ld::hl;
    const double sr = 48000;
    F32 x ((size_t) sr, 0.0f);
    for (size_t q = 0; q < 2400; ++q) x[q + 12000] = (float) (0.8 * std::sin (q * 0.2));   // one burst at 0.25 s, silence elsewhere
    for (size_t q = 0; q < x.size(); ++q) x[q] += (float) (0.0005 * std::sin (q * 0.37));  // a little bleed
    const Chs in { x };
    auto P = [] (const char* extra) { return juce::JSON::parse (juce::String (R"({"thr":-30,"range":-40,"ratio":0,"atk":0.3,"hold":20,"rel":50,"hyst":4,"look":1.5,"hpf":20,"lpf":20000)") + extra + "}"); };
    F32 g1, g2;
    gateRender (in, sr, P (""), nullptr, &g1);
    gateRender (in, sr, P (R"(,"flip":true)"), nullptr, &g2);
    REQUIRE (g1.size() >= 990); REQUIRE (g2.size() == g1.size());
    CHECK (g1[100] > 30);  CHECK (g1[255] < 1);     // closed in the silence, open on the burst
    CHECK (g2[220] < 1);   CHECK (g2[255] > 30);    // flipped: the other way round (it starts closed and opens in the silence)
    const auto L = gateRender (in, sr, P (R"(,"listen":true,"hpf":2000)"));
    REQUIRE (L.size() == 1);
    double a = 0, b = 0; for (size_t q = 12000; q < 14400; ++q) { a += std::abs (L[0][q]); b += std::abs (x[q]); }
    CHECK (a < b);   // the key filter took the low end out
}

TEST_CASE ("Horizon: width by frequency and the correlation curve")
{
    using namespace ld::hl;
    const double sr = 48000; const size_t n = 48000;
    F32 L (n), R (n);
    uint32_t s = 1; auto rnd = [&] { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) / 16777216.0 - 0.5); };
    for (size_t q = 0; q < n; ++q) { L[q] = rnd() * 0.5f; R[q] = rnd() * 0.5f; }   // uncorrelated: all side and mid
    const Chs in { L, R };
    auto P = [] (const char* j) { return juce::JSON::parse (juce::String (j)); };
    auto sideHigh = [&] (const Chs& y)   // side energy above 8 kHz, by a crude difference filter
    { double e = 0; for (size_t q = 1; q < n; ++q) { const double sd = 0.5 * ((y[0][q] - y[1][q]) - (y[0][q - 1] - y[1][q - 1])); e += sd * sd; } return e; };
    const auto same = widthRender (in, sr, P (R"({"width":1,"keep":false})"));
    const auto split = widthRender (in, sr, P (R"({"width":1,"highWidth":1,"highFrom":3000,"keep":false})"));
    double d = 0; for (size_t q = 0; q < n; ++q) d = std::max (d, (double) std::abs (same[0][q] - split[0][q]));
    CHECK (d < 1e-4);                                                  // equal widths: the split changes nothing
    const auto narrowHi = widthRender (in, sr, P (R"({"width":1,"highWidth":0,"highFrom":2000,"keep":false})"));
    CHECK (sideHigh (narrowHi) < sideHigh (same) * 0.3);              // highs pulled to mono
    F32 cc;
    widthRender (in, sr, P (R"({"width":0,"keep":false})"), &cc);
    REQUIRE (cc.size() >= 999);
    CHECK (cc[500] > 0.99f);                                          // width 0: mono, fully correlated
    widthRender (in, sr, P (R"({"width":1,"keep":false})"), &cc);
    CHECK (std::abs (cc[500]) < 0.3f);                                // uncorrelated noise stays near 0
}

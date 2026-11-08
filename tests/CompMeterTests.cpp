// The Vise's meter: every compressor reports its gain reduction per millisecond, harder settings reduce more,
// and the bus compressor reports too.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/FxSat.h"

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

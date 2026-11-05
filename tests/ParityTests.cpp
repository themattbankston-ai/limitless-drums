// Parity with Hitline 4.8: the same session through Hitline (tools/parity, answer keys in
// tests/parity) and through the C++ port must give the same results.
// Needs the session audio: set LD_PARITY_SESSIONS to the folder that holds the session folders
// named in tests/parity/sessions.json. Skipped (with a message) when it isn't set.
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "hl/Detect.h"
#include "hl/Wav.h"
#include "hl/Align.h"
#include "hl/Resolve.h"
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;

namespace
{
juce::File parityDir() { return juce::File (LD_SOURCE_DIR).getChildFile ("tests/parity"); }

struct Case { juce::var key; juce::File folder; juce::String name; };

std::vector<Case> cases()
{
    std::vector<Case> out;
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) return out;
    const auto index = juce::JSON::parse (parityDir().getChildFile ("sessions.json"));
    for (auto& e : *index.getArray())
    {
        Case c;
        c.name = e["key"].toString();
        c.key = juce::JSON::parse (parityDir().getChildFile (c.name))["state"];
        c.folder = juce::File (root).getChildFile (e["folder"].toString());
        if (c.folder.isDirectory()) out.push_back (c);
    }
    return out;
}

ld::hl::WavData trackWav (const Case& c, int index)
{
    const auto name = c.key["tracks"][index]["name"].toString() + ".wav";
    return ld::hl::readWav (c.folder.getChildFile (name));
}
} // namespace

TEST_CASE ("Parity: hit detection matches Hitline 4.8")
{
    auto all = cases();
    if (all.empty()) { WARN ("LD_PARITY_SESSIONS not set or no sessions found; parity skipped"); return; }

    for (auto& c : all)
    {
        INFO (c.name);
        const auto det = c.key["det"];
        const int ref = c.key["refIdx"];
        const auto wav = trackWav (c, ref);
        REQUIRE (wav.error.empty());
        CHECK ((double) c.key["sr"] == wav.sampleRate);

        ld::hl::DetectOptions o;
        o.expected = (int) det["V"] * (int) det["RR"];
        o.minGapMs = det["minGapMs"];
        const auto r = ld::hl::detectHits (ld::hl::monoOf (wav.channels), wav.sampleRate, o);

        const auto* keyOn = c.key["onsets"].getArray();
        REQUIRE ((int) r.onsets.size() == keyOn->size());
        for (int i = 0; i < keyOn->size(); ++i) { INFO ("hit " << i); CHECK (r.onsets[(size_t) i] == (int) (*keyOn)[i]); }
        CHECK (ld::hl::jsRound (r.thrDb * 2) / 2 == (double) det["thrDb"]);
    }
}

TEST_CASE ("Velocity and round-robin counts are read from file names like Hitline")
{
    auto c = ld::hl::guessCounts ({ "SNARE_CENTER_TOP_RR06_V06" });
    REQUIRE (c);
    CHECK (c->first == 6);
    CHECK (c->second == 6);
    c = ld::hl::guessCounts ({ "SNARE_CENTER_TOP_V04_RR08" });
    REQUIRE (c);
    CHECK (c->first == 4);
    CHECK (c->second == 8);
    CHECK_FALSE (ld::hl::guessCounts ({ "Kick In" }));
}

TEST_CASE ("Parity: alignment matches Hitline 4.8")
{
    auto all = cases();
    if (all.empty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }

    for (auto& c : all)
    {
        const int ref = c.key["refIdx"];
        const auto refWav = trackWav (c, ref);
        const auto refMono = ld::hl::monoOf (refWav.channels);
        std::vector<int> onsets;
        for (auto& v : *c.key["onsets"].getArray()) onsets.push_back ((int) v);
        const auto* tracks = c.key["tracks"].getArray();
        std::vector<ld::hl::MicAlign> mics ((size_t) tracks->size());
        for (int t = 0; t < tracks->size(); ++t)
        {
            auto& m = mics[(size_t) t];
            m.mic = (*tracks)[t]["mic"].toString().toStdString();
            m.name = (*tracks)[t]["name"].toString().toStdString();
            m.alignTo = ld::hl::defaultAlignTo (m.mic);
            CHECK (m.alignTo == (*tracks)[t]["alignTo"].toString().toStdString());
            if (t == ref) continue;
            const auto kt = (*tracks)[t];
            const auto ka = kt["auto"];
            INFO (c.name << " / " << kt["mic"].toString());
            REQUIRE (ka.isObject());
            const auto wav = trackWav (c, t);
            ld::hl::AlignOptions o;
            o.role = ld::hl::roleOf ((kt["mic"].toString() + " " + kt["name"].toString()).toStdString());
            CHECK (o.role == ka["role"].toString().toStdString());
            const auto r = ld::hl::alignMic (refMono, wav.channels, onsets, refWav.sampleRate, o);
            REQUIRE (r.valid);
            CHECK_THAT (r.lag, WithinAbs ((double) ka["lag"], 1e-9));
            CHECK (r.pol == (int) ka["pol"]);
            CHECK (r.kept == (bool) ka["kept"]);
            CHECK_THAT (r.asis, WithinAbs ((double) ka["asis"], 1e-9));
            CHECK_THAT (r.score, WithinAbs ((double) ka["score"], 1e-9));
            const auto* kap = ka["ap"].getArray();
            REQUIRE ((int) r.ap.size() == kap->size());
            for (int i = 0; i < kap->size(); ++i)
            {
                CHECK (r.ap[(size_t) i].fc == (double) (*kap)[i]["fc"]);
                CHECK (r.ap[(size_t) i].Q == (double) (*kap)[i]["Q"]);
            }
            const auto* kb = ka["bandsAfter"].getArray();
            for (int b = 0; b < ld::hl::numBands; ++b) CHECK_THAT (r.bandsAfter[(size_t) b], WithinAbs ((double) (*kb)[b], 1e-9));
            m.autoResult = r;
        }
        // what Hitline applies to each mic after resolving
        const bool flipped = ld::hl::resolveAlignment (mics, ref);
        CHECK (flipped == (bool) c.key["phase"]["flipped"]);
        for (int t = 0; t < tracks->size(); ++t)
        {
            const auto kt = (*tracks)[t];
            INFO (c.name << " / applied / " << kt["mic"].toString());
            CHECK_THAT (mics[(size_t) t].lag, WithinAbs ((double) kt["lag"], 1e-9));
            CHECK (mics[(size_t) t].pol == (int) kt["pol"]);
            CHECK ((int) mics[(size_t) t].ap.size() == kt["ap"].getArray()->size());
        }
    }
}

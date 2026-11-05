// Parity with Hitline 4.8: the same session through Hitline (tools/parity, answer keys in
// tests/parity) and through the C++ port must give the same results.
// Needs the session audio: set LD_PARITY_SESSIONS to the folder that holds the session folders
// named in tests/parity/sessions.json. Skipped (with a message) when it isn't set.
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "hl/Detect.h"
#include "hl/Wav.h"

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

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/Layers.h"

using Catch::Matchers::WithinAbs;

// A one-shot session like the ones Matt records: 6 velocities x 6 round robins, softest first,
// each hit a decaying 220 Hz tone with a little level spread inside each velocity.
static juce::AudioBuffer<float> oneShotSession (double sr, std::vector<double>& onsets)
{
    const double gap = 1.0;
    juce::AudioBuffer<float> buf (1, (int) (sr * (36 * gap + 1.0)));
    buf.clear();
    juce::Random rnd (3);
    for (int v = 0; v < 6; ++v)
        for (int r = 0; r < 6; ++r)
        {
            const double t = 0.5 + (v * 6 + r) * gap;
            onsets.push_back (t);
            const float amp = juce::Decibels::decibelsToGain (-30.0f + 5.0f * (float) v + rnd.nextFloat() * 1.0f - 0.5f);
            const int s0 = (int) (t * sr);
            for (int i = 0; i < (int) (0.6 * sr); ++i)
                buf.setSample (0, s0 + i, amp * std::exp (-(float) i / (float) (0.12 * sr))
                                              * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) sr));
        }
    return buf;
}

TEST_CASE ("Velocity layers group a one-shot session like Hitline")
{
    const double sr = 48000.0;
    std::vector<double> truth;
    auto buf = oneShotSession (sr, truth);

    auto onsets = ld::detectOneShots (buf, sr);
    REQUIRE (onsets.size() == 36);
    for (size_t i = 0; i < 36; ++i) CHECK_THAT (onsets[i], WithinAbs (truth[i], 0.001));

    std::vector<ld::Hit> hits;
    for (auto t : onsets)
        hits.push_back ({ t, ld::peakDbAt (buf, sr, t), 0, 0, ld::estimatePitch (buf, sr, t) });

    auto layers = ld::groupLayers (hits, buf.getNumSamples() / sr);
    REQUIRE (layers.size() == 6);
    for (int k = 0; k < 6; ++k)
    {
        CHECK (layers[(size_t) k].numHits == 6);
        CHECK (layers[(size_t) k].firstHit == k * 6);
        CHECK_THAT (layers[(size_t) k].pitchHz, WithinAbs (220.0, 3.0));
    }
    CHECK (layers[0].midiLow == 1);
    CHECK (layers[0].midiHigh == 21);
    CHECK (layers[5].midiLow == 106);
    CHECK (layers[5].midiHigh == 127);
    CHECK (hits[13].layer == 2);
    CHECK (hits[13].roundRobin == 1);
    CHECK (ld::noteName (220.0f) == "A3");
    CHECK (ld::noteName (311.1f) == juce::String (juce::CharPointer_UTF8 ("D♯4")));
}

TEST_CASE ("Known velocity counts come from the file name and rank runs by loudness")
{
    int v = 0, rr = 0;
    REQUIRE (ld::parseLayerCounts ("SNARE_CENTER_TOP_RR06_V06.wav", v, rr));
    CHECK (v == 6);
    CHECK (rr == 6);
    CHECK_FALSE (ld::parseLayerCounts ("01 Kick In.wav", v, rr));

    // Recorded loud to soft, 3 velocities x 4 round robins, with overlap inside runs.
    std::vector<ld::Hit> hits;
    const float levels[] = { -10, -12, -9, -11,   -20, -18, -21, -19,   -30, -28, -31, -29 };
    for (int i = 0; i < 12; ++i)
        hits.push_back ({ 1.0 + i, levels[i] });
    auto layers = ld::groupLayersKnown (hits, 14.0, 3);
    REQUIRE (layers.size() == 3);
    CHECK (hits[0].layer == 2);   // first run is the loudest
    CHECK (hits[11].layer == 0);
    CHECK (hits[5].roundRobin == 1);
    CHECK (layers[0].midiLow == 85);  // loudest run sits in the top MIDI range
    CHECK (layers[2].midiHigh == 42);
}

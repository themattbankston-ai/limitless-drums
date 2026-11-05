// M0 spike: can Tracktion Engine do grouped multitrack drum edits cleanly?
// Import 12 mics, cut every hit, conform to a tempo map, crossfade, render back,
// and prove the result with numbers.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestSignals.h"
#include "core/Conform.h"
#include "core/Analysis.h"
#include <cmath>

using namespace ldtest;

namespace
{
constexpr double kSR = 48000.0;

juce::File tempDir (const juce::String& name)
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_tests").getChildFile (name);
    d.deleteRecursively();
    d.createDirectory();
    return d;
}

std::vector<double> quarterGrid (double bpm, int firstBeat, int count)
{
    std::vector<double> g;
    for (int b = firstBeat; b < firstBeat + count; ++b)
        g.push_back (b * 60.0 / bpm);
    return g;
}

// Residual of `rendered` against `source` after the best single gain, relative to the source (dB).
double nullDepthDb (const juce::AudioBuffer<float>& source, const juce::AudioBuffer<float>& rendered)
{
    const int n = std::min (source.getNumSamples(), rendered.getNumSamples());
    const float* s = source.getReadPointer (0);
    const float* r = rendered.getReadPointer (0);
    double sr = 0, ss = 0;
    for (int i = 0; i < n; ++i) { sr += (double) s[i] * r[i]; ss += (double) s[i] * s[i]; }
    const double k = sr / ss;
    double e = 0;
    for (int i = 0; i < n; ++i) { const double d = r[i] - k * s[i]; e += d * d; }
    return 10.0 * std::log10 (e / (k * k * ss) + 1e-30);
}
} // namespace

TEST_CASE ("Null: an unedited 12-mic session renders back unchanged", "[spike]")
{
    auto dir = tempDir ("null");
    auto take = writeTake (dir, humanise (quarterGrid (120.0, 2, 16), 18.0, 1), 12, 11.0, kSR);
    auto edit = ld::createSessionEdit (engine(), take.mics, 120.0, tracktion::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);
    REQUIRE (tracktion::getAudioTracks (*edit).size() == 12);

    for (int m = 0; m < 12; ++m)
    {
        auto src = ld::readMono (take.mics[(size_t) m].file);
        auto out = ld::renderTrack (*edit, m, kSR);
        INFO ("mic " << m);
        REQUIRE (out.getNumSamples() >= src.getNumSamples() - 1);
        CHECK (ld::bestLag (src, out, 0, src.getNumSamples() / 4, 8) == 0);
        CHECK (nullDepthDb (src, out) < -100.0);
    }
}

TEST_CASE ("Conform: every lead hit lands on the grid within 1 ms", "[spike]")
{
    auto dir = tempDir ("grid");
    auto take = writeTake (dir, humanise (quarterGrid (120.0, 2, 16), 18.0, 2), 12, 11.0, kSR);
    auto edit = ld::createSessionEdit (engine(), take.mics, 120.0, tracktion::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);

    auto triggers = ld::detectTriggers (ld::readMono (take.mics[0].file), kSR);
    REQUIRE (triggers.size() == 16);

    auto report = ld::conformToGrid (*edit, triggers, {});
    CHECK (report.slices == 17);
    CHECK (report.maxMoveMs > 5.0);

    auto lead = ld::renderTrack (*edit, 0, kSR);
    auto after = ld::detectTriggers (lead, kSR);
    REQUIRE (after.size() == 16);

    double worstMs = 0;
    for (auto t : after)
        worstMs = std::max (worstMs, std::abs (t - ld::nearestGridTime (edit->tempoSequence, t, 1.0)) * 1000.0);

    INFO ("worst error " << worstMs << " ms");
    CHECK (worstMs < 1.0);
}

TEST_CASE ("Conform: all 12 mics stay phase-locked within 1 sample", "[spike]")
{
    auto dir = tempDir ("phase");
    auto take = writeTake (dir, humanise (quarterGrid (120.0, 2, 16), 18.0, 3), 12, 11.0, kSR);
    auto edit = ld::createSessionEdit (engine(), take.mics, 120.0, tracktion::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);

    auto triggers = ld::detectTriggers (ld::readMono (take.mics[0].file), kSR);
    ld::conformToGrid (*edit, triggers, {});

    auto lead = ld::renderTrack (*edit, 0, kSR);
    auto hits = ld::detectTriggers (lead, kSR);
    REQUIRE (hits.size() == 16);

    for (int m = 1; m < 12; ++m)
    {
        auto mic = ld::renderTrack (*edit, m, kSR);

        for (auto t : hits)
        {
            const int start = (int) std::lround (t * kSR) - 32;
            const int lag = ld::bestLag (lead, mic, start, 4000, 1300);
            INFO ("mic " << m << " hit at " << t << " s");
            CHECK (std::abs (lag - take.lagSamples[(size_t) m]) <= 1);
        }
    }
}

TEST_CASE ("Crossfades: seams are click-free (and would click without them)", "[spike]")
{
    auto dir = tempDir ("xfade");
    auto sine = writeSine (dir, 110.0, 0.3f, 11.0, kSR);
    auto triggers = humanise (quarterGrid (120.0, 2, 16), 18.0, 4);
    const float clean = 0.3f * (float) (2.0 * juce::MathConstants<double>::pi * 110.0 / kSR);

    auto worstStepNearSeams = [&] (double xfadeSec)
    {
        auto edit = ld::createSessionEdit (engine(), { sine }, 120.0, tracktion::Edit::EditRole::forRendering);
        ld::ConformSettings s;
        s.crossfadeSec = xfadeSec;
        ld::conformToGrid (*edit, triggers, s);
        auto out = ld::renderTrack (*edit, 0, kSR);

        // The tone is steady everywhere except at seams, so the largest step anywhere is the worst seam.
        float worst = ld::maxStep (out, 1, out.getNumSamples() - 1);
        return worst;
    };

    const float withFades = worstStepNearSeams (0.002);
    const float without = worstStepNearSeams (0.0);
    INFO ("clean step " << clean << ", with 2 ms fades " << withFades << ", hard cuts " << without);
    CHECK (withFades < 3.0f * clean);
    CHECK (without > 10.0f * clean);
}

TEST_CASE ("Conform follows a tempo map with a tempo change", "[spike]")
{
    // Tempo map: 120 bpm, then 100 bpm from beat 8.
    auto applyMap = [] (tracktion::Edit& e)
    {
        e.tempoSequence.getTempo (0)->setBpm (120.0);
        e.tempoSequence.getTempo (0)->setCurve (1.0f);
        e.tempoSequence.insertTempo (tracktion::BeatPosition::fromBeats (8.0), 100.0, 1.0f);
    };

    auto probe = tracktion::Edit::createSingleTrackEdit (engine(), tracktion::Edit::EditRole::forRendering);
    applyMap (*probe);

    std::vector<double> grid;
    for (int b = 2; b < 18; ++b)
        grid.push_back (probe->tempoSequence.toTime (tracktion::BeatPosition::fromBeats (b)).inSeconds());

    // The map really changes tempo: beat 16 is later than it would be at a steady 120.
    REQUIRE (grid.back() > 15.0 * 0.5 + 0.1);

    auto dir = tempDir ("tempomap");
    auto take = writeTake (dir, humanise (grid, 18.0, 5), 12, grid.back() + 2.0, kSR);
    auto edit = ld::createSessionEdit (engine(), take.mics, 120.0, tracktion::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);
    applyMap (*edit);

    auto triggers = ld::detectTriggers (ld::readMono (take.mics[0].file), kSR);
    REQUIRE (triggers.size() == grid.size());
    ld::conformToGrid (*edit, triggers, {});

    auto after = ld::detectTriggers (ld::renderTrack (*edit, 0, kSR), kSR);
    REQUIRE (after.size() == grid.size());

    double worstMs = 0;
    for (size_t i = 0; i < after.size(); ++i)
        worstMs = std::max (worstMs, std::abs (after[i] - grid[i]) * 1000.0);

    INFO ("worst error " << worstMs << " ms");
    CHECK (worstMs < 1.0);
}

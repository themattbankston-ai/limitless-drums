#pragma once

#include "core/Session.h"

namespace ldtest
{

/** A synthetic multitrack drum take: every mic hears the same hits, delayed by a fixed
    whole number of samples (its distance from the drum) and scaled, like a real kit. */
struct SynthTake
{
    double sampleRate = 48000.0;
    std::vector<ld::MicFile> mics;
    std::vector<int> lagSamples;       ///< Per mic, relative to the lead (mic 0)
    std::vector<double> hitTimes;      ///< True onset times at the lead mic, seconds
};

/** Writes a take to `dir`. `hitTimes` are where the drummer actually hit (on the lead mic). */
SynthTake writeTake (const juce::File& dir, const std::vector<double>& hitTimes,
                     int numMics, double lengthSec, double sampleRate = 48000.0);

/** Writes a single-track take of a steady sine, for crossfade/click tests. */
ld::MicFile writeSine (const juce::File& dir, double freq, float amp, double lengthSec, double sampleRate = 48000.0);

/** Drummer timing: grid times plus deterministic offsets within +/- maxOffsetMs. */
std::vector<double> humanise (const std::vector<double>& gridTimes, double maxOffsetMs, int seed);

/** The engine shared by all tests. */
tracktion::Engine& engine();
tracktion::Engine*& enginePtr();

} // namespace ldtest

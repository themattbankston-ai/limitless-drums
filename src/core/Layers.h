#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace ld
{
/** One hit in a one-shot session. */
struct Hit
{
    double time = 0.0;          ///< onset, seconds
    float peakDb = -100.0f;     ///< peak on the reference mic
    int layer = 0;              ///< velocity layer (0 = softest)
    int roundRobin = 0;         ///< position within its layer (0-based)
    float pitchHz = 0.0f;       ///< estimated fundamental, 0 if unknown
};

/** A velocity layer: a run of hits recorded at about the same strength, as in Hitline. */
struct VelocityLayer
{
    int firstHit = 0, numHits = 0;
    int midiLow = 1, midiHigh = 127;
    double start = 0.0, end = 0.0;  ///< seconds, from just before the first hit to just before the next layer
    float meanDb = -100.0f;
    float pitchHz = 0.0f;           ///< median pitch of its hits, 0 if unknown
};

/** Finds the hits in a one-shot session: sudden rises of at least 12 dB over the last 20 ms,
    above a floor relative to the loudest hit, so a ringing hit never re-triggers and the
    softest velocity layers are still found. Returns onset times in seconds. */
std::vector<double> detectOneShots (const juce::AudioBuffer<float>& mono, double sampleRate,
                                    float floorDbRelPeak = -50.0f, double minGapSec = 0.15);

/** Peak level (dBFS) of `mono` in [t, t + windowSec). */
float peakDbAt (const juce::AudioBuffer<float>& mono, double sampleRate, double t, double windowSec = 0.08);

/** RMS level (dBFS) of `mono` in [t, t + windowSec). Unlike the peak, this still tells
    velocities apart on a close mic that clips. */
float rmsDbAt (const juce::AudioBuffer<float>& mono, double sampleRate, double t, double windowSec = 0.05);

/** Reads the velocity and round-robin counts from a one-shot file name such as
    "SNARE_CENTER_TOP_RR06_V06.wav" (6 velocities x 6 round robins). Returns false if absent. */
bool parseLayerCounts (const juce::String& fileName, int& velocities, int& roundRobins);

/** Fundamental estimate (Hz) from the ring after the attack, by autocorrelation in 50-1000 Hz.
    Returns 0 when nothing periodic is found. */
float estimatePitch (const juce::AudioBuffer<float>& mono, double sampleRate, double onset);

/** Groups time-ordered hits into velocity layers. A new layer starts when a hit is more than
    `splitDb` away from the running mean of the current layer (and the layer already has at
    least `minPerLayer` hits). Fills each hit's layer and roundRobin, and spreads MIDI
    velocities 1-127 evenly across the layers. */
std::vector<VelocityLayer> groupLayers (std::vector<Hit>& hits, double sessionLength,
                                        float splitDb = 2.5f, int minPerLayer = 2);

/** Groups hits when the number of velocities is known (from the file name): the hits are cut,
    in recording order, into that many runs of about equal size, and each run's velocity is
    its loudness rank (softest = layer 0), as Hitline did. Layers come back in time order. */
std::vector<VelocityLayer> groupLayersKnown (std::vector<Hit>& hits, double sessionLength, int velocities);

/** "D4", "D#4" (sharp written as U+266F) for a frequency; empty for 0. */
juce::String noteName (float hz);
} // namespace ld

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace ld
{

/** Finds hit onsets (seconds) in a mono buffer: the first sample that rises above
    `thresholdDb` relative to the buffer's peak, with at least `minGapSec` between hits. */
std::vector<double> detectTriggers (const juce::AudioBuffer<float>& mono, double sampleRate,
                                    float thresholdDbRelPeak = -24.0f, double minGapSec = 0.12);

/** Lag (in samples) of `b` relative to `a` that best aligns them, searched in
    [-maxLag, maxLag] over the window [start, start + length). Positive = b is later. */
int bestLag (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
             int start, int length, int maxLag);

/** Largest absolute sample-to-sample step in [start, end). Used to detect clicks. */
float maxStep (const juce::AudioBuffer<float>& mono, int start, int end);

} // namespace ld

#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace ld
{
namespace te = tracktion;

/** One mic of a multitrack drum recording. */
struct MicFile
{
    juce::String name;
    juce::File file;
};

/** Builds an Edit with one audio track per mic, every clip starting at 0 so all mics stay
    sample-aligned, at the given tempo. Returns nullptr if a file can't be read. */
std::unique_ptr<te::Edit> createSessionEdit (te::Engine&,
                                             const std::vector<MicFile>& mics,
                                             double bpm,
                                             te::Edit::EditRole role = te::Edit::EditRole::forEditing);

/** Renders one track of the Edit, dry (no plugins), mono, 32-bit float, at the given rate. */
juce::AudioBuffer<float> renderTrack (te::Edit&, int trackIndex, double sampleRate);

/** Reads a mono audio file into a buffer (first channel). */
juce::AudioBuffer<float> readMono (const juce::File&, double* sampleRateOut = nullptr);

} // namespace ld

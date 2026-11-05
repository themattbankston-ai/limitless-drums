#pragma once
#include "Js.h"
#include <juce_core/juce_core.h>
#include <string>

namespace ld::hl
{
/** Port of Hitline's parseWav: PCM 8/16/24/32-bit, float 32/64, RF64, WAVE_FORMAT_EXTENSIBLE.
    Same scaling as Hitline (24-bit / 8388608 and so on), so samples match exactly. */
struct WavData
{
    double sampleRate = 0;
    std::vector<F32> channels;
    int bits = 0;
    bool isFloat = false;
    std::string label;
    std::string error;   ///< empty on success
};
WavData parseWav (const juce::MemoryBlock&);
WavData readWav (const juce::File&);
} // namespace ld::hl

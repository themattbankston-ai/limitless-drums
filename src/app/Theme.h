#pragma once

#include <juce_graphics/juce_graphics.h>

namespace ld::theme
{
/** Design tokens shared by every screen. Night is the default; Day is the Pro Tools 7-style light mode. */
struct Palette
{
    juce::Colour ground, panel, raised, ink, inkQuiet, line;
    juce::Colour signal;     // amber: playback and signal
    juce::Colour select;     // teal: selection
    juce::Colour reference;  // violet: references
    juce::Colour warn;       // red: warnings only
};

inline Palette night()
{
    return { juce::Colour (0xff0e0e10), juce::Colour (0xff17171a), juce::Colour (0xff222226),
             juce::Colour (0xfff2efe8), juce::Colour (0xff9aa0a8), juce::Colour (0xff2c2c31),
             juce::Colour (0xfff0a63c), juce::Colour (0xff46d3c3), juce::Colour (0xffa58cf0),
             juce::Colour (0xffe5534b) };
}

inline Palette day()
{
    return { juce::Colour (0xffdcdcd8), juce::Colour (0xffcfcfca), juce::Colour (0xffe8e8e4),
             juce::Colour (0xff1d1e20), juce::Colour (0xff4a4d52), juce::Colour (0xffb4b4ae),
             juce::Colour (0xffc27a12), juce::Colour (0xff1d6fd1), juce::Colour (0xff6a4fc4),
             juce::Colour (0xffc0362f) };
}

/** Neon accents for screens and LEDs, from the hardware-plugin references: cyan, magenta,
    violet, amber, lime, electric blue. Used for waveforms, knob rings and meters. */
namespace neon
{
    const juce::Colour cyan    (0xff2ee6ff), magenta (0xffff3da8), violet (0xff8b5cff), amber (0xffffb23e),
                       lime    (0xff7cff6b), blue    (0xff4c8dff), orange (0xffff6a3d), gold (0xffffd85a),
                       pink    (0xffff7ad9);
}

/** Glass screen colours (the recessed displays in the faceplates). */
namespace glass
{
    const juce::Colour deep (0xff06070a), face (0xff0a0c11), grid (0x14ffffff), rim (0xff1d2028);
}

namespace space { constexpr int xs = 4, s = 8, m = 16, l = 24, xl = 40; }
namespace radius { constexpr float s = 4.0f, m = 8.0f, l = 16.0f; }
} // namespace ld::theme

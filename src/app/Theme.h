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

namespace space { constexpr int xs = 4, s = 8, m = 16, l = 24, xl = 40; }
namespace radius { constexpr float s = 4.0f, m = 8.0f, l = 16.0f; }
} // namespace ld::theme

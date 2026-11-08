#pragma once
// Stand-in for the engine's FxCore.h: only what Song.cpp uses, without JUCE (for the song CLI).
#include "Js.h"
#include <functional>
namespace ld::hl
{
using Chs = std::vector<F32>;
inline double dbl (double g) { return std::pow (10.0, g / 20); }
inline double clampD (double v, double a, double b) { return v < a ? a : v > b ? b : v; }
}

#pragma once
// Shared pieces of Hitline 4.8's processing (dsp.js): FIR kernels from magnitude curves, fast
// convolution, EQ band shapes, envelope follower. Parameters are juce::var objects shaped exactly
// like Hitline's JSON settings, so its presets and projects map across unchanged.
#include "Js.h"
#include <juce_core/juce_core.h>

namespace ld::hl
{
using Chs = std::vector<F32>;

inline double dbl (double g) { return std::pow (10.0, g / 20); }
inline double clampD (double v, double a, double b) { return v < a ? a : v > b ? b : v; }

/** JavaScript `p.k ?? d`: the value unless undefined or null. */
inline double nn (const juce::var& p, const char* k, double d)
{
    const auto v = p.getProperty (k, juce::var());
    return v.isVoid() || v.isUndefined() ? d : (double) v;
}
/** JavaScript `p.k || d`: the value unless it is missing, 0, false or empty. */
inline double orr (const juce::var& p, const char* k, double d)
{
    const auto v = p.getProperty (k, juce::var());
    if (v.isVoid() || v.isUndefined()) return d;
    if (v.isBool()) return (bool) v ? 1.0 : d;
    if (v.isString()) { const auto s = v.toString(); if (s.isEmpty()) return d; return s.getDoubleValue() != 0 ? s.getDoubleValue() : d; }
    const double x = v;
    return x != 0 && ! std::isnan (x) ? x : d;
}
inline juce::String str (const juce::var& p, const char* k, const juce::String& d = {})
{
    const auto v = p.getProperty (k, juce::var());
    return v.isVoid() || v.isUndefined() ? d : v.toString();
}
inline bool isFalse (const juce::var& p, const char* k) { const auto v = p.getProperty (k, juce::var()); return v.isBool() && ! (bool) v; }
inline bool truthy (const juce::var& p, const char* k)
{
    const auto v = p.getProperty (k, juce::var());
    if (v.isVoid() || v.isUndefined()) return false;
    if (v.isBool()) return (bool) v;
    if (v.isString()) return v.toString().isNotEmpty();
    if (v.isObject() || v.isArray()) return true;
    const double x = v; return x != 0 && ! std::isnan (x);
}

struct Kernel { F64 h; long delay = 0; };

double sos2 (double f, double fc, double b2, double b1, double b0, double a2, double a1, double a0);
double bandMag (const juce::var& band, double f);
Kernel magToKernel (const F64& M, size_t N, bool minPhase);
F32 convolve (const F32& x, const Kernel&);
size_t kernN (double sr, bool minPhase);
F64 magGrid (double sr, size_t N, const std::function<double (double)>&);
F32 envDb (const F32& x, double sr, double atkMs, double relMs);
F32 scale (const F32& x, double g);   ///< x.map(v => v*g), rounded to float
} // namespace ld::hl

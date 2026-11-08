#pragma once
#include "FxCore.h"

namespace ld::hl
{
/** Ports of Hitline 4.8's insert processors (dsp.js). `p` is the insert's settings object as Hitline
    stores it. Each returns new channels; inputs are untouched. */
Chs eqRender (const Chs& chs, double sr, const juce::var& p);
double pultecMag (const juce::var& p, double f);
/** An EQ insert's response in dB at f, for the windows' curves: "pultec" or "eq" (each band on, dynamic bands at
    their static gain); band >= 0 gives that one band alone. */
double insertCurveDb (const juce::String& type, const juce::var& p, double f, int band = -1);
Chs pultecRender (const Chs& chs, double sr, const juce::var& p);
Chs widthRender (const Chs& chs, double sr, const juce::var& p);
double corrOf (const Chs& chs);
/** The gate. "flip" ducks instead (closes on the hits); "listen" outputs the filtered key, to set the key filters by ear.
    grCurve: the attenuation, dB >= 0 per GR_RATE block. */
Chs gateRender (const Chs& chs, double sr, const juce::var& p, const Chs* keyChs = nullptr, F32* grCurve = nullptr);

struct CompModel
{
    std::string m;
    double inDb = 0, thr = -12, ratio = 4, knee = 6, atk = 5, rel = 150, rel2 = 0, rms = 0, h2 = -200, h3 = -200, out = 0, hiShelf = 0, over = 0, hp = 0;
};
CompModel compModel (const juce::var& P);
double compCurve (const CompModel&, double L);
/** The compressor's gain in dB per sample for a detector signal (the bus compressor drives every channel with it). */
F32 compGainCurve (const F32& det, double sr, const CompModel&, double scHpf);
/** Per-hit gain reduction (dB, positive) reported for the meters, when onsets are given. */
constexpr double GR_RATE = 1000;   ///< gain-reduction curves: one value per millisecond (the most reduction in it)
/** A gain curve in dB (<= 0) as reduction per GR_RATE block. */
F32 grBlocks (const F32& gdb, double sr);
Chs compRender (const Chs& chs, double sr, const juce::var& p, const std::vector<long>* ons = nullptr, std::vector<F64>* grOut = nullptr, F32* grCurve = nullptr);
} // namespace ld::hl

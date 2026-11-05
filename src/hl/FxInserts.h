#pragma once
#include "FxCore.h"

namespace ld::hl
{
/** Ports of Hitline 4.8's insert processors (dsp.js). `p` is the insert's settings object as Hitline
    stores it. Each returns new channels; inputs are untouched. */
Chs eqRender (const Chs& chs, double sr, const juce::var& p);
double pultecMag (const juce::var& p, double f);
Chs pultecRender (const Chs& chs, double sr, const juce::var& p);
Chs widthRender (const Chs& chs, double sr, const juce::var& p);
double corrOf (const Chs& chs);
Chs gateRender (const Chs& chs, double sr, const juce::var& p, const Chs* keyChs = nullptr);

struct CompModel
{
    std::string m;
    double inDb = 0, thr = -12, ratio = 4, knee = 6, atk = 5, rel = 150, rel2 = 0, rms = 0, h2 = -200, h3 = -200, out = 0, hiShelf = 0, over = 0, hp = 0;
};
CompModel compModel (const juce::var& P);
double compCurve (const CompModel&, double L);
/** Per-hit gain reduction (dB, positive) reported for the meters, when onsets are given. */
Chs compRender (const Chs& chs, double sr, const juce::var& p, const std::vector<long>* ons = nullptr, std::vector<F64>* grOut = nullptr);
} // namespace ld::hl

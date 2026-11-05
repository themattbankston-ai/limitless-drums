#pragma once
#include "FxCore.h"

namespace ld::hl
{
/** Ports of Hitline 4.8's clean-up and tails (dsp.js): noise print by minimum statistics, Wiener
    denoise, natural tails. */
struct NoisePrint
{
    size_t N = 2048;
    F64 psd;
    int runs = 0;
    double floorDb = 0, trust = 0;
    std::vector<int> hum;   ///< 50 and/or 60 when mains hum was found
};
std::optional<NoisePrint> noisePrint (const F32& x, double sr, const std::vector<long>& onsets, size_t N = 2048, double bias = 2.2);
F32 denoise (const F32& x, double sr, const NoisePrint* print, double reductionDb = 18, double oversub = 1.4, double releaseS = 0.06);

struct TailPlan { long end = 0, F = 0; double need = 0; };
TailPlan tailPlan (const Chs& chs, double sr, double peak = 0, double floorRel = -90, double minRate = 60);
Chs applyTail (const Chs& chs, const TailPlan&, double sr);
} // namespace ld::hl

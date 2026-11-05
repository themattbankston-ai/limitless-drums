#pragma once
#include "FxCore.h"
#include <optional>

namespace ld::hl
{
/** Ports of Hitline 4.8's tone and shape pass and the clean-up tools around it (dsp.js): zero-phase
    kernels per hit, the shape envelope and loudness match (fxRenderMulti), click repair, the kit bleed
    gate, linked resonance taming across mics (tameMulti), the resonance scan, and the library
    measurements (bandSpec, envStats). */
struct FxPlan
{
    int K = 0;                        ///< kernel grid size
    std::vector<long> ons;            ///< lead onsets
    std::vector<F64> kern;            ///< gain curves in dB (K/2+1 bins each)
    std::vector<int> tone, cut;       ///< kernel id per hit, -1 = none
    F64 hitK;                         ///< resonance cut depth per hit (empty = 1)
    double spare = 0, rampMs = 25;
    F64 env;                          ///< shape gain table (empty = none)
    double envStepMs = 0.25, envT0 = -1;
    bool hasComp = false;
    long compWin = 0, compLag = 0;
    bool repair = false;
};
Chs fxRenderMulti (Chs chs, double sr, const FxPlan&);
std::vector<long> fxOnsets (const F32& x, double sr, const std::vector<long>& ons, long L);
int repairClicks (F32& x, double sr, const std::vector<long>& ons);
Chs bleedGate (const Chs& chs, double sr, double depthDb);

struct TameOptions
{
    std::vector<long> ons, bnd;
    F64 lag;
    double depth = 0, sharp = 0.5, spareMs = 4;
    F32 sens;                          ///< per-bin sensitivity (empty = 1)
    long win = 0;                      ///< loudness window (0 = 200 ms)
    std::vector<std::vector<long>> mons;   ///< each mic's onsets (empty = found here)
    F64 depthT;                        ///< per-mic depth (empty = all at depth)
    bool noGain = false;
};
struct TameResult { std::vector<Chs> tracks; F32 curve; F64 gains; std::vector<std::vector<long>> mo; };
TameResult tameMulti (const std::vector<Chs>& tracks, double sr, const TameOptions&);
void tameGain (std::vector<Chs>& out, const std::vector<std::vector<long>>& mo, const F64& gk, double sr);

struct ResPeak { double f, q, ex; std::optional<double> ring; F64 hit; };
struct ResScanResult { double f0 = 0; std::vector<ResPeak> peaks; size_t N = 0; double thr = 0; std::vector<std::array<double, 3>> env; };
struct ResScanOptions
{
    double lo = 250, hi = 14000, oct = 1.0 / 3, skipMs = 12, lenMs = 700, lateMs = 100, thrDb = 3.5, ringDb = 2, consistency = 0.7, f0lo = 45, f0hi = 400;
    bool ring = false, autoProtect = false;
    F64 protect;
    int max = 10;
};
ResScanResult resScan (const F32& x, const std::vector<long>& onsets, const std::vector<long>& ends, double sr, const ResScanOptions&);

F64 bandSpec (const F32& x, long on, double sr, double maxN = 1e12);
struct EnvStats { long atk = 0, t40 = 0, t60 = 0; double body = NAN, sus = NAN, tail = NAN, pk = 0; };
EnvStats envStats (const F32& x, long on, double sr, double maxN = 1e12);
const F64& bands3();   ///< third-octave centres 15.6 Hz .. 20 kHz
} // namespace ld::hl

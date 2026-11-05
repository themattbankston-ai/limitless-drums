#pragma once
#include "Align.h"
#include <optional>

namespace ld::hl
{
/** Per-mic alignment state, as Hitline keeps it on each track. */
struct MicAlign
{
    std::string mic, name;
    std::string alignTo = "lead";     ///< "lead", "none" (natural), "pro" (Standard), or a group "A".."D"
    double amount = 1;                ///< how far rooms move (0..1)
    bool apOn = true, manualLag = false, manualPol = false;
    std::optional<AlignResult> autoResult;   ///< measured against the lead
    std::optional<AlignResult> grp;          ///< measured against its group anchor
    double proLag = NAN;              ///< Standard timing target, from the library
    std::optional<int> proPol;
    // results
    double lag = 0;
    int pol = 1;
    std::vector<AllPass> ap;
};

/** Hitline's buildTrack default: rooms keep natural timing, everything else follows the lead. */
inline std::string defaultAlignTo (const std::string& mic) { return isRoomName (mic) ? "none" : "lead"; }

/** Port of Hitline's resolveAlignment: turns the measurements into each mic's applied delay,
    polarity and phase EQ, flips the lead when most mics disagree with it, and makes group members
    follow their anchor. Returns true when the lead was flipped. */
bool resolveAlignment (std::vector<MicAlign>& mics, int refIdx, bool leadFlipAuto = true,
                       const std::array<std::string, 4>& groupAnchors = { "auto", "auto", "auto", "auto" });

/** Port of renderSeg: out[n] = g * pol * x(start + n + lag), with a 32-tap Blackman-windowed sinc for
    fractional delays. */
F32 renderSeg (const F32& x, long start, long len, double lag, int pol, double g, bool sub);
} // namespace ld::hl

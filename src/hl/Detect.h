#pragma once
#include "Js.h"
#include <optional>
#include <string>

namespace ld::hl
{
/** Port of Hitline 4.8's hit detection (dsp.js: blockEnv, onsetCurves, onsetsAt, refineOnset,
    detectHits). Peak-hold envelope plus a rise curve, so a ringing tail never re-triggers; with no
    threshold given, it searches for the threshold whose hit count is steadiest near `expected`. */
struct DetectOptions
{
    int expected = 0;
    double minGapMs = 180;
    std::optional<double> thrDb;   ///< empty = automatic
};
struct DetectResult
{
    std::vector<int> onsets;   ///< sample indices
    double thrDb = 0, maxDb = 0;
};
F32 blockEnv (const F32& x, int bs);
DetectResult detectHits (const F32& x, double sr, const DetectOptions&);

/** Port of buildTrack's mono mix: channels averaged in float, one channel at a time. */
F32 monoOf (const std::vector<F32>& chs);

/** Port of guessCounts: "_V06_RR06" or "RR06_V06" in a file name gives {velocities, round robins}. */
std::optional<std::pair<int, int>> guessCounts (const std::vector<std::string>& names);
} // namespace ld::hl

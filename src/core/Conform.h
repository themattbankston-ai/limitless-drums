#pragma once

#include "Session.h"

namespace ld
{

struct ConformSettings
{
    double gridBeats = 1.0;     ///< Grid size in beats (1 = quarter notes, 0.5 = eighths)
    double strength = 1.0;      ///< 0..1, how far each hit moves toward the grid
    double preRollSec = 0.005;  ///< Cut this far before each trigger so the transient stays whole
    double crossfadeSec = 0.002;///< Equal-power crossfade at every seam
};

struct ConformReport
{
    int slices = 0;
    double maxMoveMs = 0.0;
};

/** Beat Detective-style grouped conform. Cuts every track at the same places (each
    trigger minus the pre-roll), moves every slice by the same amount so its trigger
    lands on the tempo map's grid, then fills gaps and crossfades the seams.
    Because all mics are cut and moved together, their relative timing never changes. */
ConformReport conformToGrid (te::Edit&, const std::vector<double>& triggersSec, const ConformSettings&);

/** The grid time (seconds) nearest `t`, following the Edit's tempo map. */
double nearestGridTime (const te::TempoSequence&, double t, double gridBeats);

} // namespace ld

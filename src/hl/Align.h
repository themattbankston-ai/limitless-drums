#pragma once
#include "Js.h"
#include <array>
#include <string>

namespace ld::hl
{
/** Port of Hitline 4.8's alignment v3 ("sum like the ear hears it", dsp.js alignMic and helpers):
    cross spectra per hit, candidate delays from a whitened correlation, every candidate and both
    polarities scored by how well the mic sums with the lead across eight octave bands, "as recorded"
    kept unless a move clearly wins, then up to two all-pass filters (phase EQ). */
struct AllPass { double fc = 0, Q = 0; };

struct AlignOptions
{
    std::string role = "near";        ///< "bottom", "near" or "far"
    double minMs = NAN, maxMs = NAN;  ///< NaN = the role's default range
    int allpass = -1;                 ///< -1 = the role's default
    double keepThr = NAN;
    std::vector<double> extra;        ///< extra candidate lags (samples)
};

constexpr int numBands = 8;
using Bands = std::array<double, numBands>;
extern const std::array<std::pair<double, double>, numBands> octaveBands;   ///< 45 Hz to 11.2 kHz

struct AlignResult
{
    bool valid = false;
    double lag = 0;                 ///< samples, positive = mic later than the lead
    int pol = 1;
    std::vector<AllPass> ap;
    bool kept = false;
    std::string role;
    double asis = 0, score = 0, gain = 0, conf = 0, sharp = 0, arrival = NAN;
    Bands bandsBefore {}, bandsAfter {};
    int n = 0;
};

AlignResult alignMic (const F32& ref, const std::vector<F32>& chans, const std::vector<int>& onsets, double sr, const AlignOptions& = {});

/** Score a delay / polarity / phase EQ the user set by hand (Hitline's scoreMic). */
struct ScoreResult { bool valid = false; double score = 0, asis = 0; Bands bandsAfter {}, bandsBefore {}; };
ScoreResult scoreMic (const F32& ref, const std::vector<F32>& chans, const std::vector<int>& onsets, double sr,
                      double lag, int pol, const std::vector<AllPass>& ap, double arrival);

/** When each hit first reaches this mic (Hitline's arrivals). NaN where it can't tell. */
F64 arrivals (const F32& x, const std::vector<int>& onsets, double sr, double maxMs = 80, double rel = 0.12, const F64& shift = {});
double median0 (F64 a);

/** Second-order all-pass filters in series (Hitline's applyAllpass). */
F32 applyAllpass (const F32& x, double sr, const std::vector<AllPass>&);

/** Hitline's roleOf: "bottom" for bottom/reso mics, "far" for rooms, otherwise "near". */
std::string roleOf (const std::string& micAndName);
/** Hitline's isRoomName: rooms keep their natural timing by default. */
bool isRoomName (const std::string&);
} // namespace ld::hl

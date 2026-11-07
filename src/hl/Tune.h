#pragma once
#include "Js.h"
#include <optional>
#include <string>

namespace ld::hl
{
/** Port of Hitline 4.8's pitch reading and tuning (dsp.js detectPitch, sessionPitch, renderTuned;
    app computeTune). */
struct PitchOptions
{
    double skipMs = 15, winMs = 150, fmin = 120, fmax = 400, relDb = 6;
};
struct Pitch { double f = 0, conf = 0, level = 0; };

std::optional<Pitch> detectPitch (const F32& x, long onset, double sr, const PitchOptions&);

struct SessionPitch { double est = NAN; std::vector<std::optional<Pitch>> hits; };
SessionPitch sessionPitch (const F32& x, const std::vector<long>& onsets, double sr, const PitchOptions&);

double hzToMidi (double f, double a4 = 440);
double midiToHz (double m, double a4 = 440);
std::string noteName (double midi);   ///< "D4", "C#3" (Hitline's spelling)

/** Hitline's tuning settings and computeTune result. */
struct TuneSettings
{
    bool on = false;
    std::string mode = "drum";        ///< "drum" (one ratio) or "rr" (even out round robins too)
    std::string target = "nearest";   ///< "nearest" or a MIDI note number as text
    double cents = 0, a4 = 440;
};
struct TuneInfo
{
    bool valid = false;
    F64 f, ratios, layerMed, before, after;
    double medF = NAN, medMidi = NAN, targetF = NAN;
    int nearest = 0, targetMidi = 0;
};
/** velOf(i) gives the velocity layer of hit i (or -1 when the hit count doesn't match the settings). */
TuneInfo computeTune (const SessionPitch&, const TuneSettings&, int velocities, const std::vector<int>& velOf);

/** Varispeed render (Hitline's renderTuned): out[n] = g*pol*x(onset + lag + (n - pre)*step), Kaiser-windowed
    sinc, reading only [lo, hi), with an edge fade so a pitched-up slot never pulls in the next hit. */
/** Keeping the stick attack at its real pitch (Limitless): the first keepN output samples after the onset (and
    the pre-roll) play at step0, the untuned rate; the rate then glides to the tuned step over rampN. The read
    position never jumps, so nothing smears, and every mic gets the same mapping, so their timing holds. */
struct AttackKeep { long keepN = 0, rampN = 0; double step0 = 1; };
F32 renderTuned (const F32& x, double onset, long pre, long outLen, double lag, int pol, double g, double step,
                 long lo, long hi, long fadeN, int taps = 32, AttackKeep keep = {});
} // namespace ld::hl

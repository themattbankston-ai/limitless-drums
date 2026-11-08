#pragma once
// Song mode: whole drum performances. Finds every hit across the mics (tagged by drum and velocity), tracks the tempo,
// builds the grid (Keep / Smooth / Flatten), and moves hits toward it, all mics together, by cutting just before the
// earliest mic and crossfading, so phase from Align is never broken. Also the problem finder and the MIDI exports.
#include "Js.h"
#include "FxCore.h"
#include <string>
#include <vector>

namespace ld::hl
{
enum class Drum { kick, snare, tom, hat, cymbal, other };
const char* drumName (Drum);
/** What a mic picks up, from its name (and the role the sound gave it): the drum it's the close mic for, or other. */
Drum drumOfMic (const std::string& name);
/** Close mics are what hits are found on; overheads find cymbals; rooms are never used. */
bool isOverheadMic (const std::string& name);

struct SongHit
{
    long pos = 0;          ///< samples, on the recordings' own timeline: where the earliest mic starts the hit
    Drum drum = Drum::other;
    float vel = 0;         ///< 0..1, against this drum's loudest hits
    bool ghost = false, flam = false;
};

struct SongSettings
{
    std::string tempoMode = "smooth";   ///< keep (follow the drummer), smooth (a gentle tempo map), flatten (one tempo)
    double tightness = 0;               ///< 0 leaves the performance alone, 1 snaps every hit to the grid
    double swing = 0;                   ///< 0..1: offbeats pushed late, 1 = a full triplet feel
    bool keepFeel = true;               ///< ghost notes and flams only move a little
    bool swingHatsOnly = false;
    int division = 16;                  ///< the grid: 8, 12 (eighth triplets) or 16
    double bpmScale = 1;                ///< 2 or 0.5 when the detection picked double or half time
    double flatBpm = 0;                 ///< flatten at this tempo (0: the song's average)
    double consistency = 0;             ///< 0..1: evens each drum's hit-to-hit level on its close mics (accents and ghosts kept)
};

struct SongAnalysis
{
    bool done = false;
    std::vector<SongHit> hits;          ///< every hit, in time order
    std::vector<double> beats;          ///< seconds, the beats as the drummer played them
    int downbeat = 0;                   ///< which beat (0..3) starts the bars
    double sr = 44100;
};

struct SongState { SongSettings set; SongAnalysis an; };

/** Finds every hit and the beats. monos: each mic's mono recording; names: the mics' names. */
SongAnalysis analyseSong (const std::vector<F32>& monos, const std::vector<std::string>& names, double sr);

/** The beats the grid is built on (seconds), after the tempo mode and the half/double fix. */
std::vector<double> gridBeats (const SongAnalysis&, const SongSettings&);
/** Tempo per beat (BPM), from a list of beat times. */
std::vector<double> bpmOf (const std::vector<double>& beats);
/** Every grid line (seconds) for the division. */
std::vector<double> gridLines (const std::vector<double>& beats, int division);

/** One cut: everything from `cut` until the next cut moves by `shift` samples. */
struct SongEdit { long cut = 0, shift = 0; };
std::vector<SongEdit> planEdits (const SongAnalysis&, const SongSettings&);
/** The same edits on every channel: segments moved, the boundaries placed where neither side has a transient, short
    equal-power crossfades. Length unchanged. */
Chs applyEdits (const Chs& chs, const std::vector<SongEdit>& edits, double sr);
F32 applyEdits (const F32& x, const std::vector<SongEdit>& edits, double sr);
/** Consistency: on each drum's own close mics, every hit's level pulled toward the level of the hits around it by
    `amount` (at most 6 dB), between that hit and the drum's next one. Ghost notes, flams and real accents (more than
    6 dB from their neighbours) are left as played. tracks/names: every mic and its name, in the analysis' timeline. */
void applyConsistency (std::vector<Chs>& tracks, const std::vector<std::string>& names, const SongAnalysis&, double amount);
/** Where a hit ends up after the edits. */
long movedPos (long pos, const std::vector<SongEdit>& edits);

struct SongProblem { long pos = 0; std::string kind, text; };
/** The problem finder: flams between hits, double hits, far-off hits, rushed or dragged fills. */
std::vector<SongProblem> findProblems (const SongAnalysis&, const SongSettings&);

/** Standard MIDI files: the tempo map (one tempo per beat, 4/4) and every hit as a GM drum note on that map. */
std::string midiTempoMap (const std::vector<double>& beats, double startOffset = 0);
std::string midiPerformance (const std::vector<SongHit>& hits, const std::vector<SongEdit>& edits, const std::vector<double>& beats, double sr);
} // namespace ld::hl

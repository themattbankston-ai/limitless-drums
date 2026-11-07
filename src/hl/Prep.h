#pragma once
#include "Session.h"
#include "Wav.h"

namespace ld::hl
{
/** Port of Hitline 4.8's import and Auto prep (app 10_project and 20_analysis): naming mics from file names,
    guessing the drum, articulation and hit counts, choosing the lead, the drum's defaults, then finding the
    hits, lining up every mic, reading the pitch and setting the levels. */

std::string cleanMicName (const std::string& file, const std::string& drumHint, const std::string& articHint);
std::string guessDrum (const std::string& path);
std::string guessArtic (const std::string& folder);

struct SourceFile
{
    std::string name;                 ///< file name, "SNARE_CENTER_TOP_V06_RR06.wav"
    std::string rel;                  ///< path relative to what was dropped, for the count guess (empty = name)
    WavData wav;
};

struct OpenResult { Session session; std::vector<std::string> errors; };

/** Hitline's openSession for a fresh articulation: tracks built and named, duplicates numbered, the lead
    first, counts from the file names, the drum's defaults, then the hits found. */
OpenResult openSession (std::vector<SourceFile> files, const std::string& drum, const std::string& artic);

/** Groups dropped files into articulation sessions by folder, as Hitline does: {folder key, drum, artic, files}. */
struct SessionRecord { std::string key, drum, artic; std::vector<size_t> files; };
std::vector<SessionRecord> groupSessions (const std::vector<std::string>& relPaths, bool kit = false);

void applyDrumDefaults (Session&, const std::string& drum);
int expectedHits (const Session&);
bool countsOk (const Session&);

void runDetect (Session&);
/** Lines up every mic with the lead (and group members with their anchor), then resolves what is applied. */
void runAnalyze (Session&);
/** Hitline's resolveAlignment over the session: each mic's applied delay, polarity and phase EQ from what was
    measured and what was set by hand (phaseChanged, without the redraws). */
void resolvePhase (Session&);

// The Align step's controls (Hitline's phase panel and overlay). Each resolves the session afterwards.
int alignRef (const Session&, int t);                      ///< what this mic lines up to (-1: nothing)
void setTiming (Session&, const std::string& mode);         ///< "tight", "pro" (like the pros) or "natural"
void setAlignTo (Session&, int t, const std::string& to);   ///< "lead", "pro", "none" or a group "A".."D"
void nudgeLag (Session&, int t, int quarterSamples);        ///< negative: earlier (Hitline's ovlNudge dir)
void setLag (Session&, int t, double lag);                  ///< by hand (snap to hottest, a drag)
void flipMic (Session&, int t);
void setPhaseEq (Session&, int t, bool on);
void setLrLock (Session&, int t, bool on);
void resetMic (Session&, int t);                            ///< back to measured
/** The overlay's heat: how well the mic sums with what it lines up to around each delay near the current one
    (three bands, each normalised, as the aligner listens), on the selected hit or a spread of hits. */
struct Heat { std::vector<std::pair<double, double>> curve; double best = NAN, bestC = NAN, cur = 0, curC = NAN; long win = 0, pre = 0; bool far = false; int ref = -1; };
Heat overlayHeat (const Session&, int t, int sel, bool spread);
void runPitch (Session&);
/** Whether tuning is recommended: "" yes; "cymbal" (by the drum chosen or by its sound: nearly all of it above
    2 kHz, almost nothing low); "nopitch" (read, but too few hits give a clear pitch, or they don't agree). */
std::string tuneAdvice (const Session&);
bool soundsLikeCymbal (const Session&);

/** A reference sound for the Mix step (Hitline's loadReference): its third-octave curve against the sum of your
    mics on the hardest hits (before tone and shape, so the target doesn't chase itself), smoothed and limited to
    \u00b18 dB; and its first hit as audio, at the session's rate, for Reference vs mix. Empty curve on failure. */
struct Reference { std::string name; F64 curve; Chs audio; std::string error; };
Reference loadReference (const Session&, const MixRender&, const juce::File&);
void computeTune (Session&);
/** Hitline's Auto prep. False when the hit count doesn't match the velocity and round-robin settings. */
bool autoPrep (Session&);

/** Whole-kit takes: up to 48 of the loudest hits, at least 300 ms apart. */
std::vector<long> strongHits (const F32& x, double sr);
std::string hitCountAdvice (const Session&);
} // namespace ld::hl

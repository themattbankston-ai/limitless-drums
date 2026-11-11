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
/** The phase EQ by hand: sets band `band` (0 or 1; one past the end adds it) to fc Hz and Q. From then on the mic keeps
    these bands through every nudge, flip and re-measure, until resetMic. fc is kept in 20 Hz..0.45 x the rate, Q in 0.3..8. */
void setAllPass (Session&, int t, int band, double fc, double Q);
void removeAllPass (Session&, int t, int band);
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
struct Reference { std::string name; F64 curve; Chs audio; std::string error; bool hasEnv = false; std::array<double, 3> env { 0, 0, 0 }; int engine = 1; int hitsUsed = 0; };   ///< env: body, sustain, tail (dB, theirs - ours)
/** Measures a finished sample against the take and returns the tone curve that moves the take toward it.
    engine 1 is Hitline's, kept exactly (the main app and its parity tests use it; curves already stored in a
    session never change). engine 2 (the sample build, 10 Nov 2026) differs where engine 1 was measurably wrong:
      - stereo: each channel is measured and their powers combined, so opposite-polarity channels can't cancel into
        rounding noise that the match then chases (RM01);
      - hits: every hit within 30 dB of the loudest is used (engine 1 kept only the loudest, or the file's start);
      - bands the reference has nothing in (60 dB under its total) or outside 60 Hz-15 kHz stay at exactly 0 dB,
        through the level removal and the smoothing, with the neighbouring band tapered (RM04);
      - the audition copy is resampled with a windowed sinc, low-passed first when the reference's rate is higher (RM03). */
constexpr int kReferenceEngine = 2;
Reference loadReference (const Session&, const MixRender&, const juce::File&, int engine = 1);
void computeTune (Session&);
/** Hitline's Auto prep. False when the hit count doesn't match the velocity and round-robin settings. */
bool autoPrep (Session&);
/** Song mode (whole-kit takes): finds every hit and the beats (Song.h). autoPrep runs it for whole-kit takes. */
void analyseSongOf (Session&);

/** Whole-kit takes: up to 48 of the loudest hits, at least 300 ms apart. */
std::vector<long> strongHits (const F32& x, double sr);
std::string hitCountAdvice (const Session&);
} // namespace ld::hl

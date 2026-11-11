#pragma once
#include "Align.h"
#include "Clean.h"
#include "FxSat.h"
#include "Levels.h"
#include "Resolve.h"
#include "Shape.h"
#include "Tune.h"
#include "Song.h"
#include <functional>
#include <optional>
#include <string>

namespace ld::hl
{
/** Port of Hitline 4.8's session orchestration (app modules 20_analysis, 25_tone, 26_mix, 29_inserts,
    40_audio, 55_panels, 60_export): the processing chain every mic goes through on its way to the export,
    the tone and shape moves turned into per-mic, per-velocity curves, the inserts, bus and shared room,
    and the export itself (file names, tails, hit and full-track files).

    The chain for one mic: L/R lock, clean-up, tone and shape (zero-phase), tame (linked across mics),
    channel inserts, bus inserts, phase EQ; then delay, polarity, tuning and the Level gain per hit. */

struct ToneMoves { double match = 0, bright = 0, crack = 0, smooth = 0, tame = 0; };
struct ShapeMoves { double atk = 0, dec = 0, sus = 0, rel = 0, match = 0; };

struct ToneSettings : ToneMoves
{
    bool repair = false;
    std::string proSrc;               ///< "panda" for the Panda reference set, else the libraries
    bool hasRef = false;              ///< a reference sound is loaded
    F64 refCurve;                     ///< its curve on the 30 third-octave bands (dB)
    double refAmt = 0;
    bool hasRefEnv = false;           ///< the reference's envelope against the mix: body, sustain, tail (dB, theirs - ours)
    std::array<double, 3> refEnv { 0, 0, 0 };
    double refEnvAmt = 0;             ///< Match envelope: 0..1
};

struct Strip
{
    double trim = 0, pan = 0, send = -60;   ///< send -60 = off (the old shared room)
    double r1 = -60, r2 = -60;              ///< reverb sends R1/R2 (dB, post-fader); -60 = off
    bool pol = false;
    ToneMoves tone;
    ShapeMoves shape;
    juce::var inserts;                ///< array of { type, on, p }, Hitline's shape
};

struct LrFix { double lag = 0; int pol = 1; bool kept = false; };

/** A recording shared between copies of a session (copying a session for a background render doesn't copy the audio).
    Reading is const and never copies; mut() gives a private copy first if another session still shares it. */
class SharedChs
{
public:
    SharedChs() = default;
    SharedChs (Chs c) : p (std::make_shared<Chs> (std::move (c))) {}
    SharedChs& operator= (Chs c) { p = std::make_shared<Chs> (std::move (c)); return *this; }
    const Chs& get() const { static const Chs none; return p ? *p : none; }
    operator const Chs&() const { return get(); }
    Chs& mut() { if (! p) p = std::make_shared<Chs>(); else if (p.use_count() > 1) p = std::make_shared<Chs> (*p); return *p; }
    size_t size() const { return get().size(); }
    bool empty() const { return get().empty(); }
    const F32& operator[] (size_t i) const { return get()[i]; }
    Chs::const_iterator begin() const { return get().begin(); }
    Chs::const_iterator end() const { return get().end(); }
    const F32& front() const { return get().front(); }
    /** The buffer itself, kept alive for as long as the caller holds it (a waveform view, say). */
    std::shared_ptr<const Chs> share() const { return p ? std::shared_ptr<const Chs> (p) : std::make_shared<const Chs>(); }
private:
    std::shared_ptr<Chs> p;
};

struct SessionTrack
{
    std::string mic, name;
    SharedChs ch;                     ///< the recording (shared between session copies; mut() to change it)
    F32 mono;
    long len = 0;
    std::string alignTo = "lead";
    std::string role;                 ///< what the sound says this mic is (sub, bottom, far, near)
    std::string roleOverride;         ///< what the user says it is (empty: go by the sound)
    double lag = 0;                   ///< applied delay (samples)
    int pol = 1;
    std::vector<AllPass> ap;          ///< phase EQ
    F64 clip;                         ///< clip gain per hit (dB)
    double fader = 0;
    double panL = -1, panR = 1;   ///< a stereo mic's two pans, as Pro Tools has them (left channel, right channel); hard left/right = Hitline's stereo pan
    bool mute = false, solo = false, lrLock = false;
    std::optional<LrFix> lr;
    double autoAsis = NAN;
    Strip strip;
    // alignment state (Hitline's track fields)
    double amount = 1;
    bool apOn = true, manualLag = false, manualPol = false;
    bool manualAp = false; std::vector<AllPass> userAp;   ///< the phase EQ set by hand (sample build): kept through every re-resolve until Reset
    std::optional<AlignResult> autoResult, grp;
    std::optional<AlignResult> inPlace;   ///< rooms: polarity and phase EQ measured where the mic sits (never moved in time)
    std::optional<int> proPol;
    std::string label;                ///< "24-bit" etc.
};

struct ExportSettings
{
    std::string mode = "hits";        ///< hits, full, both
    std::string hitTemplate = "{DRUM}_{ARTIC}_RR{RR}_VEL{VEL}/{MIC}.wav";
    std::string fullTemplate = "{DRUM}_{ARTIC}_{MIC}.wav";
    std::string kitTemplate = "{ARTIC}/{MIC}.wav";
    int bits = 32;
    std::string rate = "same";        ///< "same" or a rate in Hz
    std::string tailMode = "natural"; ///< natural, next, fade
    double tailFloor = -90, maxLenS = 8, fadeOutMs = 40, fadeInMs = 0.5, tailDb = -70, ceilDb = -1;
    std::string trim = "off";
    /** Whole-kit takes: also write the drum bus (a stereo mix of every mic as the mixer has it), and the reverb
        returns as their own files ("separate") or printed into the bus ("print"). */
    bool kitBus = true;
    std::string verbs = "separate";
};

struct RoomSettings { bool on = false; double ret = 0; juce::var p; };

/** One of the two reverb units in the rack (R1, R2): 100 % wet, fed by every channel's send and the drum bus send.
    type: room, plate, hall, chamber. decay x0.25..3, pre 0..250 ms, size (early reflections) 0..2, tone -1 dark..+1 bright,
    width 0..2. */
struct VerbUnit { std::string type = "room"; double decay = 1, pre = 0, size = 1, tone = 0, width = 1; };

struct TuneState { F64 ratios; double medF = NAN; };

struct Session
{
    double sr = 44100;
    std::string drum = "snare", artic, mode = "drum";
    int refIdx = 0;
    std::vector<long> onsets;
    std::vector<int> rejects;
    Det det;
    LevelSettings lev;
    ExportSettings exp;
    bool dnOn = true; double dnDb = 18;
    ToneSettings tone;
    ShapeMoves shape;
    juce::var busIns;                 ///< bus insert slots
    RoomSettings room;
    std::array<VerbUnit, 2> verbs { VerbUnit { "room" }, VerbUnit { "plate" } };
    double busR1 = -60, busR2 = -60;  ///< drum bus sends (tapped from the channel sum before the returns)
    std::optional<TuneState> tinfo;
    // detection, alignment and tuning settings and results (Hitline's det, phase, groupAnchors, tune, pitch)
    double minGapMs = 180, thrDb = -40;
    bool thrAuto = true, detRan = false;
    bool phaseAnalyzed = false, phaseStale = false, flipped = false, leadFlipAuto = true;
    std::array<std::string, 4> groupAnchors { "auto", "auto", "auto", "auto" };
    bool roomPhase = false;           ///< sample build: room mics keep their distance but get polarity and phase EQ against the lead
    std::string timing = "tight";     ///< the Align step's timing: tight, pro (like the pros), natural (as recorded)
    TuneSettings tune;
    PitchOptions pitchOpt;            ///< fmin, fmax, skip and window, per drum
    std::string tuneMic = "ref";
    bool tuneAnalyzed = false, tuneAutoWin = true;
    bool tuneAnyway = false;          ///< tune a cymbal, or a drum with no steady pitch, though it isn't recommended
    bool keepAttack = false;          ///< Limitless: on shifts of a semitone or more the stick attack keeps its real pitch
    int tuneTaps = 32;                ///< the varispeed filter on export (Hitline 32; Limitless 64)
    SessionPitch pitch;
    std::optional<TuneInfo> tuneInfo;
    std::vector<long> kitOn;          ///< whole-kit takes: the strong hits alignment is measured on
    double kitBleed = 0;              ///< whole-kit takes: close mics ease down this many dB between their own hits
    SongState song;                   ///< whole-kit takes (Song mode): the hits, tempo and the Edit & Tempo settings
    // the console (playback and the one-shot only)
    double busFader = 0;
    bool busMute = false;
    int oneShot = -1;                 ///< the hit the one-shot renders (-1 = a typical hardest hit)
    bool oneShotNorm = true;          ///< peak the one-shot at the ceiling
    int tameWorkers = 3;              ///< Hitline splits the tame pass into this many stretches of hits
    std::vector<SessionTrack> tracks;

    long maxLen() const;
    int velOfHit (size_t i) const;
};

/** The pro library measurements (assets/libref.json), shared by every session. */
void setLibraryReference (const juce::var& libref);
/** Whether the measured studio set is in the library reference (the second source for the pro tone). */
bool studioReferenceAvailable();

/** Everything the processing chain has rendered for a session. */
struct MixRender
{
    std::vector<Chs> dn, tone, ins, bus, proc;   ///< per track: after clean-up, tone/shape/tame, inserts, bus, phase EQ
    Chs room;                                    ///< the shared room's return (empty when off)
    std::array<Chs, 2> rev;                      ///< R1, R2 returns, stereo, on the lead's timeline (empty when nothing sends)
    /** Each compressor's gain reduction (dB >= 0, GR_RATE blocks), keyed "<track>:<slot>" or "bus:<slot>". */
    std::map<std::string, F32> gr;
    struct Scan { std::vector<F64> bands; std::vector<EnvStats> env; ResScanResult res; };
    std::vector<std::optional<Scan>> scan;       ///< per track, when the tone pass listened to it
};

/** The analysis Hitline's tone pass keeps per mic, and the curves it plans from it (public for the views). */
class ToneEngine
{
public:
    ToneEngine (const Session&, const std::vector<std::optional<MixRender::Scan>>& scans);
    std::string libDrum() const;
    std::string libFam (int t) const;
    ToneMoves moves (int t) const;    ///< bus + strip, clamped (Hitline's TT)
    ShapeMoves shapeMoves (int t) const;   ///< (HH)
    F64 matchBands (int t, int layer) const;
    double toneAt (int t, const F64* mb, double f) const;
    double cutAt (int t, double f, double harsh) const;
    double harshOf (int t) const;
    F32 shapeTable (int t) const;     ///< empty when flat
    FxPlan plan (int t) const;
    bool isCymbal() const;
    std::string houseKeyOf (int t) const;
    double arrOf (int t) const;       ///< first arrival after the lead (samples)
    double proTarget (int t) const;   ///< where the pros put this mic after the lead (samples), NaN if none
    double proLag (int t) const;      ///< the delay that puts it there (Standard timing)
    /** Where the pro libraries' envelope sits against this mic's (body, sustain, tail, dB, clamped +-6), at full match. */
    std::array<double, 3> proEnvDelta (int t) const;

private:
    struct Ref { juce::var v; double arr = NAN; bool ok = false; };
    Ref libRef (int t) const;
    static std::string libDrumNow (const Session&, const std::vector<std::optional<MixRender::Scan>>&);
    std::string houseKeyNow (int t) const;
    std::string libFamNow (int t) const;
    double refAt (double f) const;
    const Session& S;
    const std::vector<std::optional<MixRender::Scan>>& scans;
    mutable std::vector<double> arrCache;
    mutable std::vector<std::string> houseCache, famCache;
    mutable std::optional<std::string> drumCache;
};

bool fxOn (const Session&);
bool trackFxOn (const Session&, int t);
bool roomLive (const Session&);
bool busLive (const Session&);
bool verbLive (const Session&, int k);
bool refEnvLive (const Session&);
/** Standard: the Tone and Shape knobs set to land close to where the pro libraries put this channel (sel -1: the bus,
    every channel), as plain knob values you can then move. Rings and Tame get their usual starting amounts. */
std::pair<ToneMoves, ShapeMoves> standardMoves (const Session&, const MixRender&, int sel);   ///< anything sends to reverb unit k (0 = R1, 1 = R2)
/** The reverb unit's parameters for the reverb engine (model, programme and knobs). */
juce::var verbParams (const VerbUnit&);

/** Runs the whole chain (Hitline's runDenoise, runFx with tame, runIns with bus and room). */
MixRender renderMix (const Session&);

/** Levels, measured on what gets exported (after the mix), as Hitline's computeAll. */
LevelResult measureLevels (const Session&, const MixRender&);

/** One hit of one mic, ready for the export: phase EQ, delay, polarity, tuning, gain, output rate. */
Chs renderHitMic (const Session&, const MixRender&, int t, size_t i, const Region&, double srOut, double gain);
/** One hit of the shared room's return, on the lead's timeline. Empty when the room is off. */
Chs renderRoomHit (const Session&, const MixRender&, size_t i, const Region&, double srOut, double gain);
/** One mic, full length on the original timeline, every hit with its own alignment, tuning and gain. */
Chs renderFull (const Session&, const MixRender&, const LevelResult&, int t, double trimLin, double srOut);

struct ExportItem { std::string kind; size_t i = 0; int t = 0; std::string path; };   ///< kind: hit, room, full; t = -1 for the room
std::vector<ExportItem> exportList (const Session&);
std::string fillTokens (const Session&, const std::string& tpl, int t, std::optional<size_t> hit);

juce::MemoryBlock encodeWav (const Chs&, double sr, int bits);
void applyFades (Chs&, long inN, long outN);

/** Writes every audio file of the export through the sink, in Hitline's order. Returns the file count.
    Sampler instrument files are not written (not wanted in Limitless Drums). */
int writeSession (const Session&, const MixRender&, const LevelResult&,
                  const std::function<void (const std::string& path, const juce::MemoryBlock& bytes)>& sink);

/** Whole-kit takes: every mic full length with its delay, polarity, phase EQ and L/R lock, through the sink. */
int writeKit (const Session&, const MixRender&,
              const std::function<void (const std::string& path, const juce::MemoryBlock& bytes)>& sink);

/** One hit, every mic, ready to play: processed, on its level, one natural tail for all mics (Hitline's
    renderHitStems for processed playback). Mono mics come back as one channel at -3 dB. */
struct HitStems { std::vector<Chs> stems; Chs room; std::array<Chs, 2> rev; size_t i = 0; double ratio = 1; long start = 0, len = 0; };
struct StemOptions
{
    bool processed = true;            ///< false: the recording as it is (Hitline's A side)
    bool mono = false;                ///< every mic summed to one channel (the mono check)
    int taps = 32;                    ///< playback uses 16
    std::vector<int> only;            ///< just these tracks (empty = all)
};
HitStems renderHitStems (const Session&, const MixRender&, const LevelResult&, size_t i, const StemOptions& = {});
/** The console's pan law: {LL, RL, LR, RR}. A mono channel is equal-power, a stereo channel a balance control. */
std::array<double, 4> panGains (double pan, bool stereo);
/** A stereo mic panned per channel (Pro Tools): { left ch to L, right ch to L, left ch to R, right ch to R }. */
std::array<double, 4> panGainsLR (double panL, double panR);
/** The gains a track mixes with: Hitline's stereo pan while both channel pans are hard left/right, per channel otherwise. */
std::array<double, 4> trackPanGains (double pan, double panL, double panR, bool stereo);
/** The stems summed to stereo as the console stands: faders, mute and solo, pan, the bus fader, the room return. */
std::pair<F32, F32> mixToStereo (const Session&, const HitStems&);
/** A typical hard hit: the hardest layer's round robin closest to its median level on the lead. */
size_t defaultOneShot (const Session&, const LevelResult&);
/** The one-shot: one hit mixed as the console stands, as a stereo WAV. Empty name when the bus is silent. */
struct OneShot { std::string name; juce::MemoryBlock wav; size_t hit = 0; double scale = 1; };
OneShot exportOneShot (const Session&, const MixRender&, const LevelResult&);

/** The settings a new session can start from (New Session, Start From): detection, levels, export, clean-up,
    tone, shape, bus inserts, the room, and each mic's strip and fader (matched by mic name). No audio, no hits,
    no alignment results. Keys as in Hitline's saved state. */
juce::var settingsToVar (const Session&);
/** A mic's applied delay (samples; the lead's is 0) and its polarity with the strip's flip, as the export uses them. */
double trackLag (const Session&, int t);
int trackPol (const Session&, int t);
/** Applies saved settings onto a freshly opened session. Mics the settings don't name keep their defaults. */
void applySettings (Session&, const juce::var&);

/** A session from Hitline's saved state (its S object as JSON) and each track's audio, in track order. */
Session sessionFromHitline (const juce::var& state, std::vector<Chs> audio);
} // namespace ld::hl

#pragma once
#include "Align.h"
#include "Clean.h"
#include "FxSat.h"
#include "Levels.h"
#include "Shape.h"
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
};

struct Strip
{
    double trim = 0, pan = 0, send = -60;   ///< send -60 = off
    bool pol = false;
    ToneMoves tone;
    ShapeMoves shape;
    juce::var inserts;                ///< array of { type, on, p }, Hitline's shape
};

struct LrFix { double lag = 0; int pol = 1; bool kept = false; };

struct SessionTrack
{
    std::string mic, name;
    Chs ch;                           ///< the recording
    F32 mono;
    long len = 0;
    std::string alignTo = "lead";
    double lag = 0;                   ///< applied delay (samples)
    int pol = 1;
    std::vector<AllPass> ap;          ///< phase EQ
    F64 clip;                         ///< clip gain per hit (dB)
    double fader = 0;
    bool mute = false, lrLock = false;
    std::optional<LrFix> lr;
    double autoAsis = NAN;
    Strip strip;
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
};

struct RoomSettings { bool on = false; double ret = 0; juce::var p; };

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
    std::optional<TuneState> tinfo;
    int tameWorkers = 3;              ///< Hitline splits the tame pass into this many stretches of hits
    std::vector<SessionTrack> tracks;

    long maxLen() const;
    int velOfHit (size_t i) const;
};

/** The pro library measurements (assets/libref.json), shared by every session. */
void setLibraryReference (const juce::var& libref);

/** Everything the processing chain has rendered for a session. */
struct MixRender
{
    std::vector<Chs> dn, tone, ins, bus, proc;   ///< per track: after clean-up, tone/shape/tame, inserts, bus, phase EQ
    Chs room;                                    ///< the shared room's return (empty when off)
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

/** A session from Hitline's saved state (its S object as JSON) and each track's audio, in track order. */
Session sessionFromHitline (const juce::var& state, std::vector<Chs> audio);
} // namespace ld::hl

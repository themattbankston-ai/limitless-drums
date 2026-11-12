#pragma once
#include <condition_variable>
#include <mutex>

#include <tracktion_engine/tracktion_engine.h>
#include "app/Engine.h"
#include "app/ui/Wave.h"
#include "live/AlignPlugin.h"
#include <functional>
#include <map>
#include <set>
#include <memory>

namespace ld::samples
{
namespace te = tracktion;

/** One track of the open take, as the screens show it. */
struct TrackInfo
{
    std::string name;
    unsigned colour = 0xffe0a43c;
    int channels = 1;
    float peakDb = -100;   ///< the loudest sample of the recording
    std::shared_ptr<ui::Wave> wave;
};

/** One hit: where it starts (seconds on the timeline) and which velocity and round robin it is. */
struct HitInfo
{
    double time = 0, end = 0;
    int vel = 0, rr = 0;   ///< 0-based
    float peakDb = -100;
};

/** The open take: the analysis session, the Tracktion edit that plays it live (each track: Align, then the
    monitor fader), and the transport. Everything here runs on the message thread except Auto Prep, which works
    on a copy in the background and hands the result back. */
class SampleModel
{
public:
    /** The take sits this far into the edit, so the Align plugins' look-ahead has audio before the first hit (a
        render or playback from 0 would otherwise lose a hit that starts the file). Every time this class takes or
        gives is on the take's own timeline; only the edit is offset. */
    static constexpr double kPad = 0.25;

    explicit SampleModel (te::Engine&);
    ~SampleModel();

    /** Opens recordings as one take. Returns an error message, or empty. */
    juce::String open (const juce::Array<juce::File>& files);
    bool isOpen() const { return edit != nullptr; }
    juce::String takeName() const { return name; }

    const std::vector<TrackInfo>& tracks() const { return info; }
    const std::vector<HitInfo>& hits() const { return hitList; }
    int velocities() const;
    int roundRobins() const;
    double length() const;   ///< seconds
    double sampleRate() const { return state ? state->session.sr : 48000; }
    int leadTrack() const { return state ? state->session.refIdx : 0; }

    // Auto Prep: find the hits, align (more steps as they're built). Runs in the background.
    void autoPrep (std::function<void (juce::String)> done);
    bool isPrepping() const { return prepping; }
    bool isPrepared() const { return state && state->prepared; }
    /** Auto Prep on this thread, start to finish (for tests and tools). Returns the error, or empty. */
    juce::String autoPrepNow();

    // ---- the steps change the session through here
    enum class Redo { none, align, tune, levels, mix, reprep };
    /** Changes the session (refused while Auto Prep runs: its result would replace the change). none: export settings
        only. align: heard at once (the Align plugins), levels re-measured in the background. tune: the clips' ratios at
        once, then levels. levels: re-measured in the background (gains live when done). mix: clean-up and tone re-rendered
        in the background, then the hits play the new audio. reprep: what Auto Prep found no longer holds (counts, drum,
        lead, order): export is blocked until it runs again. Playback keeps going throughout. Every change can be
        undone; changes with the same `gesture` (one slider drag) within 2 s undo as one. Returns false if refused. */
    bool change (const std::function<void (hl::Session&)>& f, Redo what, const std::string& gesture = {});
    bool isRendering() const { return rendering; }
    /** A background render is running or wanted, or Auto Prep is: what plays isn't yet the current settings. */
    bool isBusy() const { return prepping || rendering || renderDone != renderWanted; }
    /** Why export can't run now (empty: it can). */
    juce::String exportBlocker() const;
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();
    /** How well mic t sums with what it lines up to, as it is set now (-1..1), and as recorded. NaN when unknown. */
    std::pair<double, double> sumScore (int t, bool* stale = nullptr);   ///< *stale: showing the last score while the new one is measured
    const hl::Session* session() const { return state ? &state->session : nullptr; }
    /** Room mics keep their distance but are set in phase with the lead (polarity and phase EQ only). The choice is the
        app's, not one take's: it is applied to every take opened after it, and Auto Prep follows it. */
    bool roomPhase() const { return state ? state->session.roomPhase : roomPhasePref; }
    bool roomPhaseDefault() const { return roomPhasePref; }   ///< what the next take opened starts with
    void setRoomPhase (bool on);

    // ---- projects: everything you set on a take, saved beside it and recalled
    /** Writes the project: where the recordings are, every setting, each mic's alignment as applied, the names you
        gave, and the monitor mix. No audio. Returns the error, or empty. */
    juce::String saveProject (const juce::File&);
    /** Opens the project's recordings and puts the settings back. When it was saved after Auto Prep, recallNeedsPrep()
        is true: run Auto Prep, and the saved alignment is put back exactly when it lands (the hits are restored as
        saved, so nothing is found again). Returns the error, or empty. */
    juce::String openProject (const juce::File&);
    bool recallNeedsPrep() const { return recall.isObject(); }
    /** What differed from the saved project once it was recalled (empty: nothing). */
    juce::String recallNote() const { return recallMsg; }
    juce::File projectFile() const { return projFile; }
    /** Where Save writes when the take has no project yet: beside the recordings, named after the take. */
    juce::File defaultProjectFile() const;
    /** The project in that folder saved for exactly these recordings (the newest, if several), or no file. */
    static juce::File projectFor (const juce::Array<juce::File>& items);
    bool modified() const { return isOpen() && edits != savedEdits; }
    static constexpr int kProjectVersion = 1;

    // ---- the reference (Tone match): a finished sample the take is moved toward
    /** Measures the reference against the take (hl::kReferenceEngine) and sets the Tone match from it; one undo step.
        Its first hit is kept to listen to. Returns the error, or empty. */
    juce::String loadReference (const juce::File&);
    bool hasReference() const { return state && state->session.tone.hasRef; }
    juce::String referenceName() const { return refName; }
    int referenceHits() const { return refHits; }
    /** Bands left alone at the last load because your own mics cancel there (Hz). */
    const std::vector<double>& referenceCancelled() const { return refCancelled; }
    /** The take has changed since the reference was measured against it (hits, alignment, clean-up or tuning), so the
        match was made for a sound that is no longer the one you have. Load it again to measure it again. */
    bool referenceStale() const;
    /** Plays the reference in place of the take, at `at` seconds on the take's timeline (the hit you're on), at the
        take's loudness when Level match is on. A and B are untouched and come back when it's turned off; export,
        A/B and removing the reference turn it off. Returns the error, or empty. */
    juce::String setReferenceAudition (bool on, double at = 0);
    bool referenceAudition() const { return refAud; }

    /** The name shown for a track and written for {MIC}. Empty: back to the recording's own name. Returns the error
        (two tracks can't share a name: their files would overwrite each other), or empty. */
    juce::String renameTrack (int t, const std::string& name);
    const hl::MixRender* mixRender() const { return state && state->mix ? &*state->mix : nullptr; }
    const hl::LevelResult* levelResult() const { return state && state->levels ? &*state->levels : nullptr; }
    std::function<void()> onChanged;   ///< after a background render lands (message thread)

    // for tests and tools
    te::Edit* getEdit() const { return edit.get(); }
    /** How many hits currently play from memory, worked out for the settings as they are now (tests, the busy light). */
    int hitsPreviewed() const { int n = 0; for (auto& [i, h] : hitAudio) if (h.gen == audioGen) ++n; return n; }
    bool previewRunning() const { return previewWorkers > 0; }
    bool clipsWaiting() const { return clipsStale; }
    /** Tests: the rate previewed hits are made for (0: the audio device's), and one track rendered as it plays now. */
    double previewRateForTests = 0;
    bool holdClipsForTests = false;   ///< as if something were playing: the rendered files wait, the previewed hits are what's heard
    hl::Chs renderTrackForTests (int t, double rate) { juce::String e; return renderB (t, rate, false, e); }
    const EngineState* engineState() const { return state.get(); }

    // monitoring: never part of what's exported
    float monDb (int t) const;
    void setMonDb (int t, float db);
    float pan (int t) const;
    void setPan (int t, float p);
    bool solo (int t) const;
    void setSolo (int t, bool);
    bool mute (int t) const;
    void setMute (int t, bool);
    std::pair<float, float> meter (int t);   ///< left and right peak since the last call, dB

    // alignment (the Align step)
    bool flip (int t) const;
    void setFlip (int t, bool);
    float shiftMs (int t) const;

    // A/B: A as recorded, B with everything applied. Level match plays A at B's loudness.
    void setProcessed (bool b);
    void setLevelMatch (bool on);
    bool levelMatch() const { return levelMatchOn; }
    te::AudioTrack* trackA (int t) const;   ///< the as-recorded copy of track t (for tests and tools)
    bool processed() const { return processedOn; }

    // transport
    void play();
    void stop();
    bool isPlaying() const;
    double position() const;
    void setPosition (double s);
    void setLoop (double start, double end, bool on);
    bool looping() const;

    juce::UndoManager* undoManager() { return edit ? &edit->getUndoManager() : nullptr; }

private:
    te::Engine& engine;
    std::shared_ptr<EngineState> state;
    std::unique_ptr<te::Edit> edit;
    std::vector<TrackInfo> info;
    std::vector<HitInfo> hitList;
    juce::String name;
    juce::File cacheDir;
    bool prepping = false, processedOn = true;
    int takeGen = 0;   ///< bumped by every open: work started for an older take is dropped when it lands
    struct Undo { hl::Session session; bool prepared = false; };
    std::vector<Undo> undoStack, redoStack;
    std::string lastGesture;
    double lastGestureTime = 0;
    void reapply (bool mixToo);
    /** Everything a score depends on: if any of it differs, the cached score is stale. */
    struct ScoreKey
    {
        double lag = 0; int pol = 1; bool apOn = false; std::vector<std::pair<double, double>> ap;   ///< every band's fc and Q
        int ref = -1; double arrival = 0, sr = 0; int gen = 0;
        std::vector<int> onsets;
        std::shared_ptr<const hl::Chs> refCh, ch;
        bool operator== (const ScoreKey&) const;
    };
    ScoreKey scoreKey (int t) const;
    std::map<int, std::pair<ScoreKey, std::pair<double, double>>> scoreCache;
    /** The background threads still running (Auto Prep, renders, scores). The destructor waits for them, so none
        is left running into the process's shutdown (that crashed at exit: a thread locking a destroyed mutex). */
    struct Workers
    {
        std::mutex m; std::condition_variable cv; int n = 0;
        struct Running   ///< held by a worker thread for as long as it runs
        {
            std::shared_ptr<Workers> w;
            explicit Running (std::shared_ptr<Workers> ws) : w (std::move (ws)) {}
            Running (const Running&) = delete;
            ~Running() { { const std::lock_guard<std::mutex> l (w->m); --w->n; } w->cv.notify_all(); }
        };
        /** Call on the message thread just before starting a worker; give the result to the thread. */
        static std::shared_ptr<Running> start (const std::shared_ptr<Workers>& ws) { { const std::lock_guard<std::mutex> l (ws->m); ++ws->n; } return std::make_shared<Running> (ws); }
    };
    std::shared_ptr<Workers> workers = std::make_shared<Workers>();
    bool roomPhasePref = false;
    hl::Chs refAudio; juce::File refSource, refPlayFile; juce::String refName; int refHits = 0, refEngine = 0, refSerial = 0;
    std::string refPrint;    ///< what the take was when the reference was measured against it
    bool refAud = false; double refGainDb = 0;
    std::vector<double> refCancelled;
    std::string referencePrint() const;
    te::AudioTrack* refTrack() const;
    juce::var recall;        ///< the project being recalled, until Auto Prep lands
    juce::String recallMsg;
    juce::File projFile;
    std::vector<juce::File> takeFiles;   ///< the recordings as opened
    int edits = 0, savedEdits = 0;
    juce::var projectVar (const juce::File& beside) const;
    void finishRecall();
    void syncNames();
    void monDbTo (int t, float db);   ///< the fader moved by the app itself (level match), not by you
    std::set<int> scoring;   ///< tracks being measured on another thread
    int toneVersion = 0;
    bool rendering = false;
    int renderWanted = 0, renderDone = 0;   ///< edit counters: a render that lands stale starts another
    bool wantMix = false;
    void startRender();
    std::vector<bool> userMute;
    bool levelMatchOn = true;
    double matchDb = 0;   ///< what A needs to sound as loud as B
    void applyMutes();
    void recomputeLevelMatch();
    double changeLead() const;
    // Tune: each tuned hit is rendered once into its own file (the reference's varispeed, at 128 points, onset on a
    // whole output sample), so playback and export place it exactly; Tracktion plays those files as they are.
    std::map<std::string, juce::File> hitFiles;   ///< key: track, hit, ratio, rate (cleared when the processed audio changes)
    static std::string hitKey (int t, size_t i, double ratio, double rate);
    juce::File hitFile (int t, size_t i, double outRate);
    static hl::Chs renderHit (const EngineState&, const hl::MixRender*, int t, size_t i, double ratio, double outRate);   ///< how far ahead of each hit its settings switch (seconds)
    std::vector<juce::File> prepFiles;
    const void* prepFor = nullptr;
    juce::File prepFile (int t);
    std::vector<live::AlignPlugin::HitScale> scalesFor (int t, const std::vector<int>* only) const;
    /** null: playing (every hit, each ending where the next starts). Otherwise only the hits listed, each with its whole
        tuned region (export: the lists are chosen so no two overlap). */
    void layoutHitsFor (const std::vector<int>* only, double outRate = 0, bool renderMissing = true);   ///< outRate: the rate it renders at (0: the take's)
    bool tuneLayoutWanted = false;
    // Hearing a change at once: the hits are worked out again one at a time, the one under the playhead first, and
    // handed to the track's Align plugin, which plays them instead of the clips (a short crossfade, playback carries
    // on). The whole render still runs behind it for the levels and the export; while something plays, its files
    // aren't laid on the tracks (that rebuilds playback): the clips catch up when playback stops.
    struct HitBuf { int gen = 0; double ratio = 1; juce::int64 start = 0; std::vector<std::shared_ptr<const hl::Chs>> tracks; };
    std::map<size_t, HitBuf> hitAudio;      ///< per hit, every mic (kept until the clips hold the same audio)
    double hitAudioRate = 0;
    int audioGen = 0;                       ///< counts the changes to what the hits sound like
    std::shared_ptr<std::atomic<int>> previewGen = std::make_shared<std::atomic<int>> (0);   ///< a worker stops when it moves on
    int previewWorkers = 0;
    bool previewOk = false, clipsStale = false;
    void startPreview();
    std::shared_ptr<const hl::Listening> listening;   ///< what the tone pass hears in this take, made ahead of the first knob touched
    bool listeningWanted = false;
    void warmPreview();
    void watchClips();
    void renderSoon();                      ///< startRender once the last change is renderHoldMs old (at once when nothing is previewed)
    double lastChangeMs = 0, renderHoldMs = 250;
    bool renderTimerSet = false;
    void cancelPreview() { ++*previewGen; }
    void publishHitAudio();
    void clearHitAudio();
    void settleClips();                     ///< stopped and rendered: the clips take over from the previewed hits
    void landRender (std::function<void()>);   ///< runs it once no preview worker reads the render it replaces
    std::vector<double> playedRatio;   ///< per hit, the ratio its clips play at now (the Align table follows it)
    hl::Chs renderB (int t, double srOut, bool stereo, juce::String& err);
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    std::map<int, std::pair<te::LevelMeasurer*, std::unique_ptr<te::LevelMeasurer::Client>>> meters;
    void dropMeters();
    std::vector<juce::File> sourceFiles;   ///< each track as recorded (what A plays)
    te::AudioTrack* track (int t) const;
    void buildHits();
    void applyAlignment();
public:
    te::AudioTrack* trackB (int t) const { return track (t); }
    /** Each hit becomes its own clip on every track (all cut at the same place), tuned by its ratio (varispeed, best
        sinc), and the Align plugin gets each hit's Level gain and offset scale. Needs the levels; until then each
        track plays as one clip. Called after Auto Prep and whenever Tune or Level change. */
    void layoutHits (bool renderMissing = true);
    /** Level changed only: new gains, live, no rebuild. */
    void updateHitGains();

    /** Writes the export (Level and Tune as set, every hit of every mic, the monitor faders ignored) into `dir`, rendered
        by Tracktion through the same plugins playback uses. Returns the error, or empty; `count` gets the file count. */
    juce::String exportTo (const juce::File& dir, int& count, std::function<void (float)> progress = {}, std::function<bool()> cancel = {});
    /** `cancel` is asked between renders; once it says yes the export ends before any file is written, so a cancelled
        export leaves the folder exactly as it was. This is what exportTo returns then. */
    static juce::String exportCancelled() { return "Export cancelled. Nothing was written."; }
    double hitGainDb (int t, size_t i) const;   ///< Level gain of this hit on this mic (dB, trim included)
    double hitRatio (size_t i) const;           ///< Tune: varispeed ratio of this hit (1 = untouched)
};

} // namespace ld::samples

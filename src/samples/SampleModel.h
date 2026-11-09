#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "app/Engine.h"
#include "app/ui/Wave.h"
#include "live/AlignPlugin.h"
#include <functional>
#include <map>
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
    enum class Redo { align, tune, levels, mix };
    /** Changes the session. align: heard at once (the Align plugins), levels re-measured in the background. tune: the
        clips' ratios at once, then levels. levels: re-measured in the background (gains live when done). mix: clean-up and
        tone re-rendered in the background, then the hits play the new audio. Playback keeps going throughout. */
    void change (const std::function<void (hl::Session&)>& f, Redo what);
    bool isRendering() const { return rendering; }
    const hl::Session* session() const { return state ? &state->session : nullptr; }
    const hl::MixRender* mixRender() const { return state && state->mix ? &*state->mix : nullptr; }
    const hl::LevelResult* levelResult() const { return state && state->levels ? &*state->levels : nullptr; }
    std::function<void()> onChanged;   ///< after a background render lands (message thread)

    // for tests and tools
    te::Edit* getEdit() const { return edit.get(); }
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
    int toneVersion = 0;
    bool rendering = false;
    int renderWanted = 0, renderDone = 0;   ///< edit counters: a render that lands stale starts another
    bool wantMix = false;
    void startRender();
    std::vector<bool> userMute;
    bool levelMatchOn = true;
    double matchDb = 0;   ///< what A needs to sound as loud as B
    void applyMutes();
    std::vector<juce::File> prepFiles;
    const void* prepFor = nullptr;
    juce::File prepFile (int t);
    std::vector<live::AlignPlugin::HitScale> scalesFor (int t, int parity) const;
    void layoutHitsFor (int parity);   ///< -1: playing; 0 / 1: only the even / odd hits, uncapped (export)
    hl::Chs renderB (int t, double srOut, bool stereo);
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
    void layoutHits();
    /** Level changed only: new gains, live, no rebuild. */
    void updateHitGains();

    /** Writes the export (Level and Tune as set, every hit of every mic, the monitor faders ignored) into `dir`, rendered
        by Tracktion through the same plugins playback uses. Returns the error, or empty; `count` gets the file count. */
    juce::String exportTo (const juce::File& dir, int& count, std::function<void (float)> progress = {});
    double hitGainDb (int t, size_t i) const;   ///< Level gain of this hit on this mic (dB, trim included)
    double hitRatio (size_t i) const;           ///< Tune: varispeed ratio of this hit (1 = untouched)
};

} // namespace ld::samples

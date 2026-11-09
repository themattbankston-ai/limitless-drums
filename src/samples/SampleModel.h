#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "app/Engine.h"
#include "app/ui/Wave.h"
#include "live/AlignPlugin.h"
#include <functional>
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

    // A/B: A as recorded, B with everything applied
    void setProcessed (bool b);
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
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    std::map<int, std::pair<te::LevelMeasurer*, std::unique_ptr<te::LevelMeasurer::Client>>> meters;
    void dropMeters();
    te::AudioTrack* track (int t) const;
    void buildHits();
    void applyAlignment();
};

} // namespace ld::samples

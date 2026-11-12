#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "AlignDsp.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace ld::live
{
namespace te = tracktion;

/** The Align step, live on a track: shift (ms, + = earlier), polarity and gain. The settings live in the Edit's
    state, so they save with the session and every change is undoable; playback hears them at once and an export
    renders exactly the same thing. Every Align plugin reports the same latency, so the host's compensation keeps the
    tracks on the timeline and only the shifts move them relative to each other. */
class AlignPlugin : public te::Plugin
{
public:
    explicit AlignPlugin (te::PluginCreationInfo);
    ~AlignPlugin() override;

    static const char* getPluginName() { return "Align"; }
    static const char* xmlTypeName;

    juce::String getName() const override { return "Align"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Align"; }
    juce::String getSelectableDescription() override { return "Align"; }

    void initialise (const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void reset() override { dsp.reset(); jumpNext = true; }
    double getLatencySeconds() override { return AlignDsp::latencySamples (sampleRate) / sampleRate; }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }
    void applyToBuffer (const te::PluginRenderContext&) override;

    // the stored value is the truth (it's what undo and saving see); the audio hears a snapshot published from it
    void setShiftMs (float ms) { shiftValue = ms; publish(); }
    void setFlip (bool f) { flipValue = f; publish(); }
    void setGainDb (float db) { gainValue = db; publish(); }
    /** Phase EQ band 0 or 1: an all-pass at fc (0 = off) with this Q. */
    void setPhaseEq (int band, float fc, float q) { (band ? ap2Fc : ap1Fc) = fc; (band ? ap2Q : ap1Q) = q; publish(); }

    /** From `start` (seconds on the timeline) the shift is multiplied by `scale` and `gainDb` is added, until the next
        entry. The hits use it: a hit tuned by ratio r plays its mics' offsets 1/r as long, as the export does (delay,
        then varispeed), and each hit gets its Level gain without rebuilding playback. The phase EQ's frequencies are
        multiplied by `freqScale` (the hit's ratio r): the correction was measured on the recording, and varispeed moves
        what it corrects from fc to fc*r, so it has to move with it. */
    struct HitScale { double start = 0; double scale = 1; double gainDb = 0; double freqScale = 1; };
    void setHitScales (std::vector<HitScale>);   ///< message thread; sorted by start
    /** How many hit tables are still allocated (the published one and the one the audio thread holds: never more than 2). */
    int tablesHeld() const { const juce::SpinLock::ScopedLockType l (lock); return (int) tables.size(); }

    /** A hit's audio, held in memory, heard instead of what the track's clips give from timeline sample `start` for
        as long as it runs (it goes through everything this plugin does, like the clip's audio would). It's how a
        change is heard at once: the hit is worked out again and swapped in under a short crossfade, while playback
        carries on. One channel plays on both sides. */
    struct HitAudio { juce::int64 start = 0; std::shared_ptr<const std::vector<std::vector<float>>> audio; float gain = 1; };
    /** message thread; sorted by start. Entries may overlap (an audition phrase lets every hit ring over the next):
        they're summed. `rate`: the sample rate the starts and the audio are at
        (they're only heard while playback runs at that rate). An empty list goes back to the clips. */
    void setHitAudio (std::vector<HitAudio>, double rate);
    int hitAudioTablesHeld() const { const juce::SpinLock::ScopedLockType l (lock); return (int) audTables.size(); }

    juce::CachedValue<float> shiftValue, gainValue;
    juce::CachedValue<bool> flipValue;
    juce::CachedValue<float> ap1Fc, ap1Q, ap2Fc, ap2Q;
    te::AutomatableParameter::Ptr shiftParam, gainParam;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

private:
    // The audio thread never reads the CachedValues (plain memory the message thread writes) or frees anything.
    // The message thread publishes a whole Settings snapshot (shift in ms) and the current hit table under `lock`;
    // the audio thread takes them with a try-lock (never waits: if it misses, it keeps last block's) and records
    // which table it holds, so the message thread frees a table only once the audio thread has let go of it.
    struct Snapshot { AlignDsp::Settings s; double shiftMs = 0; };
    void publish();
    AlignDsp dsp;
    std::atomic<bool> jumpNext { true };   ///< after a reset or a seek: start on the right settings, don't fade into them
    mutable juce::SpinLock lock;
    Snapshot pubSnap, audioSnap;                                   ///< pub: under lock; audio: audio thread only
    const std::vector<HitScale>* pubTbl = nullptr;                 ///< under lock
    const std::vector<HitScale>* audioTbl = nullptr;               ///< changed only by the audio thread, under lock
    std::vector<std::unique_ptr<std::vector<HitScale>>> tables;    ///< owned by the message thread, freed under lock

    // the hit audio: the same hand-over as the hit table, with two held on the audio side while one fades into the other
    struct AudioTable { double rate = 0; juce::int64 longest = 0; std::vector<HitAudio> hits; };
    void applyHitAudio (juce::AudioBuffer<float>&, int start, int num, int nch, double t0);
    const AudioTable* pubAud = nullptr;                            ///< under lock
    const AudioTable* audCur = nullptr;                            ///< changed only by the audio thread, under lock
    const AudioTable* audPrev = nullptr;                           ///< (the one being faded out)
    std::vector<std::unique_ptr<AudioTable>> audTables;            ///< owned by the message thread, freed under lock
    int audFadeLeft = 0, audFadeLen = 1;                           ///< audio thread
    juce::int64 audExpected = -1;                                  ///< audio thread: where the next block should start

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignPlugin)
};

/** Makes the live plugins available to an engine (once, at start-up). */
void registerLivePlugins (te::Engine&);

/** Puts an Align plugin first on a track (or returns the one already there). */
AlignPlugin* alignOn (te::AudioTrack&);

} // namespace ld::live

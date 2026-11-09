#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "AlignDsp.h"

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
    void reset() override { dsp.reset(); }
    double getLatencySeconds() override { return AlignDsp::latencySamples (sampleRate) / sampleRate; }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }
    void applyToBuffer (const te::PluginRenderContext&) override;

    // the stored value is the truth (it's what undo and saving see); the parameter follows it at once
    void setShiftMs (float ms) { shiftValue = ms; }
    void setFlip (bool f) { flipValue = f; }
    void setGainDb (float db) { gainValue = db; }
    /** Phase EQ band 0 or 1: an all-pass at fc (0 = off) with this Q. */
    void setPhaseEq (int band, float fc, float q) { (band ? ap2Fc : ap1Fc) = fc; (band ? ap2Q : ap1Q) = q; }

    juce::CachedValue<float> shiftValue, gainValue;
    juce::CachedValue<bool> flipValue;
    juce::CachedValue<float> ap1Fc, ap1Q, ap2Fc, ap2Q;
    te::AutomatableParameter::Ptr shiftParam, gainParam;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

private:
    AlignDsp::Settings settingsNow() const;
    AlignDsp dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignPlugin)
};

/** Makes the live plugins available to an engine (once, at start-up). */
void registerLivePlugins (te::Engine&);

/** Puts an Align plugin first on a track (or returns the one already there). */
AlignPlugin* alignOn (te::AudioTrack&);

} // namespace ld::live

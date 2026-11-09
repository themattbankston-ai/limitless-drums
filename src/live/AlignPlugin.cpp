#include "AlignPlugin.h"

namespace ld::live
{

namespace
{
const juce::Identifier shiftId ("shiftMs"), gainId ("gainDb"), flipId ("flip");
}

const char* AlignPlugin::xmlTypeName = "ldAlign";

AlignPlugin::AlignPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto* um = getUndoManager();
    shiftValue.referTo (state, shiftId, um, 0.0f);
    gainValue.referTo (state, gainId, um, 0.0f);
    flipValue.referTo (state, flipId, um, false);

    shiftParam = addParam ("shift", "Shift", { -50.0f, 50.0f },
                           [] (float v) { return juce::String (v, 3) + " ms"; },
                           [] (const juce::String& s) { return s.getFloatValue(); });
    shiftParam->attachToCurrentValue (shiftValue);
    gainParam = addParam ("gain", "Gain", { -60.0f, 12.0f },
                          [] (float v) { return juce::String (v, 1) + " dB"; },
                          [] (const juce::String& s) { return s.getFloatValue(); });
    gainParam->attachToCurrentValue (gainValue);
}

AlignPlugin::~AlignPlugin()
{
    notifyListenersOfDeletion();
    shiftParam->detachFromCurrentValue();
    gainParam->detachFromCurrentValue();
}

void AlignPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    // an undo, redo or load changes the stored value directly: bring the parameter the audio reads back in line
    if (v == state && (id == shiftId || id == gainId))
    {
        // (this can run before the cached copy has heard of the change, so refresh it first)
        if (id == shiftId) { shiftValue.forceUpdateOfCachedValue(); shiftParam->updateFromAttachedValue(); }
        else { gainValue.forceUpdateOfCachedValue(); gainParam->updateFromAttachedValue(); }
    }
    te::Plugin::valueTreePropertyChanged (v, id);
}

AlignDsp::Settings AlignPlugin::settingsNow() const
{
    AlignDsp::Settings s;
    s.advanceSamples = (double) shiftParam->getCurrentValue() * sampleRate / 1000.0;
    s.gainDb = gainParam->getCurrentValue();
    s.flip = flipValue.get();
    return s;
}

void AlignPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    dsp.setTarget (settingsNow());
    dsp.prepare (sampleRate, 2);
}

void AlignPlugin::applyToBuffer (const te::PluginRenderContext& rc)
{
    if (rc.destBuffer == nullptr) return;
    SCOPED_REALTIME_CHECK
    dsp.setTarget (settingsNow());
    auto& b = *rc.destBuffer;
    float* ch[2] = { nullptr, nullptr };
    const int nch = std::min (2, b.getNumChannels());
    for (int c = 0; c < nch; ++c) ch[c] = b.getWritePointer (c, rc.bufferStartSample);
    dsp.process (ch, nch, rc.bufferNumSamples);
}

void registerLivePlugins (te::Engine& engine)
{
    engine.getPluginManager().createBuiltInType<AlignPlugin>();
}

AlignPlugin* alignOn (te::AudioTrack& track)
{
    if (auto* a = track.pluginList.findFirstPluginOfType<AlignPlugin>()) return a;
    auto p = track.edit.getPluginCache().createNewPlugin (AlignPlugin::xmlTypeName, {});
    track.pluginList.insertPlugin (p, 0, nullptr);
    return dynamic_cast<AlignPlugin*> (p.get());
}

} // namespace ld::live

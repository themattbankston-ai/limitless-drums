#include "AlignPlugin.h"

namespace ld::live
{

namespace
{
const juce::Identifier shiftId ("shiftMs"), gainId ("gainDb"), flipId ("flip"), ap1FcId ("ap1Fc"), ap1QId ("ap1Q"), ap2FcId ("ap2Fc"), ap2QId ("ap2Q");
}

const char* AlignPlugin::xmlTypeName = "ldAlign";

AlignPlugin::AlignPlugin (te::PluginCreationInfo info) : te::Plugin (info)
{
    auto* um = getUndoManager();
    shiftValue.referTo (state, shiftId, um, 0.0f);
    gainValue.referTo (state, gainId, um, 0.0f);
    flipValue.referTo (state, flipId, um, false);
    ap1Fc.referTo (state, ap1FcId, um, 0.0f);
    ap1Q.referTo (state, ap1QId, um, 0.707f);
    ap2Fc.referTo (state, ap2FcId, um, 0.0f);
    ap2Q.referTo (state, ap2QId, um, 0.707f);

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
    s.ap[0] = { ap1Fc.get(), ap1Q.get() };
    s.ap[1] = { ap2Fc.get(), ap2Q.get() };
    return s;
}

void AlignPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    dsp.setTarget (settingsNow());
    dsp.prepare (sampleRate, 2);
    jumpNext = true;
}

void AlignPlugin::applyToBuffer (const te::PluginRenderContext& rc)
{
    if (rc.destBuffer == nullptr) return;
    SCOPED_REALTIME_CHECK
    const auto base = settingsNow();
    auto& b = *rc.destBuffer;
    const int nch = std::min (2, b.getNumChannels());
    {
        // never waits: if the message thread is swapping the table this very moment, this block keeps the last one
        const juce::SpinLock::ScopedTryLockType l (tblLock);
        if (l.isLocked() && audioScales != scales) audioScales = scales;   // (the old one stays alive in `retired`)
    }
    const auto& tbl = audioScales;
    const bool jump = jumpNext.exchange (false);
    if (tbl == nullptr || tbl->empty())
    {
        dsp.setTarget (base);
        if (jump) dsp.reset();
        float* ch[2] = { nullptr, nullptr };
        for (int c = 0; c < nch; ++c) ch[c] = b.getWritePointer (c, rc.bufferStartSample);
        dsp.process (ch, nch, rc.bufferNumSamples);
        return;
    }
    // Tracktion hands the plugin its input L samples ahead of the time it reports (that's how it compensates the
    // latency), so this block comes out at editTime + L on the timeline. Switch at the exact sample (measured against
    // the reference export: -50..-60 dB nulls this way, -9..-18 dB without).
    const double sr = sampleRate, t0 = rc.editTime.getStart().inSeconds() + AlignDsp::latencySamples (sr) / sr;
    int done = 0;
    while (done < rc.bufferNumSamples)
    {
        const double t = t0 + done / sr + 0.5 / sr;
        auto it = std::upper_bound (tbl->begin(), tbl->end(), t, [] (double x, const HitScale& h) { return x < h.start; });
        const double scale = it == tbl->begin() ? 1.0 : std::prev (it)->scale, extra = it == tbl->begin() ? 0.0 : std::prev (it)->gainDb;
        int seg = rc.bufferNumSamples - done;
        if (it != tbl->end()) seg = std::clamp ((int) std::ceil ((it->start - t) * sr), 1, seg);
        auto s = base;
        s.advanceSamples *= scale;
        s.gainDb += extra;
        dsp.setTarget (s);
        if (jump && done == 0) dsp.reset();
        float* ch[2] = { nullptr, nullptr };
        for (int c = 0; c < nch; ++c) ch[c] = b.getWritePointer (c, rc.bufferStartSample + done);
        dsp.process (ch, nch, seg);
        done += seg;
    }
}

void AlignPlugin::setHitScales (std::vector<HitScale> v)
{
    auto p = std::make_shared<const std::vector<HitScale>> (std::move (v));
    {
        const juce::SpinLock::ScopedLockType l (tblLock);
        if (scales) retired.push_back (scales);
        scales = p;
    }
    // keep the last couple alive: the audio thread may still hold one for the block it's in
    while (retired.size() > 4) retired.erase (retired.begin());
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

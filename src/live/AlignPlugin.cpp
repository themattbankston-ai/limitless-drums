#include <limits>
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
    publish();
}

AlignPlugin::~AlignPlugin()
{
    notifyListenersOfDeletion();
    shiftParam->detachFromCurrentValue();
    gainParam->detachFromCurrentValue();
}

void AlignPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    // an undo, redo or load changes the stored value directly: refresh the cached copies (this can run before they
    // have heard of the change), keep the parameters in line, and publish the new snapshot
    if (v == state && (id == shiftId || id == gainId || id == flipId || id == ap1FcId || id == ap1QId || id == ap2FcId || id == ap2QId))
    {
        for (auto* cv : { &shiftValue, &gainValue, &ap1Fc, &ap1Q, &ap2Fc, &ap2Q }) cv->forceUpdateOfCachedValue();
        flipValue.forceUpdateOfCachedValue();
        if (id == shiftId) shiftParam->updateFromAttachedValue();
        if (id == gainId) gainParam->updateFromAttachedValue();
        publish();
    }
    te::Plugin::valueTreePropertyChanged (v, id);
}

void AlignPlugin::publish()
{
    Snapshot n;
    n.shiftMs = shiftValue.get();
    n.s.gainDb = gainValue.get();
    n.s.flip = flipValue.get();
    n.s.ap[0] = { ap1Fc.get(), ap1Q.get() };
    n.s.ap[1] = { ap2Fc.get(), ap2Q.get() };
    const juce::SpinLock::ScopedLockType l (lock);
    pubSnap = n;
}

void AlignPlugin::initialise (const te::PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    publish();
    {
        const juce::SpinLock::ScopedLockType l (lock);
        audioSnap = pubSnap;
        audioTbl = pubTbl;
        audCur = pubAud; audPrev = nullptr;
    }
    audFadeLeft = 0; audFadeLen = std::max (1, (int) std::lround (0.008 * sampleRate)); audExpected = -1;
    auto s = audioSnap.s;
    s.advanceSamples = audioSnap.shiftMs * sampleRate / 1000.0;
    dsp.setTarget (s);
    dsp.prepare (sampleRate, 2);
    jumpNext = true;
}

void AlignPlugin::applyToBuffer (const te::PluginRenderContext& rc)
{
    if (rc.destBuffer == nullptr) return;
    SCOPED_REALTIME_CHECK
    {
        // never waits: if the message thread is publishing this very moment, this block keeps the last snapshot
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked())
        {
            audioSnap = pubSnap; audioTbl = pubTbl;
            if (pubAud != audCur) { audPrev = audCur; audCur = pubAud; audFadeLeft = audFadeLen; }
            else if (audFadeLeft == 0) audPrev = nullptr;
        }
    }
    auto base = audioSnap.s;
    base.advanceSamples = audioSnap.shiftMs * sampleRate / 1000.0;
    auto& b = *rc.destBuffer;
    const int nch = std::min (2, b.getNumChannels());
    const auto* tbl = audioTbl;
    const bool jump = jumpNext.exchange (false);
    if (jump) { audFadeLeft = 0; audExpected = -1; }
    if (audCur != nullptr || (audPrev != nullptr && audFadeLeft > 0))
        applyHitAudio (b, rc.bufferStartSample, rc.bufferNumSamples, nch, rc.editTime.getStart().inSeconds() + AlignDsp::latencySamples (sampleRate) / sampleRate);
    else audFadeLeft = 0;
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
        const double scale = it == tbl->begin() ? 1.0 : std::prev (it)->scale, extra = it == tbl->begin() ? 0.0 : std::prev (it)->gainDb,
                     fscale = it == tbl->begin() ? 1.0 : std::prev (it)->freqScale;
        int seg = rc.bufferNumSamples - done;
        if (it != tbl->end()) seg = std::clamp ((int) std::ceil ((it->start - t) * sr), 1, seg);
        auto s = base;
        s.advanceSamples *= scale;
        s.gainDb += extra;
        for (auto& a : s.ap) if (a.fc > 0) a.fc *= fscale;
        dsp.setTarget (s);
        if (jump && done == 0) dsp.reset();
        float* ch[2] = { nullptr, nullptr };
        for (int c = 0; c < nch; ++c) ch[c] = b.getWritePointer (c, rc.bufferStartSample + done);
        dsp.process (ch, nch, seg);
        done += seg;
    }
}

void AlignPlugin::applyHitAudio (juce::AudioBuffer<float>& b, int start, int num, int nch, double t0)
{
    // the timeline sample this block's first input sample sits on (blocks that follow on keep counting, so a
    // rounding wobble in the reported time never repeats or skips a sample)
    juce::int64 p0 = (juce::int64) std::llround (t0 * sampleRate);
    if (audExpected >= 0 && std::abs (p0 - audExpected) <= 1) p0 = audExpected;
    audExpected = p0 + num;
    const auto usable = [this] (const AudioTable* t) { return t != nullptr && ! t->hits.empty() && std::abs (t->rate - sampleRate) < 1.0e-6 ? t : nullptr; };
    const AudioTable* cur = usable (audCur);
    const AudioTable* prev = audFadeLeft > 0 ? usable (audPrev) : nullptr;
    if (cur == nullptr && prev == nullptr) { audFadeLeft = std::max (0, audFadeLeft - num); return; }
    // the hit of a table that covers sample p (or none)
    const auto find = [] (const AudioTable* t, juce::int64 p) -> const HitAudio*
    {
        if (t == nullptr) return nullptr;
        auto it = std::upper_bound (t->hits.begin(), t->hits.end(), p, [] (juce::int64 x, const HitAudio& h) { return x < h.start; });
        if (it == t->hits.begin()) return nullptr;
        const auto& h = *std::prev (it);
        return h.audio && ! h.audio->empty() && p - h.start < (juce::int64) (*h.audio)[0].size() ? &h : nullptr;
    };
    const HitAudio* hc = find (cur, p0);
    const HitAudio* hp = find (prev, p0);
    juce::int64 nextLook = p0;   // look the hits up again where one could start or end
    for (int k = 0; k < num; ++k)
    {
        const juce::int64 p = p0 + k;
        if (p >= nextLook)
        {
            hc = find (cur, p); hp = find (prev, p);
            nextLook = std::numeric_limits<juce::int64>::max();
            for (auto* t : { cur, prev })
            {
                if (t == nullptr) continue;
                auto it = std::upper_bound (t->hits.begin(), t->hits.end(), p, [] (juce::int64 x, const HitAudio& h) { return x < h.start; });
                if (it != t->hits.end()) nextLook = std::min (nextLook, it->start);
            }
            if (hc) nextLook = std::min (nextLook, hc->start + (juce::int64) (*hc->audio)[0].size());
            if (hp) nextLook = std::min (nextLook, hp->start + (juce::int64) (*hp->audio)[0].size());
        }
        const bool fading = audFadeLeft > 0;
        if (! hc && ! (fading && hp)) { if (fading) --audFadeLeft; continue; }
        const float g = fading ? 1.0f - (float) audFadeLeft / (float) audFadeLen : 1.0f;
        for (int c = 0; c < nch; ++c)
        {
            float* d = b.getWritePointer (c, start);
            const float in = d[k];
            const auto at = [&] (const HitAudio* h) { const auto& a = *h->audio; return a[(size_t) std::min (c, (int) a.size() - 1)][(size_t) (p - h->start)]; };
            const float now = hc ? at (hc) : in;
            d[k] = fading ? (hp ? at (hp) : in) * (1.0f - g) + now * g : now;
        }
        if (fading) --audFadeLeft;
    }
}

void AlignPlugin::setHitAudio (std::vector<HitAudio> v, double rate)
{
    auto p = std::make_unique<AudioTable>();
    p->rate = rate; p->hits = std::move (v);
    std::vector<std::unique_ptr<AudioTable>> unused;
    {
        const juce::SpinLock::ScopedLockType l (lock);
        pubAud = p->hits.empty() ? nullptr : p.get();
        audTables.push_back (std::move (p));
        // (as the hit table: only what's published or held by the audio thread stays)
        for (auto& t : audTables)
            if (t.get() != pubAud && t.get() != audCur && t.get() != audPrev) unused.push_back (std::move (t));
        audTables.erase (std::remove (audTables.begin(), audTables.end(), nullptr), audTables.end());
    }
}

void AlignPlugin::setHitScales (std::vector<HitScale> v)
{
    auto p = std::make_unique<std::vector<HitScale>> (std::move (v));
    std::vector<std::unique_ptr<std::vector<HitScale>>> unused;
    {
        const juce::SpinLock::ScopedLockType l (lock);
        pubTbl = p.get();
        tables.push_back (std::move (p));
        // everything but the published table and the one the audio thread holds can go; the audio thread only
        // changes which one it holds under this lock, so neither can change while we sort them
        for (auto& t : tables)
            if (t.get() != pubTbl && t.get() != audioTbl) unused.push_back (std::move (t));
        tables.erase (std::remove (tables.begin(), tables.end(), nullptr), tables.end());
    }
    // (freed here, outside the lock, on the message thread)
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

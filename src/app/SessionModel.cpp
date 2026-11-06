#include "SessionModel.h"

namespace ld
{
namespace te = tracktion;
using K = MicRole::Kind;

static std::pair<unsigned, unsigned> roleColours (const MicRole& r)
{
    switch (r.kind)
    {
        case K::kickIn: case K::kickOut: case K::kickSub: case K::kick: return { 0xffffb23e, 0xffff6a3d };
        case K::snareTop: case K::snare:                                return { 0xff2ee6ff, 0xff4c8dff };
        case K::snareBottom:                                            return { 0xff4c8dff, 0xff2ee6ff };
        case K::hiHat: case K::ride: case K::cymbal:                    return { 0xffffd85a, 0xff7cff6b };
        case K::tom: case K::floorTom:                                  return { 0xffff3da8, 0xffff6a3d };
        case K::overheads:                                              return { 0xff4c8dff, 0xff8b5cff };
        case K::roomMono: case K::roomClose: case K::room:              return { 0xff8b5cff, 0xffff3da8 };
        case K::roomFar:                                                return { 0xff8b5cff, 0xff2ee6ff };
        case K::roomCrush:                                              return { 0xffff3da8, 0xffff7ad9 };
        case K::ambient:                                                return { 0xffff7ad9, 0xff8b5cff };
        default:                                                        return { 0xffb8c0cc, 0xff7d8794 };
    }
}

static bool isRoom (K k) { return k == K::roomMono || k == K::roomClose || k == K::room || k == K::roomFar || k == K::roomCrush || k == K::ambient; }

std::vector<juce::File> editFiles (te::Edit& edit)
{
    std::vector<juce::File> out;
    for (auto* t : te::getAudioTracks (edit))
    {
        juce::File f;
        if (auto* clip = dynamic_cast<te::AudioClipBase*> (t->getClips().getFirst())) f = clip->getOriginalFile();
        out.push_back (f);
    }
    return out;
}

ui::StudioModel buildStudioModel (te::Edit& edit, const std::vector<MicRole>& roles, const juce::String& title, const EngineState* es)
{
    ui::StudioModel m;
    m.hasSession = true;
    m.title = title.toStdString();
    m.bpm = edit.tempoSequence.getTempos()[0]->getBpm();

    const auto files = editFiles (edit);
    auto tracks = te::getAudioTracks (edit);
    const hl::Session* S = es ? &es->session : nullptr;
    // the engine keeps its own track order (lead first); match by file name
    std::vector<int> map (tracks.size(), -1);
    if (S)
        for (size_t i = 0; i < tracks.size(); ++i)
            for (size_t t = 0; t < S->tracks.size(); ++t)
                if (S->tracks[t].name == files[i].getFileNameWithoutExtension().toStdString()) map[i] = (int) t;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    double sr = S ? S->sr : 48000.0;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        auto* t = tracks[i];
        const MicRole role = i < roles.size() ? roles[i] : MicRole {};
        ui::TrackInfo info;
        info.name = t->getName().toStdString();
        info.isRoom = isRoom (role.kind);
        std::tie (info.colourA, info.colourB) = roleColours (role);
        info.solo = t->isSolo (false);
        info.mute = t->isMuted (false);
        if (auto* vp = t->getVolumePlugin()) { info.volumeDb = vp->getVolumeDb(); info.pan = vp->getPan(); }

        hl::F32 mono;
        if (map[i] >= 0)
        {
            // Hitline's own mic name ("Room Close", not the file's "RmCls.cm6"), and its colour from that name
            const auto& tr = S->tracks[(size_t) map[i]];
            mono = tr.mono; info.stereo = tr.ch.size() > 1;
            info.name = tr.mic;
            const auto r = classifyMic (juce::String (S->drum == "kit" ? std::string() : S->drum + " ") + juce::String (tr.mic));   // "Top" alone is ambiguous: "Snare Top"
            info.isRoom = isRoom (r.kind);
            std::tie (info.colourA, info.colourB) = roleColours (r);
        }
        else if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor (files[i]) })
        {
            sr = r->sampleRate;
            info.stereo = r->numChannels > 1;
            juce::AudioBuffer<float> raw ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&raw, 0, (int) r->lengthInSamples, 0, true, true);
            mono.assign ((size_t) raw.getNumSamples(), 0.0f);
            for (int ch = 0; ch < raw.getNumChannels(); ++ch)
                for (int n = 0; n < raw.getNumSamples(); ++n) mono[(size_t) n] += raw.getSample (ch, n) / (float) raw.getNumChannels();
        }
        const int block = std::max (1, (int) (sr / 750.0));
        info.blocksPerSecond = sr / block;
        const size_t nb = mono.size() / (size_t) block;
        info.peaks.resize (nb);
        const bool leveled = map[i] >= 0 && es->mix && es->levels && es->levels->valid;
        if (leveled)
        {
            // what gets exported (Hitline's B view): the processed mic, at its alignment, each hit at its Level gain
            const int t = map[i];
            const auto& L = *es->levels;
            const auto& proc = es->mix->proc[(size_t) t];
            const long lagInt = (long) std::lround (t == S->refIdx ? 0.0 : S->tracks[(size_t) t].lag);
            const double trim = std::pow (10.0, L.trimDb / 20);
            size_t k = 0;
            for (size_t b = 0; b < nb; ++b)
            {
                float pk = 0;
                for (size_t n = b * (size_t) block, e = n + (size_t) block; n < e; ++n)
                {
                    while (k + 1 < L.regions.size() && (long) n >= L.regions[k + 1].start) ++k;
                    const long src = (long) n + lagInt;
                    if (src < 0 || src >= (long) proc[0].size()) continue;
                    float a = 0;
                    for (auto& c : proc) a = std::max (a, std::abs (c[(size_t) src]));
                    pk = std::max (pk, a * (float) (std::pow (10.0, L.gainsDbT[(size_t) t][k] / 20) * trim));
                }
                info.peaks[b] = pk;
            }
        }
        else
            for (size_t b = 0; b < nb; ++b)
            {
                float pk = 0;
                for (size_t n = b * (size_t) block, e = n + (size_t) block; n < e; ++n) pk = std::max (pk, std::abs (mono[n]));
                info.peaks[b] = pk;
            }
        m.length = std::max (m.length, (double) mono.size() / sr);
        m.tracks.push_back (std::move (info));
    }
    if (! S || S->onsets.empty()) return m;

    for (size_t i = 0; i < map.size(); ++i) if (map[i] == S->refIdx) m.referenceTrack = (int) i;

    // the hits, each on its velocity layer and round robin (only meaningful when the count matches)
    const bool ok = hl::countsOk (*S);
    for (size_t k = 0; k < S->onsets.size(); ++k)
        m.hits.push_back ({ (double) S->onsets[k] / sr, ok ? S->velOfHit (k) : 0, ok ? (int) (k % (size_t) S->det.RR) : (int) k });

    if (ok)
    {
        const int V = S->det.V;
        const auto* L = es->levels && es->levels->valid ? &*es->levels : nullptr;
        for (int v = 0; v < V; ++v)
        {
            ui::LayerInfo li;
            std::vector<size_t> hs;
            for (size_t k = 0; k < S->onsets.size(); ++k) if (S->velOfHit (k) == v) hs.push_back (k);
            if (hs.empty()) continue;
            li.start = (double) S->onsets[hs.front()] / sr;
            const size_t after = hs.back() + 1;
            li.end = after < S->onsets.size() ? (double) S->onsets[after] / sr : m.length;
            if (li.end < li.start) std::swap (li.start, li.end);
            li.midiLow = (int) std::floor (v * 127.0 / V) + 1;
            li.midiHigh = (int) std::floor ((v + 1) * 127.0 / V);
            li.numHits = (int) hs.size();
            if (S->tuneInfo && (size_t) v < S->tuneInfo->layerMed.size() && std::isfinite (S->tuneInfo->layerMed[(size_t) v]))
                li.note = hl::noteName (std::round (hl::hzToMidi (S->tuneInfo->layerMed[(size_t) v], S->tune.a4)));
            for (size_t i = 0; i < map.size(); ++i)
            {
                double sum = 0;
                if (L && map[i] >= 0) for (auto k : hs) sum += L->lvlRaw[(size_t) map[i]][k] + L->gainsDbT[(size_t) map[i]][k];
                li.micDb.push_back (L && map[i] >= 0 ? (float) (sum / (double) hs.size()) : NAN);
            }
            m.layers.push_back (std::move (li));
        }
        // layers are listed from the first hit in the take
        std::sort (m.layers.begin(), m.layers.end(), [] (auto& a, auto& b) { return a.start < b.start; });
    }

    // where each mic sits after the lead, as measured
    m.arrivalMs.assign (map.size(), NAN);
    for (size_t i = 0; i < map.size(); ++i)
    {
        if (map[i] < 0) continue;
        const auto& tr = S->tracks[(size_t) map[i]];
        if (map[i] == S->refIdx) m.arrivalMs[i] = 0;
        else if (tr.autoResult) m.arrivalMs[i] = (float) ((std::isfinite (tr.autoResult->arrival) ? tr.autoResult->arrival : tr.autoResult->lag) / sr * 1000.0);
    }
    return m;
}
} // namespace ld

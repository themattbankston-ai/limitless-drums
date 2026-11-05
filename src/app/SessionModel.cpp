#include "SessionModel.h"
#include "core/Layers.h"

namespace ld
{
namespace te = tracktion;
using K = MicRole::Kind;

juce::String defaultMicModel (const MicRole& r)
{
    switch (r.kind)
    {
        case K::kickIn:      return "Beta 91A";
        case K::kickOut:     return "NS10";
        case K::snareTop:    return "SM57";
        case K::snareBottom: return "Beta 57A";
        case K::tom: case K::floorTom: return "MD 421";
        case K::overheads:   return "C414";
        case K::roomMono:    return "U47";
        case K::roomClose:   return "U67";
        case K::roomFar:     return "Coles 4038";
        case K::roomCrush:   return "Earthworks";
        default:             return {};
    }
}

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
static bool isClose (K k) { return (int) k <= (int) K::cymbal; }

ui::StudioModel buildStudioModel (te::Edit& edit, const std::vector<MicRole>& roles, const juce::String& title)
{
    ui::StudioModel m;
    m.hasSession = true;
    m.title = title.toStdString();
    m.bpm = edit.tempoSequence.getTempos()[0]->getBpm();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::vector<juce::AudioBuffer<float>> mono;
    double sr = 48000.0;
    auto tracks = te::getAudioTracks (edit);
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        auto* t = tracks[i];
        const MicRole role = i < roles.size() ? roles[i] : MicRole {};
        ui::TrackInfo info;
        info.name = t->getName().toStdString();
        info.model = defaultMicModel (role).toStdString();
        info.isRoom = isRoom (role.kind);
        info.isDrumIcon = false;
        std::tie (info.colourA, info.colourB) = roleColours (role);
        info.solo = t->isSolo (false);
        info.mute = t->isMuted (false);
        if (auto* vp = t->getVolumePlugin()) { info.volumeDb = vp->getVolumeDb(); info.pan = vp->getPan(); }

        juce::AudioBuffer<float> buf;
        if (auto* clip = dynamic_cast<te::AudioClipBase*> (t->getClips().getFirst()))
            if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor (clip->getOriginalFile()) })
            {
                sr = r->sampleRate;
                info.stereo = r->numChannels > 1;
                juce::AudioBuffer<float> raw ((int) r->numChannels, (int) r->lengthInSamples);
                r->read (&raw, 0, (int) r->lengthInSamples, 0, true, true);
                buf.setSize (1, raw.getNumSamples());
                buf.clear();
                for (int ch = 0; ch < raw.getNumChannels(); ++ch)
                    buf.addFrom (0, 0, raw, ch, 0, raw.getNumSamples(), 1.0f / (float) raw.getNumChannels());
                // the clip's start in the edit (normally 0)
                const int offset = (int) (clip->getPosition().getStart().inSeconds() * sr);
                if (offset > 0)
                {
                    juce::AudioBuffer<float> shifted (1, buf.getNumSamples() + offset);
                    shifted.clear();
                    shifted.copyFrom (0, offset, buf, 0, 0, buf.getNumSamples());
                    buf = std::move (shifted);
                }
            }

        const int block = std::max (1, (int) (sr / 750.0));
        info.blocksPerSecond = sr / block;
        const int nb = buf.getNumSamples() / block;
        info.peaks.resize ((size_t) std::max (0, nb));
        for (int b = 0; b < nb; ++b)
            info.peaks[(size_t) b] = buf.getMagnitude (0, b * block, block);
        m.length = std::max (m.length, buf.getNumSamples() / sr);
        m.tracks.push_back (std::move (info));
        mono.push_back (std::move (buf));
    }
    if (mono.empty()) return m;

    // Reference: the first close mic (tracks are already in kit order), else the first track.
    int ref = 0;
    for (size_t i = 0; i < roles.size() && i < mono.size(); ++i)
        if (isClose (roles[i].kind)) { ref = (int) i; break; }
    m.referenceTrack = ref;

    // Hits, velocity layers, round robins
    // (Hit::peakDb holds the 50 ms RMS here: close mics often clip, which flattens peaks.)
    std::vector<Hit> hits;
    for (auto t : detectOneShots (mono[(size_t) ref], sr))
        hits.push_back ({ t, rmsDbAt (mono[(size_t) ref], sr, t), 0, 0, estimatePitch (mono[(size_t) ref], sr, t) });
    int velocities = 0, roundRobins = 0;
    if (auto* clip = dynamic_cast<te::AudioClipBase*> (tracks[(size_t) ref]->getClips().getFirst()))
        parseLayerCounts (clip->getOriginalFile().getFileName(), velocities, roundRobins);
    auto layers = velocities > 0 ? groupLayersKnown (hits, m.length, velocities) : groupLayers (hits, m.length);
    for (auto& h : hits) m.hits.push_back ({ h.time, h.layer, h.roundRobin });
    for (auto& L : layers)
    {
        ui::LayerInfo li;
        li.start = L.start; li.end = L.end;
        li.midiLow = L.midiLow; li.midiHigh = L.midiHigh; li.numHits = L.numHits;
        li.note = noteName (L.pitchHz).toStdString();
        for (auto& b : mono)
        {
            double sum = 0.0;
            for (int i = L.firstHit; i < L.firstHit + L.numHits; ++i) sum += rmsDbAt (b, sr, hits[(size_t) i].time);
            li.micDb.push_back ((float) (sum / std::max (1, L.numHits)));
        }
        m.layers.push_back (std::move (li));
    }
    // A session with only a handful of hits is a song take, not one-shots: no layers.
    if (hits.size() < 4) { m.layers.clear(); }

    // Arrival of each mic relative to the reference: where the direct sound of the loudest hit
    // first reaches a fifth of its peak on that mic.
    m.arrivalMs.assign (mono.size(), NAN);
    if (! hits.empty())
    {
        const auto loudest = *std::max_element (hits.begin(), hits.end(), [] (auto& a, auto& b) { return a.peakDb < b.peakDb; });
        auto onsetNear = [&] (const juce::AudioBuffer<float>& b) -> double
        {
            const int a0 = std::max (0, (int) ((loudest.time - 0.005) * sr));
            const int a1 = std::min (b.getNumSamples(), (int) ((loudest.time + 0.12) * sr));
            if (a1 <= a0) return -1.0;
            const float peak = b.getMagnitude (0, a0, a1 - a0);
            if (peak <= 0.0f) return -1.0;
            const float* d = b.getReadPointer (0);
            for (int i = a0; i < a1; ++i)
                if (std::abs (d[i]) >= 0.2f * peak) return i / sr;
            return -1.0;
        };
        const double refOnset = onsetNear (mono[(size_t) ref]);
        for (size_t i = 0; i < mono.size(); ++i)
        {
            const double o = onsetNear (mono[i]);
            if (o >= 0 && refOnset >= 0) m.arrivalMs[i] = (float) ((o - refOnset) * 1000.0);
        }
    }
    return m;
}
} // namespace ld

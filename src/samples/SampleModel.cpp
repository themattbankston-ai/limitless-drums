#include "SampleModel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <thread>

namespace ld::samples
{

namespace
{
const unsigned palette[] = { 0xffe0a43c, 0xffe07a45, 0xff4f9be8, 0xff4fc28a, 0xff9a82e0, 0xffd07ad8, 0xff5cc8d8, 0xffd8c85c,
                             0xffe06a7a, 0xff7ab85a, 0xff6a8ae0, 0xffc89a6a };

// "ROOM_CLOSE" -> "Room Close", "OH" stays "OH"
std::string niceName (std::string s)
{
    for (auto& ch : s) if (ch == '_' || ch == '-') ch = ' ';
    std::string out; bool start = true;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char ch = s[i];
        const size_t wl = s.find (' ', i) == std::string::npos ? s.size() - i : s.find (' ', i) - i;
        if (start && wl <= 2) { out += ch; start = ch == ' '; continue; }
        out += start ? (char) std::toupper ((unsigned char) ch) : (char) std::tolower ((unsigned char) ch);
        start = ch == ' ';
    }
    return out;
}

bool writeFloatWav (const juce::File& f, const hl::Chs& ch, double sr)
{
    f.deleteFile();
    if (ch.empty()) return false;
    juce::AudioBuffer<float> b ((int) ch.size(), (int) ch[0].size());
    for (size_t c = 0; c < ch.size(); ++c) b.copyFrom ((int) c, 0, ch[c].data(), (int) ch[c].size());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) ch.size(), 32, {}, 0));
    return w != nullptr && w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
} // namespace

SampleModel::SampleModel (te::Engine& e) : engine (e)
{
    live::registerLivePlugins (engine);
}

void SampleModel::dropMeters()
{
    for (auto& [t, m] : meters) if (m.first && m.second) m.first->removeClient (*m.second);
    meters.clear();
}

SampleModel::~SampleModel()
{
    *alive = false;
    dropMeters();
    if (edit) edit->getTransport().stop (false, false);
    edit.reset();
    if (cacheDir.isDirectory()) cacheDir.deleteRecursively();
}

te::AudioTrack* SampleModel::track (int t) const
{
    if (! edit) return nullptr;
    auto ts = te::getAudioTracks (*edit);
    return t >= 0 && t < ts.size() ? ts[t] : nullptr;
}

juce::String SampleModel::open (const juce::Array<juce::File>& items)
{
    std::vector<juce::File> files;
    juce::String folder;
    for (auto& f : items)
    {
        if (f.isDirectory())
        {
            folder = f.getFileName();
            for (auto& c : f.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.WAV;*.AIF;*.AIFF"))
                files.push_back (c);
        }
        else
        {
            files.push_back (f);
            if (folder.isEmpty()) folder = f.getParentDirectory().getFileName();
        }
    }
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    if (files.empty()) return "No WAV or AIFF files there.";

    auto st = openTake (files, folder);
    if (st->session.tracks.empty())
        return st->errors.empty() ? juce::String ("Those files couldn't be read.") : juce::String (st->errors.front());

    dropMeters();
    if (edit) edit->getTransport().stop (false, false);
    edit.reset();
    if (cacheDir.isDirectory()) cacheDir.deleteRecursively();
    state = st;
    name = folder;
    auto& S = state->session;

    // each track's audio as one file (a stereo pair becomes one stereo file), for Tracktion to play
    cacheDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LimitlessDrums")
                   .getChildFile (juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    cacheDir.createDirectory();
    edit = te::Edit::createSingleTrackEdit (engine, te::Edit::EditRole::forEditing);
    edit->ensureNumberOfAudioTracks ((int) S.tracks.size());
    auto ts = te::getAudioTracks (*edit);
    info.clear();
    for (size_t t = 0; t < S.tracks.size(); ++t)
    {
        const auto& tr = S.tracks[t];
        auto file = cacheDir.getChildFile (juce::String (t) + ".wav");
        if (! writeFloatWav (file, tr.ch, S.sr)) return "Couldn't prepare " + juce::String (tr.name) + " for playback.";
        auto* at = ts[(int) t];
        at->setName (tr.name);
        te::AudioFile af (engine, file);
        te::ClipPosition pos { te::TimeRange (te::TimePosition(), te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() };
        if (auto clip = at->insertWaveClip (tr.name, file, pos, false)) clip->setAutoCrossfade (false);
        live::alignOn (*at);
        TrackInfo ti;
        ti.name = niceName (tr.mic.empty() ? tr.name : tr.mic);
        ti.colour = palette[t % (sizeof (palette) / sizeof (palette[0]))];
        ti.channels = (int) tr.ch.size();
        ti.wave = std::make_shared<ui::Wave>();
        ti.wave->a = tr.ch.share();
        ti.wave->sampleRate = S.sr;
        ti.wave->build();
        info.push_back (std::move (ti));
    }
    edit->getUndoManager().clearUndoHistory();
    buildHits();
    return {};
}

int SampleModel::velocities() const { return state ? std::max (1, state->session.det.V) : 1; }
int SampleModel::roundRobins() const { return state ? std::max (1, state->session.det.RR) : 1; }
double SampleModel::length() const { return state ? (double) state->session.maxLen() / state->session.sr : 0; }

void SampleModel::buildHits()
{
    hitList.clear();
    if (! state) return;
    const auto& S = state->session;
    const auto& on = S.onsets;
    std::vector<int> seenPerVel ((size_t) velocities(), 0);
    for (size_t i = 0; i < on.size(); ++i)
    {
        HitInfo h;
        h.time = (double) on[i] / S.sr;
        h.end = i + 1 < on.size() ? (double) on[i + 1] / S.sr : length();
        h.vel = std::clamp (S.velOfHit (i), 0, velocities() - 1);
        h.rr = seenPerVel[(size_t) h.vel]++;
        if (S.refIdx >= 0 && S.refIdx < (int) S.tracks.size())
        {
            const auto& x = S.tracks[(size_t) S.refIdx].ch[0];
            float pk = 0;
            for (long n = on[i]; n < std::min ((long) x.size(), on[i] + (long) (0.05 * S.sr)); ++n) pk = std::max (pk, std::abs (x[(size_t) n]));
            h.peakDb = pk > 0 ? 20 * std::log10 (pk) : -100;
        }
        hitList.push_back (h);
    }
}

void SampleModel::autoPrep (std::function<void (juce::String)> done)
{
    if (! state || prepping) return;
    prepping = true;
    auto copy = std::make_shared<EngineState> (*state);
    auto alivePtr = alive;
    std::thread ([this, copy, alivePtr, done]
    {
        fprintf (stderr, "[prep] start\n");
        copy->prepared = hl::autoPrep (copy->session);
        fprintf (stderr, "[prep] done, prepared=%d\n", (int) copy->prepared);
        if (! copy->prepared) copy->advice = hl::hitCountAdvice (copy->session);
        juce::MessageManager::callAsync ([this, copy, alivePtr, done]
        {
            fprintf (stderr, "[prep] back on the message thread\n");
            if (! *alivePtr) return;
            prepping = false;
            state = copy;
            buildHits();
            applyAlignment();
            if (done) done (copy->prepared ? juce::String() : juce::String (copy->advice.empty() ? "Not every hit was found." : copy->advice));
        });
    }).detach();
}

void SampleModel::applyAlignment()
{
    if (! state || ! edit) return;
    const auto& S = state->session;
    for (size_t t = 0; t < S.tracks.size(); ++t)
        if (auto* at = track ((int) t))
            if (auto* a = live::alignOn (*at))
            {
                const auto& tr = S.tracks[t];
                a->setShiftMs ((float) (tr.lag / S.sr * 1000.0));   // lag: later than the lead, so play it that much earlier
                if (std::getenv ("LD_FRAMELOG")) fprintf (stderr, "[align] %s: lag %.2f smp (%.3f ms), pol %d, %d all-pass\n", tr.name.c_str(), tr.lag, tr.lag / S.sr * 1000.0, tr.pol, (int) tr.ap.size());
                a->setFlip (tr.pol < 0);
                for (int b = 0; b < 2; ++b)
                    a->setPhaseEq (b, b < (int) tr.ap.size() ? (float) tr.ap[(size_t) b].fc : 0.0f, b < (int) tr.ap.size() ? (float) tr.ap[(size_t) b].Q : 0.707f);
            }
}

float SampleModel::monDb (int t) const { auto* at = track (t); return at && at->getVolumePlugin() ? at->getVolumePlugin()->getVolumeDb() : 0.0f; }
void SampleModel::setMonDb (int t, float db) { if (auto* at = track (t)) if (auto* v = at->getVolumePlugin()) v->setVolumeDb (db); }
float SampleModel::pan (int t) const { auto* at = track (t); return at && at->getVolumePlugin() ? at->getVolumePlugin()->getPan() : 0.0f; }
void SampleModel::setPan (int t, float p) { if (auto* at = track (t)) if (auto* v = at->getVolumePlugin()) v->setPan (p); }
bool SampleModel::solo (int t) const { auto* at = track (t); return at && at->isSolo (false); }
void SampleModel::setSolo (int t, bool b) { if (auto* at = track (t)) at->setSolo (b); }
bool SampleModel::mute (int t) const { auto* at = track (t); return at && at->isMuted (false); }
void SampleModel::setMute (int t, bool b) { if (auto* at = track (t)) at->setMute (b); }

std::pair<float, float> SampleModel::meter (int t)
{
    auto* at = track (t);
    if (! at) return { -100.0f, -100.0f };
    auto* lm = at->getLevelMeterPlugin();
    if (! lm) return { -100.0f, -100.0f };
    auto& m = meters[t];
    if (m.first != &lm->measurer)
    {
        if (m.first && m.second) m.first->removeClient (*m.second);
        m.first = &lm->measurer;
        m.second = std::make_unique<te::LevelMeasurer::Client>();
        lm->measurer.addClient (*m.second);
    }
    const float l = m.second->getAndClearAudioLevel (0).dB, r = m.second->getAndClearAudioLevel (1).dB;
    return { l, r };
}

bool SampleModel::flip (int t) const { auto* at = track (t); auto* a = at ? at->pluginList.findFirstPluginOfType<live::AlignPlugin>() : nullptr; return a && a->flipValue.get(); }
void SampleModel::setFlip (int t, bool f) { if (auto* at = track (t)) if (auto* a = live::alignOn (*at)) a->setFlip (f); }
float SampleModel::shiftMs (int t) const { auto* at = track (t); auto* a = at ? at->pluginList.findFirstPluginOfType<live::AlignPlugin>() : nullptr; return a ? a->shiftValue.get() : 0.0f; }

void SampleModel::setProcessed (bool b)
{
    processedOn = b;
    if (! edit) return;
    for (auto* at : te::getAudioTracks (*edit))
        if (auto* a = at->pluginList.findFirstPluginOfType<live::AlignPlugin>())
            a->setEnabled (b);
}

void SampleModel::play() { if (edit) { edit->getTransport().ensureContextAllocated(); edit->getTransport().play (false); } }
void SampleModel::stop() { if (edit) edit->getTransport().stop (false, false); }
bool SampleModel::isPlaying() const { return edit && edit->getTransport().isPlaying(); }
double SampleModel::position() const { return edit ? edit->getTransport().getPosition().inSeconds() : 0.0; }
void SampleModel::setPosition (double s) { if (edit) edit->getTransport().setPosition (te::TimePosition::fromSeconds (std::max (0.0, s))); }

void SampleModel::setLoop (double a, double b, bool on)
{
    if (! edit) return;
    auto& tr = edit->getTransport();
    if (b > a) tr.setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (a), te::TimePosition::fromSeconds (b)));
    tr.looping = on;
}

bool SampleModel::looping() const { return edit && edit->getTransport().looping.get(); }

} // namespace ld::samples

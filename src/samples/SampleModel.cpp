#include <chrono>
#include "SampleModel.h"
#include "hl/Clean.h"
#include "hl/Tune.h"
#include "hl/Detect.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <thread>
#if defined (__linux__)
 #include <malloc.h>
#endif

namespace ld::samples
{

// the renders work on other threads; on Linux their freed working copies otherwise stay in those threads' heaps
static void releaseFreedMemory()
{
   #if defined (__linux__)
    malloc_trim (0);
   #endif
}


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
    {
        // let the background work finish (it only computes on its own copies; nothing lands here once alive is false)
        std::unique_lock<std::mutex> l (workers->m);
        workers->cv.wait_for (l, std::chrono::seconds (60), [this] { return workers->n == 0; });
    }
    dropMeters();
    if (edit) edit->getTransport().stop (false, false);
    edit.reset();
    if (cacheDir.isDirectory()) cacheDir.deleteRecursively();
}

te::AudioTrack* SampleModel::track (int t) const
{
    if (! edit || ! state) return nullptr;
    auto ts = te::getAudioTracks (*edit);
    return t >= 0 && t < (int) state->session.tracks.size() && t < ts.size() ? ts[t] : nullptr;
}

te::AudioTrack* SampleModel::trackA (int t) const
{
    if (! edit || ! state) return nullptr;
    auto ts = te::getAudioTracks (*edit);
    const int T = (int) state->session.tracks.size();
    return t >= 0 && t < T && T + t < ts.size() ? ts[T + t] : nullptr;
}

void SampleModel::applyMutes()
{
    if (! state) return;
    for (int t = 0; t < (int) state->session.tracks.size(); ++t)
    {
        const bool m = t < (int) userMute.size() && userMute[(size_t) t];
        if (auto* b = track (t)) b->setMute (m || ! processedOn || refAud);
        if (auto* a = trackA (t)) a->setMute (m || processedOn || refAud);
    }
    if (auto* r = refTrack()) { r->setMute (! refAud); r->setSolo (refAud); }   // soloed while it plays: a soloed mic doesn't silence it
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
    st->session.roomPhase = roomPhasePref;

    dropMeters();
    if (edit) edit->getTransport().stop (false, false);
    edit.reset();
    if (cacheDir.isDirectory()) cacheDir.deleteRecursively();
    cancelPreview(); hitAudio.clear(); clipsStale = false; previewOk = false; listening = nullptr;
    state = st;
    name = folder;
    refAudio.clear(); refSource = refPlayFile = juce::File(); refName.clear(); refHits = refEngine = 0; refPrint.clear(); refAud = false; refCancelled.clear();
    takeFiles = files;
    projFile = juce::File();
    recall = juce::var();
    recallMsg.clear();
    edits = savedEdits = 0;
    sourceFiles.clear();
    ++takeGen;
    prepping = false;     // an Auto Prep for the previous take is dropped when it lands
    undoStack.clear();
    redoStack.clear();
    scoreCache.clear();
    auto& S = state->session;

    // each track's audio as one file (a stereo pair becomes one stereo file), for Tracktion to play
    cacheDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LimitlessDrums")
                   .getChildFile (juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    cacheDir.createDirectory();
    // a run that was killed or crashed never cleaned up after itself (about 1 GB a take): clear anything over a day old
    for (auto& d : cacheDir.getParentDirectory().findChildFiles (juce::File::findDirectories, false))
        if (d != cacheDir && (juce::Time::getCurrentTime() - d.getLastModificationTime()).inHours() >= 24.0) d.deleteRecursively();
    edit = te::Edit::createSingleTrackEdit (engine, te::Edit::EditRole::forEditing);
    // tracks 0..T-1 play B (the hits, processed); T..2T-1 play A (as recorded). A/B mutes one set, which is instant.
    const int T = (int) S.tracks.size();
    edit->ensureNumberOfAudioTracks (2 * T + 1);   // and one for the reference, silent unless you're listening to it
    auto ts = te::getAudioTracks (*edit);
    info.clear();
    for (size_t t = 0; t < S.tracks.size(); ++t)
    {
        const auto& tr = S.tracks[t];
        auto file = cacheDir.getChildFile (juce::String (t) + ".wav");
        if (! writeFloatWav (file, tr.ch, S.sr)) return "Couldn't prepare " + juce::String (tr.name) + " for playback.";
        sourceFiles.push_back (file);
        auto* at = ts[(int) t];
        at->setName (tr.name);
        te::AudioFile af (engine, file);
        te::ClipPosition pos { te::TimeRange (te::TimePosition::fromSeconds (kPad), te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() };
        if (auto clip = at->insertWaveClip (tr.name, file, pos, false)) clip->setAutoCrossfade (false);
        live::alignOn (*at);
        auto* aTrack = ts[T + (int) t];
        aTrack->setName (tr.name + " (A)");
        if (auto clip = aTrack->insertWaveClip (tr.name, file, pos, false)) clip->setAutoCrossfade (false);
        TrackInfo ti;
        ti.name = niceName (tr.mic.empty() ? tr.name : tr.mic);
        ti.colour = palette[t % (sizeof (palette) / sizeof (palette[0]))];
        ti.channels = (int) tr.ch.size();
        {
            float pk = 0;
            for (auto& x : tr.ch.get()) for (auto v : x) pk = std::max (pk, std::abs (v));
            ti.peakDb = pk > 0 ? 20 * std::log10 (pk) : -100.0f;
        }
        ti.wave = std::make_shared<ui::Wave>();
        ti.wave->a = tr.ch.share();
        ti.wave->sampleRate = S.sr;
        ti.wave->build();
        info.push_back (std::move (ti));
    }
    edit->getUndoManager().clearUndoHistory();
    userMute.assign ((size_t) T, false);
    processedOn = true;
    applyMutes();
    setPosition (0);
    buildHits();
    return {};
}

int SampleModel::velocities() const { return state ? std::max (1, state->session.det.V) : 1; }
int SampleModel::roundRobins() const { return state ? std::max (1, state->session.det.RR) : 1; }
double SampleModel::length() const { return state ? (double) state->session.maxLen() / state->session.sr : 0; }

void SampleModel::syncNames()
{
    if (! state) return;
    for (size_t t = 0; t < std::min (info.size(), state->session.tracks.size()); ++t)
    {
        const auto& tr = state->session.tracks[t];
        info[t].name = ! tr.rename.empty() ? tr.rename : niceName (tr.mic.empty() ? tr.name : tr.mic);
    }
}

void SampleModel::buildHits()
{
    hitList.clear();
    if (! state) return;
    syncNames();
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
    const int gen = takeGen;
    std::thread ([this, copy, alivePtr, done, gen, running = Workers::start (workers)]
    {
        fprintf (stderr, "[prep] start\n");
        copy->prepared = hl::autoPrep (copy->session);
        fprintf (stderr, "[prep] done, prepared=%d\n", (int) copy->prepared);
        if (! copy->prepared) copy->advice = hl::hitCountAdvice (copy->session);
        else renderAndMeasure (*copy);
        juce::MessageManager::callAsync ([this, copy, alivePtr, done, gen]
        {
            fprintf (stderr, "[prep] back on the message thread\n");
            if (! *alivePtr) return;
            if (gen != takeGen) { releaseFreedMemory(); return; }   // another take was opened meanwhile
            prepping = false;
            cancelPreview(); clearHitAudio(); clipsStale = false; previewOk = false;
            state = copy;
            undoStack.clear();
            redoStack.clear();
            scoreCache.clear();
            releaseFreedMemory();
            buildHits();
            applyAlignment();
            layoutHits();
            if (copy->prepared) finishRecall();
            warmPreview();
            if (done) done (copy->prepared ? juce::String() : juce::String (copy->advice.empty() ? "Not every hit was found." : copy->advice));
        });
    }).detach();
}

juce::String SampleModel::autoPrepNow()
{
    if (! state || prepping) return "Nothing open.";
    auto copy = std::make_shared<EngineState> (*state);
    copy->prepared = hl::autoPrep (copy->session);
    if (! copy->prepared) copy->advice = hl::hitCountAdvice (copy->session);
    else renderAndMeasure (*copy);
    cancelPreview(); clearHitAudio(); clipsStale = false; previewOk = false;
    state = copy;
    buildHits();
    applyAlignment();
    layoutHits();
    if (copy->prepared) finishRecall();
    return copy->prepared ? juce::String() : juce::String (copy->advice.empty() ? "Not every hit was found." : copy->advice);
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
                const bool apOn = tr.apOn && ! tr.ap.empty();
                for (int b = 0; b < 2; ++b)
                    a->setPhaseEq (b, apOn && b < (int) tr.ap.size() ? (float) tr.ap[(size_t) b].fc : 0.0f, apOn && b < (int) tr.ap.size() ? (float) tr.ap[(size_t) b].Q : 0.707f);
            }
}

bool SampleModel::change (const std::function<void (hl::Session&)>& f, Redo what, const std::string& gesture)
{
    if (! state || prepping) return false;
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (gesture.empty() || gesture != lastGesture || now - lastGestureTime > 2.0)
    {
        undoStack.push_back ({ state->session, state->prepared });
        if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
    }
    lastGesture = gesture;
    lastGestureTime = now;
    redoStack.clear();
    ++edits;
    // the levels are measured on each mic with its phase EQ in: when a change moves a band, the render they read is
    // made again, so the same settings always give the same gains however they were reached
    auto bands = [] (const hl::Session& S)
    {
        std::vector<double> v;
        for (auto& tr : S.tracks) { v.push_back (tr.apOn ? 1 : 0); v.push_back ((double) tr.ap.size()); for (auto& b : tr.ap) { v.push_back (b.fc); v.push_back (b.Q); } }
        return v;
    };
    const auto bandsWere = bands (state->session);
    f (state->session);
    if (what == Redo::none) { syncNames(); return true; }
    if (bands (state->session) != bandsWere) wantMix = true;
    if (refAud && ! state->session.tone.hasRef) setReferenceAudition (false);
    if (what == Redo::reprep)
    {
        state->prepared = false;   // export waits for Auto Prep
        buildHits();
        return true;
    }
    if (what == Redo::align) applyAlignment();
    if (what == Redo::tune) { hl::computeTune (state->session); tuneLayoutWanted = true; }   // the tuned hits render in the background
    if (what == Redo::mix) wantMix = true;
    const auto q0 = juce::Time::getMillisecondCounterHiRes();
    buildHits();
    const auto q1 = juce::Time::getMillisecondCounterHiRes();
    if (! state->prepared) return true;
    if (what == Redo::mix || what == Redo::tune || wantMix) { ++audioGen; startPreview(); }
    const auto q2 = juce::Time::getMillisecondCounterHiRes();
    ++renderWanted;
    lastChangeMs = juce::Time::getMillisecondCounterHiRes();
    renderSoon();
    if (std::getenv ("LD_TIMING")) fprintf (stderr, "[timing] change(): before %.1f, buildHits %.1f, startPreview %.1f, startRender %.1f ms\n", q0 - now * 1000.0, q1 - q0, q2 - q1, juce::Time::getMillisecondCounterHiRes() - q2);
    return true;
}

void SampleModel::reapply (bool mixToo)
{
    applyAlignment();
    if (state->session.tuneAnalyzed) hl::computeTune (state->session);
    buildHits();
    ++audioGen;
    // playing, with the hits previewed: they're swapped in one by one and the clips wait for playback to stop
    if ((isPlaying() || holdClipsForTests) && state->prepared && state->mix && hl::HitPreviewer::possible (state->session, *state->mix)) { clipsStale = true; if (! holdClipsForTests) watchClips(); }
    else layoutHits (false);   // tuned hits not rendered yet keep playing as they were until the render lands
    tuneLayoutWanted = true;
    if (! state->prepared) return;
    if (mixToo) wantMix = true;
    startPreview();
    ++renderWanted;
    startRender();
}

void SampleModel::undo()
{
    if (! state || prepping || undoStack.empty()) return;
    redoStack.push_back ({ state->session, state->prepared });
    state->session = undoStack.back().session;
    state->prepared = undoStack.back().prepared;
    undoStack.pop_back();
    lastGesture.clear();
    ++edits;
    reapply (true);
}

void SampleModel::redo()
{
    if (! state || prepping || redoStack.empty()) return;
    undoStack.push_back ({ state->session, state->prepared });
    state->session = redoStack.back().session;
    state->prepared = redoStack.back().prepared;
    redoStack.pop_back();
    lastGesture.clear();
    ++edits;
    reapply (true);
}

juce::String SampleModel::exportBlocker() const
{
    if (! state || ! edit) return "Nothing open.";
    if (prepping) return "Auto Prep is still running.";
    const auto& S = state->session;
    if (! state->prepared) return "Run Auto Prep: the hits or settings changed since it last ran.";
    if ((int) S.onsets.size() != S.det.V * S.det.RR)
        return "Expecting " + juce::String (S.det.V * S.det.RR) + " hits but " + juce::String ((int) S.onsets.size()) + " were found: run Auto Prep.";
    if (! state->levels || ! state->levels->valid) return "The levels aren't measured: run Auto Prep.";
    if (isBusy()) return "Still rendering the last change.";
    return {};
}


// ---------------------------------------------------------------------------- projects
namespace
{
juce::var apVar (const std::vector<hl::AllPass>& ap)
{
    juce::Array<juce::var> a;
    for (auto& b : ap) { auto* o = new juce::DynamicObject(); o->setProperty ("fc", b.fc); o->setProperty ("Q", b.Q); a.add (juce::var (o)); }
    return a;
}
std::vector<hl::AllPass> apOf (const juce::var& v)
{
    std::vector<hl::AllPass> out;
    if (auto* a = v.getArray()) for (auto& b : *a) out.push_back ({ (double) b["fc"], (double) b["Q"] });
    return out;
}
std::vector<juce::File> projectFiles (const juce::var& p, const juce::File& projectFile)
{
    std::vector<juce::File> out;
    const auto* rel = p["relFiles"].getArray(); const auto* abs = p["files"].getArray();
    const int n = abs ? abs->size() : 0;
    for (int i = 0; i < n; ++i)
    {
        // beside the project first (the folder may have moved), then where it was when saved
        juce::File f;
        if (rel && i < rel->size()) f = projectFile.getParentDirectory().getChildFile ((*rel)[i].toString());
        if (! f.existsAsFile()) f = juce::File ((*abs)[i].toString());
        out.push_back (f);
    }
    return out;
}
} // namespace

juce::File SampleModel::defaultProjectFile() const
{
    if (takeFiles.empty()) return {};
    return takeFiles.front().getParentDirectory().getChildFile (juce::File::createLegalFileName (name.isEmpty() ? juce::String ("Take") : name) + ".ldproj");
}

juce::File SampleModel::projectFor (const juce::Array<juce::File>& items)
{
    std::set<juce::String> want; juce::File dir;
    for (auto& f : items)
    {
        if (f.isDirectory())
        {
            dir = f;
            for (auto& c : f.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.WAV;*.AIF;*.AIFF")) want.insert (c.getFullPathName());
        }
        else { want.insert (f.getFullPathName()); if (dir == juce::File()) dir = f.getParentDirectory(); }
    }
    juce::File best;
    if (! dir.isDirectory() || want.empty()) return best;
    for (auto& pf : dir.findChildFiles (juce::File::findFiles, false, "*.ldproj"))
    {
        const auto p = juce::JSON::parse (pf);
        if (! p.isObject() || ! p.hasProperty ("limitlessDrumsProject")) continue;
        std::set<juce::String> has;
        for (auto& f : projectFiles (p, pf)) has.insert (f.getFullPathName());
        if (has == want && (best == juce::File() || pf.getLastModificationTime() > best.getLastModificationTime())) best = pf;
    }
    return best;
}

juce::var SampleModel::projectVar (const juce::File& beside) const
{
    const auto& S = state->session;
    auto* p = new juce::DynamicObject();
    p->setProperty ("limitlessDrumsProject", kProjectVersion);
    p->setProperty ("take", name);
    p->setProperty ("prepared", state->prepared);
    juce::Array<juce::var> abs, rel;
    for (auto& f : takeFiles) { abs.add (f.getFullPathName()); rel.add (f.getRelativePathFrom (beside.getParentDirectory())); }
    p->setProperty ("files", abs); p->setProperty ("relFiles", rel);
    p->setProperty ("settings", hl::settingsToVar (S));
    auto* s = new juce::DynamicObject();
    s->setProperty ("sr", S.sr);
    s->setProperty ("drum", juce::String (S.drum)); s->setProperty ("artic", juce::String (S.artic));
    s->setProperty ("V", S.det.V); s->setProperty ("RR", S.det.RR);
    s->setProperty ("lead", S.refIdx); s->setProperty ("timing", juce::String (S.timing)); s->setProperty ("roomPhase", S.roomPhase);
    s->setProperty ("tuneOn", S.tune.on); s->setProperty ("tuneMode", juce::String (S.tune.mode)); s->setProperty ("tuneTarget", juce::String (S.tune.target));
    s->setProperty ("tuneCents", S.tune.cents); s->setProperty ("tuneA4", S.tune.a4);
    s->setProperty ("tuneAnyway", S.tuneAnyway); s->setProperty ("keepAttack", S.keepAttack); s->setProperty ("tuneMic", juce::String (S.tuneMic));
    juce::Array<juce::var> on, ratios;
    for (long o : S.onsets) on.add ((juce::int64) o);
    for (size_t i = 0; i < S.onsets.size(); ++i) ratios.add (hitRatio (i));
    s->setProperty ("onsets", on); s->setProperty ("ratios", ratios);
    p->setProperty ("session", juce::var (s));
    juce::Array<juce::var> tracks;
    for (int t = 0; t < (int) S.tracks.size(); ++t)
    {
        const auto& tr = S.tracks[(size_t) t];
        auto* o = new juce::DynamicObject();
        o->setProperty ("mic", juce::String (tr.mic)); o->setProperty ("rename", juce::String (tr.rename)); o->setProperty ("roleOverride", juce::String (tr.roleOverride));
        o->setProperty ("alignTo", juce::String (tr.alignTo)); o->setProperty ("lag", tr.lag); o->setProperty ("pol", tr.pol);
        o->setProperty ("apOn", tr.apOn); o->setProperty ("ap", apVar (tr.ap)); o->setProperty ("userAp", apVar (tr.userAp));
        o->setProperty ("manualLag", tr.manualLag); o->setProperty ("manualPol", tr.manualPol); o->setProperty ("manualAp", tr.manualAp);
        o->setProperty ("monDb", monDb (t)); o->setProperty ("pan", pan (t)); o->setProperty ("mute", mute (t)); o->setProperty ("solo", solo (t));
        tracks.add (juce::var (o));
    }
    p->setProperty ("tracks", tracks);
    p->setProperty ("levelMatch", levelMatchOn);
    if (S.tone.hasRef)
    {
        auto* r = new juce::DynamicObject();
        r->setProperty ("file", refSource.getFullPathName()); r->setProperty ("name", refName); r->setProperty ("hits", refHits);
        r->setProperty ("engine", refEngine); r->setProperty ("measuredFor", juce::String (refPrint));
        p->setProperty ("reference", juce::var (r));
    }
    return juce::var (p);
}

juce::String SampleModel::saveProject (const juce::File& file)
{
    if (! state || ! edit) return "Nothing open.";
    if (prepping) return "Auto Prep is still running: save when it finishes.";
    if (recall.isObject()) return "The saved project is still being recalled: run Auto Prep first.";
    // written whole to a temporary file, then swapped in: a failed save never leaves half a project behind
    juce::TemporaryFile tmp (file);
    if (! tmp.getFile().replaceWithText (juce::JSON::toString (projectVar (file))) || ! tmp.overwriteTargetFileWithTemporary())
        return "Couldn't write " + file.getFullPathName() + ".";
    projFile = file;
    savedEdits = edits;
    return {};
}

juce::String SampleModel::openProject (const juce::File& file)
{
    const auto p = juce::JSON::parse (file);
    if (! p.isObject() || ! p.hasProperty ("limitlessDrumsProject")) return file.getFileName() + " isn't a Limitless Drums project.";
    if ((int) p["limitlessDrumsProject"] > kProjectVersion) return file.getFileName() + " was saved by a newer version of Limitless Drums.";
    const auto files = projectFiles (p, file);
    juce::StringArray missing;
    for (auto& f : files) if (! f.existsAsFile()) missing.add (f.getFileName());
    if (files.empty()) return "That project names no recordings.";
    if (! missing.isEmpty()) return "Can't find " + missing.joinIntoString (", ") + ". Put the project back beside its recordings.";
    juce::Array<juce::File> items;
    for (auto& f : files) items.add (f);
    const bool pref = roomPhasePref;
    if (auto err = open (items); err.isNotEmpty()) return err;
    name = p["take"].toString().isEmpty() ? name : p["take"].toString();
    auto& S = state->session;
    const auto* tracks = p["tracks"].getArray();
    const auto s = p["session"];
    if (! tracks || tracks->size() != (int) S.tracks.size() || std::abs ((double) s["sr"] - S.sr) > 0.5)
        return "The recordings have changed since " + file.getFileName() + " was saved (different tracks or rate), so it can't be recalled. The take is open as new.";
    for (int t = 0; t < tracks->size(); ++t)
        if ((*tracks)[t]["mic"].toString() != juce::String (S.tracks[(size_t) t].mic))
            return "The recordings have changed since " + file.getFileName() + " was saved (the tracks don't match), so it can't be recalled. The take is open as new.";
    const std::string drum = s["drum"].toString().toStdString();
    if (! drum.empty() && drum != S.drum) { S.drum = drum; hl::applyDrumDefaults (S, drum); }
    S.artic = s["artic"].toString().toStdString();
    S.det.V = std::clamp ((int) s["V"], 1, 32); S.det.RR = std::clamp ((int) s["RR"], 1, 32);
    S.refIdx = std::clamp ((int) s["lead"], 0, (int) S.tracks.size() - 1);
    hl::applySettings (S, p["settings"]);
    S.timing = s["timing"].toString().isEmpty() ? S.timing : s["timing"].toString().toStdString();
    S.roomPhase = (bool) s["roomPhase"];
    roomPhasePref = pref;   // the project's choice is this take's; the next new take still starts from yours
    S.tune.on = (bool) s["tuneOn"]; S.tune.mode = s["tuneMode"].toString().toStdString(); S.tune.target = s["tuneTarget"].toString().toStdString();
    if (S.tune.mode.empty()) S.tune.mode = "drum";
    if (S.tune.target.empty()) S.tune.target = "nearest";
    S.tune.cents = (double) s["tuneCents"]; if ((double) s["tuneA4"] > 0) S.tune.a4 = (double) s["tuneA4"];
    S.tuneAnyway = (bool) s["tuneAnyway"]; S.keepAttack = (bool) s["keepAttack"];
    if (s["tuneMic"].toString().isNotEmpty()) S.tuneMic = s["tuneMic"].toString().toStdString();
    // the hits exactly as saved: nothing is found again
    if (auto* on = s["onsets"].getArray())
    {
        S.onsets.clear();
        for (auto& o : *on) S.onsets.push_back ((long) (juce::int64) o);
        S.detRan = true;
    }
    for (int t = 0; t < tracks->size(); ++t)
    {
        const auto& o = (*tracks)[t]; auto& tr = S.tracks[(size_t) t];
        tr.rename = o["rename"].toString().toStdString(); tr.roleOverride = o["roleOverride"].toString().toStdString();
        tr.alignTo = o["alignTo"].toString().isEmpty() ? tr.alignTo : o["alignTo"].toString().toStdString();
        tr.manualLag = (bool) o["manualLag"]; tr.manualPol = (bool) o["manualPol"]; tr.manualAp = (bool) o["manualAp"];
        tr.userAp = apOf (o["userAp"]);
        if (tr.manualLag) tr.lag = (double) o["lag"];
        if (tr.manualPol) tr.pol = (int) o["pol"] < 0 ? -1 : 1;
        setMonDb (t, (float) (double) o["monDb"]); setPan (t, (float) (double) o["pan"]); setMute (t, (bool) o["mute"]); setSolo (t, (bool) o["solo"]);
    }
    levelMatchOn = ! p.hasProperty ("levelMatch") || (bool) p["levelMatch"];
    if (const auto r = p["reference"]; r.isObject())
    {
        // the match itself came back with the settings, exactly as saved; this is who it was and what it was measured for
        refSource = r["file"].toString().isEmpty() ? juce::File() : juce::File (r["file"].toString());
        refName = r["name"].toString(); refHits = (int) r["hits"]; refEngine = (int) r["engine"]; refPrint = r["measuredFor"].toString().toStdString();
    }
    const bool wasPrepared = (bool) p["prepared"];
    state->prepared = false;
    buildHits();
    projFile = file;
    edits = savedEdits = 0;
    if (wasPrepared) recall = p;
    return {};
}

void SampleModel::finishRecall()
{
    if (! recall.isObject() || ! state) return;
    const auto p = recall;
    recall = juce::var();
    auto& S = state->session;
    recallMsg.clear();
    // each mic's alignment exactly as it was applied when saved, whatever today's measurement says
    if (auto* tracks = p["tracks"].getArray())
        for (int t = 0; t < std::min (tracks->size(), (int) S.tracks.size()); ++t)
        {
            const auto& o = (*tracks)[t]; auto& tr = S.tracks[(size_t) t];
            tr.alignTo = o["alignTo"].toString().isEmpty() ? tr.alignTo : o["alignTo"].toString().toStdString();
            tr.lag = (double) o["lag"]; tr.pol = (int) o["pol"] < 0 ? -1 : 1;
            tr.apOn = (bool) o["apOn"]; tr.ap = apOf (o["ap"]); tr.userAp = apOf (o["userAp"]);
            tr.manualLag = (bool) o["manualLag"]; tr.manualPol = (bool) o["manualPol"]; tr.manualAp = (bool) o["manualAp"];
        }
    undoStack.clear(); redoStack.clear(); scoreCache.clear();
    reapply (true);
    if (auto* r = p["session"]["ratios"].getArray())
    {
        double worst = 0;
        for (int i = 0; i < std::min (r->size(), (int) S.onsets.size()); ++i)
            worst = std::max (worst, std::abs (1200.0 * std::log2 (std::max (1e-9, hitRatio ((size_t) i)) / std::max (1e-9, (double) (*r)[i]))));
        if (r->size() != (int) S.onsets.size() || worst > 0.5)
            recallMsg = "The tuning was measured again and differs from when this was saved (by up to " + juce::String (worst, 1) + " cents). Everything else is as saved.";
    }
    setLevelMatch (levelMatchOn);
    edits = savedEdits = 0;
}

juce::String SampleModel::renameTrack (int t, const std::string& to)
{
    if (! state || t < 0 || t >= (int) state->session.tracks.size()) return "No such track.";
    const auto trimmed = juce::String (to).trim().toStdString();
    auto trial = state->session;
    trial.tracks[(size_t) t].rename = trimmed;
    const auto mine = hl::fillTokens (trial, "{MIC}", t, std::nullopt);
    for (int o = 0; o < (int) trial.tracks.size(); ++o)
        if (o != t && juce::String (hl::fillTokens (trial, "{MIC}", o, std::nullopt)).equalsIgnoreCase (juce::String (mine)))
            return "Another track is already called that: their files would overwrite each other.";
    if (! change ([t, trimmed] (hl::Session& S) { S.tracks[(size_t) t].rename = trimmed; }, Redo::none)) return "Auto Prep is running: try again when it finishes.";
    return {};
}

// ---------------------------------------------------------------------------- the reference
te::AudioTrack* SampleModel::refTrack() const
{
    if (! edit || ! state) return nullptr;
    auto ts = te::getAudioTracks (*edit);
    const int i = 2 * (int) state->session.tracks.size();
    return i < ts.size() ? ts[i] : nullptr;
}

std::string SampleModel::referencePrint() const
{
    // everything the take's side of the measurement is made from (hl::loadReference): the hits, each mic's delay and
    // polarity, the clean-up, and the tuning the match is made to land through
    if (! state) return {};
    const auto& S = state->session;
    std::string p = std::to_string (S.det.V) + "x" + std::to_string (S.det.RR) + S.det.order + "|";
    for (long o : S.onsets) p += std::to_string (o) + ",";
    p += "|";
    for (auto& tr : S.tracks) p += juce::String (tr.lag, 4).toStdString() + (tr.pol < 0 ? "-" : "+") + ",";
    p += "|" + std::string (S.dnOn ? "dn" : "raw") + juce::String (S.dnDb, 2).toStdString() + "|";
    if (S.tune.on && S.tinfo) for (double r : S.tinfo->ratios) p += juce::String (r, 6).toStdString() + ",";
    return p;
}

bool SampleModel::referenceStale() const
{
    return hasReference() && ! refPrint.empty() && refPrint != referencePrint();
}

juce::String SampleModel::loadReference (const juce::File& f)
{
    if (! state || ! state->mix) return "Run Auto Prep first.";
    if (prepping) return "Auto Prep is running: try again when it finishes.";
    auto R = hl::loadReference (state->session, *state->mix, f, hl::kReferenceEngine);
    if (! R.error.empty() || R.curve.empty()) return "Couldn't read that reference: " + juce::String (R.error.empty() ? "nothing to measure" : R.error) + ".";
    if (! change ([R] (hl::Session& s)
        {
            s.tone.hasRef = true; s.tone.refCurve = R.curve; if (s.tone.refAmt == 0) s.tone.refAmt = 0.6;
            s.tone.hasRefEnv = R.hasEnv; s.tone.refEnv = R.env; if (R.hasEnv && s.tone.refEnvAmt == 0) s.tone.refEnvAmt = 0.5;
        }, Redo::mix)) return "Auto Prep is running: try again when it finishes.";
    if (refAud) setReferenceAudition (false);
    refAudio = R.audio; refSource = f; refName = R.name; refHits = R.hitsUsed; refEngine = R.engine;
    refPrint = referencePrint();
    refCancelled = R.cancelled;
    refPlayFile = juce::File();
    return {};
}

juce::String SampleModel::setReferenceAudition (bool on, double at)
{
    auto* rt = refTrack();
    if (! rt) return "Nothing open.";
    if (! on)
    {
        if (! refAud) return {};
        refAud = false;
        for (auto* c : rt->getClips()) c->removeFromParent();
        applyMutes();
        return {};
    }
    if (! hasReference()) return "Load a reference first.";
    if (refAudio.empty() && refSource.existsAsFile() && state->mix)   // a recalled project: only its audio is read, the saved match stays as it is
        refAudio = hl::loadReference (state->session, *state->mix, refSource, refEngine > 0 ? refEngine : hl::kReferenceEngine).audio;
    if (refAudio.empty() || refAudio[0].empty()) return refSource == juce::File() ? juce::String ("The reference's audio isn't here: load it again to listen to it.")
                                                                                  : "Can't find " + refSource.getFileName() + " to play it.";
    const auto& S = state->session;
    if (refPlayFile == juce::File())
    {
        refPlayFile = cacheDir.getChildFile ("reference_" + juce::String (++refSerial) + ".wav");
        if (! writeFloatWav (refPlayFile, refAudio, S.sr)) { refPlayFile = juce::File(); return "Couldn't prepare the reference for playback (is the disk full?)."; }
        // its loudness against the take's hardest hit as recorded (every mic, 300 ms)
        auto rms = [] (const float* x, long n) { double e = 0; for (long i = 0; i < n; ++i) e += (double) x[i] * x[i]; return n > 0 ? std::sqrt (e / (double) n) : 0.0; };
        const long n = (long) (0.3 * S.sr);
        const double theirs = rms (refAudio[0].data(), std::min (n, (long) refAudio[0].size()));
        double ours = 0;
        if (! hitList.empty())
        {
            size_t hard = 0;
            for (size_t i = 0; i < hitList.size(); ++i) if (hitList[i].peakDb > hitList[hard].peakDb) hard = i;
            const long a = (long) S.onsets[hard];
            std::vector<float> sum ((size_t) n, 0.0f);
            for (auto& tr : S.tracks)
                for (auto& ch : tr.ch.get())
                    for (long i = 0; i < n && a + i < (long) ch.size(); ++i) sum[(size_t) i] += ch[(size_t) (a + i)] / (float) tr.ch.size();
            ours = rms (sum.data(), n);
        }
        refGainDb = theirs > 1e-9 && ours > 1e-9 ? std::clamp (20 * std::log10 (ours / theirs), -40.0, 40.0) : 0.0;
    }
    for (auto* c : rt->getClips()) c->removeFromParent();
    te::AudioFile af (engine, refPlayFile);
    te::ClipPosition pos { te::TimeRange (te::TimePosition::fromSeconds (kPad + std::max (0.0, at)), te::TimeDuration::fromSeconds (af.getLength())), te::TimeDuration() };
    auto clip = rt->insertWaveClip ("Reference", refPlayFile, pos, false);
    if (clip == nullptr) return "Couldn't place the reference for playback.";
    clip->setAutoCrossfade (false);
    clip->setGainDB ((float) (levelMatchOn ? refGainDb : 0.0));
    refAud = true;
    applyMutes();
    return {};
}

void SampleModel::setRoomPhase (bool on)
{
    roomPhasePref = on;
    if (state) change ([on] (hl::Session& S) { hl::setRoomPhase (S, on); }, Redo::align);
}

SampleModel::ScoreKey SampleModel::scoreKey (int t) const
{
    ScoreKey k;
    if (! state) return k;
    const auto& S = state->session;
    if (t < 0 || t >= (int) S.tracks.size() || t == S.refIdx) return k;
    const auto& tr = S.tracks[(size_t) t];
    const auto& res = tr.alignTo.size() == 1 ? tr.grp : tr.autoResult;
    if (! res || ! res->valid) return k;
    k.ref = hl::alignRef (S, t) >= 0 ? hl::alignRef (S, t) : S.refIdx;
    if (k.ref < 0 || k.ref >= (int) S.tracks.size()) { k.ref = -1; return k; }
    k.lag = tr.lag; k.pol = tr.pol; k.apOn = tr.apOn;
    if (tr.apOn) for (auto& a : tr.ap) k.ap.push_back ({ a.fc, a.Q });   // every band, frequency and Q
    k.arrival = res->arrival; k.sr = S.sr; k.gen = takeGen;
    k.onsets.assign (S.onsets.begin(), S.onsets.end());
    // the audio is immutable once loaded (SharedChs copies on write), so the buffers themselves identify it; holding
    // them also keeps the worker's input alive whatever Undo, Redo or a new take does to the session meanwhile
    k.refCh = S.tracks[(size_t) k.ref].ch.share();
    k.ch = tr.ch.share();
    return k;
}

bool SampleModel::ScoreKey::operator== (const ScoreKey& o) const
{
    const auto same = [] (double a, double b) { return a == b || (std::isnan (a) && std::isnan (b)); };   // NaN arrival = none
    if (ref != o.ref || ! same (lag, o.lag) || pol != o.pol || apOn != o.apOn || ! same (arrival, o.arrival) || sr != o.sr || gen != o.gen) return false;
    if (refCh != o.refCh || ch != o.ch || onsets != o.onsets || ap.size() != o.ap.size()) return false;
    for (size_t i = 0; i < ap.size(); ++i) if (! same (ap[i].first, o.ap[i].first) || ! same (ap[i].second, o.ap[i].second)) return false;
    return true;
}

std::pair<double, double> SampleModel::sumScore (int t, bool* stale)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (stale) *stale = false;
    auto k = scoreKey (t);
    if (k.ref < 0) return { nan, nan };
    auto it = scoreCache.find (t);
    const bool have = it != scoreCache.end();
    if (have && it->second.first == k) return it->second.second;
    if (stale) *stale = true;
    // measure where it sits now (and as recorded), the way Auto Prep measures, on another thread (it can take a few
    // hundred ms on a long take); until it lands, show the last measurement, marked stale. Everything the worker reads
    // was captured above on this thread (audio buffers by shared ownership, the rest by value): it never touches the
    // session, which Undo and Redo replace
    if (! scoring.count (t))
    {
        scoring.insert (t);
        auto alivePtr = alive;
        auto key = std::make_shared<const ScoreKey> (std::move (k));
        std::thread ([this, alivePtr, t, key, running = Workers::start (workers)]
        {
            const auto refMono = hl::monoOf (*key->refCh);
            std::vector<hl::AllPass> ap;
            for (auto& a : key->ap) ap.push_back ({ a.first, a.second });
            const auto sc = hl::scoreMic (refMono, *key->ch, key->onsets, key->sr, key->lag, key->pol, ap, key->arrival);
            const double nan2 = std::numeric_limits<double>::quiet_NaN();
            const std::pair<double, double> v = sc.valid ? std::make_pair (sc.score, sc.asis) : std::make_pair (nan2, nan2);
            juce::MessageManager::callAsync ([this, alivePtr, t, key, v]
            {
                if (! *alivePtr) return;
                scoring.erase (t);
                if (key->gen != takeGen) return;
                scoreCache[t] = { *key, v };   // keyed by its inputs, so it's only shown while they still match
                if (onChanged) onChanged();
            });
        }).detach();
    }
    return have ? it->second.second : std::make_pair (nan, nan);
}

void SampleModel::startRender()
{
    if (rendering || renderDone == renderWanted || ! state) return;
    rendering = true;
    const int ticket = renderWanted;
    const bool mix = wantMix || ! state->mix;
    wantMix = false;
    // the session is copied (the audio is shared, not copied); a levels-only pass reads the current render in place
    // (nothing else replaces it while this runs: only this callback does)
    auto copy = std::make_shared<EngineState>();
    copy->session = state->session;
    copy->prepared = state->prepared;
    auto base = state;
    auto alivePtr = alive;
    // (a render for a take that has since been replaced lands with state != base and is dropped)
    const auto dir = cacheDir;
    const int fileTag = ++toneVersion;
    std::thread ([this, copy, base, alivePtr, ticket, mix, dir, fileTag, running = Workers::start (workers)]
    {
        const auto tm0 = juce::Time::getMillisecondCounterHiRes();
        if (mix) renderAndMeasure (*copy, base->mix ? &*base->mix : nullptr);
        else if (base->mix) copy->levels = hl::measureLevels (copy->session, *base->mix);
        const auto tm1 = juce::Time::getMillisecondCounterHiRes();
        // every tuned hit of every mic, ready to play (rendered here, so the message thread never waits on it)
        auto made = std::make_shared<std::vector<std::pair<std::string, juce::File>>>();
        const auto& CS = copy->session;
        const hl::MixRender* M = mix ? (copy->mix ? &*copy->mix : nullptr) : (base->mix ? &*base->mix : nullptr);
        if (copy->levels && copy->levels->valid && CS.tinfo)
            for (size_t i = 0; i < copy->levels->regions.size() && i < CS.tinfo->ratios.size(); ++i)
            {
                const double r = CS.tinfo->ratios[i];
                if (! std::isfinite (r) || r <= 0 || std::abs (r - 1.0) <= 1e-12) continue;
                for (int t = 0; t < (int) CS.tracks.size(); ++t)
                {
                    const auto key = hitKey (t, i, r, CS.sr);
                    auto f = dir.getChildFile ("hit_" + juce::String (key) + "_" + juce::String (fileTag) + ".wav");
                    if (writeFloatWav (f, renderHit (*copy, M, t, i, r, CS.sr), CS.sr)) made->push_back ({ key, f });
                }
            }
        const auto tm2 = juce::Time::getMillisecondCounterHiRes();
        juce::MessageManager::callAsync ([this, copy, base, alivePtr, ticket, mix, made, tm0, tm1, tm2]
        {
            if (! *alivePtr) return;
            landRender ([this, copy, base, ticket, mix, made, tm0, tm1, tm2]
            {
            const auto tm3 = juce::Time::getMillisecondCounterHiRes();
            struct Log { double a, b, c, d; bool mix; size_t n; ~Log() { if (std::getenv ("LD_TIMING")) fprintf (stderr, "[timing] %s: process %.0f ms, hit files %.0f ms (%zu), queue %.0f ms, land %.0f ms\n", mix ? "mix" : "levels", b - a, c - b, n, d - c, juce::Time::getMillisecondCounterHiRes() - d); } } log { tm0, tm1, tm2, tm3, mix, made->size() };
            rendering = false;
            renderDone = ticket;
            if (state == base)
            {
                if (mix) state->mix = std::move (copy->mix);
                state->levels = copy->levels;
                if (mix) prepFor = nullptr;   // the hits play the new clean-up and tone
                if (prepFor != state.get()) { prepFiles.clear(); hitFiles.clear(); prepFor = state.get(); }
                bool newFiles = false;
                for (auto& [k, f] : *made) if (! hitFiles.count (k)) { hitFiles[k] = f; newFiles = true; }
                const bool current = renderDone == renderWanted;
                if (mix || newFiles || tuneLayoutWanted || clipsStale)
                {
                    // while something plays and the hits are being previewed, laying the files on the tracks would
                    // rebuild playback for audio that's already heard: it waits for playback to stop
                    if ((isPlaying() || holdClipsForTests) && previewOk) { const bool was = clipsStale; clipsStale = true; updateHitGains(); if (! was && ! holdClipsForTests) watchClips(); }
                    else { layoutHits(); clipsStale = false; if (current) clearHitAudio(); else publishHitAudio(); }
                }
                else updateHitGains();
                // (a worker reading the render this replaced was stopped first: it carries on from the new one)
                if (! hitAudio.empty() || clipsStale || ! current) startPreview();
                warmPreview();
                tuneLayoutWanted = false;
            }
            releaseFreedMemory();
            if (onChanged) onChanged();
            renderSoon();   // edits that came in meanwhile
            });
        });
    }).detach();
}

void SampleModel::renderSoon()
{
    // the whole render waits until the knob has been still for a moment: while it moves, the previewed hits are what
    // is heard, and they get the processor to themselves
    if (rendering || renderDone == renderWanted || ! state) return;
    const double wait = renderHoldMs - (juce::Time::getMillisecondCounterHiRes() - lastChangeMs);
    if (wait <= 0 || ! previewOk) { startRender(); return; }
    if (renderTimerSet) return;
    renderTimerSet = true;
    auto alivePtr = alive;
    juce::Timer::callAfterDelay ((int) std::ceil (wait) + 1, [this, alivePtr] { if (! *alivePtr) return; renderTimerSet = false; renderSoon(); });
}

void SampleModel::warmPreview()
{
    // the tone pass's listening, made as soon as there's a render to make it from: the first knob touched is then
    // heard as fast as any other
    if (! state || ! state->mix || prepping || listeningWanted || ! hl::HitPreviewer::possible (state->session, *state->mix)) return;
    const auto key = hl::HitPreviewer::listeningKey (state->session, *state->mix);
    if ((listening && listening->key == key) || state->mix->scanKey == key) return;
    listeningWanted = true;
    ++previewWorkers;   // (it reads the render: a new one waits for it like it waits for a preview)
    auto copy = std::make_shared<const hl::Session> (state->session);
    auto base = state;
    auto alivePtr = alive;
    std::thread ([this, copy, base, alivePtr, running = Workers::start (workers)]
    {
        auto L = hl::HitPreviewer::listen (*copy, *base->mix);
        juce::MessageManager::callAsync ([this, alivePtr, L]
        {
            if (! *alivePtr) return;
            --previewWorkers; listeningWanted = false;
            listening = L;
        });
    }).detach();
}

void SampleModel::watchClips()
{
    // playback that ends on its own: the clips catch up then too (not only on Stop)
    if (! clipsStale) return;
    if (! isPlaying() && ! rendering && renderDone == renderWanted) { settleClips(); return; }
    auto alivePtr = alive;
    juce::Timer::callAfterDelay (200, [this, alivePtr] { if (*alivePtr) watchClips(); });
}

void SampleModel::landRender (std::function<void()> fn)
{
    if (previewWorkers == 0) { fn(); return; }
    cancelPreview();
    auto alivePtr = alive;
    juce::Timer::callAfterDelay (2, [this, alivePtr, fn = std::move (fn)]() mutable { if (*alivePtr) landRender (std::move (fn)); });
}

void SampleModel::settleClips()
{
    if (! clipsStale || ! state || rendering || renderDone != renderWanted || isPlaying() || holdClipsForTests) return;
    clipsStale = false;
    layoutHits();
    clearHitAudio();
}

void SampleModel::clearHitAudio()
{
    if (hitAudio.empty()) return;
    hitAudio.clear();
    if (! state || ! edit) return;
    for (int t = 0; t < (int) state->session.tracks.size(); ++t)
        if (auto* at = track (t)) if (auto* a = live::alignOn (*at)) a->setHitAudio ({}, 0);
}

void SampleModel::publishHitAudio()
{
    if (! state || ! edit || ! state->levels || ! state->levels->valid) return;
    const int nT = (int) state->session.tracks.size();
    if (playedRatio.size() != state->levels->regions.size()) playedRatio.assign (state->levels->regions.size(), 1.0);
    for (auto& [i, h] : hitAudio) if (i < playedRatio.size()) playedRatio[i] = h.ratio;   // the ratio this hit is heard at
    for (int t = 0; t < nT; ++t)
    {
        auto* at = track (t);
        auto* a = at ? live::alignOn (*at) : nullptr;
        if (a == nullptr) continue;
        std::vector<live::AlignPlugin::HitAudio> v;
        for (auto& [i, h] : hitAudio) if (t < (int) h.tracks.size() && h.tracks[(size_t) t]) v.push_back ({ h.start, h.tracks[(size_t) t] });
        std::sort (v.begin(), v.end(), [] (auto& x, auto& y) { return x.start < y.start; });
        a->setHitAudio (std::move (v), hitAudioRate);
        a->setHitScales (scalesFor (t, nullptr));
    }
}

void SampleModel::startPreview()
{
    cancelPreview();
    previewOk = false;
    if (! state || ! edit || prepping || ! state->prepared || ! state->mix || ! state->levels || ! state->levels->valid) { clearHitAudio(); return; }
    if (! hl::HitPreviewer::possible (state->session, *state->mix)) { clearHitAudio(); return; }
    const double rate = previewRateForTests > 0 ? previewRateForTests : engine.getDeviceManager().getSampleRate();
    if (rate <= 0) { clearHitAudio(); return; }
    if (rate != hitAudioRate) { clearHitAudio(); hitAudioRate = rate; }
    previewOk = true;
    const auto& S = state->session;
    auto regions = std::make_shared<const std::vector<hl::Region>> (state->levels->regions);
    const size_t nH = std::min (regions->size(), S.onsets.size());
    // the hit under the playhead first, then on from there
    const double now = edit->getTransport().getPosition().inSeconds() - kPad;
    size_t first = 0;
    for (size_t i = 0; i < nH; ++i) if ((double) (*regions)[i].start / S.sr <= now) first = i;
    auto order = std::make_shared<std::vector<size_t>>();
    for (size_t k = 0; k < nH; ++k)
    {
        const size_t i = (first + k) % nH;
        auto it = hitAudio.find (i);
        if (it == hitAudio.end() || it->second.gen != audioGen) order->push_back (i);
    }
    if (order->empty()) return;
    auto copy = std::make_shared<const hl::Session> (S);
    auto base = state;
    auto alivePtr = alive;
    auto gen = previewGen;
    auto heard = listening;
    const int myGen = gen->load(), agen = audioGen;
    ++previewWorkers;
    std::thread ([this, copy, base, alivePtr, gen, myGen, agen, regions, order, rate, nH, heard, running = Workers::start (workers)]
    {
        const auto& CS = *copy;
        const int nT = (int) CS.tracks.size();
        const double sr = CS.sr;
        const auto startOf = [&] (size_t i) { return (juce::int64) std::llround (((double) (*regions)[i].start / sr + kPad) * rate); };
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        hl::HitPreviewer P (CS, *base->mix, heard);
        bool firstDone = false;
        if (P.valid())
            for (size_t i : *order)
            {
                if (gen->load() != myGen) break;
                const auto& R = (*regions)[i];
                double ratio = CS.tinfo && i < CS.tinfo->ratios.size() ? CS.tinfo->ratios[i] : 1.0;
                if (! std::isfinite (ratio) || ratio <= 0) ratio = 1.0;
                const double step = ratio * sr / rate, on = (double) CS.onsets[i];
                const long len = R.end - R.start;
                const long outLen = std::max (1L, (long) std::floor ((double) len / step + 0.5));
                // each hit stops where the next starts
                const long keep = i + 1 < nH ? (long) std::min<juce::int64> (outLen, std::max<juce::int64> (0, startOf (i + 1) - startOf (i))) : outLen;
                const bool asIs = std::abs (ratio - 1.0) <= 1e-12 && rate == sr;
                std::vector<std::shared_ptr<const hl::Chs>> bufs ((size_t) nT);
                std::vector<std::thread> th;
                for (int t = 0; t < nT; ++t)
                    th.emplace_back ([&, t]
                    {
                        auto x = P.render (t, R.start, R.end);
                        if (! asIs)
                        {
                            const long preOut = (long) std::floor ((on - (double) R.start) / step + 0.5);
                            for (auto& c : x) c = hl::renderTuned (c, on - (double) R.start, preOut, outLen, 0.0, 1, 1.0, step, 0, len, 0, 64);
                        }
                        for (auto& c : x) if ((long) c.size() > keep) c.resize ((size_t) keep);
                        bufs[(size_t) t] = std::make_shared<const hl::Chs> (std::move (x));
                    });
                for (auto& x : th) x.join();
                if (! firstDone && std::getenv ("LD_TIMING")) fprintf (stderr, "[timing] preview: first hit ready %.0f ms after the change's worker started\n", juce::Time::getMillisecondCounterHiRes() - t0);
                firstDone = true;
                const juce::int64 start = startOf (i);
                juce::MessageManager::callAsync ([this, alivePtr, base, gen, myGen, agen, i, ratio, start, bufs]
                {
                    if (! *alivePtr || gen->load() != myGen || state != base || agen != audioGen) return;
                    hitAudio[i] = { agen, ratio, start, bufs };
                    publishHitAudio();
                });
            }
        juce::MessageManager::callAsync ([this, alivePtr] { if (*alivePtr) { --previewWorkers; if (onChanged) onChanged(); } });
    }).detach();
}

double SampleModel::changeLead() const
{
    // a phase EQ that has to follow the hits' tuning needs its slot filled first; otherwise a fade is enough
    bool eqMoves = false;
    if (state)
    {
        bool tuned = false;
        for (size_t i = 0; i < hitList.size(); ++i) if (std::abs (hitRatio (i) - 1.0) > 1e-9) tuned = true;
        for (auto& tr : state->session.tracks) if (tuned && tr.apOn && ! tr.ap.empty()) eqMoves = true;
    }
    return live::AlignDsp::changeSeconds (state ? state->session.sr : 48000, eqMoves);
}

double SampleModel::hitRatio (size_t i) const
{
    if (! state || ! state->session.tinfo || i >= state->session.tinfo->ratios.size()) return 1.0;
    const double r = state->session.tinfo->ratios[i];
    return std::isfinite (r) && r > 0 ? r : 1.0;
}

double SampleModel::hitGainDb (int t, size_t i) const
{
    if (! state || ! state->levels || ! state->levels->valid) return 0;
    const auto& L = *state->levels;
    if (t < 0 || t >= (int) L.gainsDbT.size() || i >= L.gainsDbT[(size_t) t].size()) return 0;
    return L.gainsDbT[(size_t) t][i] + L.trimDb;
}

juce::File SampleModel::prepFile (int t)
{
    const auto& S = state->session;
    if (prepFiles.size() != S.tracks.size() || prepFor != state.get())
    {
        prepFiles.assign (S.tracks.size(), juce::File());
        hitFiles.clear();
        prepFor = state.get();
    }
    if (prepFiles[(size_t) t] != juce::File()) return prepFiles[(size_t) t];
    // clean-up and tone (everything before the phase EQ, which Align plays live), when they change anything, are what
    // the hits play; A stays as recorded. (The bus stage is empty where the phase EQ stage took it over.)
    juce::File src = sourceFiles[(size_t) t];
    if (state->mix && t < (int) state->mix->proc.size())
    {
        const auto& M = *state->mix;
        const auto& pre = M.bus[(size_t) t].empty() ? M.proc[(size_t) t] : M.bus[(size_t) t];
        const auto& orig = S.tracks[(size_t) t].ch;
        bool differs = pre.size() != orig.size();
        for (size_t c = 0; ! differs && c < pre.size(); ++c)
        {
            if (pre[c].size() != orig[c].size()) { differs = true; break; }
            for (size_t n = 0; n < pre[c].size(); n += 3)
                if (std::abs (pre[c][n] - orig[c][n]) > 1.0e-6f) { differs = true; break; }
        }
        if (differs)
        {
            auto f = cacheDir.getChildFile (juce::String (t) + "_prep_" + juce::String (++toneVersion) + ".wav");
            if (! writeFloatWav (f, pre, S.sr)) return {};   // (not cached: tried again next time)
            src = f;
        }
    }
    return prepFiles[(size_t) t] = src;
}

std::vector<live::AlignPlugin::HitScale> SampleModel::scalesFor (int t, const std::vector<int>* only) const
{
    std::vector<live::AlignPlugin::HitScale> scales;
    const auto& R = state->levels->regions;
    auto one = [&] (size_t i)
    {
        // switch early enough that the change has fully arrived before the hit (the end of the previous hit's tail
        // takes it instead)
        const double start = (double) R[i].start / state->session.sr;
        // the ratio this hit actually plays at (a tuned hit whose render hasn't landed yet still plays untuned)
        const double r = i < playedRatio.size() ? playedRatio[i] : hitRatio (i);
        scales.push_back ({ kPad + start - changeLead() - 0.002, 1.0 / r, hitGainDb (t, i), r });
    };
    if (only) { for (int i : *only) one ((size_t) i); }
    else for (size_t i = 0; i < R.size(); ++i) one (i);
    std::sort (scales.begin(), scales.end(), [] (auto& x, auto& y) { return x.start < y.start; });
    return scales;
}

std::string SampleModel::hitKey (int t, size_t i, double ratio, double rate)
{
    char b[96];
    std::snprintf (b, sizeof b, "%d_%zu_%.12f_%.3f", t, i, ratio, rate);
    return b;
}

hl::Chs SampleModel::renderHit (const EngineState& st, const hl::MixRender* M, int t, size_t i, double ratio, double outRate)
{
    // the audio before the phase EQ (the Align plugin adds the shift, polarity, phase EQ and gain after it)
    const auto& S = st.session;
    const hl::Chs* src = &S.tracks[(size_t) t].ch.get();
    if (M && t < (int) M->proc.size()) src = M->bus[(size_t) t].empty() ? &M->proc[(size_t) t] : &M->bus[(size_t) t];
    const auto& R = st.levels->regions[i];
    const double step = ratio * S.sr / outRate, on = (double) S.onsets[i];
    const long preOut = (long) std::floor ((on - (double) R.start) / step + 0.5);
    const long outLen = std::max (1L, (long) std::floor ((double) (R.end - R.start) / step + 0.5));
    hl::Chs out;
    for (auto& x : *src) out.push_back (hl::renderTuned (x, on, preOut, outLen, 0.0, 1, 1.0, step, R.start, R.end, 0, 64));
    return out;
}

juce::File SampleModel::hitFile (int t, size_t i, double outRate)
{
    if (prepFor != state.get()) { prepFiles.clear(); hitFiles.clear(); prepFor = state.get(); }
    const auto key = hitKey (t, i, hitRatio (i), outRate);
    if (auto it = hitFiles.find (key); it != hitFiles.end() && it->second.existsAsFile()) return it->second;
    auto f = cacheDir.getChildFile ("hit_" + juce::String (key) + "_" + juce::String (++toneVersion) + ".wav");
    if (! writeFloatWav (f, renderHit (*state, state->mix ? &*state->mix : nullptr, t, i, hitRatio (i), outRate), outRate)) return {};
    return hitFiles[key] = f;
}

void SampleModel::layoutHits (bool renderMissing) { layoutHitsFor (nullptr, 0, renderMissing); }

void SampleModel::layoutHitsFor (const std::vector<int>* only, double outRate, bool renderMissing)
{
    if (! state || ! edit || ! state->levels || ! state->levels->valid) return;
    const auto& S = state->session;
    const auto& R = state->levels->regions;
    if (R.empty()) return;
    const double sr = S.sr, end = length();
    if (outRate <= 0) outRate = sr;
    const bool wasPlaying = isPlaying();
    playedRatio.assign (R.size(), 1.0);
    for (int t = 0; t < (int) S.tracks.size(); ++t)
    {
        auto* at = track (t);
        if (at == nullptr || t >= (int) sourceFiles.size()) continue;
        auto src = prepFile (t);
        if (src == juce::File()) src = sourceFiles[(size_t) t];   // couldn't write the processed audio: export refuses, playback plays A's
        { auto clips = at->getClips(); for (auto* c : clips) c->removeFromParent(); }   // (a copy: removing changes the list)
        std::vector<int> all;
        if (! only) for (size_t i = 0; i < R.size(); ++i) all.push_back ((int) i);
        for (int ii : only ? *only : all)
        {
            const size_t i = (size_t) ii;
            const double r = hitRatio (i);
            // the clip starts on a whole output sample; a tuned hit (or any hit at another rate) plays its own
            // rendered file, an untuned one plays the processed track from the region's start
            const double start = std::round (((double) R[i].start / sr + kPad) * outRate) / outRate - kPad;
            bool own = std::abs (r - 1.0) > 1e-12 || outRate != sr;
            juce::File clipSrc = src;
            if (own)
            {
                const auto key = hitKey (t, i, r, outRate);
                if (prepFor == state.get() && hitFiles.count (key)) clipSrc = hitFiles[key];
                else if (renderMissing || only) clipSrc = hitFile (t, i, outRate);
                else clipSrc = juce::File();
                if (clipSrc == juce::File())
                {
                    if (only) continue;
                    own = false;   // not rendered yet: plays untuned until it is
                    clipSrc = src;
                }
            }
            // playing: each hit stops where the next starts. Exporting (non-overlapping sets): the whole tuned region
            const double cap = only ? 1.0e9 : (i + 1 < R.size() ? (double) R[i + 1].start / sr : end) - start;
            const double rr = own ? r : 1.0;
            if (t == 0) playedRatio[i] = rr;
            const double len = std::min ((double) (long) std::floor ((double) (R[i].end - R[i].start) / (rr * sr / outRate) + 0.5) / outRate, cap);
            if (len <= 0) continue;
            te::ClipPosition pos { te::TimeRange (te::TimePosition::fromSeconds (kPad + start), te::TimeDuration::fromSeconds (len)),
                                   te::TimeDuration::fromSeconds (own ? 0.0 : (double) R[i].start / sr) };
            auto clip = at->insertWaveClip (S.tracks[(size_t) t].name + " " + std::to_string (i + 1), clipSrc, pos, false);
            if (! clip) continue;
            clip->setAutoCrossfade (false);
            clip->setResamplingQuality (te::ResamplingQuality::sincBest);
        }
        if (auto* a = live::alignOn (*at)) a->setHitScales (scalesFor (t, only));
    }
    recomputeLevelMatch();
    edit->getUndoManager().clearUndoHistory();
    if (wasPlaying) play();
}

void SampleModel::updateHitGains()
{
    if (! state || ! edit || ! state->levels || ! state->levels->valid) return;
    for (int t = 0; t < (int) state->session.tracks.size(); ++t)
        if (auto* at = track (t)) if (auto* a = live::alignOn (*at)) a->setHitScales (scalesFor (t, nullptr));
    recomputeLevelMatch();
}

void SampleModel::recomputeLevelMatch()
{
    // A plays at the lead's average Level gain (B's loudness, on the lead)
    if (! state || ! state->levels || ! state->levels->valid) return;
    const auto& R = state->levels->regions;
    double sum = 0; int n = 0;
    for (size_t i = 0; i < R.size(); ++i) { sum += hitGainDb (state->session.refIdx, i); ++n; }
    matchDb = n ? sum / n : 0;
    setLevelMatch (levelMatchOn);
}

hl::Chs SampleModel::renderB (int t, double srOut, bool stereo, juce::String& err)
{
    juce::TemporaryFile temp (".wav");
    auto all = te::getAllTracks (*edit);
    juce::BigInteger which; which.setBit (all.indexOf (track (t)));
    te::Renderer::Parameters p (*edit);
    p.destFile = temp.getFile();
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32;
    p.sampleRateForAudio = srOut;
    p.blockSizeForAudio = 512;
    p.time = te::TimeRange (te::TimePosition(), edit->getLength());
    p.tracksToDo = which;
    p.usePlugins = true;
    p.useMasterPlugins = false;
    p.mustRenderInMono = ! stereo;
    p.ditheringEnabled = false;
    hl::Chs out;
    if (te::Renderer::renderToFile ("Export", p) == juce::File() || ! temp.getFile().existsAsFile())
    {
        err = "Tracktion couldn't render " + juce::String (info[(size_t) t].name) + " (is the disk full?).";
        return out;
    }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (temp.getFile()));
    const auto expect = (long) std::llround (edit->getLength().inSeconds() * srOut);
    if (! r || std::abs ((long) r->lengthInSamples - expect) > 2)
    {
        err = "The render of " + juce::String (info[(size_t) t].name) + " came back the wrong length.";
        return out;
    }
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    for (int c = 0; c < (stereo ? std::min (2, b.getNumChannels()) : 1); ++c) out.emplace_back (b.getReadPointer (c), b.getReadPointer (c) + b.getNumSamples());
    return out;
}

juce::String SampleModel::exportTo (const juce::File& dir, int& count, std::function<void (float)> progress, std::function<bool()> cancel)
{
    count = 0;
    if (auto why = exportBlocker(); why.isNotEmpty()) return why;
    cancelPreview(); clearHitAudio();   // the export plays the rendered files, nothing held in memory
    // the destination first: a folder that can't be made or written is reported now, not after the whole render
    {
        const auto made = dir.createDirectory();
        const auto probe = dir.getChildFile (".limitless-write-test");
        if (made.failed() || ! dir.isDirectory() || ! probe.replaceWithText ("ok"))
            return "Can't write to " + dir.getFullPathName() + (made.failed() ? " (" + made.getErrorMessage() + ")" : juce::String())
                 + ". Pick a folder you can write to.";
        probe.deleteFile();
    }
    if (refAud) setReferenceAudition (false);
    auto& S = state->session;
    const auto& L = *state->levels;
    const auto& R = L.regions;
    const int T = (int) S.tracks.size();
    const double sr = S.sr, srOut = S.exp.rate == "same" ? sr : std::stod (S.exp.rate);
    stop();
    for (int t = 0; t < T; ++t)
        if (prepFile (t) == juce::File()) return "Couldn't write the processed audio for " + juce::String (info[(size_t) t].name) + " (is the disk full?).";

    // export as set: monitor faders, pans, mutes and solos out of the way, B playing. Everything is put back as it
    // was however this ends (a failed render included).
    struct Restore
    {
        SampleModel& m;
        std::vector<bool> solo, volOn, userMute;
        bool processed;
        int edits;   // an export changes nothing you'd save
        ~Restore()
        {
            struct Count { SampleModel& m; int n; ~Count() { m.edits = n; } } count { m, edits };
            for (int t = 0; t < (int) solo.size(); ++t)
            {
                if (auto* at = m.track (t)) if (auto* v = at->getVolumePlugin()) v->setEnabled (volOn[(size_t) t]);
                m.setSolo (t, solo[(size_t) t]);
            }
            m.userMute = userMute;
            m.processedOn = processed;
            m.applyMutes();
            m.layoutHitsFor (nullptr);
        }
    } restore { *this, {}, {}, userMute, processedOn, edits };
    for (int t = 0; t < T; ++t)
    {
        restore.solo.push_back (solo (t));
        auto* v = track (t) ? track (t)->getVolumePlugin() : nullptr;
        restore.volOn.push_back (v == nullptr || v->isEnabled());
        if (auto* at = track (t)) { at->setSolo (false); if (v) v->setEnabled (false); }
        if (auto* at = trackA (t)) at->setSolo (false);
    }
    std::fill (userMute.begin(), userMute.end(), false);
    processedOn = true;
    applyMutes();

    // every hit of every mic, with its whole tuned region, in sets that never overlap: a hit joins the first set
    // whose last hit has ended (and whose gain switch, 10.5 ms early, won't touch it) before it starts
    std::vector<std::vector<int>> sets;
    {
        std::vector<double> setEnd;
        for (size_t i = 0; i < R.size(); ++i)
        {
            const double start = (double) R[i].start / sr, endT = start + (double) (R[i].end - R[i].start) / sr / hitRatio (i);
            size_t k = 0;
            while (k < sets.size() && setEnd[k] > start - changeLead() - 0.004) ++k;
            if (k == sets.size()) { sets.emplace_back(); setEnd.push_back (0); }
            sets[k].push_back ((int) i);
            setEnd[k] = endT;
        }
    }
    std::vector<std::vector<hl::Chs>> hitAudio ((size_t) T, std::vector<hl::Chs> (R.size()));
    int step = 0;
    const int steps = (int) sets.size() * T + 1;
    juce::String err;
    for (auto& set : sets)
    {
        layoutHitsFor (&set, srOut);
        for (int t = 0; t < T && err.isEmpty(); ++t)
        {
            if (cancel && cancel()) return exportCancelled();
            const auto whole = renderB (t, srOut, S.tracks[(size_t) t].ch.size() > 1, err);
            if (err.isNotEmpty()) break;
            for (int ii : set)
            {
                const size_t i = (size_t) ii;
                const double stepIn = hitRatio (i) * sr / srOut;
                const long a = (long) std::llround (((double) R[i].start / sr + kPad) * srOut);
                const long n = std::max (1L, (long) std::llround ((double) (R[i].end - R[i].start) / stepIn));
                hl::Chs h;
                for (auto& x : whole)
                {
                    hl::F32 y ((size_t) n, 0.0f);
                    for (long k = 0; k < n && a + k < (long) x.size(); ++k) y[(size_t) k] = x[(size_t) (a + k)];
                    h.push_back (std::move (y));
                }
                hitAudio[(size_t) t][i] = std::move (h);
            }
            if (progress) progress ((float) ++step / (float) steps);
        }
        if (err.isNotEmpty()) return err;
    }
    // the whole-track files (export mode "full" or "both"): the take as it plays
    std::vector<hl::Chs> fullAudio;
    layoutHitsFor (nullptr);
    const auto list = hl::exportList (S);
    const bool wantFull = std::any_of (list.begin(), list.end(), [] (auto& it) { return it.kind == "full"; });
    if (wantFull)
        for (int t = 0; t < T; ++t)
        {
            if (cancel && cancel()) return exportCancelled();
            auto x = renderB (t, srOut, S.tracks[(size_t) t].ch.size() > 1, err);
            if (err.isNotEmpty()) return err;
            const auto pad = (long) std::llround (kPad * srOut);
            for (auto& c : x) c.erase (c.begin(), c.begin() + std::min ((long) c.size(), pad));
            for (auto& c : x) c.resize ((size_t) std::llround ((double) S.tracks[(size_t) t].len / sr * srOut), 0.0f);
            fullAudio.push_back (std::move (x));
        }

    if (cancel && cancel()) return exportCancelled();   // the last chance: from here the files are written, all of them
    // the tails and fades, as the reference export does them
    const long fin = (long) std::llround (S.exp.fadeInMs * srOut / 1000);
    const bool fade = S.exp.tailMode == "fade" && S.exp.mode != "full";
    const bool natural = S.exp.tailMode == "natural" && S.exp.mode != "full";
    const long fout = (long) std::llround ((fade ? 30 : S.exp.fadeOutMs) * srOut / 1000);
    std::vector<long> cuts;
    if (fade)
    {
        const double thr = std::pow (10.0, S.exp.tailDb / 20);
        const long hold = (long) std::llround (0.02 * srOut);
        for (size_t i = 0; i < R.size(); ++i)
        {
            long last = 0;
            for (int t = 0; t < T; ++t)
                for (auto& x : hitAudio[(size_t) t][i])
                    for (long n = (long) x.size() - 1; n > last; --n) if (std::abs (x[(size_t) n]) >= thr) { last = n; break; }
            cuts.push_back (last + hold);
        }
    }
    std::map<size_t, hl::TailPlan> plans;
    auto planFor = [&] (size_t i) -> const hl::TailPlan&
    {
        auto it = plans.find (i);
        if (it != plans.end()) return it->second;
        hl::Chs all;
        for (int t = 0; t < T; ++t) for (auto& x : hitAudio[(size_t) t][i]) all.push_back (x);
        return plans[i] = hl::tailPlan (all, srOut, 0, S.exp.tailFloor);
    };
    for (auto& it : list)
    {
        hl::Chs chs;
        if (it.kind == "hit")
        {
            chs = hitAudio[(size_t) it.t][it.i];
            if (natural) { chs = hl::applyTail (chs, planFor (it.i), srOut); hl::applyFades (chs, fin, 0); }
            else
            {
                if (it.i < cuts.size() && cuts[it.i] != 0 && cuts[it.i] < (long) chs[0].size()) for (auto& x : chs) x.resize ((size_t) cuts[it.i]);
                hl::applyFades (chs, fin, fout);
            }
        }
        else if (it.kind == "full" && it.t < (int) fullAudio.size()) chs = fullAudio[(size_t) it.t];
        else continue;   // the shared room: not in this build
        auto f = dir.getChildFile (juce::String (it.path));
        f.getParentDirectory().createDirectory();
        const auto bytes = hl::encodeWav (chs, srOut, S.exp.bits);
        if (! f.replaceWithData (bytes.getData(), bytes.getSize())) { err = "Couldn't write " + f.getFullPathName(); break; }
        ++count;
    }
    if (progress) progress (1.0f);
    return err;
}

float SampleModel::monDb (int t) const { auto* at = track (t); return at && at->getVolumePlugin() ? at->getVolumePlugin()->getVolumeDb() : 0.0f; }
void SampleModel::setMonDb (int t, float db) { ++edits; monDbTo (t, db); }
void SampleModel::monDbTo (int t, float db)
{
    if (auto* at = track (t)) if (auto* v = at->getVolumePlugin()) v->setVolumeDb (db);
    if (auto* at = trackA (t)) if (auto* v = at->getVolumePlugin()) v->setVolumeDb (db + (float) (levelMatchOn ? matchDb : 0.0));
}
float SampleModel::pan (int t) const { auto* at = track (t); return at && at->getVolumePlugin() ? at->getVolumePlugin()->getPan() : 0.0f; }
void SampleModel::setPan (int t, float p)
{
    ++edits;
    for (auto* at : { track (t), trackA (t) }) if (at) if (auto* v = at->getVolumePlugin()) v->setPan (p);
}
bool SampleModel::solo (int t) const { auto* at = track (t); return at && at->isSolo (false); }
void SampleModel::setSolo (int t, bool b) { ++edits; for (auto* at : { track (t), trackA (t) }) if (at) at->setSolo (b); }
bool SampleModel::mute (int t) const { return t >= 0 && t < (int) userMute.size() && userMute[(size_t) t]; }
void SampleModel::setMute (int t, bool b) { if (t >= 0 && t < (int) userMute.size()) { userMute[(size_t) t] = b; ++edits; applyMutes(); } }

void SampleModel::setLevelMatch (bool on)
{
    levelMatchOn = on;
    if (state) for (int t = 0; t < (int) state->session.tracks.size(); ++t) monDbTo (t, monDb (t));
    if (auto* rt = refTrack()) for (auto* c : rt->getClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) w->setGainDB ((float) (on ? refGainDb : 0.0));
}

std::pair<float, float> SampleModel::meter (int t)
{
    auto* at = processedOn ? track (t) : trackA (t);
    if (! at) return { -100.0f, -100.0f };
    auto* lm = at->getLevelMeterPlugin();
    if (! lm) return { -100.0f, -100.0f };
    auto& m = meters[processedOn ? t : 1000 + t];
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

bool SampleModel::flip (int t) const { return state && t >= 0 && t < (int) state->session.tracks.size() && state->session.tracks[(size_t) t].pol < 0; }
void SampleModel::setFlip (int t, bool f)
{
    // the session is the one place polarity lives (Hits and Align both change it here; export renders it)
    if (flip (t) != f) change ([t] (hl::Session& S) { hl::flipMic (S, t); }, Redo::align);
}
float SampleModel::shiftMs (int t) const { auto* at = track (t); auto* a = at ? at->pluginList.findFirstPluginOfType<live::AlignPlugin>() : nullptr; return a ? a->shiftValue.get() : 0.0f; }

void SampleModel::setProcessed (bool b)
{
    if (refAud) setReferenceAudition (false);
    processedOn = b;
    applyMutes();
}

void SampleModel::play() { if (edit) { settleClips(); edit->getTransport().ensureContextAllocated(); edit->getTransport().play (false); } }
void SampleModel::stop() { if (edit) { edit->getTransport().stop (false, false); settleClips(); } }
bool SampleModel::isPlaying() const { return edit && edit->getTransport().isPlaying(); }
double SampleModel::position() const { return edit ? edit->getTransport().getPosition().inSeconds() - kPad : 0.0; }
void SampleModel::setPosition (double s) { if (edit) edit->getTransport().setPosition (te::TimePosition::fromSeconds (kPad + std::max (0.0, s))); }

void SampleModel::setLoop (double a, double b, bool on)
{
    if (! edit) return;
    auto& tr = edit->getTransport();
    if (b > a) tr.setLoopRange (te::TimeRange (te::TimePosition::fromSeconds (kPad + a), te::TimePosition::fromSeconds (kPad + b)));
    tr.looping = on;
}

bool SampleModel::looping() const { return edit && edit->getTransport().looping.get(); }

} // namespace ld::samples

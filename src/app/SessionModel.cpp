#include "SessionModel.h"
#include "hl/Prep.h"
#include <map>
#include <numeric>
#include <tuple>

namespace ld
{
std::string insertDisplayName (const juce::String& type)
{
    static const std::map<juce::String, std::string> names { { "gate", "PORTCULLIS" }, { "eq", "PRISM" }, { "pultec", "EMBER" }, { "comp", "THE VISE" },
                                                             { "sat", "SKULL CUTTER" }, { "width", "HORIZON" }, { "verb", "ROOM" } };
    return names.count (type) ? names.at (type) : type.toUpperCase().toStdString();
}

juce::var insertDefaults (const juce::String& type)
{
    // each built-in plugin's starting settings (Hitline's INS_TYPES defaults)
    static const std::map<juce::String, const char*> defs {
        { "gate", R"({"thr":-40,"range":-40,"ratio":0,"atk":0.3,"hold":40,"rel":120,"hyst":4,"look":1.5,"hpf":20,"lpf":20000})" },
        { "eq", R"({"phase":"min","out":0,"bands":[]})" },
        { "pultec", R"({"eqp":true,"meq":false,"lf":60,"lfBoost":0,"lfAtten":0,"hf":10,"bw":5,"hfBoost":0,"hfa":20,"hfAtten":0,"lpf":300,"lpk":0,"dipf":1000,"dip":0,"hpf":3,"hpk":0,"tube":true,"drive":1,"phase":"min","out":0})" },
        { "comp", R"({"model":"dist","mix":1,"scHpf":20,"out":0,"dist":{"input":5,"ratio":"4","attack":5,"release":5,"dist":"","hp":false},"76":{"input":6.2,"ratio":"4","attack":3,"release":3},"fair":{"input":0,"thresh":5,"tc":2},"160":{"thresh":-27,"ratio":4,"overEasy":false},"chan":{"thresh":-24,"ratio":3,"knee":6,"attack":10,"release":120}})" },
        { "sat", R"({"model":"decap","mix":1,"out":0,"auto":true,"decap":{"drive":3,"style":"A","tone":0,"lowcut":0,"highcut":0,"thump":false,"steep":false,"punish":false},"devil":{"crush":4,"darkness":2,"lowcut":false},"saturn":{"style":"gentle","drive":0,"tone":0},"fatso":{"input":0,"warmth":0,"tranny":true,"comp":"off"}})" },
        { "width", R"({"width":1,"bassMono":120,"keep":true})" } };
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", type); o->setProperty ("on", true);
    o->setProperty ("p", juce::JSON::parse (defs.count (type) ? juce::String (defs.at (type)) : juce::String ("{}")));
    return juce::var (o);
}

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

ui::StudioModel buildStudioModel (te::Edit& edit, const std::vector<MicRole>& roles, const juce::String& title, std::shared_ptr<const EngineState> esp)
{
    const EngineState* es = esp.get();
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
        auto wave = std::make_shared<ui::Wave>();
        if (map[i] >= 0)
        {
            // Hitline's own mic name ("Room Close", not the file's "RmCls.cm6"), and its colour from that name
            const auto& tr = S->tracks[(size_t) map[i]];
            mono = tr.mono; info.stereo = tr.ch.size() > 1;
            wave->a = std::shared_ptr<const ui::Wave::Chans> (esp, &tr.ch);
            wave->sampleRate = S->sr;
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
            auto chans = std::make_shared<ui::Wave::Chans>();
            for (int ch = 0; ch < raw.getNumChannels(); ++ch) chans->emplace_back (raw.getReadPointer (ch), raw.getReadPointer (ch) + raw.getNumSamples());
            wave->a = chans; wave->sampleRate = sr;
            mono.assign ((size_t) raw.getNumSamples(), 0.0f);
            for (int ch = 0; ch < raw.getNumChannels(); ++ch)
                for (int n = 0; n < raw.getNumSamples(); ++n) mono[(size_t) n] += raw.getSample (ch, n) / (float) raw.getNumChannels();
        }
        const int block = std::max (1, (int) (sr / 750.0));
        info.blocksPerSecond = sr / block;
        const size_t nb = mono.size() / (size_t) block;
        info.peaks.resize (nb);
        for (size_t b = 0; b < nb; ++b)
        {
            float pk = 0;
            for (size_t n = b * (size_t) block, e = n + (size_t) block; n < e; ++n) pk = std::max (pk, std::abs (mono[n]));
            info.peaks[b] = pk;
        }
        const bool leveled = map[i] >= 0 && es->mix && es->levels && es->levels->valid;
        if (leveled)
        {
            // what gets exported (Hitline's B view): the processed mic, at its alignment, each hit at its Level gain
            const int t = map[i];
            const auto& L = *es->levels;
            const auto& proc = es->mix->proc[(size_t) t];
            const long lagInt = (long) std::lround (t == S->refIdx ? 0.0 : S->tracks[(size_t) t].lag);
            const double trim = std::pow (10.0, L.trimDb / 20);
            info.peaksB.assign (nb, 0.0f);
            std::vector<float> gk; for (double g : L.gainsDbT[(size_t) t]) gk.push_back ((float) (std::pow (10.0, g / 20) * trim));
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
                    pk = std::max (pk, a * gk[k]);
                }
                info.peaksB[b] = pk;
            }
            wave->b = std::shared_ptr<const ui::Wave::Chans> (esp, &proc);
            wave->lagB = lagInt;
            for (auto& r : L.regions) wave->regionStart.push_back (r.start);
            wave->gainB = gk;
        }
        {
            // the pyramids take a moment on a long take: keep them while the audio behind them is the same
            static std::map<std::tuple<const void*, const void*, float, long>, std::weak_ptr<ui::Wave>> cache;
            const auto key = std::make_tuple ((const void*) wave->a.get(), (const void*) wave->b.get(),
                                              wave->gainB.empty() ? 0.0f : std::accumulate (wave->gainB.begin(), wave->gainB.end(), 0.0f), wave->lagB);
            if (auto hit = cache[key].lock()) info.wave = hit;
            else
            {
                wave->build();
                cache[key] = wave;
                info.wave = wave;
            }
            for (auto it = cache.begin(); it != cache.end();) it = it->second.expired() ? cache.erase (it) : std::next (it);
        }
        m.length = std::max (m.length, (double) mono.size() / sr);
        m.tracks.push_back (std::move (info));
    }
    if (! S || S->onsets.empty()) return m;

    for (size_t i = 0; i < map.size(); ++i) if (map[i] == S->refIdx) m.referenceTrack = (int) i;

    // the hits, each on its velocity layer and round robin (only meaningful when the count matches)
    const bool ok = hl::countsOk (*S);
    for (size_t k = 0; k < S->onsets.size(); ++k)
    {
        ui::HitInfo hi { (double) S->onsets[k] / sr, ok ? S->velOfHit (k) : 0, ok ? (int) (k % (size_t) S->det.RR) : (int) k, {} };
        const auto* Lv = es && es->levels && es->levels->valid ? &*es->levels : nullptr;
        if (Lv && ok)
            for (size_t i = 0; i < map.size(); ++i)
                hi.micDb.push_back (map[i] >= 0 && k < Lv->lvlRaw[(size_t) map[i]].size()
                                        ? (float) (Lv->lvlRaw[(size_t) map[i]][k] + Lv->gainsDbT[(size_t) map[i]][k]) : NAN);
        m.hits.push_back (std::move (hi));
    }

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
        {
            auto& lv = m.level;
            lv.valid = L != nullptr;
            lv.linked = S->lev.mode == "linked"; lv.pro = S->lev.steps == "pro";
            lv.shape = (float) S->lev.shape; lv.offsetDb = (float) S->lev.offsetDb; lv.rangeDb = (float) S->lev.rangeDb; lv.winMs = (float) S->lev.winMs;
            lv.total = (int) S->onsets.size();
            lv.onStandard = L ? (int) S->onsets.size() : 0;
        }
        // layers are listed from the first hit in the take
        std::sort (m.layers.begin(), m.layers.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        if (m.level.valid)
        {
            std::vector<std::pair<int, float>> byVel;
            for (size_t k = 0; k < m.layers.size(); ++k) byVel.push_back ({ (int) k, m.layers[k].micDb.size() > (size_t) m.referenceTrack ? m.layers[k].micDb[(size_t) m.referenceTrack] : NAN });
            for (auto& [k, db] : byVel) m.level.curveDb.push_back (db);
        }
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

ui::StudioModel::AlignView buildAlignView (const EngineState& es, int mic, int hit, bool spread, bool samples)
{
    ui::StudioModel::AlignView v;
    const auto& S = es.session;
    v.timing = S.timing; v.spread = spread; v.samples = samples; v.sr = (float) S.sr;
    if (S.tracks.empty() || ! S.phaseAnalyzed || S.onsets.empty()) return v;
    v.valid = true;
    const int nT = (int) S.tracks.size();
    const double sr = S.sr;
    double sb = 0, sa = 0; int ns = 0;
    for (int t = 0; t < nT; ++t)
    {
        const auto& tr = S.tracks[(size_t) t];
        ui::StudioModel::AlignView::Mic m;
        m.name = tr.mic;
        m.delayMs = (float) (tr.lag / sr * 1000); m.delaySmp = (float) tr.lag;
        m.flipped = tr.pol < 0; m.eq = tr.apOn && ! tr.ap.empty(); m.lr = tr.lrLock; m.lrAvail = tr.ch.size() == 2 && tr.lr.has_value();
        if (t == S.refIdx) { m.status = "lead"; m.to = "Everything lines up to this mic"; v.mics.push_back (m); continue; }
        m.changed = std::abs (tr.lag) > 0.01 || tr.pol < 0 || ! tr.ap.empty();
        m.status = tr.manualLag || tr.manualPol ? "manual" : m.changed ? "corrected" : "inphase";
        const auto& lead = S.tracks[(size_t) S.refIdx].mic;
        m.to = tr.alignTo == "lead" ? lead + " (lead)" : tr.alignTo == "pro" ? std::string ("Standard phase") : tr.alignTo == "none" ? std::string ("As recorded") : "Group " + tr.alignTo;
        const auto& res = tr.alignTo.size() == 1 ? tr.grp : tr.autoResult;
        m.eqAvail = res.has_value() && ! res->ap.empty();
        if (res && res->valid)
        {
            m.before = (float) res->asis; m.after = (float) res->score;
            for (int b = 0; b < 8; ++b) { m.bandsBefore[(size_t) b] = (float) res->bandsBefore[(size_t) b]; m.bandsAfter[(size_t) b] = (float) res->bandsAfter[(size_t) b]; }
            if (tr.manualLag || tr.manualPol || tr.alignTo == "pro" || tr.alignTo == "none")
            {
                // what it sums like where it sits now
                const int r = hl::alignRef (S, t) >= 0 ? hl::alignRef (S, t) : S.refIdx;
                std::vector<int> ons (S.onsets.begin(), S.onsets.end());
                const auto sc = hl::scoreMic (S.tracks[(size_t) r].mono, tr.ch, ons, sr, tr.lag, tr.pol, tr.apOn ? tr.ap : std::vector<hl::AllPass> {}, res->arrival);
                if (sc.valid) { m.after = (float) sc.score; for (int b = 0; b < 8; ++b) m.bandsAfter[(size_t) b] = (float) sc.bandsAfter[(size_t) b]; }
            }
            sb += m.before; sa += m.after; ++ns;
            if (m.before < 0) ++v.fightingBefore;
            if (m.after < 0) ++v.fightingAfter;
        }
        if (m.eq)
        {
            m.eqText = "all-pass at";
            for (size_t k = 0; k < tr.ap.size(); ++k) m.eqText += (k ? " + " : " ") + std::to_string ((int) std::lround (tr.ap[k].fc));
            m.eqText += " Hz, tone unchanged";
        }
        v.mics.push_back (m);
    }
    if (ns) { v.sumBefore = (float) (sb / ns); v.sumAfter = (float) (sa / ns); }

    // every row: the track over what it lines up to, around the selected hit
    const int hsel = std::clamp (hit < 0 ? (int) S.onsets.size() / 2 : hit, 0, (int) S.onsets.size() - 1);
    v.hit = hsel;
    {
        auto at = [] (const hl::F32& x, double p) { const long i = (long) std::floor (p); const double f = p - i; auto g = [&] (long q) { return q >= 0 && q < (long) x.size() ? (double) x[(size_t) q] : 0.0; }; return (float) (g (i) * (1 - f) + g (i + 1) * f); };
        const double on = (double) S.onsets[(size_t) hsel], pre = 0.0015 * sr;
        for (int t = 0; t < nT; ++t)
        {
            auto& m = v.mics[(size_t) t];
            const auto& M = S.tracks[(size_t) t];
            const int r = t == S.refIdx ? t : (hl::alignRef (S, t) >= 0 ? hl::alignRef (S, t) : S.refIdx);
            const auto& R = S.tracks[(size_t) r];
            const bool far = hl::roleOf (M.mic + " " + M.name) == "far";
            const double arr = M.autoResult && std::isfinite (M.autoResult->arrival) ? M.autoResult->arrival : 0;
            const double span = far ? std::max (0.014 * sr, arr + 0.01 * sr) : 0.008 * sr;
            m.spanMs = (float) ((span + pre) / sr * 1000);
            const double lagR = r == S.refIdx ? 0 : R.lag; const int pR = R.pol ? R.pol : 1, pM = M.pol ? M.pol : 1;
            const int N = 240;
            for (int n = 0; n < N; ++n)
            {
                const double p = on - pre + n * (span + pre) / (N - 1);
                m.ref.push_back (at (R.mono, p + lagR) * pR);
                m.cur.push_back (t == S.refIdx ? at (M.mono, p) * pM : at (M.mono, p + M.lag) * pM);
                m.rec.push_back (at (M.mono, p));
            }
        }
    }

    // the scope
    mic = std::clamp (mic, 0, nT - 1);
    if (mic == S.refIdx) for (int t = 0; t < nT; ++t) if (t != S.refIdx) { mic = t; break; }
    if (mic == S.refIdx) return v;
    v.mic = mic;
    hit = std::clamp (hit < 0 ? (int) S.onsets.size() / 2 : hit, 0, (int) S.onsets.size() - 1);
    const auto H = hl::overlayHeat (S, mic, hit, spread);
    const int r = H.ref >= 0 ? H.ref : S.refIdx;
    const auto& R = S.tracks[(size_t) r]; const auto& M = S.tracks[(size_t) mic];
    const double pre = 0.0015 * sr, span = H.far ? std::max (0.014 * sr, (M.autoResult && std::isfinite (M.autoResult->arrival) ? M.autoResult->arrival : 0) + 0.01 * sr) : 0.008 * sr;
    v.preMs = 1.5f; v.spanMs = (float) (span / sr * 1000);
    const double on = (double) S.onsets[(size_t) hit];
    const int N = 400;
    auto at = [] (const hl::F32& x, double p) { const long i = (long) std::floor (p); const double f = p - i; auto g = [&] (long q) { return q >= 0 && q < (long) x.size() ? (double) x[(size_t) q] : 0.0; }; return (float) (g (i) * (1 - f) + g (i + 1) * f); };
    const double lagR = r == S.refIdx ? 0 : R.lag; const int pR = R.pol ? R.pol : 1, pM = M.pol ? M.pol : 1;
    for (int n = 0; n < N; ++n)
    {
        const double p = on - pre + n * (span + pre) / (N - 1);
        v.ref.push_back (at (R.mono, p + lagR) * pR);
        v.cur.push_back (at (M.mono, p + M.lag) * pM);
        v.rec.push_back (at (M.mono, p));
    }
    v.preMs = (float) (pre / sr * 1000); v.spanMs = (float) ((span + pre) / sr * 1000);
    for (auto& [l, c] : H.curve) v.heat.push_back ((float) c);
    if (! H.curve.empty()) v.heatFrom = (float) H.curve.front().first;
    v.best = (float) H.best; v.bestHeat = (float) H.bestC; v.curHeat = (float) H.curC; v.lag = (float) M.lag;
    return v;
}

ui::StudioModel::TuneView buildTuneView (const EngineState& es, int hit)
{
    ui::StudioModel::TuneView v;
    const auto& S = es.session;
    if (S.tracks.empty()) return v;
    v.tunable = true;   // anything can be tuned; the advice says when it shouldn't be
    v.advice = hl::tuneAdvice (S); v.anyway = S.tuneAnyway;
    v.on = S.tune.on; v.rr = S.tune.mode == "rr"; v.cents = (float) S.tune.cents; v.a4 = (float) S.tune.a4;
    v.fmin = (float) S.pitchOpt.fmin; v.fmax = (float) S.pitchOpt.fmax; v.skipMs = (float) S.pitchOpt.skipMs;
    const int nT = (int) S.tracks.size();
    const int ft = S.tuneMic == "ref" ? S.refIdx : std::clamp (juce::String (S.tuneMic).getIntValue(), 0, nT - 1);
    v.from = S.tracks[(size_t) ft].mic + (ft == S.refIdx ? " (lead)" : "");
    v.read = S.tuneInfo.has_value() && S.tuneInfo->valid;
    if (S.tune.target == "nearest") v.target = "Nearest";
    else v.target = hl::noteName (juce::String (S.tune.target).getDoubleValue());
    if (! v.read) return v;
    const auto& ti = *S.tuneInfo;
    v.medF = (float) ti.medF;
    v.medCents = (float) (100 * (ti.medMidi - ti.nearest));
    v.sitsAt = hl::noteName (ti.nearest);
    v.tunedTo = hl::noteName (ti.targetMidi) + (S.tune.cents != 0 ? juce::String::formatted (" %+d", (int) std::lround (S.tune.cents)).toStdString() : std::string());
    if (S.tune.target == "nearest") v.target = "Nearest \xc2\xb7 " + hl::noteName (ti.nearest);
    std::vector<double> rs; for (double r : ti.ratios) if (std::isfinite (r)) rs.push_back (r);
    if (! rs.empty()) { std::sort (rs.begin(), rs.end()); v.ratio = (float) rs[rs.size() / 2]; v.shiftSemis = (float) (12 * std::log2 (v.ratio)); }
    const bool ok = hl::countsOk (S);
    v.layers = ok ? S.det.V : 0;
    for (size_t i = 0; i < ti.before.size(); ++i)
    {
        v.before.push_back ((float) ti.before[i]); v.after.push_back ((float) ti.after[i]);
        v.layer.push_back (ok ? S.velOfHit (i) : 0);
    }
    for (double f : ti.layerMed) v.layerHz.push_back ((float) f);
    // the widest spread inside one velocity, before and now; and the layer furthest from the drum
    if (ok)
    {
        float sb = 0, sa = 0;
        for (int L = 0; L < S.det.V; ++L)
        {
            float b0 = 1e9f, b1 = -1e9f, a0 = 1e9f, a1 = -1e9f;
            for (size_t i = 0; i < v.before.size(); ++i)
                if (v.layer[i] == L && std::isfinite (v.before[i]) && std::isfinite (v.after[i]))
                { b0 = std::min (b0, v.before[i]); b1 = std::max (b1, v.before[i]); a0 = std::min (a0, v.after[i]); a1 = std::max (a1, v.after[i]); }
            if (b1 >= b0) { sb = std::max (sb, b1 - b0); sa = std::max (sa, a1 - a0); }
            if ((size_t) L < ti.layerMed.size() && std::isfinite (ti.layerMed[(size_t) L]))
            {
                const float c = (float) (1200 * std::log2 (ti.layerMed[(size_t) L] / ti.medF));
                if (! std::isfinite (v.worstCents) || std::abs (c) > std::abs (v.worstCents)) { v.worstCents = c; v.worstLayer = L; }
            }
        }
        v.spreadBefore = sb; v.spreadAfter = sa;
    }
    // the selected hit
    if (! ti.f.empty())
    {
        hit = std::clamp (hit < 0 ? (int) ti.f.size() / 2 : hit, 0, (int) ti.f.size() - 1);
        v.hit = hit;
        if (std::isfinite (ti.f[(size_t) hit]) && ti.f[(size_t) hit] > 0)
        {
            v.hitF = (float) ti.f[(size_t) hit];
            const double r = (size_t) hit < ti.ratios.size() && std::isfinite (ti.ratios[(size_t) hit]) ? ti.ratios[(size_t) hit] : 1;
            v.hitTunedF = (float) (ti.f[(size_t) hit] * r);
            const double m = hl::hzToMidi (v.hitTunedF, S.tune.a4);
            v.hitNote = hl::noteName (std::round (m)); v.hitCents = (float) (100 * (m - std::round (m)));
            const double mb = hl::hzToMidi (v.hitF, S.tune.a4); v.hitCentsBefore = (float) (100 * (mb - std::round (mb)));
        }
    }
    return v;
}

ui::StudioModel::MixView buildMixView (const EngineState& es, int sel, int hit, const hl::Chs* refAudio, const std::string& refName)
{
    ui::StudioModel::MixView v;
    const auto& S = es.session;
    if (S.tracks.empty() || ! es.mix || S.onsets.empty()) return v;
    const auto& M = *es.mix;
    const int nT = (int) S.tracks.size();
    v.valid = true;
    v.sel = sel >= 0 && sel < nT ? sel : -1;
    v.selName = v.sel < 0 ? std::string ("Bus") : S.tracks[(size_t) v.sel].mic;
    v.studioAvail = hl::studioReferenceAvailable(); v.studio = S.tone.proSrc == "panda";
    v.cymbal = hl::soundsLikeCymbal (S) || ! (S.drum == "kick" || S.drum == "snare" || S.drum == "tom");
    const hl::ToneMoves& T = v.sel < 0 ? (const hl::ToneMoves&) S.tone : S.tracks[(size_t) v.sel].strip.tone;
    const hl::ShapeMoves& H = v.sel < 0 ? S.shape : S.tracks[(size_t) v.sel].strip.shape;
    v.tone = { (float) T.match, (float) T.bright, (float) T.crack, (float) T.smooth, (float) T.tame };
    v.shape = { (float) H.atk, (float) H.dec, (float) H.sus, (float) H.rel, (float) H.match };
    v.fxOn = hl::fxOn (S);
    // the hit, before (the recording, lined up) and after (the chain), on the channel or every channel summed
    hit = std::clamp (hit < 0 ? (int) S.onsets.size() - 1 : hit, 0, (int) S.onsets.size() - 1);
    const double sr = S.sr;
    const long pre = (long) hl::jsRound (S.det.prerollMs * sr / 1000), len = (long) hl::jsRound (0.4 * sr);
    hl::F32 a ((size_t) (len + pre), 0.0f), b ((size_t) (len + pre), 0.0f);
    for (int t = 0; t < nT; ++t)
    {
        if (v.sel >= 0 && t != v.sel) continue;
        const auto& tr = S.tracks[(size_t) t];
        const int pol = tr.pol ? tr.pol : 1;
        for (size_t c = 0; c < tr.ch.size(); ++c)
        {
            const double g = 1.0 / (double) tr.ch.size();
            const auto x = hl::renderSeg (tr.ch[c], (long) S.onsets[(size_t) hit] - pre, len + pre, tr.lag, pol, g, true);
            for (size_t q = 0; q < a.size() && q < x.size(); ++q) a[q] += x[q];
            if ((size_t) t < M.proc.size() && c < M.proc[(size_t) t].size())
            {
                const auto y = hl::renderSeg (M.proc[(size_t) t][c], (long) S.onsets[(size_t) hit] - pre, len + pre, tr.lag, pol, g, true);
                for (size_t q = 0; q < b.size() && q < y.size(); ++q) b[q] += y[q];
            }
        }
    }
    const auto sa = hl::bandSpec (a, pre, sr), sb = hl::bandSpec (b, pre, sr);
    for (double x : sa) v.specBefore.push_back ((float) x);
    for (double x : sb) v.specAfter.push_back ((float) x);
    v.hasRef = S.tone.hasRef; v.refAmt = (float) S.tone.refAmt; v.refName = refName;
    if (S.tone.hasRef) for (double x : S.tone.refCurve) v.refDelta.push_back ((float) x);
    // the envelope: 2 ms RMS, in dB against the recording's peak
    auto env = [&] (const hl::F32& x)
    {
        std::vector<float> e; const long w = (long) hl::jsRound (0.002 * sr);
        for (long s0 = 0; s0 + w <= (long) x.size(); s0 += w) { double q = 0; for (long i = s0; i < s0 + w; ++i) q += (double) x[(size_t) i] * x[(size_t) i]; e.push_back ((float) (10 * std::log10 (q / w + 1e-20))); }
        return e;
    };
    v.envBefore = env (a); v.envAfter = env (b);
    float pk = -200; for (float x : v.envBefore) pk = std::max (pk, x);
    for (auto& x : v.envBefore) x -= pk;
    for (auto& x : v.envAfter) x -= pk;
    if (refAudio && ! refAudio->empty())
    {
        v.refAudio = true;
        const auto& r = (*refAudio)[0];
        const size_t N = 120, step = std::max<size_t> (1, r.size() / N);
        for (size_t k = 0; k + step <= r.size() && v.refWave.size() < N; k += step) { float m2 = 0; for (size_t q = k; q < k + step; ++q) m2 = std::max (m2, std::abs (r[q])); v.refWave.push_back (m2); }
    }
    // the insert slots: the selected channel's, and every strip's for the mixer (the built-in plugins by name)
    auto slotsOf = [] (const juce::var& ins)
    {
        std::array<ui::StudioModel::MixView::Ins, 5> out {};
        if (auto* arr = ins.getArray())
            for (int k = 0; k < std::min (5, arr->size()); ++k)
            {
                const auto& x = arr->getReference (k);
                out[(size_t) k] = { insertDisplayName (x["type"].toString()), ! x.hasProperty ("on") || (bool) x["on"] };
            }
        return out;
    };
    for (auto& in : slotsOf (v.sel < 0 ? S.busIns : S.tracks[(size_t) v.sel].strip.inserts)) if (! in.name.empty()) v.inserts.push_back (in);
    for (auto& tr : S.tracks) v.chans.push_back ({ slotsOf (tr.strip.inserts), (float) tr.strip.r1, (float) tr.strip.r2 });
    v.bus = { slotsOf (S.busIns), (float) S.busR1, (float) S.busR2 };
    v.busFader = (float) S.busFader; v.busMute = S.busMute;
    v.hasRefEnv = S.tone.hasRefEnv; v.refEnvAmt = (float) S.tone.refEnvAmt;
    for (int k = 0; k < 2; ++k)
    {
        const auto& u = S.verbs[(size_t) k];
        v.verbs[(size_t) k] = { u.type, (float) u.decay, (float) u.pre, (float) u.size, (float) u.tone, (float) u.width };
    }
    return v;
}

ui::StudioModel::SongView buildSongView (const EngineState& es)
{
    ui::StudioModel::SongView v;
    const auto& S = es.session;
    if (S.mode != "kit" || ! S.song.an.done) return v;
    const auto& A = S.song.an; const auto& set = S.song.set;
    v.valid = true;
    v.beats = hl::gridBeats (A, set);
    for (double b : hl::bpmOf (v.beats)) v.bpm.push_back ((float) b);
    if (! v.bpm.empty()) { double a = 0; for (float b : v.bpm) a += b; v.avgBpm = (float) (a / v.bpm.size()); }
    v.downbeat = set.bpmScale > 1.5 ? A.downbeat * 2 % 4 : set.bpmScale < 0.75 ? A.downbeat / 2 : A.downbeat;
    const auto E = hl::planEdits (A, set);
    double mv = 0;
    for (auto& h : A.hits)
    {
        ui::StudioModel::SongView::Hit x;
        x.t = h.pos / S.sr; x.moved = hl::movedPos (h.pos, E) / S.sr; x.drum = (int) h.drum; x.vel = h.vel; x.ghost = h.ghost; x.flam = h.flam;
        if (std::abs (x.moved - x.t) > 0.0002) { ++v.moved; mv += std::abs (x.moved - x.t); }
        v.counts[std::clamp (x.drum, 0, 5)]++;
        v.hits.push_back (x);
    }
    v.avgMoveMs = v.moved ? (float) (mv / v.moved * 1000) : 0;
    for (auto& p : hl::findProblems (A, set)) v.problems.push_back ({ p.pos / S.sr, p.kind, p.text });
    v.tempoMode = set.tempoMode; v.tightness = (float) set.tightness; v.swing = (float) set.swing; v.keepFeel = set.keepFeel; v.hatsOnly = set.swingHatsOnly;
    v.division = set.division; v.bpmScale = (float) set.bpmScale; v.kitBleed = (float) S.kitBleed;
    return v;
}

} // namespace ld

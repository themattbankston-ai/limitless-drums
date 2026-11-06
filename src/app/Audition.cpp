#include "Audition.h"
#include <random>

namespace ld
{
namespace
{
std::mt19937& rng() { static std::mt19937 g { std::random_device {}() }; return g; }
double uni() { return std::uniform_real_distribution<double> (0, 1) (rng()); }
bool okCounts (const hl::Session& S) { return hl::countsOk (S); }
int hitAt (const hl::Session& S, int v, int r)
{
    for (size_t k = 0; k < S.onsets.size(); ++k) if (S.velOfHit (k) == v && (int) (k % (size_t) S.det.RR) == r) return (int) k;
    return -1;
}
/** Every round robin of a layer once, in a new random order, never the same one twice in a row. */
std::vector<int> rrShuffle (int n, int avoid)
{
    std::vector<int> a ((size_t) n);
    for (int k = 0; k < n; ++k) a[(size_t) k] = k;
    for (int k = n - 1; k > 0; --k) std::swap (a[(size_t) k], a[(size_t) (int) std::floor (uni() * (k + 1))]);
    if (n > 1 && a[0] == avoid) std::swap (a[0], a[(size_t) (1 + (int) std::floor (uni() * (n - 1)))]);
    return a;
}
}

namespace modes
{
std::vector<PlayEvent> hit (const hl::Session& S, int sel)
{
    if (sel < 0 || sel >= (int) S.onsets.size()) return {};
    return { { 0, sel, okCounts (S) ? S.velOfHit ((size_t) sel) : 0 } };
}

std::vector<PlayEvent> layer (const hl::Session& S, int sel)
{
    if (! okCounts (S) || sel < 0) return {};
    const int v = S.velOfHit ((size_t) sel);
    std::vector<PlayEvent> e;
    for (size_t i = 0; i < S.onsets.size(); ++i) if (S.velOfHit (i) == v) e.push_back ({ 0.5 * (double) e.size(), (int) i, v });
    return e;
}

std::vector<PlayEvent> sweep (const hl::Session& S, int sel)
{
    if (! okCounts (S)) return {};
    const int rr = sel >= 0 ? sel % S.det.RR : 0;
    std::vector<PlayEvent> e;
    for (int v = 0; v < S.det.V; ++v) { const int i = hitAt (S, v, rr); if (i >= 0) e.push_back ({ 0.5 * (double) e.size(), i, v }); }
    return e;
}

std::vector<PlayEvent> groove (const hl::Session& S, double bpm, double rbpm)
{
    if (! okCounts (S)) return {};
    const int V = S.det.V, RR = S.det.RR, top = V - 1;
    std::vector<PlayEvent> ev;
    // the groove (Hitline's grooveEvents)
    {
        const double q = 60 / bpm, s16 = q / 4;
        double t = 0; int prev = -1; std::vector<int> bag;
        for (int n = 0; n < 8; ++n)
        {
            if (bag.empty()) bag = rrShuffle (RR, prev);
            const int r = bag.front(); bag.erase (bag.begin()); prev = r;
            if (const int i = hitAt (S, top, r); i >= 0) ev.push_back ({ t, i, top });
            t += q;
        }
        prev = -1;
        for (int v = 0; v < V; ++v)
            for (int r : (bag = rrShuffle (RR, prev))) { prev = r; if (const int i = hitAt (S, v, r); i >= 0) ev.push_back ({ t, i, v }); t += s16; }
    }
    (void) rbpm;
    return ev;
}

std::vector<PlayEvent> rudiments (const hl::Session& S, double rbpm)
{
    if (! okCounts (S)) return {};
    const int V = S.det.V, RR = S.det.RR;
    std::vector<PlayEvent> ev;
    // the rudiment build (Hitline's rudimentEvents)
    {
        const double q = 60 / rbpm, start = 0;
        std::map<int, int> last;
        std::vector<PlayEvent> ru;
        auto jitter = [] (double a) { return (uni() + uni() + uni() - 1.5) * a; };
        auto note = [&] (double t, double vel, char hand)
        {
            const int v = std::clamp ((int) std::floor (std::clamp (vel + jitter (0.05) + (hand == 'L' ? -0.03 : 0), 0.0, 1.0) * (V - 1) + 0.5), 0, V - 1);
            int r;
            do { r = (int) std::floor (uni() * RR); } while (RR > 1 && last.count (v) && r == last[v]);
            last[v] = r;
            if (const int i = hitAt (S, v, r); i >= 0) ru.push_back ({ std::max (0.0, t + jitter (0.004)), i, v });
        };
        double t = 0;
        const char pd[] = { 'R', 'L', 'R', 'R', 'L', 'R', 'L', 'L' };
        for (int n = 0; n < 16; ++n) { note (t, n % 4 == 0 ? 0.88 : 0.34, pd[n % 8]); t += q / 4; }
        const double bb[] = { 0.18, 0.12, 0.2, 0.14, 1, 0.12, 0.22, 0.16, 0.2, 0.14, 0.24, 0.12, 1, 0.14, 0.3, 0.42 };
        for (int n = 0; n < 16; ++n) { note (t, bb[n], n % 2 ? 'L' : 'R'); t += q / 4; }
        for (int n = 0; n < 24; ++n) { note (t, 0.15 + 0.75 * n / 23 + (n % 3 == 0 ? 0.06 : 0), n % 2 ? 'L' : 'R'); t += q / 6; }
        for (int n = 0; n < 2; ++n) { note (t - 0.024, 0.28, 'L'); note (t, 0.92, 'R'); t += q; }
        for (int n = 0; n < 16; ++n) { note (t, 0.3 + 0.68 * n / 15, n % 2 ? 'L' : 'R'); t += q / 8; }
        note (t - 0.022, 0.32, 'L'); note (t, 1, 'R');
        std::stable_sort (ru.begin(), ru.end(), [] (auto& a, auto& b) { return a.at < b.at; });
        for (auto& e : ru) { e.at += start; ev.push_back (e); }
    }
    return ev;
}

std::vector<MidiNote> parseMidi (const juce::MemoryBlock& mb, juce::String& error)
{
    const auto* d = static_cast<const uint8_t*> (mb.getData());
    const size_t N = mb.getSize();
    size_t p = 0;
    auto need = [&] (size_t n) { if (p + n > N) throw std::runtime_error ("That MIDI file is cut short."); };
    auto str = [&] (int n) { need ((size_t) n); std::string s ((const char*) d + p, (size_t) n); p += (size_t) n; return s; };
    auto u32 = [&] { need (4); const uint32_t v = (uint32_t) d[p] << 24 | (uint32_t) d[p + 1] << 16 | (uint32_t) d[p + 2] << 8 | d[p + 3]; p += 4; return v; };
    auto u16 = [&] { need (2); const int v = d[p] << 8 | d[p + 1]; p += 2; return v; };
    std::vector<MidiNote> out;
    try
    {
        if (str (4) != "MThd") throw std::runtime_error ("That is not a MIDI file.");
        const uint32_t hl = u32(); u16(); const int ntr = u16(), div = u16(); p += hl - 6;
        if (div & 0x8000) throw std::runtime_error ("SMPTE-timed MIDI files are not supported.");
        struct Note { long tick; int note, vel; };
        std::vector<Note> notes;
        std::vector<std::pair<long, long>> tempos { { 0, 500000 } };
        for (int tr = 0; tr < ntr && p < N; ++tr)
        {
            if (str (4) != "MTrk") break;
            const uint32_t len = u32();
            const size_t end = std::min (N, p + len);
            long tick = 0; int run = 0;
            auto vlq = [&] { long v = 0; int b; do { need (1); b = d[p++]; v = (v << 7) | (b & 127); } while (b & 128); return v; };
            while (p < end)
            {
                tick += vlq();
                need (1);
                int st = d[p];
                if (st < 0x80) st = run; else { ++p; if (st < 0xF0) run = st; }
                if (st == 0xFF) { need (1); const int type = d[p++]; const long l = vlq(); if (type == 0x51 && l == 3) { need (3); tempos.push_back ({ tick, (long) (d[p] << 16 | d[p + 1] << 8 | d[p + 2]) }); } p += (size_t) l; }
                else if (st == 0xF0 || st == 0xF7) p += (size_t) vlq();
                else { const int hi = st & 0xF0; need (1); const int a = d[p++]; int b = 0; if (hi != 0xC0 && hi != 0xD0) { need (1); b = d[p++]; } if (hi == 0x90 && b > 0) notes.push_back ({ tick, a, b }); }
            }
            p = end;
        }
        std::stable_sort (tempos.begin(), tempos.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        auto secOf = [&] (long tick)
        {
            double s = 0; long lt = 0, us = 500000;
            for (auto [tk, tm] : tempos) { if (tk >= tick) break; s += (double) (tk - lt) * us / div / 1e6; lt = tk; us = tm; }
            return s + (double) (tick - lt) * us / div / 1e6;
        };
        for (auto& n : notes) out.push_back ({ secOf (n.tick), n.note, n.vel });
        std::stable_sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.at < b.at; });
        if (out.empty()) error = "No notes in that MIDI file.";
    }
    catch (const std::exception& e) { error = e.what(); out.clear(); }
    return out;
}

std::vector<PlayEvent> midi (const hl::Session& S, const std::vector<MidiNote>& notes)
{
    if (! okCounts (S) || notes.empty()) return {};
    const int V = S.det.V, RR = S.det.RR;
    std::map<int, int> last;
    std::vector<PlayEvent> ev;
    const double t0 = notes.front().at;
    for (auto& n : notes)
    {
        int v = -1;
        for (int z = 0; z < V; ++z) { const int lo = (int) std::floor (z * 127.0 / V) + 1, hi = (int) std::floor ((z + 1) * 127.0 / V); if (n.vel >= lo && n.vel <= hi) { v = z; break; } }
        if (v < 0) v = n.vel > 64 ? V - 1 : 0;
        int r;
        do { r = (int) std::floor (uni() * RR); } while (RR > 1 && last.count (v) && r == last[v]);
        last[v] = r;
        if (const int i = hitAt (S, v, r); i >= 0) ev.push_back ({ n.at - t0, i, v });
    }
    return ev;
}
} // namespace modes

//==============================================================================
Audition::Audition (juce::AudioDeviceManager& d) : dm (d)
{
    for (int k = 0; k < maxTracks; ++k) { tGain[k] = 1; tPan[k] = 0; tPanL[k] = -1; tPanR[k] = 1; tStereo[k] = false; }
    dm.addAudioCallback (this);
}

Audition::~Audition()
{
    stop();
    dm.removeAudioCallback (this);
}

void Audition::audioDeviceAboutToStart (juce::AudioIODevice* dev) { deviceRate = dev->getCurrentSampleRate(); }

void Audition::setEngine (std::shared_ptr<EngineState> e) { stop(); engine = std::move (e); }

void Audition::setProcessed (bool b) { wantB.store (b); }

void Audition::setConsole (const std::vector<float>& g, const std::vector<float>& pan, const std::vector<bool>& st, float bus, float room,
                           const std::vector<float>& panL, const std::vector<float>& panR)
{
    for (size_t k = 0; k < g.size() && k < (size_t) maxTracks; ++k)
    {
        tGain[k] = g[k]; tPan[k] = k < pan.size() ? pan[k] : 0.0f; tStereo[k] = k < st.size() && st[k];
        tPanL[k] = k < panL.size() ? panL[k] : -1.0f; tPanR[k] = k < panR.size() ? panR[k] : 1.0f;
    }
    busGain = bus; roomGain = room;
}

std::unique_ptr<Audition::Rendered> Audition::renderOne (const EngineState& es, int hit, bool mono, const std::vector<int>& only) const
{
    auto r = std::make_unique<Rendered>();
    static const hl::LevelResult none;
    const auto& L = es.levels ? *es.levels : none;
    hl::StemOptions oa; oa.processed = false; oa.mono = mono; oa.taps = 16; oa.only = only;
    r->a = hl::renderHitStems (es.session, *es.mix, L, (size_t) hit, oa);
    hl::StemOptions ob; ob.processed = true; ob.mono = mono; ob.taps = 16; ob.only = only;
    r->b = hl::renderHitStems (es.session, *es.mix, L, (size_t) hit, ob);
    return r;
}

void Audition::stop()
{
    live.store (nullptr);
    playing = false;
    while (inCallback.load()) std::this_thread::yield();   // the audio thread lets go of the phrase
    if (phrase) phrase->cancel = true;
    if (worker.joinable()) worker.join();
    // the audio thread may still hold the old pointer for one callback: keep the phrase alive until the next play
    curHit = -1;
}

void Audition::play (std::vector<PlayEvent> events, const Options& o)
{
    stop();
    if (! engine || ! engine->mix || events.empty()) return;
    auto ph = std::make_shared<Phrase>();
    ph->events = std::move (events);
    ph->chain = o.chain && o.side < 0 && ! o.gain; ph->mono = o.mono; ph->sr = engine->session.sr;
    ph->console = o.console; ph->gain = o.gain; ph->side = o.gain ? -1 : o.side;
    ph->nT = (int) engine->session.tracks.size(); ph->ref = engine->session.refIdx;
    const size_t n = ph->events.size();
    ph->store.resize (n);
    ph->ready.reset (new std::atomic<Rendered*>[n]);
    for (size_t k = 0; k < n; ++k) ph->ready[k] = nullptr;
    auto es = engine;

    // what the start needs: the loudest hits (shared scale) and a spread of hits (loudness match), rendered in parallel
    std::vector<size_t> first;
    int topV = 0; for (auto& e : ph->events) topV = std::max (topV, e.vel);
    for (size_t k = 0; k < n && first.size() < 6; ++k) if (ph->events[k].vel == topV) first.push_back (k);
    std::vector<size_t> smp;
    const size_t stepK = std::max<size_t> (1, n / 8);
    for (size_t k = 0; k < n && smp.size() < 8; k += stepK) smp.push_back (k);
    std::vector<size_t> pre = first;
    for (auto k : smp) if (std::find (pre.begin(), pre.end(), k) == pre.end()) pre.push_back (k);
    if (std::find (pre.begin(), pre.end(), n - 1) == pre.end()) pre.push_back (n - 1);
    {
        std::vector<std::thread> th;
        for (auto k : pre) th.emplace_back ([&, k] { ph->store[k] = renderOne (*es, ph->events[k].hit, o.mono, o.only); });
        for (auto& x : th) x.join();
        for (auto k : pre) ph->ready[k] = ph->store[k].get();
    }
    // one shared scale from the loudest hits at the current faders, so velocities keep their real steps
    auto mixPeak = [&] (const hl::HitStems& h)
    {
        if (! o.console)
        {
            // every track at unity in the middle: the plain sum
            std::vector<float> m ((size_t) h.len, 0.0f);
            for (auto& st : h.stems) for (auto& c : st) for (size_t q = 0; q < c.size() && q < m.size(); ++q) m[q] += c[q] / (float) st.size();
            float p = 0; for (float v : m) p = std::max (p, std::abs (v) * 0.7071f);
            return p;
        }
        auto [L, R] = hl::mixToStereo (es->session, h);
        float p = 0; for (size_t q = 0; q < L.size(); ++q) p = std::max ({ p, std::abs (L[q]), std::abs (R[q]) });
        return p;
    };
    float peak = 1e-9f;
    for (auto k : first) { peak = std::max (peak, mixPeak (ph->store[k]->a)); peak = std::max (peak, mixPeak (ph->store[k]->b)); }
    ph->scale = 0.89 / peak;
    // loudness-matched A/B: the original is brought to the processed version's loudness, on the same hits
    ph->kA = 1;
    if (o.loudnessMatch && ! o.gain)
    {
        auto E = [&] (const hl::HitStems& h)
        {
            double s = 0;
            for (size_t t = 0; t < h.stems.size(); ++t)
            {
                const auto& st = h.stems[t]; if (st.empty()) continue;
                const double g = o.console ? tGain[(int) std::min<size_t> (t, maxTracks - 1)].load() : 1.0;
                if (g == 0) continue;
                double q = 0; for (auto& c : st) for (float v : c) q += (double) v * v;
                s += q * g * g / (double) st.size();
            }
            return s;
        };
        double ea = 0, eb = 0;
        for (auto k : smp) { ea += E (ph->store[k]->a); eb += E (ph->store[k]->b); }
        if (ea > 1e-12 && eb > 1e-12) ph->kA = std::clamp (std::sqrt (eb / ea), 0.25, 4.0);
    }
    const auto& lastR = *ph->store[n - 1];
    const double lenA = ph->events.back().at + (double) lastR.a.len / ph->sr;
    ph->chainAt = ph->chain ? lenA + 0.9 : 0;
    ph->end = ph->chain ? ph->chainAt + lenA : lenA;

    // the rest renders ahead of the playhead
    worker = std::thread ([ph, es, this, mono = o.mono, only = o.only]
    {
        for (size_t k = 0; k < ph->events.size() && ! ph->cancel; ++k)
            if (ph->ready[k].load() == nullptr) { ph->store[k] = renderOne (*es, ph->events[k].hit, mono, only); ph->ready[k] = ph->store[k].get(); }
    });
    phrase = ph;
    currentMode = o.mode;
    t = 0; gA = gB = 0; limGain = 1; cxy = cxx = cyy = 0; cn = 0; snapGains = true;
    finishedFlag = false;
    playing = true;
    live.store (ph.get());
}

void Audition::finishLater()
{
    if (finishedFlag.exchange (true)) return;
    if (juce::MessageManager::getInstanceWithoutCreating() == nullptr) return;
    juce::MessageManager::callAsync ([this] { playing = false; live.store (nullptr); if (onFinished) onFinished(); });
}

int Audition::drainMonitor (MonitorPoint* out, int max)
{
    int s1, n1, s2, n2;
    monFifo.prepareToRead (max, s1, n1, s2, n2);
    for (int k = 0; k < n1; ++k) out[k] = monBuf[(size_t) (s1 + k)];
    for (int k = 0; k < n2; ++k) out[n1 + k] = monBuf[(size_t) (s2 + k)];
    monFifo.finishedRead (n1 + n2);
    return n1 + n2;
}

void Audition::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outs, int nOut, int nFrames, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < nOut; ++c) if (outs[c]) juce::FloatVectorOperations::clear (outs[c], nFrames);
    inCallback = true;
    Phrase* ph = live.load();
    if (ph == nullptr || nOut == 0) { pkL = pkL * 0.85f; pkR = pkR * 0.85f; inCallback = false; return; }
    const int nT = ph->nT, ref = ph->ref;
    // which side plays: the A/B switch, unless the phrase is pinned to one side (the pair buttons) or plays the
    // difference (hear what you gain: A inverted under B)
    const bool pv = ph->side >= 0 ? ph->side == 1 : wantB.load();
    float targetA = (float) (ph->chain ? ph->scale * ph->kA : (pv ? 0 : ph->scale * ph->kA));
    float targetB = (float) (ph->chain ? ph->scale : (pv ? ph->scale : 0));
    if (ph->gain) { targetA = (float) -ph->scale; targetB = (float) ph->scale; }
    const float bus = busGain.load(), room = roomGain.load();
    if (snapGains.exchange (false)) { gA = targetA; gB = targetB; }   // a phrase starts at its gains; only A/B flips glide
    float gT[maxTracks]; std::array<double, 4> P[maxTracks];
    for (int k = 0; k < std::min (nT, maxTracks); ++k)
    {
        if (! ph->console) { gT[k] = 1; P[k] = hl::trackPanGains (0, 0, 0, tStereo[k].load()); continue; }   // unity, centred
        gT[k] = tGain[k].load(); P[k] = hl::trackPanGains (tPan[k].load(), tPanL[k].load(), tPanR[k].load(), tStereo[k].load());
    }
    const int mon = monTrack.load();
    const float ramp = 1.0f - std::exp (-1.0f / (0.02f * (float) deviceRate));
    const float limRel = 1.0f - std::exp (-1.0f / (0.08f * (float) deviceRate));
    const float ceiling = 0.891f;
    float pl = 0, pr = 0;
    int hitNow = -1, velNow = 0;
    float* L = outs[0];
    float* R = nOut > 1 ? outs[1] : nullptr;

    for (int f = 0; f < nFrames; ++f)
    {
        gA += (targetA - gA) * ramp; gB += (targetB - gB) * ramp;
        double l = 0, r = 0, lead = 0, rest = 0, monV = 0;
        for (size_t k = 0; k < ph->events.size(); ++k)
        {
            const auto& e = ph->events[k];
            for (int ver = 0; ver < 2; ++ver)
            {
                const double start = e.at + (ph->chain && ver == 1 ? ph->chainAt : 0);
                if (t < start) continue;
                Rendered* rd = ph->ready[k].load();
                if (rd == nullptr) continue;
                const auto& h = ver ? rd->b : rd->a;
                const double pos = (t - start) * ph->sr;
                const long i0 = (long) pos;
                if (i0 + 1 >= h.len) continue;
                const float vg = ver ? gB : gA;
                if (std::abs (vg) < 1e-6f) continue;
                const double fr = pos - (double) i0;
                if (ph->chain ? true : (ver == (pv ? 1 : 0))) { hitNow = e.hit; velNow = e.vel; }
                for (int tr = 0; tr < (int) h.stems.size() && tr < maxTracks; ++tr)
                {
                    const auto& st = h.stems[(size_t) tr];
                    if (st.empty()) continue;
                    const auto& a = st[0]; const auto& b = st.size() > 1 ? st[1] : st[0];
                    const double xa = a[(size_t) i0] + (a[(size_t) i0 + 1] - a[(size_t) i0]) * fr;
                    const double xb = st.size() > 1 ? b[(size_t) i0] + (b[(size_t) i0 + 1] - b[(size_t) i0]) * fr : xa;
                    if (tr == mon) monV = std::max (monV, std::max (std::abs (xa), std::abs (xb)) * std::abs (vg));
                    const double g = gT[tr] * bus * vg;
                    if (g == 0) continue;
                    const bool sto = st.size() > 1;
                    const auto& p = P[tr];
                    const double ol = (xa * p[0] + (sto ? xb * p[1] : 0)) * g, orr = (xa * p[2] + (sto ? xb * p[3] : 0)) * g;
                    l += ol; r += orr;
                    (tr == ref ? lead : rest) += ol + orr;
                }
                if (! h.room.empty() && room > 0 && ph->console)
                {
                    const double g = room * bus * vg;
                    l += (h.room[0][(size_t) i0] + (h.room[0][(size_t) i0 + 1] - h.room[0][(size_t) i0]) * fr) * g;
                    r += (h.room[1][(size_t) i0] + (h.room[1][(size_t) i0 + 1] - h.room[1][(size_t) i0]) * fr) * g;
                }
            }
        }
        // safety limiter on the preview only: overlapping tails can sum past full scale
        const float pk = (float) std::max (std::abs (l), std::abs (r));
        const float want = pk * limGain > ceiling ? ceiling / std::max (pk, 1e-9f) : 1.0f;
        limGain = want < limGain ? want : limGain + (1.0f - limGain) * limRel;
        L[f] = (float) l * limGain;
        if (R) R[f] = (float) r * limGain;
        pl = std::max (pl, std::abs (L[f])); pr = std::max (pr, R ? std::abs (R[f]) : std::abs (L[f]));
        cxy += lead * rest; cxx += lead * lead; cyy += rest * rest;
        if (++cn >= 1024)
        {
            corr = cxx > 1e-12 && cyy > 1e-12 ? (float) (cxy / std::sqrt (cxx * cyy)) : NAN;
            cxy = cxx = cyy = 0; cn = 0;
        }
        monPeak = std::max (monPeak, (float) monV);
        monAcc += 600.0 / deviceRate;
        if (monAcc >= 1)
        {
            monAcc -= 1;
            int s1, n1, s2, n2;
            monFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) monBuf[(size_t) s1] = { monPeak, velNow };
            monFifo.finishedWrite (n1);
            monPeak = 0;
        }
        t += 1.0 / deviceRate;
    }
    pkL = std::max (pl, pkL.load() * 0.85f); pkR = std::max (pr, pkR.load() * 0.85f);
    if (hitNow >= 0) { curHit = hitNow; curVel = velNow; }
    if (t > ph->end + 0.05) finishLater();
    inCallback = false;
}
} // namespace ld

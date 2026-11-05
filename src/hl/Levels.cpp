#include "Levels.h"
#include <algorithm>
#include <map>
#include <regex>

namespace ld::hl
{
namespace
{
    double msWindow (const std::vector<F32>& chs, long start, long len, long lagInt)
    {
        double s = 0;
        for (auto& x : chs)
        {
            const long N = (long) x.size();
            for (long n = 0; n < len; ++n) { const long i = start + n + lagInt; if (i >= 0 && i < N) { const double v = x[(size_t) i]; s += v * v; } }
        }
        return s / ((double) len * (double) chs.size());
    }
    double msSum (const std::vector<std::pair<const LevelTrack*, double>>& items, long start, long len)
    {
        F64 b ((size_t) len, 0.0);
        for (auto& [tr, pol] : items)
            for (auto& x : (tr->raw.empty() ? tr->ch : tr->raw))
            {
                const long N = (long) x.size();
                for (long n = 0; n < len; ++n) { const long i = start + n + tr->lagInt; if (i >= 0 && i < N) b[(size_t) n] += pol * x[(size_t) i]; }
            }
        double s = 0;
        for (double v : b) s += v * v;
        return s / len;
    }
    double dbFromMs (double ms) { return 10 * std::log10 (ms + 1e-30); }

    struct Zone { int lo, hi; double c; };
    std::vector<Zone> velZones (int V)
    {
        std::vector<Zone> z;
        for (int v = 0; v < V; ++v) { const int lo = (int) std::floor (v * 127.0 / V) + 1, hi = (int) std::floor ((v + 1) * 127.0 / V); z.push_back ({ lo, hi, (lo + hi) / 2.0 }); }
        return z;
    }
    F64 midiWeights (int V, double shape)
    {
        if (V <= 1) return { 1 };
        const auto z = velZones (V);
        auto f = [] (double c) { return 40 * std::log10 (c / 127); };
        const double c0 = z[0].c, c1 = z[(size_t) V - 1].c;
        F64 out;
        for (auto& zz : z) out.push_back ((1 - shape) * (zz.c - c0) / (c1 - c0) + shape * (f (zz.c) - f (c0)) / (f (c1) - f (c0)));
        return out;
    }

    struct House { double shape, range; std::map<std::string, double> top; };
    const std::map<std::string, House>& houseTable()
    {
        static const std::map<std::string, House> H {
            { "kick",   { 0.8,  20, { { "lead", -11 },   { "bottom", -13 },   { "sub", -13.3 }, { "oh", -30.7 }, { "room", -17 },   { "other", -14 } } } },
            { "snare",  { 0.9,  25, { { "lead", -12.5 }, { "bottom", -14.6 }, { "sub", -16 },   { "oh", -20.3 }, { "room", -19 },   { "other", -16 } } } },
            { "tom",    { 0.75, 25, { { "lead", -14.5 }, { "bottom", -16.6 }, { "sub", -17 },   { "oh", -23.8 }, { "room", -17.5 }, { "other", -18 } } } },
            { "hat",    { 0.6,  18, { { "lead", -23.7 }, { "bottom", -26 },   { "sub", -26 },   { "oh", -27.7 }, { "room", -28.6 }, { "other", -26 } } } },
            { "ride",   { 0.85, 25, { { "lead", -32.3 }, { "bottom", -34 },   { "sub", -34 },   { "oh", -32.4 }, { "room", -31.9 }, { "other", -32 } } } },
            { "crash",  { 0.45, 19, { { "lead", -30.3 }, { "bottom", -32 },   { "sub", -32 },   { "oh", -31.2 }, { "room", -33.5 }, { "other", -31 } } } },
            { "china",  { 0.75, 23, { { "lead", -16 },   { "bottom", -18 },   { "sub", -18 },   { "oh", -18 },   { "room", -20 },   { "other", -18 } } } },
            { "splash", { 0.4,  24, { { "lead", -21.4 }, { "bottom", -23 },   { "sub", -23 },   { "oh", -33.6 }, { "room", -35.4 }, { "other", -26 } } } },
            { "other",  { 0.75, 22, { { "lead", -14 },   { "bottom", -16 },   { "sub", -16 },   { "oh", -22 },   { "room", -20 },   { "other", -18 } } } },
        };
        return H;
    }
    // every room mic is measured over 400 ms and sits on one long-window standard (Hitline's ROOM_LONG)
    const std::map<std::string, double> roomLong { { "kick", -22 }, { "snare", -24 }, { "tom", -22.5 }, { "hat", -33.6 }, { "ride", -36.9 },
                                                   { "crash", -38.5 }, { "china", -25 }, { "splash", -40.4 }, { "other", -25 } };
    constexpr double roomWinMs = 400;
}

std::pair<double, double> houseStd (const std::string& drum, const std::string& key)
{
    const auto& H = houseTable();
    const auto it = H.find (drum);
    const auto& h = it != H.end() ? it->second : H.at ("other");
    const std::string d = it != H.end() ? drum : "other";
    if (key == "room") return { roomLong.count (d) ? roomLong.at (d) : roomLong.at ("other"), h.range };
    const auto t = h.top.find (key);
    return { t != h.top.end() ? t->second : h.top.at ("other"), h.range };
}

std::string houseKey (const LevelTrack& tr, bool isRef)
{
    if (isRef) return "lead";
    std::string n = tr.mic + " " + tr.name;
    std::transform (n.begin(), n.end(), n.begin(), ::tolower);
    if (std::regex_search (n, std::regex ("bottom|btm|\\bbot\\b|reso"))) return "bottom";
    if (std::regex_search (n, std::regex ("\\bsub\\b"))) return "sub";
    if (std::regex_search (n, std::regex ("\\boh\\b|overhead|ovh"))) return "oh";
    if (std::regex_search (n, std::regex ("mono|crush|crsh|trash|far|room|\\brm|amb|hall"))) return "room";
    // soundKey: what the mic sounds like, from when the hit reaches it
    const double a = tr.arrivalMs;
    if (! std::isfinite (a)) return "other";
    if (a > 3.2) return "room";
    if (a > 1.2) return "oh";
    if (std::isfinite (tr.autoAsis) && tr.autoAsis < -0.2) return "bottom";
    return "other";
}

F64 houseTargets (const std::string& drum, const std::string& key, int V, const std::string& steps)
{
    const auto [top, range] = houseStd (drum, key);
    const double p = steps == "pro" ? 1.8 : 1;
    F64 T;
    for (int v = 0; v < V; ++v) T.push_back (V == 1 ? top : top - range * std::pow (1 - (double) v / (V - 1), p));
    return T;
}

Targets computeTargets (const F64& L, int V, int RR, const std::string& order, const LevelSettings& o, const F64* presetT)
{
    Targets t;
    const size_t N = L.size();
    t.velOf.resize (N); t.rrOf.resize (N);
    for (size_t i = 0; i < N; ++i) { const int g = (int) (i / (size_t) RR); t.velOf[i] = order == "loud" ? V - 1 - g : g; t.rrOf[i] = (int) (i % (size_t) RR); }
    t.means.assign ((size_t) V, 0.0);
    std::vector<int> cnt ((size_t) V, 0);
    for (size_t i = 0; i < N; ++i) { t.means[(size_t) t.velOf[i]] += L[i]; cnt[(size_t) t.velOf[i]]++; }
    for (int v = 0; v < V; ++v) t.means[(size_t) v] /= std::max (1, cnt[(size_t) v]);
    t.T.assign ((size_t) V, 0.0);
    const double lo = t.means[0], hi = t.means[(size_t) V - 1];
    const int d = V > 1 ? V - 1 : 1;
    const bool usePreset = o.curve == "preset" && presetT && (int) presetT->size() == V;
    const auto mw = midiWeights (V, o.shape);
    const double midiRange = o.midiRange == "set" ? o.rangeDb : std::max (0.0, hi - lo);
    for (int v = 0; v < V; ++v)
    {
        double T;
        if (usePreset) T = (*presetT)[(size_t) v];
        else if (o.curve == "rr" || V == 1) T = t.means[(size_t) v];
        else if (o.curve == "midi") T = o.anchor == "soft" ? lo + midiRange * mw[(size_t) v] : hi - midiRange * (1 - mw[(size_t) v]);
        else if (o.curve == "even" || o.curve == "preset") T = lo + (hi - lo) * v / d;
        else if (o.curve == "step") T = o.anchor == "soft" ? lo + v * o.stepDb : hi - (V - 1 - v) * o.stepDb;
        else T = o.anchor == "soft" ? lo + o.rangeDb * v / d : hi - o.rangeDb * (V - 1 - v) / d;
        t.T[(size_t) v] = T + o.offsetDb;
    }
    t.gains.assign (N, 0.0);
    for (size_t i = 0; i < N; ++i) t.gains[i] = std::isfinite (L[i]) && L[i] > -200 ? t.T[(size_t) t.velOf[i]] - L[i] : 0;
    return t;
}

std::vector<Region> sliceRegions (const std::vector<long>& onsets, long pre, long maxLenS, long total)
{
    std::vector<Region> r;
    for (size_t k = 0; k < onsets.size(); ++k)
    {
        const long start = std::max (0L, onsets[k] - pre);
        const long next = k < onsets.size() - 1 ? std::max (0L, onsets[k + 1] - pre) : total;
        r.push_back ({ start, std::max (start + 1, std::min ({ next, start + maxLenS, total })) });
    }
    return r;
}

LevelResult computeLevels (const std::vector<LevelTrack>& tracks, int refIdx, const std::vector<long>& onsets, double sr,
                           const Det& det, const LevelSettings& lev, const ExportLevelSettings& exp, const std::string& drum,
                           const F64& tuneRatios, const std::vector<int>& rejects)
{
    LevelResult R;
    if (tracks.empty() || onsets.empty() || (int) onsets.size() != det.V * det.RR) return R;
    const long win = std::max (8L, (long) jsRound (lev.winMs * sr / 1000));
    auto ratio = [&] (size_t i) { return i < tuneRatios.size() ? tuneRatios[i] : 1.0; };
    const long winLong = (long) jsRound (roomWinMs * sr / 1000);
    R.house = lev.std == "house";
    auto key = [&] (int t) { return houseKey (tracks[(size_t) t], t == refIdx); };
    auto winOf = [&] (int t) { return R.house && t != refIdx && key (t) == "room" ? winLong : win; };
    auto clipDb = [&] (int t, size_t i) { const auto& c = tracks[(size_t) t].clipDb; return i < c.size() ? c[i] : 0.0; };

    for (int t = 0; t < (int) tracks.size(); ++t)
    {
        F64 L;
        for (size_t i = 0; i < onsets.size(); ++i)
            L.push_back (dbFromMs (msWindow (tracks[(size_t) t].ch, onsets[i], std::max (8L, (long) jsRound (winOf (t) * ratio (i))), tracks[(size_t) t].lagInt)));
        R.lvlRaw.push_back (L);
        for (size_t i = 0; i < L.size(); ++i) L[i] += clipDb (t, i);
        R.lvl.push_back (L);
    }
    auto targetsFor = [&] (int t) -> std::optional<F64> { if (R.house) return houseTargets (drum, key (t), det.V, lev.steps); return std::nullopt; };
    auto run = [&] (const F64& L, int t)
    {
        auto T = targetsFor (t);
        if (T) { LevelSettings o = lev; o.curve = "preset"; return computeTargets (L, det.V, det.RR, det.order, o, &*T); }
        LevelSettings o = lev; if (o.curve == "preset") o.curve = "midi";
        return computeTargets (L, det.V, det.RR, det.order, o);
    };
    if (lev.mode == "linked")
    {
        int si = refIdx;
        if (lev.source == "sum")
        {
            std::vector<std::pair<const LevelTrack*, double>> items;
            for (size_t i = 0; i < onsets.size(); ++i) { (void) i; }
            for (size_t i = 0; i < onsets.size(); ++i)
            {
                items.clear();
                for (int t = 0; t < (int) tracks.size(); ++t) items.push_back ({ &tracks[(size_t) t], tracks[(size_t) t].pol * std::pow (10.0, clipDb (t, i) / 20) });
                R.srcL.push_back (dbFromMs (msSum (items, onsets[i], std::max (8L, (long) jsRound (win * ratio (i))))));
            }
        }
        else
        {
            if (lev.source != "ref") si = std::clamp (std::atoi (lev.source.c_str()), 0, (int) tracks.size() - 1);
            R.srcL = R.lvl[(size_t) si];
        }
        R.linked = run (R.srcL, si);
        for (int t = 0; t < (int) tracks.size(); ++t)
        {
            F64 g;
            for (size_t i = 0; i < onsets.size(); ++i) g.push_back (R.linked->gains[i] + clipDb (t, i));
            R.gainsDbT.push_back (g);
        }
    }
    else
    {
        for (int t = 0; t < (int) tracks.size(); ++t) R.groupsT.push_back (run (R.lvl[(size_t) t], t));
        for (int t = 0; t < (int) tracks.size(); ++t)
        {
            F64 g;
            for (size_t i = 0; i < onsets.size(); ++i) g.push_back (R.groupsT[(size_t) t].gains[i] + clipDb (t, i));
            R.gainsDbT.push_back (g);
        }
    }

    long total = 0;
    for (auto& t : tracks) total = std::max (total, (long) t.ch[0].size());
    R.regions = sliceRegions (onsets, (long) jsRound (det.prerollMs * sr / 1000), (long) jsRound (exp.maxLenS * sr), total);
    for (int t = 0; t < (int) tracks.size(); ++t)
    {
        const auto& tr = tracks[(size_t) t];
        double pm = 0, pd = 0;
        for (size_t i = 0; i < R.regions.size(); ++i)
        {
            double pch = 0, pmo = 0;
            const auto& r = R.regions[i];
            for (auto& x : (tr.raw.empty() ? tr.ch : tr.raw))
                for (long k = std::max (0L, r.start + tr.lagInt), e = std::min ((long) x.size(), r.end + tr.lagInt); k < e; ++k)
                { const double a = x[(size_t) k] < 0 ? -x[(size_t) k] : x[(size_t) k]; if (a > pch) pch = a; }
            for (long k = std::max (0L, r.start + tr.lagInt), e = std::min ((long) tr.mono.size(), r.end + tr.lagInt); k < e; ++k)
            { const double a = std::abs ((double) tr.mono[(size_t) k]); if (a > pmo) pmo = a; }
            const double g = std::pow (10.0, R.gainsDbT[(size_t) t][i] / 20);
            pm = std::max (pm, pch * g); pd = std::max (pd, pmo * g);
            R.maxBoost = std::max (R.maxBoost, R.gainsDbT[(size_t) t][i]);
        }
        R.peakAfter.push_back (20 * std::log10 (pm + 1e-12));
        R.dispPeak.push_back (pd != 0 ? pd : 1e-9);
        R.peakMax = std::max (R.peakMax, R.peakAfter.back());
    }
    R.trimDb = (exp.trim == "auto" && ! R.locked && ! R.house) ? std::min (0.0, exp.ceilDb - R.peakMax) : 0;
    R.velOf = R.linked ? R.linked->velOf : R.groupsT[0].velOf;
    auto spread = [&] (int t, bool after)
    {
        double worst = 0;
        for (int v = 0; v < det.V; ++v)
        {
            double lo = INFINITY, hi = -INFINITY;
            for (size_t i = 0; i < onsets.size(); ++i)
                if (R.velOf[i] == v) { const double x = R.lvlRaw[(size_t) t][i] + (after ? R.gainsDbT[(size_t) t][i] : 0); lo = std::min (lo, x); hi = std::max (hi, x); }
            if (hi > lo) worst = std::max (worst, hi - lo);
        }
        return worst;
    };
    for (int t = 0; t < (int) tracks.size(); ++t)
    {
        F64 b;
        for (int v = 0; v < det.V; ++v)
        {
            double s = 0; int c = 0;
            for (size_t i = 0; i < onsets.size(); ++i)
                if (R.velOf[i] == v && std::find (rejects.begin(), rejects.end(), (int) i) == rejects.end()) { s += R.lvlRaw[(size_t) t][i] + R.gainsDbT[(size_t) t][i]; ++c; }
            b.push_back (s / std::max (1, c) + R.trimDb);
        }
        R.badge.push_back (b);
        R.spreadBefore.push_back (spread (t, false));
        R.spreadAfter.push_back (spread (t, true));
    }
    for (double x : (R.linked ? R.linked->T : R.groupsT[(size_t) refIdx].T)) R.targetsOut.push_back (x + R.trimDb);
    R.valid = true;
    return R;
}
} // namespace ld::hl

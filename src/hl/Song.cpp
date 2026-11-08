#include "Song.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <climits>
#include <cstdint>
#include <cmath>
#include <numeric>
#include <regex>

namespace ld::hl
{
const char* drumName (Drum d)
{
    switch (d) { case Drum::kick: return "kick"; case Drum::snare: return "snare"; case Drum::tom: return "tom";
                 case Drum::hat: return "hat"; case Drum::cymbal: return "cymbal"; default: return "other"; }
}

static std::string low (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }
static bool has (const std::string& n, const char* re) { return std::regex_search (n, std::regex (re)); }

Drum drumOfMic (const std::string& name)
{
    const auto n = low (name);
    if (has (n, R"(room|amb|mono|crush|far|trash|verb)")) return Drum::other;
    if (has (n, R"(kick|\bkik|\bbd\b|bass ?drum|sub)")) return Drum::kick;
    if (has (n, R"(snare|\bsn\b|\bsd\b)")) return Drum::snare;
    if (has (n, R"(tom|floor|\bft\b|rack)")) return Drum::tom;
    if (has (n, R"(hat|\bhh\b)")) return Drum::hat;
    if (has (n, R"(ride|crash|china|splash|cym)")) return Drum::cymbal;
    return Drum::other;
}
bool isOverheadMic (const std::string& name) { const auto n = low (name); return has (n, R"(\boh\b|overhead|over|\boh[ _-]?[lr12]?\b)"); }

// ---------------------------------------------------------------- detection
namespace
{
constexpr int H = 128;   // onset frames

struct Onset { long pos; int track; Drum drum; float strength, vel; float level = 0, bright = 0; int voice = 0; };

/** Onsets on one mic: a log-energy rise of the differentiated signal (transients over ringing), peaks over an adaptive
    floor, refined to the sample where the hit starts. */
std::vector<Onset> onsetsOf (const F32& x, double sr, int track, Drum drum)
{
    std::vector<Onset> r;
    const size_t nf = x.size() / H;
    if (nf < 8) return r;
    std::vector<double> e (nf, 0.0), lev (nf, 0.0);
    for (size_t f = 0; f < nf; ++f)
    {
        double s = 0, m = 0;
        for (size_t q = f * H; q < f * H + H; ++q) { const double d = q ? (double) x[q] - x[q - 1] : 0; s += d * d; m = std::max (m, (double) std::abs (x[q])); }
        e[f] = std::log10 (s / H + 1e-12); lev[f] = m;
    }
    double peak = 0; for (double v : lev) peak = std::max (peak, v);
    if (peak <= 1e-6) return r;
    std::vector<double> odf (nf, 0.0);
    for (size_t f = 3; f < nf; ++f) odf[f] = std::max (0.0, e[f] - std::max ({ e[f - 1], e[f - 2], e[f - 3] }));
    // adaptive floor: the median-ish level of the odf around each frame
    const int W = (int) (0.6 * sr / H);
    std::vector<double> pre (nf + 1, 0.0), pre2 (nf + 1, 0.0);
    for (size_t f = 0; f < nf; ++f) { pre[f + 1] = pre[f] + odf[f]; pre2[f + 1] = pre2[f] + odf[f] * odf[f]; }
    const long minGap = (long) (0.035 * sr);
    long last = -minGap * 2;
    for (size_t f = 3; f + 3 < nf; ++f)
    {
        const double v = odf[f];
        if (v < 0.35) continue;   // at least ~3.5 dB of rise in one frame
        bool isMax = true;
        for (int k = -3; k <= 3 && isMax; ++k) if (k && odf[f + (size_t) k] > v) isMax = false;
        if (! isMax) continue;
        const size_t a = (size_t) std::max (0, (int) f - W), b = std::min (nf, f + (size_t) W);
        const double mean = (pre[b] - pre[a]) / (double) (b - a), var = std::max (0.0, (pre2[b] - pre2[a]) / (double) (b - a) - mean * mean);
        if (v < mean + 2.0 * std::sqrt (var)) continue;
        // loud enough to be a hit on this mic, not its quiet bleed floor
        double m = 0; for (size_t g = f; g < std::min (nf, f + 6); ++g) m = std::max (m, lev[g]);
        if (m < peak * 0.03) continue;
        // where it starts: on a 1 ms peak envelope, the quietest point before the hit's peak is the floor (the last
        // hit's tail, or silence); the start is the first sample after it that rises 20 % of the way to the peak
        const long s0 = std::max (0L, (long) (f * H) - 6 * H), s1 = std::min ((long) x.size(), (long) (f * H) + 8 * H);
        const long w1 = std::max (1L, (long) (0.001 * sr));
        long pkAt = s0; double pk = 0;
        for (long q = s0; q < s1; ++q) if (std::abs (x[(size_t) q]) > pk) { pk = std::abs (x[(size_t) q]); pkAt = q; }
        auto envAt = [&] (long q) { double m = 0; for (long j = std::max (0L, q - w1); j <= q; ++j) m = std::max (m, (double) std::abs (x[(size_t) j])); return m; };
        long flAt = s0; double fl = 1e9;
        for (long q = s0; q <= pkAt; q += std::max (1L, w1 / 4)) { const double e2 = envAt (q); if (e2 < fl) { fl = e2; flAt = q; } }
        long pos = pkAt;
        for (long q = flAt; q <= pkAt; ++q) if (std::abs (x[(size_t) q]) >= fl + 0.2 * (pk - fl)) { pos = q; break; }
        if (pos - last < minGap) continue;
        last = pos;
        // brightness: how much of the hit's first 8 ms is in the differenced (high-frequency) signal
        double hf = 0, en = 0;
        for (long q = pos + 1; q < std::min ((long) x.size(), pos + (long) (0.008 * sr)); ++q) { const double d = (double) x[(size_t) q] - x[(size_t) q - 1]; hf += d * d; en += (double) x[(size_t) q] * x[(size_t) q]; }
        r.push_back ({ pos, track, drum, (float) v, (float) m, (float) m, (float) (hf / (en + 1e-12)) });
    }
    // velocity against this mic's loudest hits
    std::vector<float> vs; for (auto& o : r) vs.push_back (o.vel);
    if (! vs.empty())
    {
        std::sort (vs.begin(), vs.end());
        const float ref = std::max (1e-6f, vs[(size_t) std::min (vs.size() - 1, (size_t) (vs.size() * 0.95))]);
        for (auto& o : r) o.vel = std::min (1.0f, o.vel / ref);
    }
    return r;
}

/** The onset-strength signal for tempo (100 frames/s): each hit an impulse by drum and velocity, smoothed. */
std::vector<double> tempoOdf (const std::vector<SongHit>& hits, double sr, size_t nf)
{
    std::vector<double> o (nf, 0.0);
    for (auto& h : hits)
    {
        const double w = h.drum == Drum::kick || h.drum == Drum::snare ? 1.0 : h.drum == Drum::tom ? 0.7 : 0.5;
        const long f = (long) std::lround (h.pos / sr * 100);
        if (f >= 0 && f < (long) nf) o[(size_t) f] += w * std::sqrt (std::max (0.05f, h.vel));
    }
    std::vector<double> s (nf, 0.0);
    for (size_t f = 0; f < nf; ++f)
        for (int k = -3; k <= 3; ++k) { const long g = (long) f + k; if (g >= 0 && g < (long) nf) s[f] += o[(size_t) g] * std::exp (-0.5 * k * k / 2.0); }
    return s;
}

/** Best beat period (frames of 10 ms) in o[a, b), between lo and hi, with a prior toward 120 BPM. */
double periodIn (const std::vector<double>& o, size_t a, size_t b, double lo, double hi, double prefer)
{
    double best = -1, bp = prefer;
    for (int L = (int) std::floor (lo); L <= (int) std::ceil (hi); ++L)
    {
        double acc = 0;
        for (size_t f = a; f + (size_t) L < b; ++f) acc += o[f] * o[f + (size_t) L];
        // a beat period also lines up at twice the lag; and a gentle preference for the expected tempo
        for (size_t f = a; f + (size_t) (2 * L) < b; ++f) acc += 0.5 * o[f] * o[f + (size_t) (2 * L)];
        const double prior = std::exp (-0.5 * std::pow (std::log2 (L / prefer) / 0.6, 2));
        const double score = acc * prior;
        if (score > best) { best = score; bp = L; }
    }
    // parabolic refinement isn't worth it at 10 ms; the beat tracker settles the exact positions
    return bp;
}
} // namespace

SongAnalysis analyseSong (const std::vector<F32>& monos, const std::vector<std::string>& names, double sr)
{
    SongAnalysis A; A.sr = sr;
    // 1. onsets on every close mic (and cymbals on the overheads)
    std::vector<Onset> all;
    for (size_t t = 0; t < monos.size(); ++t)
    {
        Drum d = drumOfMic (names[t]);
        const bool oh = d == Drum::other && isOverheadMic (names[t]);
        if (d == Drum::other && ! oh) continue;
        auto o = onsetsOf (monos[t], sr, (int) t, oh ? Drum::cymbal : d);
        all.insert (all.end(), o.begin(), o.end());
    }
    // names that say nothing (Audio 1, Track 2): every mic is a source, its hits untagged
    if (all.empty())
        for (size_t t = 0; t < monos.size(); ++t) { auto o = onsetsOf (monos[t], sr, (int) t, Drum::other); all.insert (all.end(), o.begin(), o.end()); }
    std::sort (all.begin(), all.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
    // which mics are the same drum: two mics of one kind (kick in and out; two kick drums) are one drum when their strong
    // hits are also strong on the other mic, within 3 ms, most of the time; a second drum is heard on the other's mic only
    // as quiet bleed. Each drum of a kind gets its own voice.
    const size_t nt = monos.size();
    A.micVoice.assign (nt, -1);
    {
        std::vector<std::vector<size_t>> byTrack (nt);
        for (size_t i = 0; i < all.size(); ++i) byTrack[(size_t) all[i].track].push_back (i);
        // one drum on both mics: its strong hits show on the other mic too, at a steady level offset, and the offset is
        // the same seen from either mic (a is c dB over b on a's hits, so b is c dB under a on b's hits: the two medians
        // cancel). Two drums: each mic is louder on its own drum's hits, so the medians add up instead of cancelling.
        auto offsetFrom = [&] (size_t a, size_t b, int& n, int& hit)
        {
            n = 0; hit = 0; size_t k = 0;
            std::vector<double> diff;
            const auto& B = byTrack[b];
            for (auto i : byTrack[a])
            {
                if (all[i].vel < 0.5f) continue;
                ++n;
                while (k < B.size() && all[B[k]].pos < all[i].pos - (long) (0.003 * sr)) ++k;
                for (size_t q = k; q < B.size() && all[B[q]].pos <= all[i].pos + (long) (0.003 * sr); ++q)
                { ++hit; diff.push_back (20 * std::log10 ((all[i].level + 1e-9) / (all[B[q]].level + 1e-9))); break; }
            }
            if (diff.empty()) return 0.0;
            std::nth_element (diff.begin(), diff.begin() + (long) diff.size() / 2, diff.end());
            return diff[diff.size() / 2];
        };
        auto sameDrum = [&] (size_t a, size_t b)
        {
            int na, ha, nb, hb;
            const double ma = offsetFrom (a, b, na, ha), mb = offsetFrom (b, a, nb, hb);
            if (na < 8 || nb < 8) return true;
            const double both = std::min ((double) ha / na, (double) hb / nb), sum = ma + mb;
            if (std::getenv ("LD_SONG_DEBUG")) fprintf (stderr, "mics %zu+%zu: on both %.0f%%, offsets %+.1f / %+.1f dB (sum %.1f)\n", a, b, 100 * both, ma, mb, sum);
            return both >= 0.6 && sum < 10;
        };
        for (Drum d : { Drum::kick, Drum::snare, Drum::tom, Drum::hat })
        {
            const bool canSplit = d == Drum::kick || d == Drum::tom;   // two snares (or hats) on separate mics: not worth the risk
            std::vector<size_t> ts;
            for (size_t t = 0; t < nt; ++t) if (! byTrack[t].empty() && all[byTrack[t][0]].drum == d) ts.push_back (t);
            std::vector<int> root (ts.size()); for (size_t k = 0; k < ts.size(); ++k) root[k] = (int) k;
            std::function<int (int)> find = [&] (int x) { return root[(size_t) x] == x ? x : root[(size_t) x] = find (root[(size_t) x]); };
            for (size_t a = 0; a < ts.size(); ++a)
                for (size_t b = a + 1; b < ts.size(); ++b)
                {
                    if (! canSplit || sameDrum (ts[a], ts[b])) root[(size_t) find ((int) a)] = find ((int) b);
                }
            std::map<int, int> vid;
            for (size_t k = 0; k < ts.size(); ++k) { const int r = find ((int) k); if (! vid.count (r)) { const int n = (int) vid.size(); vid[r] = n; } A.micVoice[ts[k]] = vid[r]; }
        }
        for (auto& o : all) o.voice = std::max (0, A.micVoice[(size_t) o.track]);
    }
    auto same = [&] (size_t a, size_t b) { return all[a].drum == all[b].drum && all[a].voice == all[b].voice; };
    // 2. one hit per drum per moment: bleed (much weaker than another drum's mic at the same time) dropped,
    //    the same drum's mics (snare top and bottom) merged, the earliest arrival kept
    const long win = (long) (0.018 * sr);
    // how loud each drum's close mic is heard on every other close mic (bleed), learnt from this song: for each pair, the
    // median level ratio when the two mics' onsets coincide (most coincidences are the one drum heard on both)
    std::vector<float> bleedRatio (nt * nt, 0.0f);
    {
        std::vector<std::vector<float>> rs (nt * nt);
        for (size_t i = 0; i < all.size(); ++i)
            for (size_t j = i + 1; j < all.size() && all[j].pos - all[i].pos <= (long) (0.004 * sr); ++j)
            {
                const auto& a = all[i]; const auto& b = all[j];
                if (a.track == b.track || same (i, j)) continue;
                if (a.level > b.level) rs[(size_t) a.track * nt + (size_t) b.track].push_back (b.level / a.level);
                else rs[(size_t) b.track * nt + (size_t) a.track].push_back (a.level / b.level);
            }
        for (size_t k = 0; k < rs.size(); ++k)
            if (rs[k].size() >= 12) { std::nth_element (rs[k].begin(), rs[k].begin() + (long) rs[k].size() / 2, rs[k].end()); bleedRatio[k] = rs[k][rs[k].size() / 2]; }
    }
    // each hi-hat mic's typical brightness, from its hits that no drum coincides with
    std::vector<float> hatBright (nt, 0.0f);
    {
        std::vector<std::vector<float>> bs (nt);
        for (size_t i = 0; i < all.size(); ++i)
        {
            if (all[i].drum != Drum::hat) continue;
            bool alone = true;
            for (size_t j = i; j-- > 0 && all[i].pos - all[j].pos <= win;) if (all[j].drum != Drum::hat && all[j].drum != Drum::cymbal) alone = false;
            for (size_t j = i + 1; j < all.size() && all[j].pos - all[i].pos <= win; ++j) if (all[j].drum != Drum::hat && all[j].drum != Drum::cymbal) alone = false;
            if (alone) bs[(size_t) all[i].track].push_back (all[i].bright);
        }
        for (size_t t = 0; t < nt; ++t)
            if (bs[t].size() >= 8) { std::nth_element (bs[t].begin(), bs[t].begin() + (long) bs[t].size() / 2, bs[t].end()); hatBright[t] = bs[t][bs[t].size() / 2]; }
    }
    std::vector<bool> used (all.size(), false);
    for (size_t i = 0; i < all.size(); ++i)
    {
        if (used[i]) continue;
        std::vector<size_t> grp { i };
        for (size_t j = i + 1; j < all.size() && all[j].pos - all[i].pos <= win; ++j) if (! used[j]) grp.push_back (j);
        float strongest = 0; for (auto g : grp) strongest = std::max (strongest, all[g].vel);
        std::vector<std::pair<int, int>> keys;
        for (auto g : grp) { const std::pair<int, int> k { (int) all[g].drum, all[g].voice }; if (std::find (keys.begin(), keys.end(), k) == keys.end()) keys.push_back (k); }
        std::sort (keys.begin(), keys.end());
        for (auto [d, voice] : keys)
        {
            long pos = -1; float vel = 0;
            for (auto g : grp)
            {
                if ((int) all[g].drum != d || all[g].voice != voice) continue;
                if (all[g].vel < strongest * 0.28f && all[g].drum != Drum::cymbal) continue;   // bleed from the louder drum
                // within twice (6 dB) the level this mic normally hears from a louder drum in the group: that drum's bleed
                bool bleed = false;
                for (auto o : grp)
                {
                    if (o == g || same (o, g) || all[o].drum == Drum::cymbal || all[o].level <= all[g].level) continue;
                    const float br = bleedRatio[(size_t) all[o].track * nt + (size_t) all[g].track];
                    if (br > 0 && all[g].level < all[o].level * br * 2.0f) bleed = true;
                }
                if (bleed && all[g].drum != Drum::cymbal) continue;
                // the snare or a tom on the hi-hat mic: duller than the hats' own hits
                if (all[g].drum == Drum::hat && hatBright[(size_t) all[g].track] > 0 && all[g].bright < hatBright[(size_t) all[g].track] * 0.6f)
                {
                    bool drum = false; for (auto o : grp) if (all[o].drum == Drum::kick || all[o].drum == Drum::snare || all[o].drum == Drum::tom) drum = true;
                    if (drum) continue;
                }
                pos = pos < 0 ? all[g].pos : std::min (pos, all[g].pos);
                vel = std::max (vel, all[g].vel);
            }
            if (pos < 0) continue;
            // a cymbal on the overheads that's really a close-miked drum arriving: only kept when nothing else hit
            if (d == (int) Drum::cymbal && grp.size() > 1)
            {
                bool other = false; for (auto g : grp) if (all[g].drum != Drum::cymbal && all[g].vel >= strongest * 0.28f) other = true;
                if (other) continue;
            }
            A.hits.push_back ({ pos, (Drum) d, vel });
            A.hits.back().voice = voice;
        }
        for (auto g : grp) used[g] = true;
    }
    std::sort (A.hits.begin(), A.hits.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
    // the same drum twice within 15 ms is one hit seen on two of its mics (a group boundary fell between them)
    {
        std::vector<SongHit> m;
        for (auto& h : A.hits)
        {
            bool merged = false;
            for (size_t k = m.size(); k-- > 0;)
            {
                if (h.pos - m[k].pos > (long) (0.015 * sr)) break;
                if (m[k].drum == h.drum && m[k].voice == h.voice) { m[k].vel = std::max (m[k].vel, h.vel); merged = true; break; }
            }
            if (! merged) m.push_back (h);
        }
        A.hits = std::move (m);
    }
    // 3. ghost notes and flams
    std::vector<float> sv; for (auto& h : A.hits) if (h.drum == Drum::snare) sv.push_back (h.vel);
    if (! sv.empty())
    {
        std::nth_element (sv.begin(), sv.begin() + (long) sv.size() / 2, sv.end());
        const float med = sv[sv.size() / 2];
        for (auto& h : A.hits) if (h.drum == Drum::snare && h.vel < med * 0.4f) h.ghost = true;
    }
    // a flam: a grace note just before the main stroke on the same drum, on its own. Inside a run (a double-pedal
    // stream, a roll) the same spacing is just the run, so a pair with another hit on that drum close before it or
    // after it isn't a flam.
    {
        auto sameAt = [&] (size_t k, size_t i) { return A.hits[k].drum == A.hits[i].drum && A.hits[k].voice == A.hits[i].voice; };
        for (size_t i = 1; i < A.hits.size(); ++i)
            for (size_t j = i; j-- > 0;)
            {
                if (A.hits[i].pos - A.hits[j].pos > (long) (0.045 * sr)) break;
                if (! sameAt (j, i)) continue;
                const long gap = A.hits[i].pos - A.hits[j].pos, reach = std::max (gap * 3, (long) (0.06 * sr));
                bool run = false;
                for (size_t k = j; k-- > 0 && A.hits[j].pos - A.hits[k].pos <= reach;) if (sameAt (k, i)) { run = true; break; }
                for (size_t k = i + 1; ! run && k < A.hits.size() && A.hits[k].pos - A.hits[i].pos <= reach; ++k) if (sameAt (k, i)) run = true;
                // on a kick, a very quiet partner is a leftover from a second kick mic rather than a played note
                const bool phantom = A.hits[i].drum == Drum::kick && std::min (A.hits[i].vel, A.hits[j].vel) < 0.15f;
                if (! run && ! phantom) A.hits[i].flam = true;
                break;
            }
    }
    // 4. the beats: tempo per stretch of the song, then a beat tracker that follows the hits (Ellis' dynamic programme)
    long len = 0; for (auto& m : monos) len = std::max (len, (long) m.size());
    const size_t nf = (size_t) (len / sr * 100) + 1;
    const auto o = tempoOdf (A.hits, sr, nf);
    if (A.hits.size() < 8 || nf < 400) { A.done = true; return A; }
    const double P0 = periodIn (o, 0, nf, 100 * 60 / 180.0, 100 * 60 / 70.0, 50);
    std::vector<double> P (nf, P0);
    {
        const size_t Wn = 800, hop = 200;
        std::vector<std::pair<size_t, double>> pts;
        for (size_t a = 0; a + Wn / 2 < nf; a += hop)
            pts.push_back ({ a + Wn / 2, periodIn (o, a, std::min (nf, a + Wn), P0 * 0.85, P0 * 1.15, P0) });
        for (size_t f = 0; f < nf && ! pts.empty(); ++f)
        {
            size_t k = 0; while (k + 1 < pts.size() && pts[k + 1].first <= f) ++k;
            if (k + 1 < pts.size() && f >= pts[k].first)
            { const double t = (double) (f - pts[k].first) / (double) (pts[k + 1].first - pts[k].first); P[f] = pts[k].second * (1 - t) + pts[k + 1].second * t; }
            else P[f] = pts[k].second;
        }
    }
    std::vector<double> C (nf, 0.0); std::vector<long> back (nf, -1);
    const double alpha = 400;
    for (size_t f = 0; f < nf; ++f)
    {
        const double p = P[f];
        double best = 0; long bi = -1;
        for (long tau = (long) f - (long) std::lround (2 * p); tau <= (long) f - (long) std::lround (p / 2); ++tau)
        {
            if (tau < 0) continue;
            const double pen = -alpha * std::pow (std::log ((f - tau) / p), 2);
            const double v = C[(size_t) tau] + pen;
            if (bi < 0 || v > best) { best = v; bi = tau; }
        }
        C[f] = o[f] + (bi >= 0 ? std::max (0.0, best) : 0.0);
        back[f] = bi >= 0 && best > 0 ? bi : -1;
    }
    // the last beat: the best score in the final period
    size_t endF = nf - 1; double bestEnd = -1;
    for (size_t f = nf - (size_t) std::min ((double) nf - 1, P[nf - 1]); f < nf; ++f) if (C[f] > bestEnd) { bestEnd = C[f]; endF = f; }
    std::vector<double> beats;
    for (long f = (long) endF; f >= 0; f = back[(size_t) f]) beats.push_back (f / 100.0);
    std::reverse (beats.begin(), beats.end());
    // refine each beat onto the kick/snare/hat that played it (10 ms frames are coarse), within 25 ms
    for (auto& b : beats)
    {
        // the kick or snare that played the beat if there is one, else the nearest other hit
        double bestD = 0.025, at = b; int rank = 9;
        for (auto& h : A.hits)
        {
            const double t = h.pos / sr; if (std::abs (t - b) >= 0.025 || h.drum == Drum::cymbal || h.ghost) continue;
            const int r = h.drum == Drum::kick || h.drum == Drum::snare ? 0 : 1;
            if (r < rank || (r == rank && std::abs (t - b) < bestD)) { rank = r; bestD = std::abs (t - b); at = t; }
        }
        b = at;
    }
    // a beat the tracker skipped or doubled: intervals far from their neighbours' median are split or merged
    for (int pass = 0; pass < 2 && beats.size() > 8; ++pass)
    {
        std::vector<double> fixed { beats[0] };
        for (size_t i = 1; i < beats.size(); ++i)
        {
            std::vector<double> iv;
            for (size_t k = (i > 6 ? i - 6 : 1); k < std::min (beats.size(), i + 6); ++k) iv.push_back (beats[k] - beats[k - 1]);
            std::nth_element (iv.begin(), iv.begin() + (long) iv.size() / 2, iv.end());
            const double med = iv[iv.size() / 2], d = beats[i] - fixed.back();
            if (d < med * 0.6) continue;                                        // a doubled beat
            if (d > med * 1.6) { const int n = (int) std::lround (d / med); for (int k = 1; k < n; ++k) fixed.push_back (fixed.back() + d / n); }
            fixed.push_back (beats[i]);
        }
        beats = fixed;
    }
    // extend back to the first hit with the opening tempo
    while (beats.size() > 1 && beats.front() - (beats[1] - beats[0]) > (A.hits.front().pos / sr) - 0.02) beats.insert (beats.begin(), beats.front() - (beats[1] - beats[0]));
    A.beats = beats;
    // downbeat: the beat in each bar the kick lands on most (and the snare least)
    double sc[4] {};
    for (auto& h : A.hits)
    {
        const double t = h.pos / sr;
        auto it = std::lower_bound (beats.begin(), beats.end(), t - 0.03);
        if (it == beats.end() || std::abs (*it - t) > 0.03) continue;
        const size_t bi = (size_t) (it - beats.begin());
        if (h.drum == Drum::kick) sc[bi % 4] += h.vel; else if (h.drum == Drum::snare && ! h.ghost) sc[bi % 4] -= h.vel;
    }
    A.downbeat = (int) (std::max_element (sc, sc + 4) - sc);
    // the grid the playing sits on: how far kick, snare and tom hits are from the nearest 16th and from the nearest
    // eighth-note triplet (in parts of a beat); triplets win when they fit clearly better
    {
        // hits close (within 4 % of a beat) to the lines only one grid has: 1/4, 1/2, 3/4 for sixteenths, 1/3, 2/3 for
        // triplets; per line, so the two grids' different line counts don't weigh in
        double on16 = 0, on12 = 0; int n = 0;
        for (auto& h : A.hits)
        {
            if (h.ghost || (h.drum != Drum::kick && h.drum != Drum::snare && h.drum != Drum::tom)) continue;
            const double t = h.pos / sr;
            auto it = std::upper_bound (A.beats.begin(), A.beats.end(), t);
            if (it == A.beats.begin() || it == A.beats.end()) continue;
            const double p = (t - *(it - 1)) / (*it - *(it - 1));
            for (double l : { 0.25, 0.5, 0.75 }) if (std::abs (p - l) < 0.04) on16 += 1.0 / 3;
            for (double l : { 1.0 / 3, 2.0 / 3 }) if (std::abs (p - l) < 0.04) on12 += 1.0 / 2;
            ++n;
        }
        A.suggestedDivision = n >= 32 && on12 > on16 * 2 ? 12 : 16;
    }
    A.done = true;
    return A;
}

// ---------------------------------------------------------------- the grid
std::vector<double> gridBeats (const SongAnalysis& A, const SongSettings& S)
{
    std::vector<double> b = A.beats;
    if (b.size() < 4) return b;
    // half or double time
    if (S.bpmScale > 1.5) { std::vector<double> d; for (size_t i = 0; i + 1 < b.size(); ++i) { d.push_back (b[i]); d.push_back (0.5 * (b[i] + b[i + 1])); } d.push_back (b.back()); b = d; }
    else if (S.bpmScale < 0.75) { std::vector<double> h; for (size_t i = 0; i < b.size(); i += 2) h.push_back (b[i]); b = h; }
    const size_t n = b.size();
    auto fit = [&] (size_t a, size_t e)   // a line through beats [a, e), refit without the beats far off it (robust)
    {
        std::vector<char> use (e - a, 1);
        double k = 0, c = 0;
        for (int pass = 0; pass < 3; ++pass)
        {
            double sx = 0, sy = 0, sxx = 0, sxy = 0, m = 0;
            for (size_t i = a; i < e; ++i) if (use[i - a]) { sx += (double) i; sy += b[i]; sxx += (double) i * i; sxy += (double) i * b[i]; m += 1; }
            if (m < 2) break;
            k = (m * sxy - sx * sy) / std::max (1e-12, m * sxx - sx * sx); c = (sy - k * sx) / m;
            double ss = 0; for (size_t i = a; i < e; ++i) if (use[i - a]) { const double r = b[i] - (c + k * (double) i); ss += r * r; }
            const double sd = std::sqrt (ss / m);
            for (size_t i = a; i < e; ++i) use[i - a] = std::abs (b[i] - (c + k * (double) i)) <= std::max (0.004, 2 * sd);
        }
        return std::make_pair (k, c);
    };
    if (S.tempoMode == "flatten")
    {
        auto [k, c] = fit (0, n);
        if (S.flatBpm > 0) { const double k2 = 60 / S.flatBpm; double mi = 0; for (size_t i = 0; i < n; ++i) mi += b[i] - k2 * (double) i; c = mi / (double) n; k = k2; }
        std::vector<double> r (n); for (size_t i = 0; i < n; ++i) r[i] = c + k * (double) i;
        return r;
    }
    if (S.tempoMode == "smooth")
    {
        // a local quadratic through the beats two bars either side (robust), so the map follows the drummer's slow
        // pushes and pulls but not each beat's wobble
        std::vector<double> r (n);
        for (size_t i = 0; i < n; ++i)
        {
            const size_t a = i >= 8 ? i - 8 : 0, e = std::min (n, i + 9);
            std::vector<char> use (e - a, 1);
            double val = b[i];
            for (int pass = 0; pass < 3; ++pass)
            {
                // weighted least squares in x = j - i: b = c0 + c1 x + c2 x^2, tricube weights
                double M[3][4] {};
                for (size_t j = a; j < e; ++j)
                {
                    if (! use[j - a]) continue;
                    const double x = (double) j - (double) i, w = std::pow (1 - std::pow (std::abs (x) / 9.0, 3), 3);
                    const double v[3] = { 1, x, x * x };
                    for (int p = 0; p < 3; ++p) { for (int q = 0; q < 3; ++q) M[p][q] += w * v[p] * v[q]; M[p][3] += w * v[p] * b[j]; }
                }
                // solve 3x3
                for (int p = 0; p < 3; ++p)
                {
                    int piv = p; for (int q = p + 1; q < 3; ++q) if (std::abs (M[q][p]) > std::abs (M[piv][p])) piv = q;
                    std::swap (M[p], M[piv]);
                    if (std::abs (M[p][p]) < 1e-12) break;
                    for (int q = 0; q < 3; ++q) if (q != p) { const double f = M[q][p] / M[p][p]; for (int z = p; z < 4; ++z) M[q][z] -= f * M[p][z]; }
                }
                const double c0 = std::abs (M[0][0]) > 1e-12 ? M[0][3] / M[0][0] : b[i], c1 = std::abs (M[1][1]) > 1e-12 ? M[1][3] / M[1][1] : 0, c2 = std::abs (M[2][2]) > 1e-12 ? M[2][3] / M[2][2] : 0;
                val = c0;
                double ss = 0, m = 0;
                for (size_t j = a; j < e; ++j) if (use[j - a]) { const double x = (double) j - (double) i, res = b[j] - (c0 + c1 * x + c2 * x * x); ss += res * res; m += 1; }
                const double sd = std::sqrt (ss / std::max (1.0, m));
                for (size_t j = a; j < e; ++j) { const double x = (double) j - (double) i; use[j - a] = std::abs (b[j] - (c0 + c1 * x + c2 * x * x)) <= std::max (0.004, 2.5 * sd); }
            }
            r[i] = val;
        }
        return r;
    }
    return b;
}

std::vector<double> bpmOf (const std::vector<double>& beats)
{
    std::vector<double> r;
    for (size_t i = 0; i + 1 < beats.size(); ++i) r.push_back (60 / std::max (1e-6, beats[i + 1] - beats[i]));
    if (! r.empty()) r.push_back (r.back());
    return r;
}

std::vector<double> gridLines (const std::vector<double>& beats, int division)
{
    const int per = division == 8 ? 2 : division == 12 ? 3 : 4;
    std::vector<double> g;
    for (size_t i = 0; i + 1 < beats.size(); ++i) for (int k = 0; k < per; ++k) g.push_back (beats[i] + (beats[i + 1] - beats[i]) * k / per);
    if (! beats.empty()) g.push_back (beats.back());
    return g;
}

// ---------------------------------------------------------------- the edits
std::vector<SongEdit> planEdits (const SongAnalysis& A, const SongSettings& S)
{
    std::vector<SongEdit> E;
    if (! A.done || A.hits.empty() || (S.tightness <= 0 && S.swing <= 0)) return E;
    const double sr = A.sr;
    const auto beats = gridBeats (A, S);
    if (beats.size() < 2) return E;
    const int per = S.division == 8 ? 2 : S.division == 12 ? 3 : 4;
    const auto lines = gridLines (beats, S.division);
    // events: the hits that belong to the same grid line move as one (a kick and a hat together, a flam, a snare
    // played late against an on-time hat), so nothing that was together gets pulled apart or piled up
    auto nearest = [&] (double t)
    {
        auto it = std::lower_bound (lines.begin(), lines.end(), t);
        size_t gi = it == lines.end() ? lines.size() - 1 : (size_t) (it - lines.begin());
        if (gi > 0 && std::abs (lines[gi - 1] - t) < std::abs (lines[gi] - t)) --gi;
        return gi;
    };
    struct Ev { long pos; size_t a, b; size_t gi; };
    std::vector<Ev> ev;
    for (size_t i = 0; i < A.hits.size(); ++i)
    {
        const size_t gi = nearest (A.hits[i].pos / sr);
        if (! ev.empty() && (ev.back().gi == gi || A.hits[i].pos - A.hits[ev.back().b - 1].pos <= (long) (0.008 * sr))) { ev.back().b = i + 1; continue; }
        ev.push_back ({ A.hits[i].pos, i, i + 1, gi });
    }
    const long pre = (long) (0.003 * sr);
    long prevCut = -1, prevTarget = LONG_MIN, prevShift = 0;
    for (auto& e : ev)
    {
        // the hit the event is timed by: its loudest kick or snare (then tom, then anything), not a hat that happened
        // to be a few ms early; the cut still goes before the earliest of them
        long anchor = e.pos; int rank = 99; float best = -1;
        for (size_t i = e.a; i < e.b; ++i)
        {
            const auto& h = A.hits[i];
            const int r = h.drum == Drum::kick || h.drum == Drum::snare ? 0 : h.drum == Drum::tom ? 1 : 2;
            if (r < rank || (r == rank && h.vel > best)) { rank = r; best = h.vel; anchor = h.pos; }
        }
        const double t = anchor / sr;
        auto it = std::lower_bound (lines.begin(), lines.end(), t);
        size_t gi = it == lines.end() ? lines.size() - 1 : (size_t) (it - lines.begin());
        if (gi > 0 && std::abs (lines[gi - 1] - t) < std::abs (lines[gi] - t)) --gi;
        const double step = gi + 1 < lines.size() ? lines[gi + 1] - lines[gi] : gi > 0 ? lines[gi] - lines[gi - 1] : 0.125;
        double g = lines[gi];
        if (std::abs (g - t) > 0.45 * step) continue;   // nowhere near the grid (a triplet in a straight grid): left alone
        // swing: the offbeat subdivision late, up to a triplet
        const bool off = per >= 2 && (gi % (size_t) per) % 2 == 1 && per != 3;
        bool hatsOnly = true; for (size_t i = e.a; i < e.b; ++i) if (A.hits[i].drum != Drum::hat) hatsOnly = false;
        if (off && S.swing > 0 && (! S.swingHatsOnly || hatsOnly)) g += S.swing * step / 3;
        double amt = S.tightness;
        if (S.swing > 0 && S.tightness <= 0) amt = (off && (! S.swingHatsOnly || hatsOnly)) ? 1.0 : 0.0;   // swing alone moves only the offbeats
        bool soft = true; for (size_t i = e.a; i < e.b; ++i) if (! A.hits[i].ghost && ! A.hits[i].flam) soft = false;
        if (S.keepFeel && soft) amt *= 0.3;
        const long target = (long) std::lround (g * sr);
        long shift = (long) std::lround (amt * (g * sr - (double) anchor));
        if (target == prevTarget) shift = prevShift;   // (events already gather a grid line's hits; kept as a guard)
        const long cut = e.pos - pre;
        if (cut <= prevCut + (long) (0.004 * sr)) continue;
        E.push_back ({ cut, shift });
        prevCut = cut; prevTarget = target; prevShift = shift;
    }
    return E;
}

F32 applyEdits (const F32& x, const std::vector<SongEdit>& E, double sr)
{
    if (E.empty()) return x;
    const long n = (long) x.size(), xf = std::max (8L, (long) (0.0015 * sr));
    // boundaries: between region k-1 and region k, at cut_k + min(shift_k-1, shift_k), so neither side reads past a
    // transient it doesn't own
    std::vector<long> bnd (E.size()), sh (E.size() + 1, 0);
    for (size_t k = 0; k < E.size(); ++k) sh[k + 1] = E[k].shift;
    for (size_t k = 0; k < E.size(); ++k) bnd[k] = E[k].cut + std::min (sh[k], sh[k + 1]);
    for (size_t k = 1; k < bnd.size(); ++k) bnd[k] = std::max (bnd[k], bnd[k - 1] + 2 * xf);
    auto src = [&] (long t, long s) { const long q = t - s; return q >= 0 && q < n ? x[(size_t) q] : 0.0f; };
    F32 y ((size_t) n, 0.0f);
    size_t k = 0;   // region index into sh (0 = before the first cut)
    for (long t = 0; t < n; ++t)
    {
        while (k < bnd.size() && t >= bnd[k] + xf / 2) ++k;
        // inside a crossfade around bnd[k]?
        if (k < bnd.size() && t >= bnd[k] - xf / 2)
        {
            const double a = (double) (t - (bnd[k] - xf / 2)) / (double) xf;   // 0..1
            const double ga = std::cos (a * M_PI / 2), gb = std::sin (a * M_PI / 2);
            y[(size_t) t] = (float) (ga * src (t, sh[k]) + gb * src (t, sh[k + 1]));
        }
        else y[(size_t) t] = src (t, sh[k]);
    }
    return y;
}

Chs applyEdits (const Chs& chs, const std::vector<SongEdit>& E, double sr)
{
    if (E.empty()) return chs;
    Chs r; r.reserve (chs.size());
    for (auto& x : chs) r.push_back (applyEdits (x, E, sr));
    return r;
}

long movedPos (long pos, const std::vector<SongEdit>& E)
{
    long s = 0;
    for (auto& e : E) { if (e.cut > pos) break; s = e.shift; }
    return pos + s;
}

// ---------------------------------------------------------------- consistency
void applyConsistency (std::vector<Chs>& tracks, const std::vector<std::string>& names, const SongAnalysis& A, double amount)
{
    if (amount <= 0 || ! A.done) return;
    const double sr = A.sr;
    const long win = (long) (0.03 * sr), fade = (long) (0.002 * sr);
    for (Drum d : { Drum::kick, Drum::snare, Drum::tom })
      for (int voice = 0; voice < 8; ++voice)
    {
        // this drum's own mics (one kick drum of two, one tom)
        std::vector<size_t> mics;
        for (size_t t = 0; t < names.size() && t < tracks.size(); ++t)
            if (drumOfMic (names[t]) == d && (t >= A.micVoice.size() ? voice == 0 : std::max (0, A.micVoice[t]) == voice)) mics.push_back (t);
        if (mics.empty()) continue;
        std::vector<long> pos; std::vector<double> lev; std::vector<bool> keep;
        for (auto& h : A.hits)
        {
            if (h.drum != d || h.voice != voice) continue;
            double pk = 0;
            for (auto t : mics) for (auto& x : tracks[t]) for (long q = h.pos; q < std::min ((long) x.size(), h.pos + win); ++q) pk = std::max (pk, (double) std::abs (x[(size_t) q]));
            pos.push_back (h.pos); lev.push_back (20 * std::log10 (pk + 1e-9)); keep.push_back (h.ghost || h.flam);
        }
        const size_t n = pos.size();
        if (n < 4) continue;
        for (size_t i = 0; i < n; ++i)
        {
            if (keep[i]) continue;
            std::vector<double> nb;
            for (size_t j = i >= 6 ? i - 6 : 0; j < std::min (n, i + 7); ++j) if (! keep[j]) nb.push_back (lev[j]);
            std::nth_element (nb.begin(), nb.begin() + (long) nb.size() / 2, nb.end());
            const double med = nb[nb.size() / 2], off = med - lev[i];
            if (std::abs (off) > 6) continue;   // an accent (or a deliberate soft hit): as played
            const double g = std::pow (10.0, std::clamp (amount * off, -6.0, 6.0) / 20);
            if (std::abs (g - 1) < 1e-4) continue;
            const long a = std::max (0L, pos[i] - fade), e = i + 1 < n ? pos[i + 1] - fade : pos[i] + (long) (0.5 * sr);
            for (auto t : mics)
                for (auto& x : tracks[t])
                    for (long q = a; q < std::min ((long) x.size(), e + fade); ++q)
                    {
                        // fade in over the first 2 ms, out over the last 2 ms before the next hit
                        const double w = std::min ({ 1.0, (double) (q - a) / (double) fade, (double) (e + fade - q) / (double) fade });
                        x[(size_t) q] = (float) ((double) x[(size_t) q] * (1 + (g - 1) * std::max (0.0, w)));
                    }
        }
    }
}

// ---------------------------------------------------------------- problems
std::vector<SongProblem> findProblems (const SongAnalysis& A, const SongSettings& S)
{
    std::vector<SongProblem> P;
    if (! A.done) return P;
    const double sr = A.sr;
    const auto beats = gridBeats (A, S);
    const auto lines = gridLines (beats, S.division);
    for (size_t i = 0; i < A.hits.size(); ++i)
    {
        const auto& h = A.hits[i];
        if (h.flam && ! h.ghost) P.push_back ({ h.pos, "flam", std::string ("Flam on the ") + drumName (h.drum) });
        if (h.drum != Drum::hat && ! h.flam)
        {
            // the previous and next hits on the same drum
            long prev = -1, next = -1, prev2 = -1; float prevVel = 1;
            for (size_t j = i; j-- > 0;) if (A.hits[j].drum == h.drum && A.hits[j].voice == h.voice) { if (prev < 0) { prev = A.hits[j].pos; prevVel = A.hits[j].vel; } else { prev2 = A.hits[j].pos; break; } }
            for (size_t j = i + 1; j < A.hits.size(); ++j) if (A.hits[j].drum == h.drum && A.hits[j].voice == h.voice) { next = A.hits[j].pos; break; }
            if (prev >= 0)
            {
                // a double: closer than half a 16th at the local tempo, and not part of a roll (three or more evenly spaced hits)
                const double t = h.pos / sr;
                auto it = std::upper_bound (beats.begin(), beats.end(), t);
                const double beat = beats.size() > 1 ? (it == beats.begin() || it == beats.end() ? beats[1] - beats[0] : *it - *(it - 1)) : 0.5;
                const long gap = h.pos - prev;
                // part of a run (a roll, a double-pedal stream): another hit on this drum within three gaps either side
                const long reach = std::max (gap * 3, (long) (0.06 * sr));
                const bool roll = (prev2 >= 0 && prev - prev2 <= reach) || (next >= 0 && next - h.pos <= reach);
                const bool phantom = h.drum == Drum::kick && std::min (h.vel, prevVel) < 0.15f;
                if (gap < (long) (std::min (0.09, beat / 8) * sr) && ! roll && ! phantom)
                    P.push_back ({ h.pos, "double", std::string ("Double ") + drumName (h.drum) + " hit" });
            }
        }
        // inside a fast run (a roll or a triplet fill): another hit on any drum within a 16th either side
        bool fast = false;
        {
            const double t = h.pos / sr;
            auto bi = std::upper_bound (beats.begin(), beats.end(), t);
            const double beat = beats.size() > 1 ? (bi == beats.begin() || bi == beats.end() ? beats[1] - beats[0] : *bi - *(bi - 1)) : 0.5;
            const long w16 = (long) (beat / 4 * sr);
            const bool before = i > 0 && h.pos - A.hits[i - 1].pos < w16 && A.hits[i - 1].drum != Drum::hat;
            const bool after = i + 1 < A.hits.size() && A.hits[i + 1].pos - h.pos < w16 && A.hits[i + 1].drum != Drum::hat;
            fast = before || after;
        }
        if (lines.size() > 1 && ! h.ghost && ! fast)
        {
            const double t = h.pos / sr;
            auto it = std::lower_bound (lines.begin(), lines.end(), t);
            if (it != lines.end() && it != lines.begin())
            {
                const double d = std::min (*it - t, t - *(it - 1)), step = *it - *(it - 1);
                if (d > 0.3 * step && d < 0.5 * step) P.push_back ({ h.pos, "off", std::string ("The ") + drumName (h.drum) + " is far off the grid" });
            }
        }
    }
    // fills: a run of toms/snares between two beats that drifts from the beat by more than 20 ms on average
    const auto bp = bpmOf (A.beats);
    for (size_t i = 4; i + 4 < bp.size(); ++i)
    {
        double before = 0; for (size_t k = i - 4; k < i; ++k) before += bp[k]; before /= 4;
        // a new section: the new tempo holds for the next 8 beats (within 3 %), so it's a tempo change, not rushing or dragging
        if (i + 9 < bp.size())
        {
            std::vector<double> nx (bp.begin() + (long) i + 2, bp.begin() + (long) i + 10);
            std::nth_element (nx.begin(), nx.begin() + 4, nx.end());
            const double med = nx[4];
            bool holds = true;
            for (size_t k = i + 2; k < i + 10; ++k) if (std::abs (bp[k] / med - 1) > 0.03) { holds = false; break; }
            if (holds && std::abs (med / before - 1) > 0.04) { i += 9; continue; }
        }
        if (bp[i] > before * 1.06 && bp[i + 1] > before * 1.04) { P.push_back ({ (long) (A.beats[i] * sr), "rush", "Rushing here" }); i += 4; }
        else if (bp[i] < before * 0.94 && bp[i + 1] < before * 0.96) { P.push_back ({ (long) (A.beats[i] * sr), "drag", "Dragging here" }); i += 4; }
    }
    std::sort (P.begin(), P.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
    return P;
}

// ---------------------------------------------------------------- MIDI
namespace
{
constexpr int PPQ = 960;
void vlq (std::string& s, uint32_t v)
{
    char buf[5]; int n = 0; buf[n++] = (char) (v & 0x7f);
    while ((v >>= 7)) buf[n++] = (char) (0x80 | (v & 0x7f));
    while (n) s += buf[--n];
}
std::string be32 (uint32_t v) { return { (char) (v >> 24), (char) (v >> 16), (char) (v >> 8), (char) v }; }
std::string chunk (const char* id, const std::string& data) { return std::string (id, 4) + be32 ((uint32_t) data.size()) + data; }
std::string header (int tracks) { std::string h; h += (char) 0; h += (char) 1; h += (char) 0; h += (char) tracks; h += (char) (PPQ >> 8); h += (char) (PPQ & 0xff); return chunk ("MThd", h); }

/** Ticks for a time in seconds, on a beat list (one quarter per beat), the first beat at startTick. */
long ticksAt (double t, const std::vector<double>& beats, long startTick)
{
    if (beats.size() < 2) return startTick;
    auto it = std::upper_bound (beats.begin(), beats.end(), t);
    size_t i = it == beats.begin() ? 0 : (size_t) (it - beats.begin()) - 1;
    i = std::min (i, beats.size() - 2);
    const double frac = (t - beats[i]) / (beats[i + 1] - beats[i]);
    return startTick + (long) std::lround ((i + frac) * PPQ);
}
std::string tempoTrack (const std::vector<double>& beats, double startOffset, long& startTick)
{
    // before the first beat: one beat's worth at the opening tempo, so the song's first sample is tick 0
    std::string d;
    const double first = beats.empty() ? 0 : beats[0] - startOffset;
    const double p0 = beats.size() > 1 ? beats[1] - beats[0] : 0.5;
    startTick = (long) std::lround (first / p0 * PPQ);
    auto tempo = [&] (long dt, double secPerBeat)
    {
        vlq (d, (uint32_t) dt); d += (char) 0xff; d += (char) 0x51; d += (char) 3;
        const uint32_t us = (uint32_t) std::lround (secPerBeat * 1e6);
        d += (char) (us >> 16); d += (char) (us >> 8); d += (char) us;
    };
    vlq (d, 0); d += (char) 0xff; d += (char) 0x58; d += (char) 4; d += (char) 4; d += (char) 2; d += (char) 24; d += (char) 8;   // 4/4
    tempo (0, p0);
    for (size_t i = 0; i + 1 < beats.size(); ++i) tempo (i == 0 ? startTick : PPQ, beats[i + 1] - beats[i]);
    vlq (d, 0); d += (char) 0xff; d += (char) 0x2f; d += (char) 0;
    return d;
}
} // namespace

std::string midiTempoMap (const std::vector<double>& beats, double startOffset)
{
    long st = 0;
    return header (1) + chunk ("MTrk", tempoTrack (beats, startOffset, st));
}

std::string midiPerformance (const std::vector<SongHit>& hits, const std::vector<SongEdit>& edits, const std::vector<double>& beats, double sr)
{
    long st = 0;
    const auto tt = tempoTrack (beats, 0, st);
    struct Ev { long tick; int note, vel; bool on; };
    std::vector<Ev> ev;
    for (auto& h : hits)
    {
        // GM: the second kick drum on 35, each tom on its own note (high to low in mic order)
        static const int toms[] = { 48, 47, 45, 43, 41 };
        const int note = h.drum == Drum::kick ? (h.voice ? 35 : 36) : h.drum == Drum::snare ? 38 : h.drum == Drum::tom ? toms[std::min (h.voice, 4)]
                       : h.drum == Drum::hat ? 42 : h.drum == Drum::cymbal ? 49 : 37;
        const long tk = ticksAt (movedPos (h.pos, edits) / sr, beats, st);
        const int v = std::clamp ((int) std::lround (h.vel * 127), 1, 127);
        ev.push_back ({ tk, note, v, true }); ev.push_back ({ tk + PPQ / 8, note, 0, false });
    }
    std::stable_sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.tick < b.tick; });
    std::string d; long last = 0;
    vlq (d, 0); d += (char) 0xff; d += (char) 0x03; const std::string nm = "Drums"; d += (char) nm.size(); d += nm;
    for (auto& e : ev) { vlq (d, (uint32_t) std::max (0L, e.tick - last)); last = e.tick; d += (char) (e.on ? 0x99 : 0x89); d += (char) e.note; d += (char) e.vel; }
    vlq (d, 0); d += (char) 0xff; d += (char) 0x2f; d += (char) 0;
    return header (2) + chunk ("MTrk", tt) + chunk ("MTrk", d);
}
} // namespace ld::hl

#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

/** The Hits screen's decisions that don't need a window, so they can be tested on their own. */
namespace ld::samples
{
struct Target { float x, y, w, h; };

/** Which of the clickable rectangles is under (x, y): the smallest one wins (a button inside a row, never the row over
    the button, whatever order they were added in); ties go to the last added, which is drawn on top. -1 for none. */
template <typename T>
int pickTarget (const std::vector<T>& rects, float x, float y)
{
    int best = -1;
    for (int i = 0; i < (int) rects.size(); ++i)
    {
        const auto& h = rects[(size_t) i];
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h)
            if (best < 0 || h.w * h.h <= rects[(size_t) best].w * rects[(size_t) best].h) best = i;
    }
    return best;
}

/** What plays when hit `sel` is picked: mode 0 the hit, 1 (Layer) every round robin of its velocity, 2 (Sweep) every
    hit. Starts 5 ms early, never before 0. H needs .vel, .time and .end (seconds). */
template <typename H>
std::pair<double, double> auditionRange (const std::vector<H>& hits, int sel, int mode)
{
    if (hits.empty()) return { 0, 0 };
    sel = std::clamp (sel, 0, (int) hits.size() - 1);
    const auto& s = hits[(size_t) sel];
    double a = s.time, b = s.end;
    for (auto& h : hits)
        if (mode == 2 || (mode == 1 && h.vel == s.vel)) { a = std::min (a, h.time); b = std::max (b, h.end); }
    return { std::max (0.0, a - 0.005), b };
}
/** The three views. Whether hit i is on show: 0 Hit: only the selected one; 1 Velocity layer: every round robin of the
    selected hit's layer; 2 All layers: every hit. A view decides what is shown and auditioned, never what an edit
    applies to. */
template <typename H>
bool inView (const std::vector<H>& hits, int sel, int mode, int i)
{
    if (i < 0 || i >= (int) hits.size()) return false;
    if (mode == 2) return true;
    sel = std::clamp (sel, 0, (int) hits.size() - 1);
    return mode == 1 ? hits[(size_t) i].vel == hits[(size_t) sel].vel : i == sel;
}

/** The hits on show, in order; thinned evenly to at most `cap`, always keeping the selected one. */
template <typename H>
std::vector<int> viewHits (const std::vector<H>& hits, int sel, int mode, int cap = 1 << 30)
{
    std::vector<int> all;
    for (int i = 0; i < (int) hits.size(); ++i) if (inView (hits, sel, mode, i)) all.push_back (i);
    if ((int) all.size() <= cap || cap < 1) return all;
    std::vector<int> out;
    for (int k = 0; k < cap; ++k) out.push_back (all[(size_t) ((long) k * (long) all.size() / cap)]);
    sel = std::clamp (sel, 0, (int) hits.size() - 1);
    if (std::find (out.begin(), out.end(), sel) == out.end() && std::find (all.begin(), all.end(), sel) != all.end())
    {
        auto it = std::lower_bound (out.begin(), out.end(), sel);   // swap the nearest kept hit for the selected one
        if (it == out.end()) --it;
        *it = sel;
        std::sort (out.begin(), out.end());
    }
    return out;
}

/** The audition phrases (Hitline's): which hit sounds when. H needs .vel and .rr. Times in seconds from the start. */
struct PhraseNote { double at = 0; int hit = 0; };

template <typename H>
int hitOf (const std::vector<H>& hits, int vel, int rr)
{
    for (int i = 0; i < (int) hits.size(); ++i) if (hits[(size_t) i].vel == vel && hits[(size_t) i].rr == rr) return i;
    return -1;
}
template <typename H>
int layersOf (const std::vector<H>& hits) { int v = 0; for (auto& h : hits) v = std::max (v, h.vel + 1); return v; }
template <typename H>
int robinsOf (const std::vector<H>& hits, int vel) { int r = 0; for (auto& h : hits) if (h.vel == vel) r = std::max (r, h.rr + 1); return r; }

/** Layer: every round robin of the selected hit's velocity, half a second apart, each ringing out. */
template <typename H>
std::vector<PhraseNote> layerPhrase (const std::vector<H>& hits, int sel, double step = 0.5)
{
    std::vector<PhraseNote> ev;
    if (hits.empty()) return ev;
    sel = std::clamp (sel, 0, (int) hits.size() - 1);
    for (int i = 0; i < (int) hits.size(); ++i) if (hits[(size_t) i].vel == hits[(size_t) sel].vel) ev.push_back ({ step * (double) ev.size(), i });
    return ev;
}
/** Sweep: the selected hit's round robin, from the softest layer to the hardest. */
template <typename H>
std::vector<PhraseNote> sweepPhrase (const std::vector<H>& hits, int sel, double step = 0.5)
{
    std::vector<PhraseNote> ev;
    if (hits.empty()) return ev;
    sel = std::clamp (sel, 0, (int) hits.size() - 1);
    for (int v = 0; v < layersOf (hits); ++v)
    {
        int i = hitOf (hits, v, hits[(size_t) sel].rr);
        if (i < 0) i = hitOf (hits, v, 0);
        if (i >= 0) ev.push_back ({ step * (double) ev.size(), i });
    }
    return ev;
}
/** Every round robin of a layer once, in a new order, never starting on `avoid` (a sampler's random mode). */
template <typename Rng>
std::vector<int> rrShuffle (int n, int avoid, Rng& rng)
{
    std::vector<int> a ((size_t) std::max (0, n));
    for (int k = 0; k < n; ++k) a[(size_t) k] = k;
    for (int k = n - 1; k > 0; --k) std::swap (a[(size_t) k], a[(size_t) (rng() % (unsigned) (k + 1))]);
    if (n > 1 && a[0] == avoid) std::swap (a[0], a[(size_t) (1 + rng() % (unsigned) (n - 1))]);
    return a;
}
/** Groove: two bars of full-strength quarter notes, then sixteenths climbing through every velocity, every round
    robin of each in turn, the last left to ring out. */
template <typename H, typename Rng>
std::vector<PhraseNote> groovePhrase (const std::vector<H>& hits, double bpm, Rng& rng)
{
    std::vector<PhraseNote> ev;
    const int V = layersOf (hits);
    if (V == 0) return ev;
    const double q = 60.0 / bpm, s16 = q / 4;
    double t = 0; int prev = -1;
    std::vector<int> bag;
    for (int n = 0; n < 8; ++n)
    {
        if (bag.empty()) bag = rrShuffle (robinsOf (hits, V - 1), prev, rng);
        const int r = bag.front(); bag.erase (bag.begin()); prev = r;
        const int i = hitOf (hits, V - 1, r);
        if (i >= 0) ev.push_back ({ t, i });
        t += q;
    }
    prev = -1;
    for (int v = 0; v < V; ++v)
        for (int r : rrShuffle (robinsOf (hits, v), prev, rng))
        {
            prev = r;
            const int i = hitOf (hits, v, r);
            if (i >= 0) ev.push_back ({ t, i });
            t += s16;
        }
    return ev;
}
/** Rudiments: paradiddles with accents, a backbeat with ghost notes, a triplet crescendo, flams and a roll into a
    flammed downbeat; timing and dynamics humanised a little, round robins at random, never the same twice running. */
template <typename H, typename Rng>
std::vector<PhraseNote> rudimentPhrase (const std::vector<H>& hits, double bpm, Rng& rng)
{
    std::vector<PhraseNote> ev;
    const int V = layersOf (hits);
    if (V == 0) return ev;
    const double q = 60.0 / bpm;
    std::vector<int> last ((size_t) V, -1);
    auto u = [&] { return (double) (rng() % 100000u) / 100000.0; };
    auto jitter = [&] (double a) { return (u() + u() + u() - 1.5) * a; };
    auto note = [&] (double t, double vel, bool left)
    {
        const int v = std::clamp ((int) std::lround (std::clamp (vel + jitter (0.05) + (left ? -0.03 : 0.0), 0.0, 1.0) * (V - 1)), 0, V - 1);
        const int RR = robinsOf (hits, v);
        if (RR <= 0) return;
        int r = 0;
        do r = (int) (rng() % (unsigned) RR); while (RR > 1 && r == last[(size_t) v]);
        last[(size_t) v] = r;
        const int i = hitOf (hits, v, r);
        if (i >= 0) ev.push_back ({ std::max (0.0, t + jitter (0.004)), i });
    };
    double t = 0;
    static const bool pd[8] = { false, true, false, false, true, false, true, true };
    for (int n = 0; n < 16; ++n) { note (t, n % 4 == 0 ? 0.88 : 0.34, pd[n % 8]); t += q / 4; }
    static const double bb[16] = { 0.18, 0.12, 0.2, 0.14, 1, 0.12, 0.22, 0.16, 0.2, 0.14, 0.24, 0.12, 1, 0.14, 0.3, 0.42 };
    for (int n = 0; n < 16; ++n) { note (t, bb[n], n % 2 == 1); t += q / 4; }
    for (int n = 0; n < 24; ++n) { note (t, 0.15 + 0.75 * n / 23.0 + (n % 3 == 0 ? 0.06 : 0.0), n % 2 == 1); t += q / 6; }
    for (int n = 0; n < 2; ++n) { note (t - 0.024, 0.28, true); note (t, 0.92, false); t += q; }
    for (int n = 0; n < 16; ++n) { note (t, 0.3 + 0.68 * n / 15.0, n % 2 == 1); t += q / 8; }
    note (t - 0.022, 0.32, true); note (t, 1.0, false);
    std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.at < b.at; });
    return ev;
}
} // namespace ld::samples

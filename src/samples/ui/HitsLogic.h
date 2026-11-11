#pragma once
#include <algorithm>
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
} // namespace ld::samples

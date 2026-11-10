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
} // namespace ld::samples

#include "Wave.h"
#include <algorithm>
#include <cmath>

namespace ld::ui
{
float Wave::sampleB (int ch, long n) const
{
    if (! b || b->empty()) return 0;
    const auto& c = (*b)[(size_t) std::min (ch, (int) b->size() - 1)];
    const long src = n + lagB;
    if (src < 0 || src >= (long) c.size()) return 0;
    float g = 1;
    if (! gainB.empty())
    {
        auto it = std::upper_bound (regionStart.begin(), regionStart.end(), n);
        const size_t k = it == regionStart.begin() ? 0 : (size_t) (it - regionStart.begin() - 1);
        g = gainB[std::min (k, gainB.size() - 1)];
    }
    return c[(size_t) src] * g;
}

void Wave::build()
{
    pyrA.clear(); pyrB.clear();
    const int nch = channels();
    const long len = length();
    if (nch == 0 || len == 0) return;
    for (int side = 0; side < (b ? 2 : 1); ++side)
    {
        auto& pyr = side ? pyrB : pyrA;
        // level 0 from the samples
        const long nb = (len + base - 1) / base;
        pyr.emplace_back ((size_t) nch);
        for (int ch = 0; ch < nch; ++ch)
        {
            auto& out = pyr[0][(size_t) ch];
            out.resize ((size_t) nb * 2);
            for (long k = 0; k < nb; ++k)
            {
                float lo = 0, hi = 0;
                for (long n = k * base, e = std::min (len, n + base); n < e; ++n)
                {
                    const float v = side ? sampleB (ch, n) : (*a)[(size_t) ch][(size_t) n];
                    lo = std::min (lo, v); hi = std::max (hi, v);
                }
                out[(size_t) k * 2] = lo; out[(size_t) k * 2 + 1] = hi;
            }
        }
        // each level halves the one below
        while (pyr.back()[0].size() / 2 > 64)
        {
            const auto& prev = pyr.back();
            std::vector<std::vector<float>> next ((size_t) nch);
            for (int ch = 0; ch < nch; ++ch)
            {
                const auto& p = prev[(size_t) ch];
                const size_t n = p.size() / 2;
                auto& o = next[(size_t) ch];
                o.resize (((n + 1) / 2) * 2);
                for (size_t k = 0; k < (n + 1) / 2; ++k)
                {
                    const size_t i = k * 2, j = std::min (n - 1, i + 1);
                    o[k * 2] = std::min (p[i * 2], p[j * 2]);
                    o[k * 2 + 1] = std::max (p[i * 2 + 1], p[j * 2 + 1]);
                }
            }
            pyr.push_back (std::move (next));
        }
    }
}

std::pair<float, float> Wave::range (int ch, long s0, long s1, float mix) const
{
    auto one = [&] (bool side) -> std::pair<float, float>
    {
        const auto& pyr = side ? pyrB : pyrA;
        const long len = length();
        s0 = std::max (0L, s0); s1 = std::min (len, std::max (s0 + 1, s1));
        if (s0 >= len) return { 0, 0 };
        const long span = s1 - s0;
        float lo = 0, hi = 0;
        if (span < base || pyr.empty())
        {
            for (long n = s0; n < s1; ++n)
            {
                const float v = side ? sampleB (ch, n) : (*a)[(size_t) ch][(size_t) n];
                lo = std::min (lo, v); hi = std::max (hi, v);
            }
            return { lo, hi };
        }
        int level = 0;
        while (level + 1 < (int) pyr.size() && ((long) base << (level + 1)) <= span) ++level;
        const long bs = (long) base << level;
        const auto& p = pyr[(size_t) level][(size_t) ch];
        const long n = (long) p.size() / 2;
        for (long k = s0 / bs, e = std::min (n, (s1 + bs - 1) / bs); k < e; ++k)
        {
            lo = std::min (lo, p[(size_t) k * 2]); hi = std::max (hi, p[(size_t) k * 2 + 1]);
        }
        return { lo, hi };
    };
    const auto A = one (false);
    if (mix <= 0 || pyrB.empty()) return A;
    const auto B = one (true);
    return { A.first + (B.first - A.first) * mix, A.second + (B.second - A.second) * mix };
}

float Wave::peak (float mix) const
{
    float pa = 0, pb = 0;
    for (auto& c : pyrA.empty() ? std::vector<std::vector<float>>() : pyrA.back()) for (float v : c) pa = std::max (pa, std::abs (v));
    for (auto& c : pyrB.empty() ? std::vector<std::vector<float>>() : pyrB.back()) for (float v : c) pb = std::max (pb, std::abs (v));
    return pyrB.empty() ? pa : pa + (pb - pa) * mix;
}
} // namespace ld::ui

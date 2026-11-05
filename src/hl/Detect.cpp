#include "Detect.h"
#include <algorithm>
#include <regex>
#include <set>

namespace ld::hl
{
F32 blockEnv (const F32& x, int bs)
{
    const size_t n = (x.size() + (size_t) bs - 1) / (size_t) bs;
    F32 env (n);
    for (size_t b = 0; b < n; ++b)
    {
        float m = 0;
        const size_t e = std::min (x.size(), (b + 1) * (size_t) bs);
        for (size_t i = b * (size_t) bs; i < e; ++i) { const float a = x[i] < 0 ? -x[i] : x[i]; if (a > m) m = a; }
        env[b] = (float) (20.0 * std::log10 ((double) m + 1e-12));
    }
    return env;
}

namespace
{
    struct Curves { F32 held, rise; };

    Curves onsetCurves (const F32& env, int bs, double sr)
    {
        const int n = (int) env.size();
        const int H = std::max (2, jsRoundI (0.02 * sr / bs)), P = std::max (4, jsRoundI (0.015 * sr / bs));
        Curves c { F32 ((size_t) n), F32 ((size_t) n) };
        for (int b = 0; b < n; ++b)
        {
            double m = -240;
            for (int j = std::max (0, b - H + 1); j <= b; ++j) if (env[(size_t) j] > m) m = env[(size_t) j];
            c.held[(size_t) b] = (float) m;
        }
        for (int b = 0; b < n; ++b)
        {
            double mn = INFINITY;
            for (int j = std::max (0, b - H - P); j < b - H; ++j) if (c.held[(size_t) j] < mn) mn = c.held[(size_t) j];
            if (mn == INFINITY) mn = -240;
            c.rise[(size_t) b] = (float) ((double) c.held[(size_t) b] - mn);
        }
        return c;
    }

    std::vector<int> onsetsAt (const Curves& cur, double thr, int minGapB, double riseDb)
    {
        std::vector<int> out;
        double last = -1e12;
        for (int b = 0; b < (int) cur.held.size(); ++b)
        {
            if (cur.held[(size_t) b] < thr || cur.rise[(size_t) b] < riseDb || b - last < minGapB) continue;
            out.push_back (b);
            last = b;
        }
        return out;
    }

    int refineOnset (const F32& x, int cross, double sr)
    {
        const int len = (int) x.size();
        const int pkEnd = std::min (len, cross + jsRoundI (0.006 * sr));
        double pk = 0;
        int pkI = cross;
        for (int i = cross; i < pkEnd; ++i) { const double a = std::abs ((double) x[(size_t) i]); if (a > pk) { pk = a; pkI = i; } }
        const double thr = pk * 0.06;
        const int quiet = jsRoundI (0.0015 * sr), lim = std::max (0, cross - jsRoundI (0.01 * sr));
        int on = pkI, run = 0;
        for (int i = pkI; i >= lim; --i)
        {
            if (std::abs ((double) x[(size_t) i]) >= thr) { on = i; run = 0; }
            else if (++run >= quiet) break;
        }
        return on;
    }
}

DetectResult detectHits (const F32& x, double sr, const DetectOptions& opt)
{
    const int bs = 32;
    const F32 env = blockEnv (x, bs);
    double maxDb = -240;
    for (float e : env) if (e > maxDb) maxDb = e;
    const int minGapB = std::max (1, jsRoundI ((opt.minGapMs / 1000.0) * sr / bs));
    const double rise = 10;
    const Curves cur = onsetCurves (env, bs, sr);

    double thr;
    if (! opt.thrDb)
    {
        struct Step { double t; int n; };
        std::vector<Step> steps;
        for (double t = maxDb - 1; t >= maxDb - 90; t -= 0.5) steps.push_back ({ t, (int) onsetsAt (cur, t, minGapB, rise).size() });
        struct Best { size_t i, j, len; };
        std::optional<Best> best;
        double bestDiff = INFINITY;
        for (size_t i = 0; i < steps.size();)
        {
            size_t j = i;
            while (j + 1 < steps.size() && steps[j + 1].n == steps[i].n) ++j;
            const double diff = std::abs ((double) steps[i].n - opt.expected);
            const size_t len = j - i + 1;
            if (diff < bestDiff || (diff == bestDiff && best && len > best->len)) { bestDiff = diff; best = Best { i, j, len }; }
            i = j + 1;
        }
        thr = best ? steps[(best->i + best->j) / 2].t : maxDb - 30;
    }
    else
        thr = *opt.thrDb;

    const auto blocks = onsetsAt (cur, thr, minGapB, rise);
    std::set<int> uniq;
    const double lin = std::pow (10.0, thr / 20.0);
    for (int b : blocks)
    {
        int cross = b * bs;
        const int e = std::min ((int) x.size(), cross + bs);
        for (int i = cross; i < e; ++i) if (std::abs ((double) x[(size_t) i]) >= lin) { cross = i; break; }
        uniq.insert (refineOnset (x, cross, sr));
    }
    return { std::vector<int> (uniq.begin(), uniq.end()), thr, maxDb };
}

F32 monoOf (const std::vector<F32>& chs)
{
    if (chs.size() == 1) return chs[0];
    const size_t len = chs[0].size();
    F32 mono (len, 0.0f);
    const double k = 1.0 / (double) chs.size();
    for (const auto& x : chs)
        for (size_t i = 0; i < len; ++i) mono[i] = (float) ((double) mono[i] + (double) x[i] * k);
    return mono;
}

std::optional<std::pair<int, int>> guessCounts (const std::vector<std::string>& names)
{
    static const std::regex re ("(?:^|[_\\- ])V(?:EL)?(\\d{1,2})[_\\- ]?RR(\\d{1,2})|RR(\\d{1,2})[_\\- ]V(?:EL)?(\\d{1,2})", std::regex::icase);
    for (const auto& n : names)
    {
        std::smatch m;
        if (std::regex_search (n, m, re))
            return m[1].matched ? std::make_pair (std::stoi (m[1]), std::stoi (m[2])) : std::make_pair (std::stoi (m[4]), std::stoi (m[3]));
    }
    return std::nullopt;
}
} // namespace ld::hl

#include "Rules.h"
#include "Prep.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ld::hl
{
AlignOptions rulesFor (const std::string& role)
{
    AlignOptions o;
    if (role == "sub")
    {
        o.role = "near";
        o.weight = { 1, 1, 0.35, 0.05, 0, 0, 0, 0 };   // 45-355 Hz: where a sub kick lives
        o.minMs = -2; o.maxMs = 25;                     // a speaker-cone mic can sit well behind the batter side
        o.keepThr = -1;                                 // always line up the lows: that is the sub's whole job
    }
    else if (role == "bottom")
    {
        o.role = "bottom";
        o.weight = { 0.6, 0.8, 1, 1, 1, 1.25, 1.5, 1.5 };   // the wires
        o.preferArrival = true;   // no lean to flipped: libraries often flip the bottom already; the sound decides
    }
    else if (role == "far") o.role = "far";
    else { o.role = "near"; o.preferArrival = true; }
    return o;
}

namespace
{
    /** Energy of x in [a, b), through n one-pole low-passes (or high-passes) at fc. */
    double bandEnergy (const F32& x, long a, long b, double sr, double fc, bool high)
    {
        a = std::max (0L, a); b = std::min ((long) x.size(), b);
        if (b <= a) return 0;
        const double k = std::exp (-2 * M_PI * fc / sr);
        double l1 = 0, l2 = 0, e = 0;
        for (long i = a; i < b; ++i)
        {
            const double v = x[(size_t) i];
            l1 = (1 - k) * v + k * l1; l2 = (1 - k) * l1 + k * l2;
            const double y = high ? v - l2 : l2;
            e += y * y;
        }
        return e;
    }
    double energy (const F32& x, long a, long b)
    {
        a = std::max (0L, a); b = std::min ((long) x.size(), b);
        double e = 0; for (long i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
        return e;
    }
}

std::string detectRole (const Session& S, int t)
{
    if (t < 0 || t >= (int) S.tracks.size() || t == S.refIdx || S.onsets.empty()) return "";
    const auto& x = S.tracks[(size_t) t].mono;
    const auto& lead = S.tracks[(size_t) S.refIdx].mono;
    const double sr = S.sr;
    std::vector<int> ons;
    for (size_t k = 0; k < S.onsets.size(); k += std::max<size_t> (1, S.onsets.size() / 16)) ons.push_back ((int) S.onsets[k]);

    // when it first hears each hit, after the lead
    const auto ra = arrivals (lead, ons, sr, 3), ma = arrivals (x, ons, sr, 80);
    F64 d; for (size_t k = 0; k < ra.size() && k < ma.size(); ++k) d.push_back (ma[k] - ra[k]);
    const double arrMs = median0 (d) / sr * 1000;

    // where its energy sits, and how much ring follows the hit
    double low = 0, high = 0, all = 0;
    for (size_t k = 0; k < ons.size(); ++k)
    {
        const long o = ons[k] + (long) (std::isfinite (ma[k]) ? ma[k] - ons[k] : 0);
        const long a = o, b = o + (long) (0.3 * sr);
        low += bandEnergy (x, a, b, sr, 150, false); high += bandEnergy (x, a, b, sr, 2000, true); all += energy (x, a, b);
    }
    if (all <= 0) return "";
    const double lowFrac = low / all, highFrac = high / all;
    // the lead's balance, to compare against
    double lHigh = 0, lAll = 0;
    for (int o : ons) { lHigh += bandEnergy (lead, o, o + (long) (0.3 * sr), sr, 2000, true); lAll += energy (lead, o, o + (long) (0.3 * sr)); }
    const double leadHigh = lAll > 0 ? lHigh / lAll : 0;
    if (std::getenv ("LD_RULES_DEBUG")) std::fprintf (stderr, "    [%s/%s] arr %.2f low %.3f high %.4f lead high %.4f\n", S.drum.c_str(), S.tracks[(size_t) t].mic.c_str(), arrMs, lowFrac, highFrac, leadHigh);
    if (lowFrac > 0.75 && highFrac < 0.008) return "sub";
    if (std::isfinite (arrMs) && arrMs > 4.5) return "far";
    // a close mic on the other side of the drum: much brighter than the lead (the wires)
    if (S.drum != "kick" && std::isfinite (arrMs) && std::abs (arrMs) < 2 && leadHigh > 0 && highFrac > 1.5 * leadHigh) return "bottom";
    return "near";
}

std::string effectiveRole (const Session& S, int t)
{
    const auto& tr = S.tracks[(size_t) t];
    if (! tr.roleOverride.empty()) return tr.roleOverride;
    const auto byName = roleOf (tr.mic + " " + tr.name);
    if (byName == "far") return "far";
    if (! tr.role.empty()) return tr.role;
    return byName == "far" ? "far" : byName == "bottom" ? "bottom" : "near";
}
} // namespace ld::hl

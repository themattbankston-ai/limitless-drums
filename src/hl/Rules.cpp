#include "Rules.h"
#include "Prep.h"
#include <algorithm>
#include <cmath>

namespace ld::hl
{
AlignOptions alignOptionsFor (const std::string& role)
{
    AlignOptions o;
    o.role = role == "far" ? "far" : role == "bottom" ? "bottom" : "near";
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
    double lHigh = 0, lLow = 0, lAll = 0;
    for (int o : ons)
    {
        lHigh += bandEnergy (lead, o, o + (long) (0.3 * sr), sr, 2000, true); lLow += bandEnergy (lead, o, o + (long) (0.3 * sr), sr, 150, false);
        lAll += energy (lead, o, o + (long) (0.3 * sr));
    }
    const double leadHigh = lAll > 0 ? lHigh / lAll : 0, leadLow = lAll > 0 ? lLow / lAll : 0;
    const bool kick = S.drum == "kick";
    // a room: hears the hit well after the lead. Only for mics whose name doesn't say what they are: on a kick
    // the overheads can hear it as late as a close room, and a delayed close mic is still a close mic
    const auto nm = juce::String (S.tracks[(size_t) t].mic).toLowerCase() + " ";   // the mic name only: the file name also names the drum
    bool named = false;
    for (auto w : { "oh", "overhead", "hat", "ride", "crash", "cym", "kick", "kik", "sub", "snare", "sn ", "top", "bottom", "btm", "tom", "rack", "floor" })
        if (nm.contains (w)) named = true;
    if (std::isfinite (arrMs) && arrMs > 4.5 && ! named) return "far";
    // a sub kick: on a kick, at least as low as the lead and with almost none of its beater click
    if (kick && lowFrac >= leadLow - 0.02 && leadHigh > 0 && highFrac < 0.35 * leadHigh) return "sub";
    // a close mic on the other side of the drum: much brighter than the lead (the wires)
    if (! kick && std::isfinite (arrMs) && std::abs (arrMs) < 2 && leadHigh > 0 && highFrac > 1.4 * leadHigh) return "bottom";
    return "near";
}

std::string effectiveRole (const Session& S, int t)
{
    const auto& tr = S.tracks[(size_t) t];
    if (! tr.roleOverride.empty()) return tr.roleOverride;
    const auto byName = roleOf (tr.mic + " " + tr.name);
    if (byName == "far") return "far";
    if (! tr.role.empty() && tr.role != "near") return tr.role;   // the sound adds what the name can't tell
    return byName == "far" ? "far" : byName == "bottom" ? "bottom" : "near";
}
} // namespace ld::hl

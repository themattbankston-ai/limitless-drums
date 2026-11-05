#include "Resolve.h"
#include <algorithm>

namespace ld::hl
{
namespace
{
    const std::array<std::string, 4> groups { "A", "B", "C", "D" };
    bool isGroup (const std::string& g) { return std::find (groups.begin(), groups.end(), g) != groups.end(); }
    int natPol (const MicAlign& m) { return m.autoResult && m.autoResult->asis <= -0.05 ? -1 : 1; }

    std::vector<int> groupMembers (const std::vector<MicAlign>& mics, int refIdx, const std::string& g)
    {
        std::vector<int> out;
        for (int i = 0; i < (int) mics.size(); ++i) if (i != refIdx && mics[(size_t) i].alignTo == g) out.push_back (i);
        return out;
    }

    int groupAnchor (const std::vector<MicAlign>& mics, int refIdx, const std::string& g, const std::array<std::string, 4>& anchors)
    {
        const auto mem = groupMembers (mics, refIdx, g);
        if (mem.empty()) return -1;
        const auto& want = anchors[(size_t) (std::find (groups.begin(), groups.end(), g) - groups.begin())];
        if (! want.empty() && want != "auto")
            for (int k : mem) if (mics[(size_t) k].mic == want) return k;
        auto arr = [&] (int k)
        {
            const auto& a = mics[(size_t) k].autoResult;
            return a ? (std::isfinite (a->arrival) ? a->arrival : a->lag) : INFINITY;
        };
        int best = mem[0];
        for (int k : mem) if (arr (k) < arr (best)) best = k;
        return best;
    }
}

bool resolveAlignment (std::vector<MicAlign>& mics, int refIdx, bool leadFlipAuto, const std::array<std::string, 4>& anchors)
{
    if (refIdx < 0 || refIdx >= (int) mics.size()) return false;
    auto& L = mics[(size_t) refIdx];
    if (! L.manualPol) L.pol = 1;
    auto setAp = [] (MicAlign& m, const std::vector<AllPass>& ap) { m.ap = m.apOn ? ap : std::vector<AllPass> {}; };
    auto isAnchor = [&] (int t) { return isGroup (mics[(size_t) t].alignTo) && groupAnchor (mics, refIdx, mics[(size_t) t].alignTo, anchors) == t; };
    auto amountOf = [] (const MicAlign& m) { return roleOf (m.mic + " " + m.name) == "far" ? std::clamp (m.amount, 0.0, 1.0) : 1.0; };

    for (int t = 0; t < (int) mics.size(); ++t)
    {
        auto& m = mics[(size_t) t];
        if (t == refIdx) { m.lag = 0; setAp (m, {}); continue; }
        if (isGroup (m.alignTo) && ! isAnchor (t)) continue;
        double lag = 0; int pol = 1; std::vector<AllPass> ap;
        const double amt = amountOf (m);
        if (m.alignTo == "lead" && m.autoResult) { lag = m.autoResult->lag * amt; pol = m.autoResult->pol; ap = m.autoResult->ap; }
        else if (m.alignTo == "pro") { lag = std::isfinite (m.proLag) ? m.proLag : 0; pol = m.proPol ? *m.proPol : natPol (m); }
        else pol = natPol (m);
        if (! m.manualLag) m.lag = lag;
        if (! m.manualPol) m.pol = pol;
        setAp (m, ap);
    }

    bool flipped = false;
    if (leadFlipAuto && ! L.manualPol)
    {
        int voters = 0, flips = 0;
        for (int t = 0; t < (int) mics.size(); ++t)
        {
            const auto& m = mics[(size_t) t];
            if (t != refIdx && m.alignTo == "lead" && m.autoResult && roleOf (m.mic + " " + m.name) != "bottom" && ! m.manualPol)
            { ++voters; if (m.pol < 0) ++flips; }
        }
        if (voters >= 2 && (double) flips / voters >= 2.0 / 3.0)
        {
            L.pol = -1; flipped = true;
            for (int t = 0; t < (int) mics.size(); ++t)
            {
                auto& m = mics[(size_t) t];
                if (t != refIdx && ! m.manualPol && (m.alignTo == "lead" || m.alignTo == "none" || m.alignTo == "pro" || isAnchor (t))) m.pol = -m.pol;
            }
        }
    }

    for (int t = 0; t < (int) mics.size(); ++t)
    {
        auto& m = mics[(size_t) t];
        if (t == refIdx || ! isGroup (m.alignTo) || isAnchor (t)) continue;
        const int a = groupAnchor (mics, refIdx, m.alignTo, anchors);
        const double amt = amountOf (m);
        const double anchorLag = a == refIdx ? 0 : mics[(size_t) a].lag;
        const double lag = m.grp ? anchorLag + m.grp->lag * amt : anchorLag;
        const int pol = (m.grp ? m.grp->pol : 1) * mics[(size_t) a].pol;
        if (! m.manualLag) m.lag = lag;
        if (! m.manualPol) m.pol = pol;
        setAp (m, m.grp ? m.grp->ap : std::vector<AllPass> {});
    }
    return flipped;
}

namespace
{
    constexpr int HALF = 16;
    F64 fracKernel (double f)
    {
        F64 w (2 * HALF);
        double s = 0;
        for (int j = 0; j < 2 * HALF; ++j)
        {
            const double t = (j - HALF + 1) - f;
            const double sinc = std::abs (t) < 1e-12 ? 1 : std::sin (M_PI * t) / (M_PI * t);
            const double a = (t + HALF) / (2.0 * HALF);
            const double win = 0.42 - 0.5 * std::cos (2 * M_PI * a) + 0.08 * std::cos (4 * M_PI * a);
            w[(size_t) j] = sinc * win; s += w[(size_t) j];
        }
        for (auto& v : w) v /= s;
        return w;
    }
}

F32 renderSeg (const F32& x, long start, long len, double lag, int pol, double g, bool sub)
{
    F32 out ((size_t) len, 0.0f);
    const long N = (long) x.size();
    const double k = pol * g;
    long I = (long) std::floor (lag);
    double f = lag - (double) I;
    if (! sub) { I = (long) jsRound (lag); f = 0; }
    if (f < 1e-6 || f > 1 - 1e-6)
    {
        if (f > 0.5) I += 1;
        for (long n = 0; n < len; ++n) { const long i = start + n + I; out[(size_t) n] = (i >= 0 && i < N) ? (float) (k * x[(size_t) i]) : 0.0f; }
        return out;
    }
    const auto w = fracKernel (f);
    for (long n = 0; n < len; ++n)
    {
        const long b = start + n + I - HALF + 1;
        double s = 0;
        if (b >= 0 && b + 2 * HALF <= N) { for (int j = 0; j < 2 * HALF; ++j) s += x[(size_t) (b + j)] * w[(size_t) j]; }
        else for (int j = 0; j < 2 * HALF; ++j) { const long i = b + j; if (i >= 0 && i < N) s += x[(size_t) i] * w[(size_t) j]; }
        out[(size_t) n] = (float) (k * s);
    }
    return out;
}
} // namespace ld::hl

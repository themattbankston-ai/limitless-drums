#include "Align.h"
#include "Fft.h"
#include <algorithm>
#include <map>
#include <optional>
#include <regex>

namespace ld::hl
{
const std::array<std::pair<double, double>, numBands> octaveBands { { { 45, 90 }, { 90, 180 }, { 180, 355 }, { 355, 710 },
                                                                      { 710, 1400 }, { 1400, 2800 }, { 2800, 5600 }, { 5600, 11200 } } };

double median0 (F64 a)
{
    a.erase (std::remove_if (a.begin(), a.end(), [] (double v) { return ! std::isfinite (v); }), a.end());
    if (a.empty()) return NAN;
    std::sort (a.begin(), a.end());
    const size_t m = a.size() >> 1;
    return a.size() % 2 ? a[m] : (a[m - 1] + a[m]) / 2;
}

F64 arrivals (const F32& x, const std::vector<int>& onsets, double sr, double maxMs, double rel, const F64& shift)
{
    const int pre = jsRoundI (0.003 * sr), maxD = jsRoundI (maxMs * sr / 1000), fl = jsRoundI (0.02 * sr), tail = jsRoundI (0.04 * sr);
    const int quiet = std::max (3, jsRoundI (0.0003 * sr));
    const int len = (int) x.size();
    F64 out;
    for (size_t k = 0; k < onsets.size(); ++k)
    {
        const int c0 = onsets[k] + jsRoundI (k < shift.size() ? shift[k] : 0.0);
        const int s0 = std::max (0, c0 - pre), s1 = std::min (len - 1, c0 + maxD);
        double e = 0; int ne = 0;
        for (int i = std::max (0, c0 - fl); i < s0; ++i) { e += (double) x[(size_t) i] * x[(size_t) i]; ++ne; }
        const double floor = ne ? std::sqrt (e / ne) : 0;
        double pk = 0;
        for (int i = s0; i < std::min (len, s1 + tail); ++i) { const double a = std::abs ((double) x[(size_t) i]); if (a > pk) pk = a; }
        if (pk <= 0) { out.push_back (NAN); continue; }
        const double thr = std::max (6 * floor, rel * pk);
        int c = -1;
        for (int i = s0; i <= s1; ++i) if (std::abs ((double) x[(size_t) i]) >= thr) { c = i; break; }
        if (c < 0) { out.push_back (NAN); continue; }
        int st = c, run = 0;
        const double low = 0.25 * thr;
        for (int i = c; i >= std::max (0, c - pre * 2); --i)
        {
            if (std::abs ((double) x[(size_t) i]) >= low) { st = i; run = 0; }
            else if (++run >= quiet) break;
        }
        out.push_back (st);
    }
    return out;
}

namespace
{
    struct XSpec
    {
        F64 Sr, Si, ER, EM;
        size_t N = 0, H = 0;
        double sr = 0;
        int neg = 0, mx = 0, used = 0;
        std::array<std::pair<int, int>, numBands> bandBins {};
        Bands bandNorm {}, w {};
    };

    XSpec xspec (const F32& ref, const F32& mic, const std::vector<int>& onsets, double sr, double Lms, int negS, int mxS, int emShift)
    {
        const int pre = jsRoundI (0.001 * sr), L = jsRoundI (Lms / 1000 * sr);
        size_t N = 1;
        while ((int) N < 2 * L + negS + mxS) N <<= 1;
        XSpec X;
        X.N = N; X.H = N / 2 + 1; X.sr = sr; X.neg = negS; X.mx = mxS;
        const size_t H = X.H;
        X.Sr.assign (H, 0); X.Si.assign (H, 0); X.ER.assign (H, 0); X.EM.assign (H, 0);
        const int t0 = (int) std::floor (L * 0.7);
        F64 tp ((size_t) L);
        for (int n = 0; n < L; ++n) tp[(size_t) n] = n < t0 ? 1 : 0.5 + 0.5 * std::cos (M_PI * (n - t0) / (double) (L - t0));
        F64 ar (N), ai (N), br (N), bi (N), mr (N), mi (N);
        const int micLen = (int) mic.size(), refLen = (int) ref.size();
        for (int o : onsets)
        {
            const int s = o - pre;
            if (s - negS < 0 || s + L + mxS >= micLen || s + L >= refLen || s + emShift + L >= micLen) continue;
            std::fill (ar.begin(), ar.end(), 0.0); std::fill (ai.begin(), ai.end(), 0.0);
            std::fill (br.begin(), br.end(), 0.0); std::fill (bi.begin(), bi.end(), 0.0);
            std::fill (mr.begin(), mr.end(), 0.0); std::fill (mi.begin(), mi.end(), 0.0);
            for (int n = 0; n < L; ++n) { ar[(size_t) n] = ref[(size_t) (s + n)] * tp[(size_t) n]; mr[(size_t) n] = mic[(size_t) (s + emShift + n)] * tp[(size_t) n]; }
            for (int n = 0, e = L + negS + mxS; n < e; ++n) br[(size_t) n] = mic[(size_t) (s - negS + n)];
            fft (ar, ai, false); fft (br, bi, false); fft (mr, mi, false);
            double ex = 0, em = 0;
            for (size_t k = 0; k < H; ++k) { ex += ar[k] * ar[k] + ai[k] * ai[k]; em += mr[k] * mr[k] + mi[k] * mi[k]; }
            if (ex < 1e-20 || em < 1e-20) continue;
            const double g = 1 / std::sqrt (ex * em);
            for (size_t k = 0; k < H; ++k)
            {
                const double xr = ar[k], xi = -ai[k], yr = br[k], yi = bi[k], cr = xr * yr - xi * yi, ci = xr * yi + xi * yr;
                const double w = 2 * M_PI * (double) k * negS / (double) N, c = std::cos (w), sn = std::sin (w);
                X.Sr[k] += g * (cr * c - ci * sn); X.Si[k] += g * (cr * sn + ci * c);
                X.ER[k] += g * (ar[k] * ar[k] + ai[k] * ai[k]); X.EM[k] += g * (mr[k] * mr[k] + mi[k] * mi[k]);
            }
            X.used++;
        }
        for (int b = 0; b < numBands; ++b)
        {
            X.bandBins[(size_t) b] = { (int) std::ceil (octaveBands[(size_t) b].first * N / sr),
                                       std::min ((int) H - 1, (int) std::floor (octaveBands[(size_t) b].second * N / sr)) };
            double a = 0, c = 0;
            for (int k = X.bandBins[(size_t) b].first; k <= X.bandBins[(size_t) b].second; ++k) { a += X.ER[(size_t) k]; c += X.EM[(size_t) k]; }
            const double v = std::sqrt (a * c);
            X.bandNorm[(size_t) b] = v != 0 ? v : 1e-30;
        }
        double mx = *std::max_element (X.bandNorm.begin(), X.bandNorm.end());
        if (mx == 0) mx = 1;
        for (int b = 0; b < numBands; ++b) X.w[(size_t) b] = std::max (0.15, std::sqrt (X.bandNorm[(size_t) b] / mx));
        return X;
    }

    struct ApResp { F64 re, im; };
    ApResp apResp (size_t N, size_t H, double sr, const std::vector<AllPass>& aps)
    {
        ApResp r { F64 (H, 1.0), F64 (H, 0.0) };
        for (auto& a : aps)
        {
            const double w0 = 2 * M_PI * a.fc / sr, al = std::sin (w0) / (2 * a.Q), cw = std::cos (w0);
            const double b0 = 1 - al, b1 = -2 * cw, b2 = 1 + al, a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
            for (size_t k = 0; k < H; ++k)
            {
                const double w = 2 * M_PI * (double) k / (double) N, c1 = std::cos (w), s1 = -std::sin (w), c2 = std::cos (2 * w), s2 = -std::sin (2 * w);
                const double nr = b0 + b1 * c1 + b2 * c2, ni = b1 * s1 + b2 * s2, dr = a0 + a1 * c1 + a2 * c2, di = a1 * s1 + a2 * s2, dd = dr * dr + di * di;
                const double hr = (nr * dr + ni * di) / dd, hi = (ni * dr - nr * di) / dd, rr = r.re[k] * hr - r.im[k] * hi;
                r.im[k] = r.re[k] * hi + r.im[k] * hr; r.re[k] = rr;
            }
        }
        return r;
    }

    Bands bandCoh (const XSpec& X, double tau, int pol, const ApResp* ap)
    {
        Bands out {};
        for (int b = 0; b < numBands; ++b)
        {
            double s = 0;
            for (int k = X.bandBins[(size_t) b].first; k <= X.bandBins[(size_t) b].second; ++k)
            {
                const double w = 2 * M_PI * k * tau / (double) X.N, c = std::cos (w), sn = std::sin (w);
                double zr = X.Sr[(size_t) k] * c - X.Si[(size_t) k] * sn, zi = X.Sr[(size_t) k] * sn + X.Si[(size_t) k] * c;
                if (ap) { const double r = zr * ap->re[(size_t) k] - zi * ap->im[(size_t) k]; zi = zr * ap->im[(size_t) k] + zi * ap->re[(size_t) k]; zr = r; }
                (void) zi;
                s += zr;
            }
            out[(size_t) b] = pol * s / X.bandNorm[(size_t) b];
        }
        return out;
    }

    double scoreOf (const std::vector<XSpec>& Xs, double tau, int pol, const std::vector<AllPass>* aps)
    {
        double s = 0, W = 0;
        for (auto& X : Xs)
        {
            ApResp r;
            const ApResp* ap = nullptr;
            if (aps && ! aps->empty()) { r = apResp (X.N, X.H, X.sr, *aps); ap = &r; }
            const auto c = bandCoh (X, tau, pol, ap);
            for (int b = 0; b < numBands; ++b) { s += X.w[(size_t) b] * c[(size_t) b]; W += X.w[(size_t) b]; }
        }
        return s / (W != 0 ? W : 1);
    }

    struct Cand { int lag; double sharp; };
    std::vector<Cand> phatLags (const XSpec& X, double lo, double hi, int minL, int maxL)
    {
        const size_t N = X.N, H = X.H;
        F64 re (N, 0.0), im (N, 0.0);
        const int k0 = (int) std::ceil (lo * N / X.sr), k1 = std::min ((int) H - 1, (int) std::floor (hi * N / X.sr));
        for (int k = k0; k <= k1; ++k)
        {
            const double m = std::hypot (X.Sr[(size_t) k], X.Si[(size_t) k]) + 1e-20;
            re[(size_t) k] = X.Sr[(size_t) k] / m; im[(size_t) k] = X.Si[(size_t) k] / m;
            if (k > 0 && (size_t) k < N / 2) { re[N - (size_t) k] = re[(size_t) k]; im[N - (size_t) k] = -im[(size_t) k]; }
        }
        fft (re, im, true);
        auto val = [&] (int lag) { return re[(size_t) (((lag % (int) N) + (int) N) % (int) N)]; };
        std::vector<std::pair<int, double>> out;
        for (int l = std::max (minL, -X.neg) + 1; l < std::min (maxL, X.mx); ++l)
        {
            const double a = std::abs (val (l));
            if (a >= std::abs (val (l - 1)) && a >= std::abs (val (l + 1))) out.push_back ({ l, a });
        }
        std::stable_sort (out.begin(), out.end(), [] (auto& p, auto& q) { return q.second < p.second; });
        std::vector<std::pair<int, double>> pick;
        for (auto& [l, a] : out)
        {
            bool ok = true;
            for (auto& p : pick) if (std::abs (p.first - l) < 6) ok = false;
            if (ok) pick.push_back ({ l, a });
            if (pick.size() >= 5) break;
        }
        double mean = 1;
        if (! out.empty()) { mean = 0; for (auto& p : out) mean += p.second; mean /= (double) out.size(); }
        std::vector<Cand> res;
        for (auto& [l, a] : pick) res.push_back ({ l, a / (mean != 0 ? mean : 1e-9) });
        return res;
    }

    struct Role { double mn, mxMs, keepThr; bool ap; };
    Role roleRange (const std::string& r)
    {
        if (r == "bottom") return { -1.5, 3, 0.05, true };
        if (r == "far") return { -2, 80, 0.03, false };
        return { -1, 8, 0.03, true };
    }
}

AlignResult alignMic (const F32& ref, const std::vector<F32>& chans, const std::vector<int>& onsets, double sr, const AlignOptions& o)
{
    AlignResult R;
    const std::string role = (o.role == "bottom" || o.role == "far") ? o.role : "near";
    const Role rr = roleRange (role);
    int minL = jsRoundI ((std::isnan (o.minMs) ? rr.mn : o.minMs) * sr / 1000);
    int maxL = jsRoundI ((std::isnan (o.maxMs) ? rr.mxMs : o.maxMs) * sr / 1000);
    double arrival = NAN;
    if (role == "far")
    {
        F32 mono;
        if (chans.size() == 1) mono = chans[0];
        else { mono.resize (chans[0].size()); for (size_t i = 0; i < mono.size(); ++i) mono[i] = (float) (((double) chans[0][i] + chans[1][i]) / 2); }
        const auto refArr = arrivals (ref, onsets, sr, 3);
        const auto arr = arrivals (mono, onsets, sr, std::isnan (o.maxMs) ? rr.mxMs : o.maxMs);
        F64 d; for (size_t k = 0; k < arr.size(); ++k) d.push_back (arr[k] - refArr[k]);
        arrival = median0 (d);
        if (std::isfinite (arrival)) { minL = std::max (minL, jsRoundI (arrival - 0.002 * sr)); maxL = std::min (maxL, jsRoundI (arrival + 0.004 * sr)); }
    }
    if (o.inPlace) { minL = 0; maxL = 0; }   // the mic stays where it was recorded
    const int negS = std::max (4, -minL + 4), mxS = maxL + 4;
    const int emS = std::isfinite (arrival) && arrival > 0 ? jsRoundI (arrival) : 0;
    std::vector<XSpec> Xl, Xs;
    for (auto& x : chans) Xl.push_back (xspec (ref, x, onsets, sr, 60, negS, mxS, emS));
    for (auto& x : chans) Xs.push_back (xspec (ref, x, onsets, sr, 12, negS, mxS, emS));
    if (! Xl[0].used) return R;

    // candidates, in insertion order (a JS Map iterates in insertion order)
    std::vector<int> order;
    std::map<int, double> cand;
    auto put = [&] (int l, double v) { if (! cand.count (l)) order.push_back (l); cand[l] = v; };
    put (0, 0);
    for (auto& X : Xs)
    {
        if (o.inPlace) break;   // the only candidate is "where it is"
        auto pk = phatLags (X, 300, 12000, minL, maxL);
        if (role == "far" && ! std::isfinite (arrival) && ! pk.empty())
        {
            const double top = pk[0].sharp;
            pk.erase (std::remove_if (pk.begin(), pk.end(), [&] (const Cand& c) { return c.sharp < 0.5 * top; }), pk.end());
            std::stable_sort (pk.begin(), pk.end(), [] (auto& p, auto& q) { return p.lag < q.lag; });
            if (pk.size() > 2) pk.resize (2);
        }
        for (auto& c : pk) put (c.lag, std::max (cand.count (c.lag) ? cand[c.lag] : 0.0, c.sharp));
    }
    if (! o.inPlace && std::isfinite (arrival) && role == "far") { const int a = jsRoundI (arrival); put (a, cand.count (a) && cand[a] != 0 ? cand[a] : 1); }
    for (double l : o.extra)
        if (std::isfinite (l) && l >= minL && l <= maxL) { const int a = jsRoundI (l); put (a, cand.count (a) && cand[a] != 0 ? cand[a] : 1); }

    struct Best { double v, tau; int pol; };
    std::optional<Best> best;
    for (int l0 : order)
        for (int pol : { 1, -1 })
        {
            std::vector<double> grid;
            if (l0 == 0) grid.push_back (0);
            else for (double t = l0 - 0.25e-3 * sr; t <= l0 + 0.25e-3 * sr + 1e-9; t += 0.25) if (t >= minL && t <= maxL) grid.push_back (t);
            for (double t : grid) { const double v = scoreOf (Xl, t, pol, nullptr); if (! best || v > best->v) best = Best { v, t, pol }; }
        }
    const double asis = scoreOf (Xl, 0, 1, nullptr);
    bool kept = false;
    const double keepThr = std::isnan (o.keepThr) ? rr.keepThr : o.keepThr;
    const bool followArrival = ! o.inPlace && role == "far" && std::isfinite (arrival) && arrival > 0.001 * sr;
    if (! best || (! followArrival && (best->v - asis < keepThr || (best->v < 0.2 && best->v - asis < 0.08)))) { best = Best { asis, 0, 1 }; kept = true; }

    std::vector<AllPass> aps;
    double cur = best->v;
    int pol = best->pol;
    if (o.allpass < 0 ? rr.ap : o.allpass != 0)
    {
        for (int s = 0; s < 2; ++s)
        {
            struct C { double v, fc, Q; int p; };
            std::optional<C> c;
            for (int i = 0; i < 40; ++i)
            {
                const double fc = 35 * std::pow (2500.0 / 35.0, i / 39.0);
                for (double Q : { 0.5, 0.71, 1.0, 1.6 })
                    for (int p : { pol, -pol })
                    {
                        auto trial = aps; trial.push_back ({ fc, Q });
                        const double v = scoreOf (Xl, best->tau, p, &trial);
                        if (! c || v > c->v) c = C { v, fc, Q, p };
                    }
            }
            if (c && c->v > cur + 0.03) { cur = c->v; pol = c->p; aps.push_back ({ jsRound (c->fc), c->Q }); }
            else break;
        }
    }
    ApResp apR;
    const ApResp* apP = nullptr;
    if (! aps.empty()) { apR = apResp (Xl[0].N, Xl[0].H, sr, aps); apP = &apR; }
    R.valid = true;
    R.bandsAfter = bandCoh (Xl[0], best->tau, pol, apP);
    R.bandsBefore = bandCoh (Xl[0], 0, 1, nullptr);
    double sharp = -INFINITY; for (auto& [l, v] : cand) sharp = std::max (sharp, v);
    R.lag = best->tau; R.pol = pol; R.ap = aps; R.kept = kept; R.role = role; R.asis = asis; R.score = cur; R.gain = cur - asis;
    R.conf = std::max (0.0, std::min (1.0, (cur - 0.1) / 0.5)); R.sharp = sharp; R.arrival = arrival; R.n = Xl[0].used;
    return R;
}

ScoreResult scoreMic (const F32& ref, const std::vector<F32>& chans, const std::vector<int>& onsets, double sr,
                      double lag, int pol, const std::vector<AllPass>& ap, double arrival)
{
    ScoreResult r;
    if (pol == 0) pol = 1;
    const int negS = std::max (4, (int) std::ceil (-std::min (0.0, lag)) + 8), mxS = std::max (4, (int) std::ceil (std::max (0.0, lag)) + 8);
    const int emS = arrival > 0 ? jsRoundI (arrival) : 0;
    std::vector<XSpec> Xl;
    for (auto& x : chans) Xl.push_back (xspec (ref, x, onsets, sr, 60, negS, mxS, emS));
    if (! Xl[0].used) return r;
    ApResp apR;
    const ApResp* apP = nullptr;
    if (! ap.empty()) { apR = apResp (Xl[0].N, Xl[0].H, sr, ap); apP = &apR; }
    r.valid = true;
    r.score = scoreOf (Xl, lag, pol, ap.empty() ? nullptr : &ap);
    r.asis = scoreOf (Xl, 0, 1, nullptr);
    r.bandsAfter = bandCoh (Xl[0], lag, pol, apP);
    r.bandsBefore = bandCoh (Xl[0], 0, 1, nullptr);
    return r;
}

F32 applyAllpass (const F32& x, double sr, const std::vector<AllPass>& aps)
{
    if (aps.empty()) return x;
    F32 y = x;
    for (auto& a : aps)
    {
        const double w0 = 2 * M_PI * a.fc / sr, al = std::sin (w0) / (2 * a.Q), cw = std::cos (w0), a0 = 1 + al;
        const double b0 = (1 - al) / a0, b1 = -2 * cw / a0, b2 = 1, a1 = -2 * cw / a0, a2 = (1 - al) / a0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        F32 z (y.size());
        for (size_t n = 0; n < y.size(); ++n)
        {
            const double v = y[n], out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = v; y2 = y1; y1 = out; z[n] = (float) out;
        }
        y = std::move (z);
    }
    return y;
}

std::string roleOf (const std::string& s)
{
    std::string n = s;
    std::transform (n.begin(), n.end(), n.begin(), ::tolower);
    static const std::regex bottom ("bottom|btm|\\bbot\\b|reso"), far ("room|\\brm|crush|crsh|amb|hall|far|trash|mono");
    if (std::regex_search (n, bottom)) return "bottom";
    if (std::regex_search (n, far)) return "far";
    return "near";
}

bool isRoomName (const std::string& s)
{
    static const std::regex re ("room|\\brm|crush|crsh|amb|hall|far|trash", std::regex::icase);
    return std::regex_search (s, re);
}
} // namespace ld::hl

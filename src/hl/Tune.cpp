#include "Tune.h"
#include "Fft.h"
#include "Resolve.h"
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

namespace ld::hl
{
double hzToMidi (double f, double a4) { return 69 + 12 * std::log2 (f / (a4 != 0 ? a4 : 440)); }
double midiToHz (double m, double a4) { return (a4 != 0 ? a4 : 440) * std::pow (2.0, (m - 69) / 12); }

std::string noteName (double m)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int r = jsRoundI (m);
    return std::string (names[((r % 12) + 12) % 12]) + std::to_string ((int) std::floor (r / 12.0) - 1);
}

std::optional<Pitch> detectPitch (const F32& x, long onset, double sr, const PitchOptions& o)
{
    const long s0 = onset + jsRoundI (o.skipMs * sr / 1000), L = std::max (64, jsRoundI (o.winMs * sr / 1000));
    size_t N = 1;
    while ((double) N < std::max ((double) L * 4, 65536.0 * sr / 44100.0)) N <<= 1;
    F64 re (N, 0.0), im (N, 0.0);
    for (long n = 0; n < L; ++n)
    {
        const long i = s0 + n;
        if (i >= 0 && i < (long) x.size()) re[(size_t) n] = x[(size_t) i] * (0.5 - 0.5 * std::cos (2 * M_PI * n / (double) (L - 1)));
    }
    fft (re, im, false);
    const long kmin = std::max (2L, (long) std::floor ((o.fmin != 0 ? o.fmin : 40) * N / sr));
    const long kmax = std::min ((long) N / 2 - 2, (long) std::ceil ((o.fmax != 0 ? o.fmax : 1000) * N / sr));
    auto mag = [&] (long k) { return std::hypot (re[(size_t) k], im[(size_t) k]); };
    std::vector<std::pair<long, double>> peaks;
    double best = 0, sum = 0;
    long cnt = 0;
    for (long k = kmin; k <= kmax; ++k)
    {
        const double m = mag (k);
        sum += m; ++cnt;
        if (m > mag (k - 1) && m >= mag (k + 1)) { peaks.push_back ({ k, m }); if (m > best) best = m; }
    }
    if (peaks.empty() || best <= 0) return std::nullopt;
    const double thr = best * std::pow (10.0, -o.relDb / 20);
    auto pk = *std::find_if (peaks.begin(), peaks.end(), [&] (auto& p) { return p.second >= thr; });
    const long k = pk.first;
    const double a = std::log (mag (k - 1) + 1e-20), b = std::log (mag (k) + 1e-20), c = std::log (mag (k + 1) + 1e-20);
    const double den = a - 2 * b + c, d = den < 0 ? std::max (-0.5, std::min (0.5, 0.5 * (a - c) / den)) : 0;
    return Pitch { (k + d) * sr / N, std::min (1.0, (pk.second / (sum / cnt) - 1) / 12), pk.second };
}

SessionPitch sessionPitch (const F32& x, const std::vector<long>& onsets, double sr, const PitchOptions& o)
{
    SessionPitch r;
    std::vector<std::optional<Pitch>> first;
    for (long on : onsets) first.push_back (detectPitch (x, on, sr, o));
    std::vector<long> order;
    std::map<long, double> bins;
    for (auto& p : first)
    {
        if (! p) continue;
        const long b = (long) jsRound (1200 * std::log2 (p->f / 55) / 20);
        for (int d = -2; d <= 2; ++d)
        {
            if (! bins.count (b + d)) { order.push_back (b + d); bins[b + d] = 0; }
            bins[b + d] += (p->conf + 0.05) * (1 - std::abs (d) * 0.3);
        }
    }
    if (order.empty()) { r.hits.assign (onsets.size(), std::nullopt); return r; }
    long best = 0;
    double bv = -1;
    for (long b : order) if (bins[b] > bv) { bv = bins[b]; best = b; }
    r.est = 55 * std::pow (2.0, best * 20 / 1200.0);
    PitchOptions narrow = o;
    narrow.fmin = r.est * std::pow (2.0, -2.5 / 12);
    narrow.fmax = r.est * std::pow (2.0, 2.5 / 12);
    narrow.relDb = 1;
    for (long on : onsets)
    {
        auto p = detectPitch (x, on, sr, narrow);
        r.hits.push_back (p && p->conf > 0.08 ? p : std::nullopt);
    }
    return r;
}

static double median (F64 a)
{
    a.erase (std::remove_if (a.begin(), a.end(), [] (double v) { return ! std::isfinite (v); }), a.end());
    if (a.empty()) return NAN;
    std::sort (a.begin(), a.end());
    const size_t m = a.size() >> 1;
    return a.size() % 2 ? a[m] : (a[m - 1] + a[m]) / 2;
}
static double centsOf (double f, double ref) { return 1200 * std::log2 (f / ref); }

TuneInfo computeTune (const SessionPitch& sp, const TuneSettings& s, int V, const std::vector<int>& velOf)
{
    TuneInfo t;
    if (sp.hits.empty()) return t;
    for (auto& p : sp.hits) t.f.push_back (p ? p->f : NAN);
    const double est = std::isfinite (sp.est) ? sp.est : median (t.f);
    F64 near;
    for (double x : t.f) if (std::isfinite (x) && std::abs (centsOf (x, est)) < 80) near.push_back (x);
    t.medF = ! near.empty() ? median (near) : median (t.f);
    if (! std::isfinite (t.medF)) return t;
    t.medMidi = hzToMidi (t.medF, s.a4);
    t.nearest = jsRoundI (t.medMidi);
    t.targetMidi = s.target == "nearest" ? t.nearest : std::stoi (s.target);
    t.targetF = midiToHz (t.targetMidi + s.cents / 100, s.a4);
    const double R = std::clamp (t.targetF / t.medF, 0.5, 2.0);
    const bool ok = ! velOf.empty();
    if (ok)
        for (int v = 0; v < V; ++v)
        {
            F64 fs;
            for (size_t i = 0; i < t.f.size(); ++i) if (velOf[i] == v) fs.push_back (t.f[i]);
            t.layerMed.push_back (median (fs));
        }
    for (size_t i = 0; i < t.f.size(); ++i)
    {
        const double fi = t.f[i];
        double ratio;
        if (! s.on) ratio = 1;
        else if (s.mode != "rr" || ! std::isfinite (fi)) ratio = R;
        else
        {
            const double lm = ok ? t.layerMed[(size_t) velOf[i]] : t.medF;
            ratio = ! std::isfinite (lm) ? R : R * std::pow (2.0, std::clamp (centsOf (lm, fi), -30.0, 30.0) / 1200);
        }
        t.ratios.push_back (ratio);
        t.before.push_back (std::isfinite (fi) ? centsOf (fi, t.targetF) : NAN);
        t.after.push_back (std::isfinite (fi) ? centsOf (fi * ratio, t.targetF) : NAN);
    }
    t.valid = true;
    return t;
}

namespace
{
    constexpr int RS_PH = 1024;
    constexpr double KB = 10;
    double besselI0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; if (t < 1e-12 * s) break; }
        return s;
    }
    struct Table { F32 T; int W; };
    std::shared_ptr<Table> rsTable (double cut, int taps)
    {
        auto t = std::make_shared<Table>();
        t->W = (int) std::ceil (taps / cut);
        const int W = t->W;
        t->T.assign ((size_t) (RS_PH + 1) * 2 * (size_t) W, 0.0f);
        const double i0b = besselI0 (KB);
        for (int p = 0; p <= RS_PH; ++p)
        {
            const double fr = (double) p / RS_PH;
            for (int j = 0; j < 2 * W; ++j)
            {
                const double tt = (j - W + 1) - fr, u = tt * cut, r = tt / W;
                const double sinc = std::abs (u) < 1e-9 ? 1 : std::sin (M_PI * u) / (M_PI * u);
                const double win = std::abs (r) >= 1 ? 0 : besselI0 (KB * std::sqrt (1 - r * r)) / i0b;
                t->T[(size_t) p * 2 * (size_t) W + (size_t) j] = (float) (cut * sinc * win);
            }
        }
        return t;
    }
    std::shared_ptr<Table> getTable (double cut, int taps)
    {
        static std::map<std::pair<long, int>, std::shared_ptr<Table>> cache;
        static std::mutex m;
        const long key = (long) jsRound (cut * 1000);
        std::lock_guard<std::mutex> lock (m);
        auto it = cache.find ({ key, taps });
        if (it != cache.end()) return it->second;
        auto t = rsTable (key / 1000.0, taps);
        if (cache.size() > 64) cache.erase (cache.begin());
        cache[{ key, taps }] = t;
        return t;
    }
    template <typename PosOf>
    void applyEdgeFade (F32& out, PosOf posOf, double hi, long fadeN)
    {
        if (! fadeN) return;
        for (size_t n = 0; n < out.size(); ++n)
        {
            const double d = hi - posOf ((double) n);
            if (d < fadeN) { const double u = std::max (0.0, d) / fadeN; out[n] = (float) (out[n] * u * u); }
        }
    }
}

F32 renderTuned (const F32& x, double onset, long pre, long outLen, double lag, int pol, double g, double step,
                 long lo, long hi, long fadeN, int taps)
{
    lo = std::max (0L, lo);
    hi = std::min ((long) x.size(), hi);
    if (std::abs (step - 1) < 1e-9)
    {
        auto out = renderSeg (x, (long) onset - pre, outLen, lag, pol, g, true);
        for (long n = 0; n < outLen; ++n) { const double pos = onset + lag + n - pre; if (pos < lo || pos >= hi) out[(size_t) n] = 0; }
        applyEdgeFade (out, [&] (double n) { return onset + lag + n - pre; }, (double) hi, fadeN);
        return out;
    }
    const double cut = std::min (1.0, 1 / step) * 0.96;
    const auto tab = getTable (cut, taps);
    const int W = tab->W, W2 = 2 * W;
    const float* T = tab->T.data();
    F32 out ((size_t) outLen, 0.0f);
    const double k = pol * g, base = onset + lag;
    for (long n = 0; n < outLen; ++n)
    {
        const double pos = base + (n - pre) * step;
        if (pos < lo - W || pos >= hi) continue;
        const long i0 = (long) std::floor (pos);
        const double fr = pos - i0, pf = fr * RS_PH;
        const long pi = (long) std::floor (pf);
        const double w1 = pf - pi, w0 = 1 - w1;
        const long r0 = pi * W2, r1 = r0 + W2, a = i0 - W + 1;
        double s = 0;
        if (a >= lo && a + W2 <= hi) { for (int j = 0; j < W2; ++j) s += x[(size_t) (a + j)] * (T[r0 + j] * w0 + T[r1 + j] * w1); }
        else for (int j = 0; j < W2; ++j) { const long idx = a + j; if (idx < lo || idx >= hi) continue; s += x[(size_t) idx] * (T[r0 + j] * w0 + T[r1 + j] * w1); }
        out[(size_t) n] = (float) (k * s);
    }
    applyEdgeFade (out, [&] (double n) { return base + (n - pre) * step; }, (double) hi, fadeN);
    return out;
}
} // namespace ld::hl

#include "Shape.h"
#include "Detect.h"
#include "Fft.h"
#include <algorithm>
#include <map>

namespace ld::hl
{
static F64 hannP (size_t N) { F64 w (N); for (size_t n = 0; n < N; ++n) w[n] = 0.5 - 0.5 * std::cos (2 * M_PI * n / (double) N); return w; }
static double cos01 (double u) { return u <= 0 ? 0 : u >= 1 ? 1 : 0.5 - 0.5 * std::cos (M_PI * u); }
static double toFixed (double v, int d) { const double m = std::pow (10.0, d); return std::round (v * m) / m; }

const F64& bands3()
{
    static const F64 b = [] { F64 v; for (int k = -16; k <= 13; ++k) v.push_back (1000 * std::pow (2.0, k / 3.0)); return v; }();
    return b;
}

// ------------------------------------------------------------------ library measurements
F64 bandSpec (const F32& x, long on, double sr, double maxN)
{
    const int N = 4096;
    const auto w = hannP (N);
    F64 P (N / 2 + 1, 0.0), re (N), im (N);
    const long a = std::max (0L, on - 64), L = std::max ((long) N, (long) std::min (maxN, jsRound (0.3 * sr)));
    int cnt = 0;
    for (long o = a; o < a + L; o += N / 2)
    {
        for (int n = 0; n < N; ++n) { const long i = o + n; re[(size_t) n] = (i >= 0 && i < (long) x.size() ? x[(size_t) i] : 0) * w[(size_t) n]; im[(size_t) n] = 0; }
        fft (re, im, false);
        for (int k = 0; k <= N / 2; ++k) P[(size_t) k] += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
        ++cnt;
    }
    F64 b;
    for (double fc : bands3())
    {
        const double lo = fc / std::pow (2.0, 1 / 6.0), hi = fc * std::pow (2.0, 1 / 6.0);
        double s = 0;
        for (long k = (long) std::ceil (lo * N / sr); k <= (long) std::floor (hi * N / sr) && k <= N / 2; ++k) s += P[(size_t) k];
        b.push_back (10 * std::log10 (s / cnt + 1e-30));
    }
    double tot = 0;
    for (double v : b) tot += std::pow (10.0, v / 10);
    tot = 10 * std::log10 (tot + 1e-30);
    for (auto& v : b) v -= tot;
    return b;
}

EnvStats envStats (const F32& x, long on, double sr, double maxN)
{
    const long B = jsRoundI (0.001 * sr), nb = std::max (0L, (long) std::floor (std::min ((double) x.size() - on, maxN) / B));
    F64 e ((size_t) nb);
    for (long b = 0; b < nb; ++b) { double s = 0; for (long i = on + b * B, z = i + B; i < z; ++i) s += (double) x[(size_t) i] * x[(size_t) i]; e[(size_t) b] = 10 * std::log10 (s / B + 1e-30); }
    EnvStats r;
    long pb = 0;
    for (long b = 0; b < std::min (nb, 80L); ++b) if (e[(size_t) b] > e[(size_t) pb]) pb = b;
    auto t = [&] (double rel) { long last = pb; for (long b = pb; b < nb; ++b) if (e[(size_t) b] >= e[(size_t) pb] - rel) last = b; return last - pb; };
    auto avg = [&] (long a, long b2) { double s = 0; int c = 0; for (long b = a; b < std::min (nb, b2); ++b) { s += std::pow (10.0, e[(size_t) b] / 10); ++c; } return c ? 10 * std::log10 (s / c + 1e-30) - e[(size_t) pb] : NAN; };
    if (nb == 0) return r;
    r.atk = pb; r.t40 = t (40); r.t60 = t (60); r.body = avg (pb + 10, pb + 50); r.sus = avg (pb + 60, pb + 200); r.tail = avg (pb + 400, pb + 800); r.pk = e[(size_t) pb];
    return r;
}

static F64 logSmooth (const F64& L, int oct3, double oct, long k0)
{
    (void) oct3;
    const size_t K = L.size();
    F64 cs (K + 1, 0.0), out (K);
    const double r = std::pow (2.0, oct);
    for (size_t k = 0; k < K; ++k) cs[k + 1] = cs[k] + L[k];
    for (size_t k = 0; k < K; ++k)
    {
        const long lo = std::max (k0 != 0 ? k0 : 1L, (long) std::floor (k / r)), hi = std::min ((long) K - 1, std::max (lo, (long) std::ceil (k * r)));
        out[k] = (cs[(size_t) hi + 1] - cs[(size_t) lo]) / (double) (hi - lo + 1);
    }
    return out;
}

// ------------------------------------------------------------------ resonance scan
ResScanResult resScan (const F32& x, const std::vector<long>& onsets, const std::vector<long>& ends, double sr, const ResScanOptions& o)
{
    const int N = sr > 64000 ? 16384 : 8192, K = N / 2 + 1;
    const auto w = hannP ((size_t) N);
    const double lo = o.lo != 0 ? o.lo : 250, hi = std::min (o.hi != 0 ? o.hi : 14000, sr * 0.45);
    const long kLo = (long) std::ceil (lo * N / sr), kHi = (long) std::floor (hi * N / sr);
    const int H = (int) onsets.size();
    ResScanResult res;
    res.N = (size_t) N;
    if (! H) return res;
    F64 re ((size_t) N), im ((size_t) N);
    const long base = std::max (1L, (long) std::floor (60.0 * N / sr));
    const double oct = o.oct != 0 ? o.oct : 1.0 / 3;
    auto ms = [&] (double v) { return (long) jsRound (v * sr / 1000); };
    struct Spectra { std::vector<F64> Ls; F64 S, E, X; };
    auto spectra = [&] (double a0, double a1)
    {
        Spectra sp;
        for (int h = 0; h < H; ++h)
        {
            const long a = onsets[(size_t) h] + ms (a0), e = std::min ({ ends[(size_t) h], onsets[(size_t) h] + ms (a1), (long) x.size() });
            F64 P ((size_t) K, 0.0);
            int cnt = 0;
            for (long s = a; s < std::max (a + 1, e - N / 2); s += N / 2)
            {
                for (int n = 0; n < N; ++n) { const long i = s + n; re[(size_t) n] = i < e ? x[(size_t) i] * w[(size_t) n] : 0; im[(size_t) n] = 0; }
                fft (re, im, false);
                for (int k = 0; k < K; ++k) P[(size_t) k] += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
                ++cnt;
            }
            F64 L ((size_t) K);
            double m = 0;
            for (int k = 0; k < K; ++k) { L[(size_t) k] = 10 * std::log10 (P[(size_t) k] / std::max (1, cnt) + 1e-30); if (k >= kLo && k <= kHi) m += L[(size_t) k]; }
            m /= std::max (1L, kHi - kLo + 1);
            for (auto& v : L) v -= m;
            sp.Ls.push_back (std::move (L));
        }
        sp.S.resize ((size_t) K);
        F64 col ((size_t) H);
        for (int k = 0; k < K; ++k)
        {
            for (int h = 0; h < H; ++h) col[(size_t) h] = sp.Ls[(size_t) h][(size_t) k];
            std::sort (col.begin(), col.end());
            sp.S[(size_t) k] = H % 2 ? col[(size_t) (H >> 1)] : (col[(size_t) (H / 2 - 1)] + col[(size_t) (H / 2)]) / 2;
        }
        auto E = logSmooth (sp.S, N, oct, base);
        F64 C ((size_t) K);
        for (int it = 0; it < 2; ++it) { for (int k = 0; k < K; ++k) C[(size_t) k] = std::min (sp.S[(size_t) k], E[(size_t) k] + 1); E = logSmooth (C, N, oct, base); }
        sp.E = E;
        sp.X.resize ((size_t) K);
        for (int k = 0; k < K; ++k) sp.X[(size_t) k] = sp.S[(size_t) k] - E[(size_t) k];
        return sp;
    };
    const double skip = o.skipMs, len = o.lenMs != 0 ? o.lenMs : 700;
    const auto M = spectra (o.ring ? skip + (o.lateMs != 0 ? o.lateMs : 100) : skip, len);
    std::optional<Spectra> Ea;
    if (o.ring) Ea = spectra (skip, skip + 70);
    const auto& Ls = M.Ls;
    const auto& S = M.S;
    const auto& E = M.E;
    const auto& X = M.X;
    F64 sd;
    for (long k = kLo; k <= kHi; k += 3)
    {
        double m1 = 0, m2 = 0;
        for (int h = 0; h < H; ++h) { const double v = Ls[(size_t) h][(size_t) k] - E[(size_t) k]; m1 += v; m2 += v * v; }
        m1 /= H;
        sd.push_back (std::sqrt (std::max (0.0, m2 / H - m1 * m1)));
    }
    std::sort (sd.begin(), sd.end());
    const double sig = sd.empty() ? 0 : sd[sd.size() >> 1];
    const double thr = std::max (o.thrDb, 5.6 * sig / std::sqrt ((double) H));
    F64 prot;
    for (double f : o.protect) if (f > 0) prot.push_back (f);
    auto ripple = [&] (long k)
    {
        const long a = std::max (kLo, (long) std::floor (k / 1.41)), b = std::min (kHi, (long) std::ceil (k * 1.41));
        F64 v;
        for (long j = a; j <= b; ++j) if (std::abs (j - k) > 4) v.push_back (X[(size_t) j]);
        if (v.size() < 8) return 0.0;
        std::sort (v.begin(), v.end());
        const double md = v[v.size() >> 1];
        F64 dv;
        for (double z : v) dv.push_back (std::abs (z - md));
        std::sort (dv.begin(), dv.end());
        return md + 1.4826 * dv[dv.size() >> 1];
    };
    long kf = 0;
    for (long k = (long) std::ceil (o.f0lo * N / sr); k <= (long) std::floor (o.f0hi * N / sr); ++k) if (! kf || S[(size_t) k] > S[(size_t) kf]) kf = k;
    if (o.autoProtect && kf) { prot.push_back (kf * sr / N); prot.push_back (1.59 * kf * sr / N); }
    std::vector<ResPeak> peaks;
    for (long k = std::max (kLo, 3L); k <= std::min (kHi, (long) K - 4); ++k)
    {
        if (X[(size_t) k] < thr) continue;
        bool isMax = true;
        for (int j = -3; j <= 3; ++j) if (j && X[(size_t) (k + j)] > X[(size_t) k]) { isMax = false; break; }
        if (! isMax) continue;
        if (X[(size_t) k] < 3 * ripple (k)) continue;
        double ringDb = NAN;
        if (Ea)
        {
            double e = -1e9;
            for (int j = -2; j <= 2; ++j) e = std::max (e, Ea->X[(size_t) (k + j)]);
            ringDb = X[(size_t) k] - e;
            if (ringDb < o.ringDb) continue;
        }
        double den = 2 * (S[(size_t) k - 1] - 2 * S[(size_t) k] + S[(size_t) k + 1]);
        if (den == 0 || std::isnan (den)) den = 1;
        const double d = (S[(size_t) k - 1] - S[(size_t) k + 1]) / den, kk = k + clampD (d, -0.5, 0.5), f = kk * sr / N;
        bool protectedF = false;
        for (double p : prot) if (std::abs (std::log2 (f / p)) < 0.25) protectedF = true;
        if (protectedF) continue;
        long l = k, r = k;
        while (l > kLo && X[(size_t) l] > X[(size_t) k] / 2) --l;
        while (r < kHi && X[(size_t) r] > X[(size_t) k] / 2) ++r;
        const double bw = std::max (2.5, (double) (r - l)) * sr / N, q = clampD (f / bw, 1.5, 16);
        F64 per;
        int okCount = 0;
        for (auto& L : Ls) { per.push_back (L[(size_t) k] - E[(size_t) k]); if (per.back() > thr * 0.6) ++okCount; }
        if ((double) okCount / H < o.consistency) continue;
        F64 srt = per;
        std::sort (srt.begin(), srt.end());
        double md = srt[per.size() >> 1];
        if (md == 0 || std::isnan (md)) md = 1;
        ResPeak pk;
        pk.f = toFixed (f, 1); pk.q = toFixed (q, 2); pk.ex = toFixed (X[(size_t) k], 2);
        if (std::isfinite (ringDb)) pk.ring = toFixed (ringDb, 1);
        for (double v : per) pk.hit.push_back (toFixed (clampD (v / std::max (0.5, md), 0, 1.6), 2));
        peaks.push_back (pk);
    }
    std::stable_sort (peaks.begin(), peaks.end(), [] (auto& p, auto& q) { return q.ex < p.ex; });
    std::vector<ResPeak> keep;
    for (auto& p : peaks)
    {
        bool near = false;
        for (auto& q : keep) if (std::abs (std::log2 (p.f / q.f)) < 1 / 12.0) near = true;
        if (! near) keep.push_back (p);
    }
    if ((int) keep.size() > o.max) keep.resize ((size_t) o.max);
    std::stable_sort (keep.begin(), keep.end(), [] (auto& p, auto& q) { return p.f < q.f; });
    res.f0 = toFixed (kf * sr / N, 1);
    res.peaks = keep;
    res.thr = toFixed (thr, 2);
    for (int j = 0; j < 64; ++j)
    {
        const double f = 100 * std::pow (160.0, j / 63.0);
        const long k = std::min ((long) K - 1, (long) jsRound (f * N / sr));
        res.env.push_back ({ toFixed (f, 0), toFixed (S[(size_t) k], 1), toFixed (E[(size_t) k], 1) });
    }
    return res;
}

// ------------------------------------------------------------------ zero-phase kernels and the tone/shape pass
namespace
{
    struct ZKernel { F64 g; long c, T; };
    ZKernel zeroKernel (const F64& db, int K)
    {
        F64 re ((size_t) K, 0.0), im ((size_t) K, 0.0);
        for (int k = 0; k <= K / 2; ++k) { const double g = std::pow (10.0, db[(size_t) k] / 20); re[(size_t) k] = g; if (k > 0 && k < K / 2) re[(size_t) (K - k)] = g; }
        fft (re, im, true);
        ZKernel z;
        z.T = K - 1; z.c = K / 2 - 1;
        z.g.resize ((size_t) z.T);
        for (long j = 0; j < z.T; ++j)
        {
            const double u = (double) j / (z.T - 1);
            z.g[(size_t) j] = re[(size_t) ((((j - z.c) % K) + K) % K)] * (0.42 - 0.5 * std::cos (2 * M_PI * u) + 0.08 * std::cos (4 * M_PI * u));
        }
        return z;
    }
    struct Spec { F64 re, im; };
    Spec kernelSpec (const ZKernel& k, int M)
    {
        Spec s { F64 ((size_t) M, 0.0), F64 ((size_t) M, 0.0) };
        std::copy (k.g.begin(), k.g.end(), s.re.begin());
        fft (s.re, s.im, false);
        return s;
    }
}

std::vector<long> fxOnsets (const F32& x, double sr, const std::vector<long>& ons, long L)
{
    std::vector<long> out;
    for (size_t h = 0; h < ons.size(); ++h)
    {
        const long on = ons[h];
        const long s0 = std::max (0L, on - jsRoundI (0.001 * sr)), s1 = std::min ({ L, on + jsRoundI (0.05 * sr), h + 1 < ons.size() ? ons[h + 1] : L });
        double pk = 0;
        for (long n = s0; n < s1; ++n) { const double v = x[(size_t) n] < 0 ? -x[(size_t) n] : x[(size_t) n]; if (v > pk) pk = v; }
        long m = s0;
        for (long n = s0; n < s1; ++n) if ((x[(size_t) n] < 0 ? -x[(size_t) n] : x[(size_t) n]) >= 0.1 * pk) { m = n; break; }
        for (long n = m; n > std::max (0L, m - jsRoundI (0.002 * sr)); --n) if ((x[(size_t) n] < 0 ? -x[(size_t) n] : x[(size_t) n]) < 0.02 * pk) { m = n; break; }
        out.push_back (m);
    }
    return out;
}

int repairClicks (F32& x, double sr, const std::vector<long>& ons)
{
    const long L = (long) x.size(), W = std::max (16, jsRoundI (0.005 * sr)), maxW = std::max (4, jsRoundI (0.0015 * sr)), skip = jsRoundI (0.025 * sr);
    F32 d2 ((size_t) L, 0.0f);
    for (long n = 1; n < L - 1; ++n) d2[(size_t) n] = (float) ((double) x[(size_t) n + 1] - 2.0 * x[(size_t) n] + x[(size_t) n - 1]);
    std::vector<uint8_t> ok ((size_t) L, 1);
    for (long on : ons) for (long n = std::max (0L, on - jsRoundI (0.002 * sr)); n < std::min (L, on + skip); ++n) ok[(size_t) n] = 0;
    double s = 0;
    for (long n = 0; n < std::min (L, 2 * W); ++n) s += (double) d2[(size_t) n] * d2[(size_t) n];
    int fixed = 0;
    for (long n = W; n < L - W - 1; ++n)
    {
        s += (double) d2[(size_t) (n + W)] * d2[(size_t) (n + W)] - (double) d2[(size_t) (n - W)] * d2[(size_t) (n - W)];
        if (! ok[(size_t) n]) continue;
        const double rms = std::sqrt (std::max (s, 0.0) / (2 * W)), a = std::abs ((double) d2[(size_t) n]);
        if (a < 1e-4 || a < 12 * rms) continue;
        long b = n;
        while (b + 1 < L - 2 && b - n < maxW && std::abs ((double) d2[(size_t) b + 1]) > 4 * rms) ++b;
        if (b - n >= maxW) { n = b; continue; }
        const long p0 = std::max (2L, n - 4), p1 = std::min (L - 3, b + 4);
        const double y0 = x[(size_t) p0], y1 = x[(size_t) p1], m0 = (double) x[(size_t) p0] - x[(size_t) p0 - 1], m1 = (double) x[(size_t) p1 + 1] - x[(size_t) p1];
        const long len = p1 - p0;
        for (long q = 1; q < len; ++q)
        {
            const double u = (double) q / len, h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u, h01 = -2 * u * u * u + 3 * u * u, h11 = u * u * u - u * u;
            x[(size_t) (p0 + q)] = (float) (h00 * y0 + h10 * len * m0 + h01 * y1 + h11 * len * m1);
        }
        ++fixed;
        n = p1;
    }
    return fixed;
}

Chs bleedGate (const Chs& chs, double sr, double depthDb)
{
    F32 x;
    if (chs.size() == 1) x = chs[0];
    else { x.resize (chs[0].size()); for (size_t n = 0; n < x.size(); ++n) x[n] = std::max (std::abs (chs[0][n]), std::abs (chs[1][n])); }
    const long L = (long) x.size();
    DetectOptions o1; o1.expected = 1; o1.minGapMs = 40;
    const auto d0 = detectHits (x, sr, o1);
    DetectOptions o2; o2.expected = 0; o2.minGapMs = 40; o2.thrDb = d0.maxDb - 26;
    const auto d = detectHits (x, sr, o2);
    const long B = std::max (16, jsRoundI (0.001 * sr)), nb = (L + B - 1) / B;
    F32 env ((size_t) nb);
    for (long b = 0; b < nb; ++b) { double m = 0; for (long n = b * B, e = std::min (L, n + B); n < e; ++n) { const double a = x[(size_t) n] < 0 ? -x[(size_t) n] : x[(size_t) n]; if (a > m) m = a; } env[(size_t) b] = (float) m; }
    F32 srt = env;
    std::sort (srt.begin(), srt.end());
    const double floor = srt.empty() ? 0 : srt[(size_t) std::floor (nb * 0.3)];
    std::vector<uint8_t> open ((size_t) nb, 0);
    for (size_t k = 0; k < d.onsets.size(); ++k)
    {
        const long on = d.onsets[k];
        const long b0 = std::max (0L, (long) std::floor (on / (double) B) - 3), nxt = k + 1 < d.onsets.size() ? (long) std::floor (d.onsets[k + 1] / (double) B) : nb;
        double pk = 0;
        long pb = b0;
        for (long b = b0; b < std::min (nb, b0 + 20); ++b) if (env[(size_t) b] > pk) { pk = env[(size_t) b]; pb = b; }
        long e = pb;
        while (e < nxt && e < nb && (env[(size_t) e] > std::max (pk * 0.01, floor * 2.5) || e - pb < 60)) ++e;
        for (long b = b0; b < std::min ({ e, nxt + 3, nb }); ++b) open[(size_t) b] = 1;
    }
    const double gmin = std::pow (10.0, -std::abs (depthDb) / 20), up = 1 / 3.0, dn = 1 / 60.0;
    F32 g ((size_t) nb);
    double v = gmin;
    for (long b = 0; b < nb; ++b) { const double tgt = open[(size_t) b] ? 1 : gmin; v = tgt > v ? std::min (tgt, v + up) : std::max (tgt, v - dn * (1 - gmin)); g[(size_t) b] = (float) v; }
    for (long b = nb - 2; b >= 0; --b) if (g[(size_t) b + 1] > g[(size_t) b]) g[(size_t) b] = (float) std::max ((double) g[(size_t) b], (double) g[(size_t) b + 1] - up);
    Chs res;
    for (auto& c : chs)
    {
        F32 y ((size_t) L);
        for (long n = 0; n < L; ++n)
        {
            const double p = (double) n / B - 0.5;
            const long i = std::max (0L, std::min (nb - 1, (long) std::floor (p)));
            const double f = std::min (1.0, std::max (0.0, p - i)), gg = g[(size_t) i] + (((double) g[(size_t) std::min (nb - 1, i + 1)] - g[(size_t) i]) * f);
            y[(size_t) n] = (float) (c[(size_t) n] * gg);
        }
        res.push_back (std::move (y));
    }
    return res;
}

Chs fxRenderMulti (Chs chs, double sr, const FxPlan& plan)
{
    if (plan.repair) for (auto& x : chs) repairClicks (x, sr, plan.ons);
    const long L = (long) chs[0].size();
    const int C = (int) chs.size(), K = plan.K, M = 4 * K;
    const long T = K - 1, c = K / 2 - 1, Hs = M - T + 1;
    Chs ys ((size_t) C, F32 ((size_t) L, 0.0f));
    std::map<int, Spec> specs;
    auto spec = [&] (int id) -> const Spec*
    {
        if (id < 0) return nullptr;
        auto it = specs.find (id);
        if (it == specs.end()) it = specs.emplace (id, kernelSpec (zeroKernel (plan.kern[(size_t) id], K), M)).first;
        return &it->second;
    };
    F64 re ((size_t) M), im ((size_t) M), r2 ((size_t) M), i2 ((size_t) M);
    auto ms = [&] (double v) { return (long) jsRound (v * sr / 1000); };
    F32 env1;
    if (C == 1) env1 = chs[0];
    else { env1.assign ((size_t) L, 0.0f); for (auto& x : chs) for (long n = 0; n < L; ++n) { const float v = x[(size_t) n] < 0 ? -x[(size_t) n] : x[(size_t) n]; if (v > env1[(size_t) n]) env1[(size_t) n] = v; } }
    const int nH = (int) plan.ons.size();
    const auto mons = fxOnsets (env1, sr, plan.ons, L);
    std::vector<std::vector<long>> monsC;
    if (C == 1) monsC.push_back (mons);
    else for (auto& x : chs) monsC.push_back (fxOnsets (x, sr, plan.ons, L));
    const long rampN = std::max (1L, ms (plan.rampMs));
    std::vector<long> bnd { 0 };
    for (int h = 1; h < nH; ++h) bnd.push_back (std::max (bnd[(size_t) h - 1] + 1, std::min (mons[(size_t) h] - ms (3), mons[(size_t) h])));
    bnd.push_back (L);
    const bool haveEnv = ! plan.env.empty();
    const double envStep = (plan.envStepMs != 0 ? plan.envStepMs : 0.25) * sr / 1000, envT0 = plan.envT0 * sr / 1000;
    auto envAt = [&] (double d)
    {
        if (! haveEnv) return 1.0;
        const double p = (d - envT0) / envStep;
        if (p <= 0) return plan.env[0];
        if (p >= (double) plan.env.size() - 1) return plan.env.back();
        const long q = (long) p;
        return plan.env[(size_t) q] + (plan.env[(size_t) q + 1] - plan.env[(size_t) q]) * (p - q);
    };
    const long fadeIn = ms (1.5), fadeOut = ms (3), tap = ms (3);
    F64 gPrev ((size_t) C, 1.0);
    for (int h = 0; h < nH; ++h)
    {
        const long a = bnd[(size_t) h], b = bnd[(size_t) h + 1];
        if (b <= a) continue;
        const Spec* H1 = spec (plan.tone[(size_t) h]);
        const Spec* H2 = spec (plan.cut.empty() ? -1 : plan.cut[(size_t) h]);
        const double kh = plan.hitK.empty() ? 1 : plan.hitK[(size_t) h];
        const long mEnd = h + 1 < nH ? mons[(size_t) h + 1] : L;
        if (! H1 && ! H2) for (int ci = 0; ci < C; ++ci) for (long n = a; n < b; ++n) ys[(size_t) ci][(size_t) n] = chs[(size_t) ci][(size_t) n];
        else
        {
            std::vector<std::pair<int, long>> blocks;
            for (long st = a; st < b; st += Hs) for (int ci = 0; ci < C; ++ci) blocks.push_back ({ ci, st });
            for (size_t q = 0; q < blocks.size(); q += 2)
            {
                const auto A = blocks[q];
                const bool haveB = q + 1 < blocks.size();
                const auto Bk = haveB ? blocks[q + 1] : std::pair<int, long> { -1, 0 };
                bool live = false;
                auto fill = [&] (int ci, long st, F64& out)
                {
                    const auto& x = chs[(size_t) ci];
                    for (int m = 0; m < M; ++m)
                    {
                        const long j = st - c + m;
                        double v = j >= 0 && j < L ? x[(size_t) j] : 0;
                        if (j >= mEnd) v = 0;
                        else if (j > mEnd - tap) v *= cos01 ((double) (mEnd - j) / tap);
                        out[(size_t) m] = v;
                        if (v != 0) live = true;
                    }
                };
                fill (A.first, A.second, re);
                if (haveB) fill (Bk.first, Bk.second, im); else std::fill (im.begin(), im.end(), 0.0);
                if (! live)
                {
                    for (auto blk : { A, Bk })
                    {
                        if (blk.first < 0) continue;
                        for (long t = blk.second, e = std::min (b, blk.second + Hs); t < e; ++t) ys[(size_t) blk.first][(size_t) t] = chs[(size_t) blk.first][(size_t) t];
                    }
                    continue;
                }
                fft (re, im, false);
                if (H1 && H2)
                {
                    for (int k = 0; k < M; ++k)
                    {
                        const double zr = re[(size_t) k], zi = im[(size_t) k];
                        r2[(size_t) k] = zr * H2->re[(size_t) k] - zi * H2->im[(size_t) k]; i2[(size_t) k] = zr * H2->im[(size_t) k] + zi * H2->re[(size_t) k];
                        re[(size_t) k] = zr * H1->re[(size_t) k] - zi * H1->im[(size_t) k]; im[(size_t) k] = zr * H1->im[(size_t) k] + zi * H1->re[(size_t) k];
                    }
                    fft (re, im, true); fft (r2, i2, true);
                }
                else
                {
                    const Spec* Hk = H1 ? H1 : H2;
                    for (int k = 0; k < M; ++k)
                    {
                        const double zr = re[(size_t) k], zi = im[(size_t) k];
                        re[(size_t) k] = zr * Hk->re[(size_t) k] - zi * Hk->im[(size_t) k]; im[(size_t) k] = zr * Hk->im[(size_t) k] + zi * Hk->re[(size_t) k];
                    }
                    fft (re, im, true);
                }
                auto emit = [&] (std::pair<int, long> blk, const F64& o1, const F64& o2)
                {
                    if (blk.first < 0) return;
                    const auto& x = chs[(size_t) blk.first];
                    auto& y = ys[(size_t) blk.first];
                    const long st = blk.second, e = std::min (b, st + Hs), mon = std::max (a, monsC[(size_t) blk.first][(size_t) h]);
                    for (long t = st; t < e; ++t)
                    {
                        const long n = t - st + T - 1;
                        double v1, v2 = 0;
                        bool hasV2 = false;
                        if (H1 && H2) { v1 = o1[(size_t) n]; v2 = o2[(size_t) n]; hasV2 = true; }
                        else if (H1) v1 = o1[(size_t) n];
                        else { v1 = x[(size_t) t]; v2 = o1[(size_t) n]; hasV2 = true; }
                        if (hasV2)
                        {
                            double w = 0;
                            if (t >= mon) { const double u = (double) (t - mon) / rampN; w = u >= 1 ? 1 : plan.spare + (1 - plan.spare) * cos01 (u); w *= kh; }
                            v1 += w * (v2 - v1);
                        }
                        double gt = cos01 ((double) (t - (mon - fadeIn)) / fadeIn);
                        if (h + 1 < nH) gt *= cos01 ((double) (b - t) / fadeOut);
                        y[(size_t) t] = (float) (x[(size_t) t] + gt * (v1 - x[(size_t) t]));
                    }
                };
                emit (A, re, r2);
                emit (Bk, im, i2);
            }
        }
        for (int ci = 0; ci < C; ++ci)
        {
            const auto& x = chs[(size_t) ci];
            auto& y = ys[(size_t) ci];
            const long mon = std::max (a, monsC[(size_t) ci][(size_t) h]);
            if (haveEnv) for (long t = a; t < b; ++t) y[(size_t) t] = (float) (y[(size_t) t] * envAt ((double) (t - mon)));
            double g = 1;
            if (plan.hasComp)
            {
                const long w0 = std::max (a, plan.ons[(size_t) h] + plan.compLag), w1 = std::min (b, w0 + plan.compWin);
                double p = 0, qq = 0;
                for (long n = w0; n < w1; ++n) { p += (double) x[(size_t) n] * x[(size_t) n]; qq += (double) y[(size_t) n] * y[(size_t) n]; }
                if (p > 1e-20 && qq > 1e-20) g = std::sqrt (p / qq);
            }
            const double e0 = haveEnv ? envAt (-1e9) : 1;
            const long gl = std::max (a + 1, mon - fadeIn), span = gl - a;
            const double from = gPrev[(size_t) ci] / (e0 != 0 ? e0 : 1);
            for (long t = a; t < b; ++t) { double gg = g; if (t < gl) gg = from + (g - from) * cos01 ((double) (t - a) / span); y[(size_t) t] = (float) (y[(size_t) t] * gg); }
            gPrev[(size_t) ci] = g * (haveEnv ? envAt ((double) (b - 1 - mon)) : 1);
        }
    }
    return ys;
}

// ------------------------------------------------------------------ linked resonance taming
void tameGain (std::vector<Chs>& out, const std::vector<std::vector<long>>& mo, const F64& gk, double sr)
{
    const int K = (int) gk.size();
    const long g0 = jsRoundI (0.008 * sr), gN = jsRoundI (0.04 * sr);
    for (size_t tt = 0; tt < out.size(); ++tt)
        for (auto& y : out[tt])
        {
            long i = 0;
            for (int k = 0; k < K; ++k)
            {
                const long end = std::max (0L, mo[tt][(size_t) k] - g0), st = std::max (i, end - gN);
                const double gp = k ? gk[(size_t) k - 1] : gk[0];
                for (; i < st; ++i) y[(size_t) i] = (float) (y[(size_t) i] * gp);
                for (; i < end; ++i) { const double u = (double) (i - st) / std::max (1L, end - st); y[(size_t) i] = (float) (y[(size_t) i] * (gp + (gk[(size_t) k] - gp) * (0.5 - 0.5 * std::cos (M_PI * u)))); }
            }
            for (; i < (long) y.size(); ++i) y[(size_t) i] = (float) (y[(size_t) i] * gk[(size_t) K - 1]);
        }
}

TameResult tameMulti (const std::vector<Chs>& tracks, double sr, const TameOptions& o)
{
    const int N = sr > 64000 ? 2048 : 1024, H = N >> 2, B = N / 2 + 1;
    F64 w ((size_t) N);
    for (int n = 0; n < N; ++n) w[(size_t) n] = std::sqrt (0.5 - 0.5 * std::cos (2 * M_PI * n / N));
    F64 re ((size_t) N), im ((size_t) N);
    const double d = std::max (0.0, std::min (1.0, o.depth)), sharp = std::max (0.0, std::min (1.0, o.sharp));
    const auto& ons = o.ons;
    const auto& bnd = o.bnd;
    const int K = (int) ons.size();
    F64 lag = o.lag.empty() ? F64 (tracks.size(), 0.0) : o.lag;
    const F32 sens = o.sens.empty() ? F32 ((size_t) B, 1.0f) : o.sens;
    const long spare = jsRoundI (o.spareMs * sr / 1000), rampN = jsRoundI (0.010 * sr);
    const double maxCut = 4 + 10 * d, ratio = 0.35 + 0.65 * d, Wo = 0.22 + 0.5 * (1 - sharp);
    std::vector<int> lo ((size_t) B), hi ((size_t) B);
    for (int k = 0; k < B; ++k)
    {
        lo[(size_t) k] = std::max (1, (int) std::floor (k / std::pow (2.0, Wo / 2)));
        hi[(size_t) k] = std::min (B - 1, std::max (lo[(size_t) k] + 2, (int) std::ceil (k * std::pow (2.0, Wo / 2))));
    }
    TameResult R;
    R.tracks = tracks;
    long Lmax = 0;
    for (auto& chs : tracks) Lmax = std::max (Lmax, (long) chs[0].size());
    const double tauF = std::exp (-H / (0.008 * sr));
    F64 mean ((size_t) B, 0.0);
    if (! o.mons.empty()) R.mo = o.mons;
    else
        for (size_t t = 0; t < tracks.size(); ++t)
        {
            const auto& x = tracks[t][0];
            const long sh = jsRoundI (lag[t]);
            std::vector<long> shifted;
            for (long v : ons) shifted.push_back (std::max (0L, v + sh));
            R.mo.push_back (fxOnsets (x, sr, shifted, (long) x.size()));
        }
    const auto& mo = R.mo;
    const long win = o.win != 0 ? o.win : jsRoundI (0.2 * sr);
    for (int k = 0; k < K; ++k)
    {
        long Lrel = LONG_MAX;
        for (size_t tt = 0; tt < tracks.size(); ++tt)
        {
            const long nx = k + 1 < K ? mo[tt][(size_t) k + 1] : (long) tracks[tt][0].size();
            Lrel = std::min (Lrel, nx - mo[tt][(size_t) k]);
        }
        Lrel -= N / 2 + jsRoundI (0.009 * sr);
        if (Lrel < N) continue;
        long Fm = std::max (1L, Lrel / H);
        {
            F64 en ((size_t) Fm, 0.0);
            for (size_t tt = 0; tt < tracks.size(); ++tt)
            {
                const auto& x = tracks[tt][0];
                const long o0 = mo[tt][(size_t) k];
                double tot = 0;
                F64 e0 ((size_t) Fm, 0.0);
                for (long f = 0; f < Fm; ++f)
                {
                    double s2 = 0;
                    for (long i = o0 + f * H, i1 = std::min ((long) x.size(), i + H); i < i1; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i];
                    e0[(size_t) f] = s2; tot += s2;
                }
                if (tot > 0) for (long f = 0; f < Fm; ++f) en[(size_t) f] += e0[(size_t) f] / tot;
            }
            double mx = 0;
            for (double v : en) mx = std::max (mx, v);
            long last = 0;
            for (long f = 0; f < Fm; ++f) if (en[(size_t) f] > mx * 1e-6) last = f;
            Fm = std::min (Fm, last + 4);
        }
        const long F = Fm, tailN = jsRoundI (0.03 * sr), LrelF = std::min (Lrel, F * H);
        F64 P ((size_t) (F * B), 0.0);
        struct Sp { F32 re, im; };
        std::vector<std::vector<Sp>> spec;
        for (auto& chs : tracks) { std::vector<Sp> v; for (size_t c = 0; c < chs.size(); ++c) v.push_back ({ F32 ((size_t) (F * B), 0.0f), F32 ((size_t) (F * B), 0.0f) }); spec.push_back (std::move (v)); }
        auto frame = [&] (const F32& x, long c0, F64& dst) { for (int n = 0; n < N; ++n) { const long i = c0 + n; dst[(size_t) n] = (i >= 0 && i < (long) x.size() ? x[(size_t) i] : 0) * w[(size_t) n]; } };
        for (size_t t = 0; t < tracks.size(); ++t)
        {
            const long off = mo[t][(size_t) k];
            F64 Pt ((size_t) (F * B), 0.0);
            double tot = 0;
            for (size_t c = 0; c < tracks[t].size(); ++c)
            {
                const auto& x = tracks[t][c];
                auto& S = spec[t][c];
                for (long f = 0; f < F; f += 2)
                {
                    frame (x, off + f * H - N / 2, re);
                    if (f + 1 < F) frame (x, off + (f + 1) * H - N / 2, im); else std::fill (im.begin(), im.end(), 0.0);
                    fft (re, im, false);
                    for (int b = 0; b < B; ++b)
                    {
                        const int j = (N - b) & (N - 1);
                        const double ar = 0.5 * (re[(size_t) b] + re[(size_t) j]), ai = 0.5 * (im[(size_t) b] - im[(size_t) j]), br = 0.5 * (im[(size_t) b] + im[(size_t) j]), bi = -0.5 * (re[(size_t) b] - re[(size_t) j]);
                        S.re[(size_t) (f * B + b)] = (float) ar; S.im[(size_t) (f * B + b)] = (float) ai;
                        const double p = ar * ar + ai * ai;
                        Pt[(size_t) (f * B + b)] += p; tot += p;
                        if (f + 1 < F)
                        {
                            S.re[(size_t) ((f + 1) * B + b)] = (float) br; S.im[(size_t) ((f + 1) * B + b)] = (float) bi;
                            const double qv = br * br + bi * bi;
                            Pt[(size_t) ((f + 1) * B + b)] += qv; tot += qv;
                        }
                    }
                }
            }
            if (tot > 0) { const double g = 1 / tot; for (size_t j = 0; j < Pt.size(); ++j) P[j] += Pt[j] * g; }
        }
        F64 G ((size_t) (F * B), 0.0), cs ((size_t) B + 1, 0.0);
        for (long f = 0; f < F; ++f)
        {
            const long row = f * B;
            double mx = 0;
            for (int b = 0; b < B; ++b) mx = std::max (mx, P[(size_t) (row + b)]);
            if (mx <= 0) continue;
            const long tc = f * H;
            double r = tc < spare ? 0 : tc < spare + rampN ? 0.5 - 0.5 * std::cos (M_PI * (tc - spare) / (double) rampN) : 1;
            r *= std::min (1.0, std::max (0.0, (double) (LrelF - tc) / tailN));
            if (r == 0) continue;
            cs[0] = 0;
            for (int b = 0; b < B; ++b) cs[(size_t) b + 1] = cs[(size_t) b] + 10 * std::log10 (P[(size_t) (row + b)] + mx * 1e-9);
            for (int b = 2; b < B; ++b)
            {
                if (P[(size_t) (row + b)] < mx * 1e-6) continue;
                const double envv = (cs[(size_t) hi[(size_t) b] + 1] - cs[(size_t) lo[(size_t) b]]) / (hi[(size_t) b] + 1 - lo[(size_t) b]);
                const double Lv = 10 * std::log10 (P[(size_t) (row + b)] + mx * 1e-9), ex = Lv - envv - 1.5;
                if (ex > 0) G[(size_t) (row + b)] = -std::min (maxCut, ratio * ex) * sens[(size_t) b] * r;
            }
        }
        for (long f = 0; f < F; ++f)
        {
            const long row = f * B;
            double p = G[(size_t) row], c = G[(size_t) row + 1];
            for (int b = 1; b < B - 1; ++b) { const double n = G[(size_t) (row + b + 1)], v = 0.25 * p + 0.5 * c + 0.25 * n; p = c; c = n; G[(size_t) (row + b)] = v; }
        }
        for (int b = 0; b < B; ++b)
        {
            double s = 0;
            for (long f = 0; f < F; ++f) { s = tauF * s + (1 - tauF) * G[(size_t) (f * B + b)]; G[(size_t) (f * B + b)] = std::min (G[(size_t) (f * B + b)], s); }
            s = 0;
            for (long f = F - 1; f >= 0; --f) { s = tauF * s + (1 - tauF) * G[(size_t) (f * B + b)]; G[(size_t) (f * B + b)] = 0.5 * (G[(size_t) (f * B + b)] + s); }
        }
        for (long f = 0; f < F; ++f) for (int b = 0; b < B; ++b) mean[(size_t) b] += G[(size_t) (f * B + b)] / F / K;
        for (size_t t = 0; t < tracks.size(); ++t)
        {
            const long off = mo[t][(size_t) k], m0 = off - jsRoundI (0.0006 * sr), m1 = off + jsRoundI (0.0002 * sr);
            const double sc = ! o.depthT.empty() && d > 0 ? std::max (0.0, o.depthT[t]) / d : 1;
            if (! (sc > 0)) continue;
            for (size_t c = 0; c < tracks[t].size(); ++c)
            {
                auto& y = R.tracks[t][c];
                F64 dl ((size_t) (F * H + N), 0.0);
                const auto& S = spec[t][c];
                auto live = [&] (long f) { if (f >= F) return false; const long row = f * B; for (int b = 0; b < B; ++b) if (G[(size_t) (row + b)] < -0.01) return true; return false; };
                for (long f = 0; f < F; f += 2)
                {
                    const bool l0 = live (f), l1 = live (f + 1);
                    if (! l0 && ! l1) continue;
                    std::fill (re.begin(), re.end(), 0.0); std::fill (im.begin(), im.end(), 0.0);
                    for (int b = 0; b < B; ++b)
                    {
                        const double g0 = l0 ? std::pow (10.0, sc * G[(size_t) (f * B + b)] / 20) - 1 : 0, g1 = l1 ? std::pow (10.0, sc * G[(size_t) ((f + 1) * B + b)] / 20) - 1 : 0;
                        const double ar = S.re[(size_t) (f * B + b)] * g0, ai = S.im[(size_t) (f * B + b)] * g0;
                        const double br = l1 ? S.re[(size_t) ((f + 1) * B + b)] * g1 : 0, bi = l1 ? S.im[(size_t) ((f + 1) * B + b)] * g1 : 0;
                        re[(size_t) b] = ar - bi; im[(size_t) b] = ai + br;
                        if (b && b < N / 2) { const int j = N - b; re[(size_t) j] = ar + bi; im[(size_t) j] = -ai + br; }
                    }
                    fft (re, im, true);
                    for (int n = 0; n < N; ++n) { dl[(size_t) (f * H + n)] += re[(size_t) n] * w[(size_t) n] * 0.5; if (l1) dl[(size_t) ((f + 1) * H + n)] += im[(size_t) n] * w[(size_t) n] * 0.5; }
                }
                const long base = off - N / 2, stop = (long) y.size();
                for (size_t j = 0; j < dl.size(); ++j)
                {
                    const long i = base + (long) j;
                    if (i < 0 || i >= stop || i < m0) continue;
                    const double mk = i < m1 ? 0.5 - 0.5 * std::cos (M_PI * (i - m0) / (double) (m1 - m0)) : 1;
                    y[(size_t) i] = (float) (y[(size_t) i] + dl[j] * mk);
                }
            }
        }
        (void) bnd;
    }
    F64 gk ((size_t) K, 1.0);
    for (int k = 0; k < K; ++k)
    {
        double eb = 0, ea = 0;
        for (size_t t = 0; t < tracks.size(); ++t)
        {
            if (! o.depthT.empty() && ! (o.depthT[t] > 0)) continue;
            const long s0 = mo[t][(size_t) k], s1 = std::min (s0 + win, (long) tracks[t][0].size());
            for (size_t c = 0; c < tracks[t].size(); ++c)
                for (long i = std::max (0L, s0); i < s1; ++i) { eb += (double) tracks[t][c][(size_t) i] * tracks[t][c][(size_t) i]; ea += (double) R.tracks[t][c][(size_t) i] * R.tracks[t][c][(size_t) i]; }
        }
        if (ea > 0 && eb > 0) gk[(size_t) k] = std::min (std::sqrt (eb / ea), 2.0);
    }
    if (! o.noGain) tameGain (R.tracks, mo, gk, sr);
    R.curve.resize ((size_t) B);
    for (int b = 0; b < B; ++b) R.curve[(size_t) b] = (float) mean[(size_t) b];
    R.gains = gk;
    return R;
}
} // namespace ld::hl

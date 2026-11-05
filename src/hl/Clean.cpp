#include "Clean.h"
#include "Fft.h"
#include <algorithm>
#include <set>

namespace ld::hl
{
static F64 hannP (size_t N) { F64 w (N); for (size_t n = 0; n < N; ++n) w[n] = 0.5 - 0.5 * std::cos (2 * M_PI * n / (double) N); return w; }

std::optional<NoisePrint> noisePrint (const F32& x, double sr, const std::vector<long>& onsets, size_t N, double bias)
{
    const size_t H = N >> 2, K = (N >> 1) + 1;
    const auto w = hannP (N);
    F64 re (N), im (N);
    if (x.size() < N) return std::nullopt;
    const long nf = (long) ((x.size() - N) / H) + 1;
    if (nf < 8) return std::nullopt;
    F64 en ((size_t) nf);
    for (long f = 0; f < nf; ++f) { double s = 0; const size_t o0 = (size_t) f * H; for (size_t n = 0; n < N; ++n) { const double v = x[o0 + n]; s += v * v; } en[(size_t) f] = s / N; }
    auto live = [&] (long f) { return f >= 0 && f < nf && en[(size_t) f] > 1e-14; };
    const long edge = (long) std::ceil (0.08 * sr / H) + 3;
    std::set<long> cut;
    for (long on : onsets) cut.insert (std::max (0L, (long) std::floor (on / (double) H) - 1));
    struct Run { long a, b; bool cont; };
    std::vector<Run> runs;
    long a = -1;
    for (long f = 0; f <= nf; ++f)
    {
        const bool lv = f < nf && live (f);
        if (lv && a < 0) a = f;
        if (a >= 0 && (! lv || (cut.count (f) && f > a))) { if (f - a >= 12) runs.push_back ({ a, f, lv }); a = lv ? f : -1; }
    }
    std::vector<F64> mins;
    F64 peaks;
    for (auto& r : runs)
    {
        F64 P (K, 0.0), mn (K, INFINITY);
        int n0 = 0;
        double pk = 0;
        const long last = r.cont ? r.b : r.b - edge;
        for (long f = r.a; f < r.b; ++f)
        {
            const size_t o0 = (size_t) f * H;
            for (size_t n = 0; n < N; ++n) { re[n] = x[o0 + n] * w[n]; im[n] = 0; }
            fft (re, im, false);
            for (size_t k = 0; k < K; ++k) { const double p = re[k] * re[k] + im[k] * im[k]; P[k] = n0 ? 0.8 * P[k] + 0.2 * p : p; }
            ++n0;
            pk = std::max (pk, en[(size_t) f]);
            if (n0 > 4 && f < last) for (size_t k = 0; k < K; ++k) if (P[k] < mn[k]) mn[k] = P[k];
        }
        if (std::isfinite (mn[1])) { mins.push_back (mn); peaks.push_back (10 * std::log10 (pk)); }
    }
    const int R = (int) mins.size();
    if (R < 3) return std::nullopt;
    F64 psd (K), trustK (K), v ((size_t) R);
    double pm = 0; for (double p : peaks) pm += p; pm /= R;
    double pv = 0; for (double p : peaks) pv += (p - pm) * (p - pm);
    for (size_t k = 0; k < K; ++k)
    {
        for (int r = 0; r < R; ++r) v[(size_t) r] = 10 * std::log10 (mins[(size_t) r][k] + 1e-30);
        F64 srt = v; std::sort (srt.begin(), srt.end());
        const double M = srt[(size_t) (R >> 1)], spread = srt[(size_t) std::floor (R * 0.75)] - srt[(size_t) std::floor (R * 0.25)];
        double vm = 0; for (double q : v) vm += q; vm /= R;
        double c = 0, vv = 0;
        for (int r = 0; r < R; ++r) { c += (v[(size_t) r] - vm) * (peaks[(size_t) r] - pm); vv += (v[(size_t) r] - vm) * (v[(size_t) r] - vm); }
        const double corr = pv > 1e-9 && vv > 1e-9 ? c / std::sqrt (pv * vv) : 0;
        const double trust = std::min (1.0, std::max (0.0, (0.6 - corr) / 0.4)) * std::min (1.0, std::max (0.0, (10 - spread) / 6));
        trustK[k] = trust;
        psd[k] = std::pow (10.0, (M - (1 - trust) * 60 - (trust < 0.5 ? 30 : 0)) / 10) * bias;
    }
    NoisePrint np;
    np.N = N;
    np.psd.resize (K);
    for (size_t k = 0; k < K; ++k)
    {
        const double p = psd[k > 0 ? k - 1 : 0], q = psd[k], r = psd[std::min (K - 1, k + 1)];
        np.psd[k] = std::max (std::min (p, q), std::min (std::max (p, q), r));
    }
    double tot = 0, tr = 0;
    for (size_t k = 1; k < K; ++k) { tot += np.psd[k] / bias; tr += trustK[k]; }
    for (int base : { 50, 60 })
    {
        int hits = 0;
        for (int h = 1; h <= 6; ++h)
        {
            const long k = jsRoundI (base * h * (double) N / sr);
            if (k + 20 >= (long) K) break;
            F64 loc;
            for (int j = -20; j <= 20; ++j) if (std::abs (j) > 2) loc.push_back (np.psd[(size_t) (k + j)]);
            std::sort (loc.begin(), loc.end());
            const double pk = std::max ({ np.psd[(size_t) k - 1], np.psd[(size_t) k], np.psd[(size_t) k + 1] });
            if (pk > loc[loc.size() >> 1] * 10 && trustK[(size_t) k] > 0.5) ++hits;
        }
        if (hits >= 3) np.hum.push_back (base);
    }
    np.runs = R;
    np.floorDb = 10 * std::log10 (tot * 2 / ((double) N * N * 0.375) + 1e-30);
    np.trust = tr / (double) (K - 1);
    return np;
}

F32 denoise (const F32& x, double sr, const NoisePrint* print, double reductionDb, double os, double releaseS)
{
    if (! print) return x;
    const size_t N = print->N, H = N >> 2, K = (N >> 1) + 1;
    const auto w = hannP (N);
    const auto& noise = print->psd;
    const double gmin = std::pow (10.0, -reductionDb / 20), a = 0.98, rel = std::exp (-(double) H / (sr * releaseS));
    const long L = (long) x.size();
    F64 y ((size_t) L + 2 * N, 0.0), re (N), im (N);
    F64 Gp (K, 1.0), gp (K, 1.0), G (K), Gs (K), Ar (K), Ai (K), Br (K), Bi (K), nkv (K);
    std::vector<int> width (K);
    for (size_t k = 0; k < K; ++k) { const double f = k * sr / N; width[k] = f < 1500 ? 1 : f < 6000 ? 2 : 3; nkv[k] = noise[k] * os + 1e-30; }
    auto gain = [&] (F64& Xr, F64& Xi, bool liveF)
    {
        if (! liveF) { std::fill (Gp.begin(), Gp.end(), 1.0); std::fill (gp.begin(), gp.end(), 1.0); return; }
        for (size_t k = 0; k < K; ++k)
        {
            const double g = (Xr[k] * Xr[k] + Xi[k] * Xi[k]) / nkv[k];
            const double xi = std::max (a * Gp[k] * Gp[k] * gp[k] + (1 - a) * std::max (g - 1, 0.0), 1e-4);
            G[k] = xi / (1 + xi); gp[k] = g;
        }
        for (size_t k = 0; k < K; ++k)
        {
            const int d = width[k];
            double s = 0, c = 0;
            for (int j = -d; j <= d; ++j) { const long q = (long) k + j; if (q >= 0 && q < (long) K) { const double wt = d + 1 - std::abs (j); s += G[(size_t) q] * wt; c += wt; } }
            Gs[k] = s / c;
        }
        for (size_t k = 0; k < K; ++k)
        {
            double v = Gs[k] > Gp[k] * rel ? Gs[k] : Gp[k] * rel;
            v = v < gmin ? gmin : v > 1 ? 1 : v;
            Gp[k] = v; Xr[k] *= v; Xi[k] *= v;
        }
    };
    for (long o0 = -(long) N + (long) H; o0 < L; o0 += 2 * (long) H)
    {
        const long o1 = o0 + (long) H;
        bool liveA = false, liveB = false;
        for (size_t n = 0; n < N; ++n)
        {
            const long i = o0 + (long) n, j = o1 + (long) n;
            const double va = i >= 0 && i < L ? x[(size_t) i] : 0, vb = j >= 0 && j < L ? x[(size_t) j] : 0;
            re[n] = va * w[n]; im[n] = vb * w[n];
            if (va != 0) liveA = true;
            if (vb != 0) liveB = true;
        }
        if (! liveA && ! liveB) { std::fill (Gp.begin(), Gp.end(), 1.0); std::fill (gp.begin(), gp.end(), 1.0); continue; }
        fft (re, im, false);
        for (size_t k = 0; k < K; ++k)
        {
            const size_t m = (N - k) % N;
            const double zr = re[k], zi = im[k], cr = re[m], ci = -im[m];
            Ar[k] = (zr + cr) / 2; Ai[k] = (zi + ci) / 2;
            Br[k] = (zi - ci) / 2; Bi[k] = -(zr - cr) / 2;
        }
        gain (Ar, Ai, liveA);
        gain (Br, Bi, liveB);
        for (size_t k = 0; k < K; ++k)
        {
            re[k] = Ar[k] - Bi[k]; im[k] = Ai[k] + Br[k];
            if (k > 0 && k < N / 2) { re[N - k] = Ar[k] + Bi[k]; im[N - k] = -Ai[k] + Br[k]; }
        }
        fft (re, im, true);
        for (size_t n = 0; n < N; ++n)
        {
            const long i = o0 + (long) n, j = o1 + (long) n;
            if (i >= 0) y[(size_t) i] += re[n] * w[n] / 1.5;
            if (j >= 0) y[(size_t) j] += im[n] * w[n] / 1.5;
        }
    }
    F32 out ((size_t) L);
    for (long n = 0; n < L; ++n) out[(size_t) n] = (float) y[(size_t) n];
    for (long n = 0; n < L;)
    {
        if (x[(size_t) n] != 0) { ++n; continue; }
        long m = n;
        while (m < L && x[(size_t) m] == 0) ++m;
        if (m - n >= 32) std::fill (out.begin() + n, out.begin() + m, 0.0f);
        n = m;
    }
    return out;
}

TailPlan tailPlan (const Chs& chs, double sr, double peak, double floorRel, double minRate)
{
    const long L = (long) chs[0].size(), B = std::max (32, jsRoundI (0.005 * sr)), nb = L / B;
    if (nb < 8) return { L, std::min (L, (long) jsRound (0.003 * sr)), 0 };
    double pk = peak;
    if (pk == 0) for (auto& x : chs) for (float s : x) { const double a = s < 0 ? -s : s; if (a > pk) pk = a; }
    if (pk <= 0) return { L, 0, 0 };
    const double ref = 20 * std::log10 (pk);
    F64 env ((size_t) nb);
    for (long b = 0; b < nb; ++b)
    {
        double s = 0;
        for (auto& x : chs) for (long n = b * B, e = n + B; n < e; ++n) s += (double) x[(size_t) n] * x[(size_t) n];
        env[(size_t) b] = 10 * std::log10 (s / (double) (B * (long) chs.size()) + 1e-30) - ref;
    }
    const long hold = jsRoundI (0.02 * sr);
    long last = -1;
    for (long b = nb - 1; b >= 0; --b) if (env[(size_t) b] > floorRel) { last = b; break; }
    if (last < 0) return { std::min (L, hold), std::min (L, hold), 0 };
    if (last < nb - 6) { const long end = std::min (L, (last + 1) * B + hold); return { end, std::min (hold, end), 0 }; }
    const long nfit = std::max (6L, std::min (nb - 1, (long) jsRound (0.3 * sr / B))), b0 = nb - nfit;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (long b = b0; b < nb; ++b) { const double u = (double) (b - b0); sx += u; sy += env[(size_t) b]; sxx += u * u; sxy += u * env[(size_t) b]; }
    const double slope = (nfit * sxy - sx * sy) / std::max (1e-9, nfit * sxx - sx * sx);
    double lend = -INFINITY;
    for (long b = nb - 4; b < nb; ++b) lend = std::max (lend, env[(size_t) b]);
    const double need = std::max (0.0, lend - floorRel);
    const double rate = std::max (-slope * sr / B * 1.5, minRate);
    const long F = std::max ((long) jsRound (0.03 * sr), std::min ((long) jsRound (L * 0.6), (long) jsRound (need / rate * sr)));
    return { L, F, need };
}

Chs applyTail (const Chs& chs, const TailPlan& plan, double sr)
{
    Chs out;
    for (auto& x : chs) out.push_back (plan.end < (long) x.size() ? F32 (x.begin(), x.begin() + plan.end) : x);
    const long L = plan.end, F = std::min (plan.F, L), c = std::min (F, (long) jsRound (0.003 * sr));
    for (auto& y : out)
        for (long n = 0; n < F; ++n)
        {
            const double u = (n + 1) / (double) F;
            const long i = L - F + n;
            double g = plan.need != 0 ? std::pow (10.0, -plan.need * u / 20) : 1;
            if (plan.need == 0 || n >= F - c)
            {
                const double v = plan.need != 0 ? (n - (F - c) + 1) / (double) c : u;
                const double cs = std::cos (std::min (1.0, v) * M_PI / 2);
                g *= cs * cs;
            }
            y[(size_t) i] = (float) (y[(size_t) i] * g);
        }
    return out;
}
} // namespace ld::hl

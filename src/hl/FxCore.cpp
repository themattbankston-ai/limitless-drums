#include "FxCore.h"
#include "Fft.h"

namespace ld::hl
{
double sos2 (double f, double fc, double b2, double b1, double b0, double a2, double a1, double a0)
{
    const double w = f / fc, w2 = w * w, nr = b0 - b2 * w2, ni = b1 * w, dr = a0 - a2 * w2, di = a1 * w;
    return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di + 1e-300));
}

double bandMag (const juce::var& b, double f)
{
    const double fc = std::max (5.0, (double) b.getProperty ("f", 0.0)), G = orr (b, "g", 0), Q = std::max (0.05, orr (b, "q", 0.71));
    const auto type = str (b, "type");
    if (type == "bell") { const double A = dbl (G / 2); return sos2 (f, fc, 1, A / Q, 1, 1, 1 / (A * Q), 1); }
    if (type == "lowshelf") { const double A = dbl (G / 2), sA = std::sqrt (A); return A * sos2 (f, fc, 1, sA / Q, A, A, sA / Q, 1); }
    if (type == "highshelf") { const double A = dbl (G / 2), sA = std::sqrt (A); return A * sos2 (f, fc, A, sA / Q, 1, 1, sA / Q, A); }
    if (type == "tilt") { const double A = dbl (G / 4), sA = std::sqrt (A); return sos2 (f, fc, A, sA / 0.6, 1, 1, sA / 0.6, A) / A; }
    if (type == "notch") return sos2 (f, fc, 1, 0, 1, 1, 1 / Q, 1);
    if (type == "bandpass") return sos2 (f, fc, 0, 1 / Q, 0, 1, 1 / Q, 1);
    if (type == "lowcut" || type == "highcut")
    {
        const int n = std::max (1, jsRoundI (orr (b, "slope", 12) / 6));
        const double x = type == "lowcut" ? fc / std::max (f, 1e-3) : f / fc;
        if (n == 2) return type == "lowcut" ? sos2 (f, fc, 1, 0, 0, 1, 1 / Q, 1) : sos2 (f, fc, 0, 0, 1, 1, 1 / Q, 1);
        return 1 / std::sqrt (1 + std::pow (x, 2 * n));
    }
    return 1;
}

Kernel magToKernel (const F64& M, size_t N, bool minPhase)
{
    F64 re (N, 0.0), im (N, 0.0);
    const size_t H = N >> 1;
    if (minPhase)
    {
        for (size_t k = 0; k <= H; ++k) { const double v = std::log (std::max (M[k], 1e-7)); re[k] = v; if (k && k < H) re[N - k] = v; }
        fft (re, im, true);
        for (size_t n = 1; n < H; ++n) { re[n] *= 2; im[n] = 0; }
        im[0] = 0; im[H] = 0;
        for (size_t n = H + 1; n < N; ++n) { re[n] = 0; im[n] = 0; }
        fft (re, im, false);
        for (size_t k = 0; k < N; ++k) { const double e = std::exp (re[k]), p = im[k]; re[k] = e * std::cos (p); im[k] = e * std::sin (p); }
        fft (re, im, true);
        Kernel K;
        K.h.resize (N);
        const size_t tail = N >> 3;
        for (size_t n = 0; n < N; ++n) K.h[n] = re[n] * (n > N - tail ? 0.5 + 0.5 * std::cos (M_PI * (double) (n - (N - tail)) / (double) tail) : 1);
        return K;
    }
    for (size_t k = 0; k <= H; ++k) { re[k] = M[k]; if (k && k < H) re[N - k] = M[k]; }
    fft (re, im, true);
    Kernel K;
    K.h.resize (N);
    for (size_t n = 0; n < N; ++n) { const size_t m = (n + H) % N; const double w = 0.5 - 0.5 * std::cos (2 * M_PI * (n + 0.5) / (double) N); K.h[n] = re[m] * w; }
    K.delay = (long) H;
    return K;
}

F32 convolve (const F32& x, const Kernel& K)
{
    const auto& h = K.h;
    const long L = (long) h.size(), N = 2 * L, n = (long) x.size(), d = K.delay;
    F32 y ((size_t) n, 0.0f);
    F64 hr ((size_t) N, 0.0), hi ((size_t) N, 0.0);
    std::copy (h.begin(), h.end(), hr.begin());
    fft (hr, hi, false);
    F64 re ((size_t) N), im ((size_t) N);
    for (long s = -L; s < n; s += L)
    {
        std::fill (re.begin(), re.end(), 0.0); std::fill (im.begin(), im.end(), 0.0);
        bool any = false;
        for (long j = 0; j < L; ++j) { const long q = s + j; if (q >= 0 && q < n) { re[(size_t) j] = x[(size_t) q]; if (x[(size_t) q] != 0) any = true; } }
        if (! any) continue;
        fft (re, im, false);
        for (long k = 0; k < N; ++k) { const double a = re[(size_t) k], b = im[(size_t) k]; re[(size_t) k] = a * hr[(size_t) k] - b * hi[(size_t) k]; im[(size_t) k] = a * hi[(size_t) k] + b * hr[(size_t) k]; }
        fft (re, im, true);
        for (long j = 0; j < N; ++j) { const long q = s + j - d; if (q >= 0 && q < n) y[(size_t) q] = (float) ((double) y[(size_t) q] + re[(size_t) j]); }
    }
    return y;
}

size_t kernN (double sr, bool minPhase)
{
    size_t N = 1;
    const double want = (minPhase ? 0.75 : 0.37) * sr;
    while ((double) N < want) N <<= 1;
    return N;
}

F64 magGrid (double sr, size_t N, const std::function<double (double)>& fn)
{
    const size_t H = N >> 1;
    F64 M (H + 1);
    for (size_t k = 0; k <= H; ++k) M[k] = fn (std::max ((double) k, 0.5) * sr / (double) N);
    return M;
}

F32 envDb (const F32& x, double sr, double atkMs, double relMs)
{
    const double a = std::exp (-1 / (std::max (0.01, atkMs) * sr / 1000)), r = std::exp (-1 / (std::max (0.1, relMs) * sr / 1000));
    F32 e (x.size());
    double v = 0;
    for (size_t n = 0; n < x.size(); ++n)
    {
        const double z = std::abs ((double) x[n]);
        v = z > v ? a * v + (1 - a) * z : r * v + (1 - r) * z;
        e[n] = (float) (20 * std::log10 (v + 1e-9));
    }
    return e;
}

F32 scale (const F32& x, double g)
{
    F32 y (x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = (float) (x[i] * g);
    return y;
}
} // namespace ld::hl

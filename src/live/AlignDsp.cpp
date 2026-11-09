#include "AlignDsp.h"

#include <algorithm>
#include <cmath>

namespace ld::live
{

namespace
{
constexpr double kMaxShiftSeconds = 0.05;   // 50 ms each way: further than any room mic needs
// (kFadeSeconds: in the header)
constexpr double kPrimeSeconds = 0.085;     // a new phase EQ is run over this much of the past before it fades in
constexpr double kBeta = 10.0;              // Kaiser shape: about -100 dB sidelobes

double besselI0 (double x)
{
    double sum = 1, term = 1;
    for (int k = 1; k < 64; ++k)
    {
        term *= (x / (2 * k)) * (x / (2 * k));
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}
} // namespace

int AlignDsp::latencySamples (double sampleRate)
{
    return (int) std::lround (kMaxShiftSeconds * sampleRate) + taps / 2;
}

double AlignDsp::maxShiftSamples (double sampleRate)
{
    return std::lround (kMaxShiftSeconds * sampleRate);
}

void AlignDsp::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    L = latencySamples (sr);
    // room for the latency, a full shift later than recorded, and the taps
    primeLen = (int) std::lround (kPrimeSeconds * sr);
    const long need = (long) L + (long) maxShiftSamples (sr) + taps + primeLen + 2;
    long size = 1;
    while (size < need) size <<= 1;
    mask = size - 1;
    ring.assign ((size_t) std::max (1, numChannels), std::vector<float> ((size_t) size, 0.0f));
    fadeLen = std::max (1, (int) std::lround (kFadeSeconds * sr));
    fadeCurve.resize ((size_t) fadeLen);
    for (int i = 0; i < fadeLen; ++i)
        fadeCurve[(size_t) i] = (float) (0.5 - 0.5 * std::cos (M_PI * (i + 0.5) / fadeLen));
    reset();
}

void AlignDsp::reset()
{
    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    pos = 0;
    fading = false;
    fadePos = 0;
    cur = make (target);   // a fresh start goes straight to the target, no fade
    curState = {};
    nextState = {};
}

AlignDsp::Kernel AlignDsp::make (const Settings& s) const
{
    Kernel k;
    k.s = s;
    const double lim = maxShiftSamples (sr);
    const double adv = std::clamp (s.advanceSamples, -lim, lim);
    const double delay = (double) L - adv;   // never below taps/2, so every tap reads the past
    k.whole = (long) std::floor (delay);
    const double frac = delay - (double) k.whole;
    const float g = (float) (std::pow (10.0, s.gainDb / 20.0) * (s.flip ? -1.0 : 1.0));
    k.g = g;
    for (const auto& a : s.ap)
    {
        if (a.fc <= 0 || a.fc >= sr / 2) continue;
        const double w0 = 2 * M_PI * a.fc / sr, al = std::sin (w0) / (2 * std::max (0.05, a.Q)), cw = std::cos (w0), a0 = 1 + al;
        k.ap[(size_t) k.nAp++] = { (1 - al) / a0, -2 * cw / a0, 1.0, -2 * cw / a0, (1 - al) / a0 };
    }
    if (frac < 1e-12)
    {
        k.impulse = true;
        return k;
    }
    // y[n] = sum_j h[j] x[n - whole - j], j = -(taps/2 - 1) .. taps/2; h[j] = sinc(j - frac) * window
    const double half = taps / 2.0, i0b = besselI0 (kBeta);
    double sum = 0;
    for (int i = 0; i < taps; ++i)
    {
        const int j = i - (taps / 2 - 1);
        const double t = j - frac;
        const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (M_PI * t) / (M_PI * t);
        const double r = t / half;
        const double w = std::abs (r) >= 1 ? 0.0 : besselI0 (kBeta * std::sqrt (1 - r * r)) / i0b;
        k.h[(size_t) i] = (float) (sinc * w);
        sum += sinc * w;
    }
    for (auto& v : k.h) v = (float) (v / sum * g);   // unity at DC
    return k;
}

float AlignDsp::tapOut (const std::vector<float>& r, long at, const Kernel& k) const
{
    if (k.impulse) return k.g * r[(size_t) ((at - k.whole) & mask)];
    double acc = 0;
    const long base = at - k.whole + (taps / 2 - 1);   // the sample for j = -(taps/2 - 1)
    for (int i = 0; i < taps; ++i)
        acc += (double) k.h[(size_t) i] * r[(size_t) ((base - i) & mask)];
    return (float) acc;
}

float AlignDsp::pathOut (const std::vector<float>& r, long at, const Kernel& k, std::array<std::array<double, 4>, 2>& st) const
{
    double v = tapOut (r, at, k);
    for (int i = 0; i < k.nAp; ++i)
    {
        const auto& b = k.ap[(size_t) i];
        auto& z = st[(size_t) i];
        const double out = b.b0 * v + b.b1 * z[0] + b.b2 * z[1] - b.a1 * z[2] - b.a2 * z[3];
        z[1] = z[0]; z[0] = v; z[3] = z[2]; z[2] = out;
        v = out;
    }
    return (float) v;
}

void AlignDsp::prime (Kernel& k, State& st) const
{
    // run the new phase EQ over the recent past, so it fades in already settled instead of ringing up from silence
    st = {};
    if (k.nAp == 0) return;
    for (size_t c = 0; c < ring.size(); ++c)
        for (long at = pos - primeLen; at < pos; ++at)
            pathOut (ring[c], at, k, st[c]);
}

void AlignDsp::startFadeIfNeeded()
{
    if (! fading && target != cur.s)
    {
        next = make (target);
        prime (next, nextState);
        fading = true;
        fadePos = 0;
    }
}

void AlignDsp::process (float* const* channels, int numChannels, int numSamples)
{
    const int nch = std::min (numChannels, (int) ring.size());
    startFadeIfNeeded();
    for (int n = 0; n < numSamples; ++n)
    {
        const long at = pos;
        for (int c = 0; c < nch; ++c) ring[(size_t) c][(size_t) (at & mask)] = channels[c][n];
        if (! fading)
        {
            for (int c = 0; c < nch; ++c) channels[c][n] = pathOut (ring[(size_t) c], at, cur, curState[(size_t) c]);
            ++pos;
        }
        else
        {
            const float b = fadeCurve[(size_t) fadePos], a = 1.0f - b;
            for (int c = 0; c < nch; ++c)
                channels[c][n] = a * pathOut (ring[(size_t) c], at, cur, curState[(size_t) c]) + b * pathOut (ring[(size_t) c], at, next, nextState[(size_t) c]);
            ++pos;
            if (++fadePos >= fadeLen)
            {
                cur = next;
                curState = nextState;
                fading = false;
                startFadeIfNeeded();   // a change that arrived during the fade starts now, not at the next block
            }
        }
    }
    for (int c = nch; c < numChannels; ++c) std::fill (channels[c], channels[c] + numSamples, 0.0f);
}

} // namespace ld::live

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
    nch = std::max (1, numChannels);
    // a slot being filled starts this far back: the furthest any tap reads, plus time for the all-passes to settle
    history = (long) L + (long) maxShiftSamples (sr) + taps + (long) std::lround (kPrimeSeconds * sr);
    long size = 1;
    while (size < 2 * history + 8) size <<= 1;   // (the history, plus what arrives while a slot catches up)
    mask = size - 1;
    raw.assign ((size_t) nch, std::vector<float> ((size_t) size, 0.0f));
    for (auto& sl : slots) sl.ring.assign ((size_t) nch, std::vector<float> ((size_t) size, 0.0f));
    fadeLen = std::max (1, (int) std::lround (kFadeSeconds * sr));
    fadeCurve.resize ((size_t) fadeLen);
    for (int i = 0; i < fadeLen; ++i)
        fadeCurve[(size_t) i] = (float) (0.5 - 0.5 * std::cos (M_PI * (i + 0.5) / fadeLen));
    reset();
}

void AlignDsp::reset()
{
    for (auto& r : raw) std::fill (r.begin(), r.end(), 0.0f);
    for (auto& sl : slots) { for (auto& r : sl.ring) std::fill (r.begin(), r.end(), 0.0f); sl.st = {}; }
    pos = 0;
    fading = false;
    building = false;
    fadePos = 0;
    curSlot = nextSlot = 0;
    setSlot (slots[0], target.ap);   // a fresh start goes straight to the target, no fade
    cur = make (target);
}

void AlignDsp::setSlot (Slot& sl, const std::array<AllPass, 2>& ap)
{
    sl.ap = ap;
    sl.nAp = 0;
    sl.st = {};
    for (const auto& a : ap)
    {
        if (a.fc <= 0 || a.fc >= sr / 2) continue;
        const double w0 = 2 * M_PI * a.fc / sr, al = std::sin (w0) / (2 * std::max (0.05, a.Q)), cw = std::cos (w0), a0 = 1 + al;
        sl.bq[(size_t) sl.nAp++] = { (1 - al) / a0, -2 * cw / a0, 1.0, -2 * cw / a0, (1 - al) / a0 };
    }
}

void AlignDsp::filterInto (Slot& sl, long at, int n)
{
    for (int c = 0; c < n; ++c)
    {
        double v = raw[(size_t) c][(size_t) (at & mask)];
        for (int i = 0; i < sl.nAp; ++i)
        {
            const auto& b = sl.bq[(size_t) i];
            auto& z = sl.st[(size_t) c][(size_t) i];
            const double out = b.b0 * v + b.b1 * z[0] + b.b2 * z[1] - b.a1 * z[2] - b.a2 * z[3];
            z[1] = z[0]; z[0] = v; z[3] = z[2]; z[2] = out;
            v = out;
        }
        sl.ring[(size_t) c][(size_t) (at & mask)] = (float) v;
    }
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

void AlignDsp::startChangeIfNeeded()
{
    if (fading) return;
    if (building)
    {
        // the phase EQ asked for changed again before the slot was ready: start that one instead
        if (! sameAp (slots[nextSlot].ap, target.ap)) { setSlot (slots[nextSlot], target.ap); buildPos = pos - history; }
        return;
    }
    if (target == cur.s && sameAp (target.ap, slots[curSlot].ap)) return;
    if (sameAp (target.ap, slots[curSlot].ap))
    {
        nextSlot = curSlot;   // shift, gain or polarity: the same slot, straight into the fade
        next = make (target);
        fading = true;
        fadePos = 0;
        return;
    }
    nextSlot = 1 - curSlot;
    setSlot (slots[nextSlot], target.ap);
    buildPos = pos - history;
    building = true;
}

void AlignDsp::process (float* const* channels, int numChannels, int numSamples)
{
    const int n = std::min (numChannels, (int) raw.size());
    startChangeIfNeeded();
    for (int i = 0; i < numSamples; ++i)
    {
        const long at = pos;
        for (int c = 0; c < n; ++c) raw[(size_t) c][(size_t) (at & mask)] = channels[c][i];
        filterInto (slots[curSlot], at, n);
        if (building)
        {
            for (int k = 0; k < 4 && buildPos < at; ++k) filterInto (slots[nextSlot], buildPos++, n);
            if (buildPos == at)
            {
                filterInto (slots[nextSlot], at, n);
                building = false;
                next = make (target);
                fading = true;
                fadePos = 0;
            }
        }
        else if (fading && nextSlot != curSlot) filterInto (slots[nextSlot], at, n);

        if (! fading)
            for (int c = 0; c < n; ++c) channels[c][i] = tapOut (slots[curSlot].ring[(size_t) c], at, cur);
        else
        {
            const float b = fadeCurve[(size_t) fadePos], a = 1.0f - b;
            for (int c = 0; c < n; ++c)
                channels[c][i] = a * tapOut (slots[curSlot].ring[(size_t) c], at, cur) + b * tapOut (slots[nextSlot].ring[(size_t) c], at, next);
        }
        ++pos;
        if (fading && ++fadePos >= fadeLen)
        {
            cur = next;
            curSlot = nextSlot;
            fading = false;
            startChangeIfNeeded();   // a change that arrived during the fade starts now, not at the next block
        }
    }
    for (int c = n; c < numChannels; ++c) std::fill (channels[c], channels[c] + numSamples, 0.0f);
}

} // namespace ld::live

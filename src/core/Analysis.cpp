#include "Analysis.h"
#include <cmath>

namespace ld
{

std::vector<double> detectTriggers (const juce::AudioBuffer<float>& mono, double sampleRate,
                                    float thresholdDbRelPeak, double minGapSec)
{
    std::vector<double> out;
    const int n = mono.getNumSamples();

    if (n == 0)
        return out;

    const float* x = mono.getReadPointer (0);
    const float peak = mono.getMagnitude (0, 0, n);

    if (peak <= 0.0f)
        return out;

    const float thr = peak * juce::Decibels::decibelsToGain (thresholdDbRelPeak);
    const int minGap = (int) std::lround (minGapSec * sampleRate);
    int last = -minGap;

    for (int i = 0; i < n; ++i)
    {
        if (std::abs (x[i]) >= thr && i - last >= minGap)
        {
            out.push_back (i / sampleRate);
            last = i;
        }
        else if (std::abs (x[i]) >= thr)
        {
            last = std::max (last, i - minGap / 2); // stay in the same hit while it rings
        }
    }

    return out;
}

int bestLag (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
             int start, int length, int maxLag)
{
    const float* x = a.getReadPointer (0);
    const float* y = b.getReadPointer (0);
    const int na = a.getNumSamples(), nb = b.getNumSamples();

    double best = -1.0e300;
    int bestL = 0;

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double s = 0.0, ea = 0.0, eb = 0.0;

        for (int i = start; i < start + length; ++i)
        {
            const int j = i + lag;

            if (i < 0 || i >= na || j < 0 || j >= nb)
                continue;

            s += (double) x[i] * y[j];
            ea += (double) x[i] * x[i];
            eb += (double) y[j] * y[j];
        }

        const double c = (ea > 0 && eb > 0) ? s / std::sqrt (ea * eb) : 0.0;

        if (c > best)
        {
            best = c;
            bestL = lag;
        }
    }

    return bestL;
}

float maxStep (const juce::AudioBuffer<float>& mono, int start, int end)
{
    const float* x = mono.getReadPointer (0);
    start = std::max (1, start);
    end = std::min (end, mono.getNumSamples());
    float m = 0.0f;

    for (int i = start; i < end; ++i)
        m = std::max (m, std::abs (x[i] - x[i - 1]));

    return m;
}

} // namespace ld

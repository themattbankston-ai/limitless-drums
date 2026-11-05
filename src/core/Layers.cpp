#include "Layers.h"
#include <algorithm>
#include <cmath>

namespace ld
{
std::vector<double> detectOneShots (const juce::AudioBuffer<float>& mono, double sr, float floorDbRel, double minGapSec)
{
    std::vector<double> out;
    const int n = mono.getNumSamples();
    const int block = std::max (1, (int) (sr * 0.002));
    const int nb = n / block;
    if (nb < 12) return out;

    const float* x = mono.getReadPointer (0);
    std::vector<float> env ((size_t) nb, 0.0f);
    float peak = 0.0f;
    for (int b = 0; b < nb; ++b)
    {
        float m = 0.0f;
        for (int i = b * block; i < (b + 1) * block; ++i) m = std::max (m, std::abs (x[i]));
        env[(size_t) b] = m;
        peak = std::max (peak, m);
    }
    if (peak <= 0.0f) return out;

    const float floor = peak * juce::Decibels::decibelsToGain (floorDbRel);
    const int look = 10, gapBlocks = (int) (minGapSec / 0.002);
    int last = -gapBlocks;
    for (int b = 0; b < nb; ++b)
    {
        if (env[(size_t) b] < floor || b - last < gapBlocks) continue;
        // A file trimmed right at the first hit starts mid-attack: nothing before it counts.
        float before = b == 0 ? 0.0f : env[(size_t) b - 1];
        for (int k = std::max (0, b - look); k < b; ++k) before = std::min (before, env[(size_t) k]);
        if (env[(size_t) b] < 4.0f * before + floor * 0.25f) continue;

        // Refine to the first sample that reaches a tenth of the attack's peak.
        float local = 0.0f;
        for (int k = b; k < std::min (nb, b + 5); ++k) local = std::max (local, env[(size_t) k]);
        int onset = b * block;
        for (int i = std::max (0, (b - 2) * block); i < (b + 1) * block; ++i)
            if (std::abs (x[i]) >= 0.1f * local) { onset = i; break; }
        out.push_back (onset / sr);
        last = b;
    }
    return out;
}

float peakDbAt (const juce::AudioBuffer<float>& mono, double sr, double t, double windowSec)
{
    const int n = mono.getNumSamples();
    const int a = juce::jlimit (0, n, (int) std::floor (t * sr));
    const int b = juce::jlimit (0, n, (int) std::ceil ((t + windowSec) * sr));
    float peak = 0.0f;
    const float* d = mono.getReadPointer (0);
    for (int i = a; i < b; ++i)
        peak = std::max (peak, std::abs (d[i]));
    return juce::Decibels::gainToDecibels (peak, -100.0f);
}

float rmsDbAt (const juce::AudioBuffer<float>& mono, double sr, double t, double windowSec)
{
    const int n = mono.getNumSamples();
    const int a = juce::jlimit (0, n, (int) std::floor (t * sr));
    const int b = juce::jlimit (0, n, (int) std::ceil ((t + windowSec) * sr));
    if (b <= a) return -100.0f;
    return juce::Decibels::gainToDecibels (mono.getRMSLevel (0, a, b - a), -100.0f);
}

bool parseLayerCounts (const juce::String& fileName, int& velocities, int& roundRobins)
{
    velocities = roundRobins = 0;
    juce::StringArray tokens;
    tokens.addTokens (fileName.upToLastOccurrenceOf (".", false, false).toUpperCase(), "_- .", "");
    for (auto& t : tokens)
    {
        if (t.startsWith ("RR") && t.substring (2).containsOnly ("0123456789") && t.length() > 2) roundRobins = t.substring (2).getIntValue();
        else if (t.startsWith ("VEL") && t.substring (3).containsOnly ("0123456789") && t.length() > 3) velocities = t.substring (3).getIntValue();
        else if (t.startsWith ("V") && t.substring (1).containsOnly ("0123456789") && t.length() > 1) velocities = t.substring (1).getIntValue();
    }
    return velocities > 0;
}

float estimatePitch (const juce::AudioBuffer<float>& mono, double sr, double onset)
{
    const int n = mono.getNumSamples();
    const int start = (int) ((onset + 0.025) * sr);
    const int len = (int) (0.12 * sr);
    if (start < 0 || start + len + (int) (sr / 50.0) >= n)
        return 0.0f;

    const float* d = mono.getReadPointer (0) + start;
    const int minLag = (int) (sr / 1000.0), maxLag = (int) (sr / 50.0);

    double energy = 0.0;
    for (int i = 0; i < len; ++i) energy += (double) d[i] * d[i];
    if (energy < 1e-9) return 0.0f;

    // Normalised autocorrelation; take the first strong peak rather than the global one so
    // that octave-down lags don't win.
    std::vector<double> r ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double s = 0.0, e2 = 0.0;
        for (int i = 0; i < len; ++i) { s += (double) d[i] * d[i + lag]; e2 += (double) d[i + lag] * d[i + lag]; }
        r[(size_t) lag] = s / std::sqrt (energy * e2 + 1e-12);
    }
    double best = 0.0;
    for (int lag = minLag + 1; lag <= maxLag; ++lag) best = std::max (best, r[(size_t) lag]);
    if (best < 0.5) return 0.0f;

    for (int lag = minLag + 1; lag <= maxLag; ++lag)
    {
        const double v = r[(size_t) lag];
        if (v >= 0.9 * best && v >= r[(size_t) lag - 1] && v >= r[(size_t) lag + 1])
        {
            // Parabolic interpolation for a sub-sample lag
            const double y0 = r[(size_t) lag - 1], y2 = r[(size_t) lag + 1];
            const double den = y0 - 2.0 * v + y2;
            const double off = std::abs (den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0.0;
            return (float) (sr / (lag + off));
        }
    }
    return 0.0f;
}

static void finishLayers (std::vector<VelocityLayer>&, const std::vector<Hit>&, double, bool);

std::vector<VelocityLayer> groupLayers (std::vector<Hit>& hits, double sessionLength, float splitDb, int minPerLayer)
{
    std::vector<VelocityLayer> layers;
    if (hits.empty())
        return layers;

    std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.time < b.time; });

    VelocityLayer cur;
    double sum = 0.0;
    for (int i = 0; i < (int) hits.size(); ++i)
    {
        const float db = hits[(size_t) i].peakDb;
        if (cur.numHits >= minPerLayer && std::abs (db - (float) (sum / cur.numHits)) > splitDb)
        {
            cur.meanDb = (float) (sum / cur.numHits);
            layers.push_back (cur);
            cur = {};
            cur.firstHit = i;
            sum = 0.0;
        }
        hits[(size_t) i].layer = (int) layers.size();
        hits[(size_t) i].roundRobin = cur.numHits;
        ++cur.numHits;
        sum += db;
    }
    cur.meanDb = (float) (sum / cur.numHits);
    layers.push_back (cur);

    finishLayers (layers, hits, sessionLength, false);
    return layers;
}

static void finishLayers (std::vector<VelocityLayer>& layers, const std::vector<Hit>& hits, double sessionLength, bool byRank)
{
    const int n = (int) layers.size();
    for (int k = 0; k < n; ++k)
    {
        auto& L = layers[(size_t) k];
        const int v = byRank ? hits[(size_t) L.firstHit].layer : k;
        L.midiLow = 127 * v / n + 1;
        L.midiHigh = 127 * (v + 1) / n;
        const double first = hits[(size_t) L.firstHit].time;
        L.start = k == 0 ? 0.0 : std::max (0.0, first - 0.25 * (first - hits[(size_t) L.firstHit - 1].time));
        if (k > 0) layers[(size_t) k - 1].end = L.start;

        std::vector<float> p;
        for (int i = L.firstHit; i < L.firstHit + L.numHits; ++i)
            if (hits[(size_t) i].pitchHz > 0.0f) p.push_back (hits[(size_t) i].pitchHz);
        if (! p.empty())
        {
            std::nth_element (p.begin(), p.begin() + (long) p.size() / 2, p.end());
            L.pitchHz = p[p.size() / 2];
        }
    }
    layers.back().end = std::max (sessionLength, hits.back().time + 0.5);
}

std::vector<VelocityLayer> groupLayersKnown (std::vector<Hit>& hits, double sessionLength, int velocities)
{
    std::vector<VelocityLayer> layers;
    if (hits.empty() || velocities <= 0) return layers;
    std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.time < b.time; });
    const int nh = (int) hits.size(), nv = std::min (velocities, nh);

    for (int k = 0; k < nv; ++k)
    {
        VelocityLayer L;
        L.firstHit = nh * k / nv;
        L.numHits = nh * (k + 1) / nv - L.firstHit;
        double sum = 0.0;
        for (int i = L.firstHit; i < L.firstHit + L.numHits; ++i) sum += hits[(size_t) i].peakDb;
        L.meanDb = (float) (sum / L.numHits);
        layers.push_back (L);
    }
    // velocity = loudness rank of the run
    std::vector<int> order ((size_t) nv);
    for (int k = 0; k < nv; ++k) order[(size_t) k] = k;
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return layers[(size_t) a].meanDb < layers[(size_t) b].meanDb; });
    for (int rank = 0; rank < nv; ++rank)
    {
        const auto& L = layers[(size_t) order[(size_t) rank]];
        for (int i = L.firstHit; i < L.firstHit + L.numHits; ++i)
        {
            hits[(size_t) i].layer = rank;
            hits[(size_t) i].roundRobin = i - L.firstHit;
        }
    }
    finishLayers (layers, hits, sessionLength, true);
    return layers;
}

juce::String noteName (float hz)
{
    if (hz <= 0.0f) return {};
    static const char* names[] = { "C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B" };
    const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::String (juce::CharPointer_UTF8 (names[((midi % 12) + 12) % 12])) + juce::String (midi / 12 - 1);
}
} // namespace ld

#include "TestSignals.h"
#include <cmath>

namespace ldtest
{

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 32, {}, 0));
    jassert (w != nullptr);
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

SynthTake writeTake (const juce::File& dir, const std::vector<double>& hitTimes,
                     int numMics, double lengthSec, double sr)
{
    static const int lags[]   = { 0, 16, 143, 319, 476, 560, 600, 672, 820, 904, 1010, 1150 };
    static const float gains[] = { 1.0f, 0.7f, 0.55f, 0.5f, 0.45f, 0.4f, 0.38f, 0.35f, 0.3f, 0.28f, 0.25f, 0.22f };
    static const char* names[] = { "Snare Top", "Snare Bottom", "Kick In", "Overheads", "Room Mono", "Room Close L",
                                   "Room Close R", "Room Far L", "Room Far R", "Room Crush", "Ambient 1", "Ambient 2" };

    dir.createDirectory();
    SynthTake take;
    take.sampleRate = sr;
    take.hitTimes = hitTimes;

    const int n = (int) std::lround (lengthSec * sr);
    const int hitLen = (int) std::lround (0.25 * sr);

    // One hit shape: instant attack (so onsets are exact), tone + noise, exponential decay.
    juce::Random rng (1234);
    std::vector<float> hit ((size_t) hitLen);
    for (int i = 0; i < hitLen; ++i)
    {
        const double env = std::exp (-i / (0.04 * sr));
        hit[(size_t) i] = (float) (env * (0.6 * std::cos (2.0 * juce::MathConstants<double>::pi * 180.0 * i / sr)
                                          + 0.4 * (rng.nextFloat() * 2.0f - 1.0f)));
    }

    for (int m = 0; m < numMics; ++m)
    {
        juce::AudioBuffer<float> b (1, n);
        b.clear();
        auto* x = b.getWritePointer (0);
        juce::Random noise (100 + m);

        for (int i = 0; i < n; ++i)
            x[i] = 0.0003f * (noise.nextFloat() * 2.0f - 1.0f);   // noise floor around -70 dBFS

        for (size_t h = 0; h < hitTimes.size(); ++h)
        {
            const float vel = 0.55f + 0.35f * (float) ((h * 7) % 5) / 4.0f;
            const int start = (int) std::lround (hitTimes[h] * sr) + lags[m];

            for (int i = 0; i < hitLen && start + i < n; ++i)
                if (start + i >= 0)
                    x[start + i] += gains[m] * vel * hit[(size_t) i];
        }

        auto f = dir.getChildFile (juce::String (m + 1).paddedLeft ('0', 2) + " " + names[m] + ".wav");
        writeWav (f, b, sr);
        take.mics.push_back ({ names[m], f });
        take.lagSamples.push_back (lags[m]);
    }

    return take;
}

ld::MicFile writeSine (const juce::File& dir, double freq, float amp, double lengthSec, double sr)
{
    dir.createDirectory();
    const int n = (int) std::lround (lengthSec * sr);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i)
        b.setSample (0, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr));
    auto f = dir.getChildFile ("Sine.wav");
    writeWav (f, b, sr);
    return { "Sine", f };
}

std::vector<double> humanise (const std::vector<double>& grid, double maxOffsetMs, int seed)
{
    juce::Random r (seed);
    std::vector<double> out;
    for (auto t : grid)
        out.push_back (t + (r.nextDouble() * 2.0 - 1.0) * maxOffsetMs / 1000.0);
    return out;
}

tracktion::Engine*& enginePtr()
{
    static tracktion::Engine* e = nullptr;
    return e;
}

tracktion::Engine& engine()
{
    jassert (enginePtr() != nullptr);
    return *enginePtr();
}

} // namespace ldtest

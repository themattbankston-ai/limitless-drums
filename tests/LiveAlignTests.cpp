// The live Align step: the DSP on its own, then inside Tracktion playback and rendering.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "live/AlignDsp.h"
#include "live/AlignPlugin.h"
#include <atomic>
#include <thread>
#include "hl/Align.h"
#include <cmath>
#include <random>

using ld::live::AlignDsp;
namespace te = tracktion;

namespace
{
constexpr double SR = 48000;

std::vector<float> noise (int n, unsigned seed)
{
    std::mt19937 g (seed);
    std::uniform_real_distribution<float> u (-0.5f, 0.5f);
    std::vector<float> x ((size_t) n);
    for (auto& v : x) v = u (g);
    return x;
}

std::vector<float> run (std::vector<float> x, const std::vector<int>& blocks, const std::vector<std::pair<int, AlignDsp::Settings>>& changes, AlignDsp::Settings start = {})
{
    AlignDsp d;
    d.setTarget (start);
    d.prepare (SR, 1);
    int at = 0, b = 0, c = 0;
    while (at < (int) x.size())
    {
        while (c < (int) changes.size() && changes[(size_t) c].first <= at) d.setTarget (changes[(size_t) c++].second);
        const int n = std::min (blocks[(size_t) (b++ % blocks.size())], (int) x.size() - at);
        float* ch[1] = { x.data() + at };
        d.process (ch, 1, n);
        at += n;
    }
    return x;
}

double db (double r) { return 20 * std::log10 (std::max (r, 1e-30)); }

void writeFloatWav (const juce::File& f, const std::vector<float>& x)
{
    f.deleteFile();
    juce::AudioBuffer<float> b (1, (int) x.size());
    b.copyFrom (0, 0, x.data(), (int) x.size());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 1, 32, {}, 0));
    REQUIRE (w != nullptr);
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

std::vector<float> renderTrack (te::Edit& edit, te::AudioTrack& track, int block)
{
    juce::TemporaryFile temp (".wav");
    auto all = te::getAllTracks (edit);
    juce::BigInteger which; which.setBit (all.indexOf (&track));
    te::Renderer::Parameters p (edit);
    p.destFile = temp.getFile();
    p.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32;
    p.sampleRateForAudio = SR;
    p.blockSizeForAudio = block;
    p.time = te::TimeRange (te::TimePosition(), edit.getLength());
    p.tracksToDo = which;
    p.usePlugins = true;
    p.useMasterPlugins = false;
    p.mustRenderInMono = true;
    p.ditheringEnabled = false;
    te::Renderer::renderToFile ("Render", p);
    auto b = ld::readMono (temp.getFile());
    return std::vector<float> (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
}

int peakAt (const std::vector<float>& x) { return (int) (std::max_element (x.begin(), x.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) - x.begin()); }

void registerOnce()
{
    static bool done = false;
    if (! done) { ld::live::registerLivePlugins (ldtest::engine()); done = true; }
}
} // namespace

TEST_CASE ("Live align: a whole-sample shift is exact")
{
    const auto x = noise (20000, 1);
    AlignDsp::Settings s; s.advanceSamples = 88;
    const auto y = run (x, { 512 }, {}, s);
    const int D = AlignDsp::latencySamples (SR) - 88;
    for (int n = D; n < (int) x.size(); ++n) REQUIRE (y[(size_t) n] == x[(size_t) (n - D)]);
}

TEST_CASE ("Live align: a fractional shift is accurate across the audible band")
{
    AlignDsp::Settings s; s.advanceSamples = 0.37;
    const double D = AlignDsp::latencySamples (SR) - 0.37;
    for (double f : { 50.0, 1000.0, 10000.0, 16000.0, 20000.0 })
    {
        std::vector<float> x (40000);
        for (size_t n = 0; n < x.size(); ++n) x[n] = (float) (0.5 * std::sin (2 * M_PI * f * (double) n / SR));
        const auto y = run (x, { 256 }, {}, s);
        double e = 0, p = 0;
        for (size_t n = 8000; n < x.size(); ++n)
        {
            const double ideal = 0.5 * std::sin (2 * M_PI * f * ((double) n - D) / SR);
            e += (y[n] - ideal) * (y[n] - ideal); p += ideal * ideal;
        }
        const double err = db (std::sqrt (e / p));
        INFO (f << " Hz: error " << err << " dB");
        CHECK (err < -100.0);   // measured -107 to -137 dB: the floor of 32-bit float
    }
}

TEST_CASE ("Live align: changes fade without clicks, the same whatever the block size")
{
    std::vector<float> x (30000);
    for (size_t n = 0; n < x.size(); ++n) x[n] = (float) (0.8 * std::sin (2 * M_PI * 200 * (double) n / SR));
    AlignDsp::Settings to; to.advanceSamples = 240.5; to.flip = true; to.gainDb = -3; to.ap[0] = { 202, 0.7 };
    const std::vector<std::pair<int, AlignDsp::Settings>> ch { { 9600, to } };
    const auto a = run (x, { 64 }, ch), b = run (x, { 480 }, ch), c = run (x, { 9600, 7, 333, 1, 4096 }, { { 9600, to } });
    CHECK (a == b);
    CHECK (a == c);
    // the steepest step anywhere is no bigger than the sine's own (a click would be a jump near the full level)
    const double sineStep = 0.8 * 2 * M_PI * 200 / SR;
    double worst = 0;
    for (size_t n = 4000; n < a.size(); ++n) worst = std::max (worst, (double) std::abs (a[n] - a[n - 1]));
    INFO ("steepest step " << worst << " vs the sine's " << sineStep);
    CHECK (worst < sineStep * 1.6);
}

TEST_CASE ("Live align: inside Tracktion the shifts land on the timeline, and the render ignores block size")
{
    registerOnce();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_live_align");
    dir.deleteRecursively(); dir.createDirectory();
    // the lead hears the hit at 1.0 s; the far mic 88 samples later
    std::vector<float> lead (96000, 0.0f), far (96000, 0.0f);
    lead[48000] = 0.9f; far[48000 + 88] = 0.6f;
    writeFloatWav (dir.getChildFile ("Lead.wav"), lead);
    writeFloatWav (dir.getChildFile ("Far.wav"), far);
    auto edit = ld::createSessionEdit (ldtest::engine(), { { "Lead", dir.getChildFile ("Lead.wav") }, { "Far", dir.getChildFile ("Far.wav") } }, 120.0, te::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);
    auto tracks = te::getAudioTracks (*edit);
    auto* aLead = ld::live::alignOn (*tracks[0]);
    auto* aFar = ld::live::alignOn (*tracks[1]);
    REQUIRE (aLead != nullptr);
    REQUIRE (aFar != nullptr);
    aFar->setShiftMs (88.0f / 48.0f);

    const auto rLead = renderTrack (*edit, *tracks[0], 512), rFar = renderTrack (*edit, *tracks[1], 512);
    INFO ("lead peak at " << peakAt (rLead) << ", far peak at " << peakAt (rFar));
    CHECK (peakAt (rLead) == 48000);
    CHECK (rLead[48000] == 0.9f);
    CHECK (peakAt (rFar) == 48000);
    CHECK (std::abs (rFar[48000] - 0.6f) < 1e-4f);

    // the same render in odd-sized blocks: identical
    const auto rFar37 = renderTrack (*edit, *tracks[1], 37);
    CHECK (rFar37 == rFar);

    // a polarity flip and gain land too, and undo puts them back
    edit->getUndoManager().beginNewTransaction();
    aFar->setFlip (true);
    aFar->setGainDb (-6.0f);
    const auto rFlip = renderTrack (*edit, *tracks[1], 512);
    CHECK (std::abs (rFlip[48000] + 0.6f * std::pow (10.0f, -6.0f / 20.0f)) < 1e-4f);
    edit->getUndoManager().undo();
    CHECK (aFar->flipValue.get() == false);
    CHECK (aFar->gainParam->getCurrentValue() == 0.0f);
    CHECK (std::abs (aFar->shiftParam->getCurrentValue() - 88.0f / 48.0f) < 1e-5f);
    CHECK (renderTrack (*edit, *tracks[1], 512) == rFar);
}

TEST_CASE ("Live align: the phase EQ is the analysis engine's all-pass, and changes level nowhere")
{
    const auto x = noise (48000, 7);
    AlignDsp::Settings s; s.ap[0] = { 202, 0.7 }; s.ap[1] = { 1200, 1.5 };
    const auto y = run (x, { 512 }, {}, s);
    const int D = AlignDsp::latencySamples (SR);
    const auto ref = ld::hl::applyAllpass (ld::hl::F32 (x.begin(), x.end()), SR, { { 202, 0.7 }, { 1200, 1.5 } });
    double e = 0, p = 0;
    for (int n = D; n < (int) x.size(); ++n) { const double d = y[(size_t) n] - ref[(size_t) (n - D)]; e += d * d; p += (double) ref[(size_t) (n - D)] * ref[(size_t) (n - D)]; }
    INFO ("difference " << db (std::sqrt (e / p)) << " dB");
    CHECK (db (std::sqrt (e / p)) < -100);
    double px = 0, py = 0;
    for (int n = D; n < (int) x.size(); ++n) { px += (double) x[(size_t) (n - D)] * x[(size_t) (n - D)]; py += (double) y[(size_t) n] * y[(size_t) n]; }
    CHECK (std::abs (10 * std::log10 (py / px)) < 0.05);
}

TEST_CASE ("Live align: a hit right after the edit's lead-in comes through whole, and per-hit gains switch on time")
{
    registerOnce();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_live_align_start");
    dir.deleteRecursively(); dir.createDirectory();
    // the sample build sits every take 0.25 s into the edit: Tracktion's render doesn't run the look-ahead before 0
    const int pad = 12000;
    std::vector<float> x (96000 + pad, 0.0f);
    x[pad + 5] = 0.5f; x[pad + 48000] = 0.5f;
    writeFloatWav (dir.getChildFile ("X.wav"), x);
    auto edit = ld::createSessionEdit (ldtest::engine(), { { "X", dir.getChildFile ("X.wav") } }, 120.0, te::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);
    auto tracks = te::getAudioTracks (*edit);
    auto* a = ld::live::alignOn (*tracks[0]);
    REQUIRE (a != nullptr);
    a->setHitScales ({ { 0.24, 1.0, -6.0 }, { 0.25 + 0.9, 1.0, -12.0 } });
    const auto r = renderTrack (*edit, *tracks[0], 512);
    INFO ("at 5: " << r[pad + 5] << ", at 48000: " << r[pad + 48000]);
    CHECK (std::abs (r[pad + 5] - 0.5f * std::pow (10.0f, -6.0f / 20.0f)) < 1e-4f);
    CHECK (std::abs (r[pad + 48000] - 0.5f * std::pow (10.0f, -12.0f / 20.0f)) < 1e-4f);
    CHECK (renderTrack (*edit, *tracks[0], 37) == r);
}

TEST_CASE ("Live align SA05: hit tables are swapped while the audio thread plays them, and never pile up")
{
    // the audio thread plays blocks the way Tracktion calls it while the message thread replaces the hit table
    // thousands of times. Under AddressSanitizer or ThreadSanitizer a table freed while the audio thread still read
    // it fails here; in a plain build it checks the output stays sane and only two tables are ever held.
    registerOnce();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_live_sa05");
    dir.deleteRecursively(); dir.createDirectory();
    writeFloatWav (dir.getChildFile ("X.wav"), std::vector<float> (48000, 0.0f));
    auto edit = ld::createSessionEdit (ldtest::engine(), { { "X", dir.getChildFile ("X.wav") } }, 120.0, te::Edit::EditRole::forRendering);
    REQUIRE (edit != nullptr);
    auto* a = ld::live::alignOn (*te::getAudioTracks (*edit)[0]);
    REQUIRE (a != nullptr);
    a->initialise ({ te::TimePosition(), SR, 64 });
    std::atomic<bool> stop { false };
    std::atomic<int> blocks { 0 };
    std::atomic<float> worst { 0.0f };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> b (2, 64);
        double t = 0;
        while (! stop.load())
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i) b.setSample (c, i, 0.5f);
            te::PluginRenderContext rc (&b, 0, 64, nullptr, 0.0, te::TimeRange (te::TimePosition::fromSeconds (t), te::TimePosition::fromSeconds (t + 64 / SR)), true, false, false, false);
            a->applyToBuffer (rc);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i)
            {
                const float v = std::abs (b.getSample (c, i));
                if (! std::isfinite (v) || v > worst.load()) worst = std::isfinite (v) ? v : 1e9f;
            }
            t = std::fmod (t + 64 / SR, 1.0);   // loops a second, so it keeps crossing the table's segments
            ++blocks;
        }
    });
    for (int k = 0; k < 4000; ++k)
    {
        std::vector<ld::live::AlignPlugin::HitScale> v;
        for (int h = 0; h < 1 + k % 9; ++h) v.push_back ({ h * 0.1, 1.0, -(double) ((k + h) % 13), 1.0 + 0.01 * (h % 3) });
        a->setHitScales (std::move (v));
        CHECK (a->tablesHeld() <= 2);
        if (k % 50 == 0) std::this_thread::yield();
    }
    while (blocks.load() < 2000) std::this_thread::yield();
    stop = true;
    audio.join();
    INFO ("blocks " << blocks.load() << ", largest output " << worst.load());
    CHECK (worst.load() <= 0.5f * 1.001f);   // gains are all <= 0 dB: nothing reads garbage
    a->setHitScales ({});
    CHECK (a->tablesHeld() <= 2);
}

TEST_CASE ("Live align: compute cost per block (benchmark, not live-device evidence)")
{
    // COMPUTE ONLY: the DSP alone, offline, on whatever machine runs the tests. It says nothing about a real audio
    // device's callbacks (their sizes, the driver, other tracks, the UI); dropout-free playback needs testing in the
    // app on the device. Ten seconds of stereo at 96 kHz in 64-sample blocks (a 0.667 ms deadline), two all-passes,
    // a change every other block so it is always crossfading, and the phase EQ changing every 100 ms. Three runs.
    // Reported: the raw times of every block of every run (max, percentiles, deadline misses), and, separately, the
    // "repeatable worst": each block's fastest of the three, then the slowest of those, which hides OS pre-emption
    // to show setup bursts the DSP does every time. Only the averages and the repeatable worst are checked: raw
    // timings on a shared CI machine are noise.
    const double sr = 96000, deadline = 64 / sr * 1000.0;
    const int n = 960000, block = 64, blocks = (n + block - 1) / block;
    std::vector<double> best ((size_t) blocks, 1e9), raw;
    raw.reserve ((size_t) blocks * 3);
    double total = 0;
    for (int run = 0; run < 3; ++run)
    {
        auto l = noise (n, 3), r = noise (n, 4);
        AlignDsp d;
        AlignDsp::Settings s; s.advanceSamples = 37.25; s.ap[0] = { 300, 0.7 }; s.ap[1] = { 1200, 0.9 };
        d.setTarget (s);
        d.prepare (sr, 2);
        for (int at = 0, k = 0; at < n; at += block, ++k)
        {
            if (k % 2 == 0) { s.advanceSamples = 30.37 + (k % 5); s.gainDb = -(k % 3); s.flip = (k % 7) == 0; }
            if (k % 150 == 0) s.ap[0].fc = 250 + (k % 11) * 10;
            d.setTarget (s);
            float* ch[2] = { l.data() + at, r.data() + at };
            const auto t0 = juce::Time::getHighResolutionTicks();
            d.process (ch, 2, std::min (block, n - at));
            const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
            best[(size_t) k] = std::min (best[(size_t) k], ms);
            raw.push_back (ms);
            if (run == 0) total += ms;
        }
    }
    std::vector<double> sorted = raw;
    std::sort (sorted.begin(), sorted.end());
    auto pct = [&] (double p) { return sorted[(size_t) std::min ((double) sorted.size() - 1, p / 100.0 * (double) sorted.size())]; };
    const auto misses = std::count_if (raw.begin(), raw.end(), [&] (double v) { return v > deadline; });
    const double worst = *std::max_element (best.begin(), best.end());
    const double load = total / 10000.0 * 100.0;
    WARN ("Align DSP compute only (offline, not a device test), stereo, 96 kHz, 64-sample blocks, always fading, "
          << raw.size() << " blocks: mean load " << load << " % of one core; raw max " << sorted.back() << " ms, p95 " << pct (95)
          << ", p99 " << pct (99) << ", p99.9 " << pct (99.9) << " ms; raw blocks over the " << deadline << " ms deadline: " << misses
          << "; repeatable worst (fastest of 3 per block) " << worst << " ms");
    CHECK (load < 10.0);
    CHECK (worst < deadline * 0.5);
}

// The sample build's model under the scenarios the 9 Nov audit reproduced (SA01-SA03, SA07, SA08, SA12 and the
// overlapping-export case). A small synthetic take (three mics, four hits) so it runs anywhere, CI included.
#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "samples/SampleModel.h"
#include "core/Session.h"
#include "hl/Session.h"
#include <cmath>

namespace te = tracktion;
using ld::samples::SampleModel;

namespace
{
constexpr double SR = 48000;

void writeMono (const juce::File& f, const std::vector<float>& x)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 1, 32, {}, 0));
    REQUIRE (w != nullptr);
    const float* ch[1] = { x.data() };
    w->writeFromFloatArrays (ch, 1, (int) x.size());
}

/** Three mics of four snare-ish hits a second apart: the lead, a copy 20 samples later, a copy 35 later and flipped. */
juce::File makeTake (const juce::String& name, int mics = 3)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_model_" + name);
    dir.deleteRecursively();
    dir.createDirectory();
    const int n = (int) (5.0 * SR);
    std::vector<float> lead ((size_t) n, 0.0f);
    juce::Random rnd (7);
    for (int h = 0; h < 4; ++h)
    {
        const int at = (int) ((0.5 + h) * SR);
        const float amp = 0.3f + 0.15f * h;
        for (int k = 0; k < (int) (0.25 * SR); ++k)
        {
            const double t = k / SR, env = std::exp (-t * 18.0);
            lead[(size_t) (at + k)] = (float) (amp * env * (std::sin (2 * M_PI * 185 * t) + 0.5 * (rnd.nextFloat() * 2 - 1)));
        }
    }
    for (auto& v : lead) v += (float) (1e-4 * (rnd.nextFloat() * 2 - 1));
    const char* micName[3] = { "TOP", "BOTTOM", "OH" };
    const int delay[3] = { 0, 20, 35 };
    const float gain[3] = { 1.0f, 0.7f, -0.5f };
    for (int m = 0; m < mics; ++m)
    {
        std::vector<float> x ((size_t) n, 0.0f);
        for (int i = delay[m]; i < n; ++i) x[(size_t) i] = gain[m] * lead[(size_t) (i - delay[m])];
        writeMono (dir.getChildFile (juce::String ("SNARE_CENTER_") + micName[m] + "_V01_RR04.wav"), x);
    }
    return dir;
}

void pumpUntilIdle (SampleModel& m, int ms = 60000)
{
    const auto t0 = juce::Time::getMillisecondCounter();
    do juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    while ((m.isPrepping() || m.isBusy()) && juce::Time::getMillisecondCounter() - t0 < (juce::uint32) ms);
}

void prep (SampleModel& m)
{
    bool done = false;
    m.autoPrep ([&] (juce::String) { done = true; });
    const auto t0 = juce::Time::getMillisecondCounter();
    while (! done && juce::Time::getMillisecondCounter() - t0 < 60000) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    pumpUntilIdle (m);
}

juce::AudioBuffer<float> readWav (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    REQUIRE (r != nullptr);
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}

double diffDb (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double e = 0, d = 0;
    for (int c = 0; c < std::min (a.getNumChannels(), b.getNumChannels()); ++c)
        for (int n = 0; n < std::min (a.getNumSamples(), b.getNumSamples()); ++n)
        { const double x = a.getSample (c, n), y = b.getSample (c, n); e += x * x; d += (x - y) * (x - y); }
    return 10 * std::log10 (std::max (d, 1e-30) / std::max (e, 1e-30));
}

juce::File freshDir (const juce::String& name)
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_model_out_" + name);
    d.deleteRecursively(); d.createDirectory();
    return d;
}
} // namespace

TEST_CASE ("Sample model: the synthetic take preps")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("base") }).isEmpty());
    prep (m);
    REQUIRE (m.isPrepared());
    CHECK (m.session()->onsets.size() == 4);
    CHECK (m.exportBlocker().isEmpty());
}

TEST_CASE ("Sample model SA01: an Auto Prep that lands after another take opened is dropped")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa01a") }).isEmpty());
    bool done = false;
    m.autoPrep ([&] (juce::String) { done = true; });
    REQUIRE (m.open ({ makeTake ("sa01b", 1) }).isEmpty());   // before the prep comes back
    const auto t0 = juce::Time::getMillisecondCounter();
    while (juce::Time::getMillisecondCounter() - t0 < 8000) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    CHECK (m.tracks().size() == 1);
    CHECK (m.session()->tracks.size() == 1);
    CHECK_FALSE (done);   // its callback never reported
}

TEST_CASE ("Sample model SA02: export never writes a render older than the settings")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa02") }).isEmpty());
    prep (m);
    REQUIRE (m.isPrepared());
    int n = 0;
    auto base = freshDir ("sa02_base");
    REQUIRE (m.exportTo (base, n).isEmpty());
    REQUIRE (n > 0);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 1.0; }, SampleModel::Redo::mix));
    // straight away: refused, not written from the old render
    auto early = freshDir ("sa02_early");
    int n2 = 0;
    CHECK (m.exportTo (early, n2).isNotEmpty());
    CHECK (n2 == 0);
    pumpUntilIdle (m);
    auto settled = freshDir ("sa02_settled");
    REQUIRE (m.exportTo (settled, n2).isEmpty());
    auto first = base.findChildFiles (juce::File::findFiles, true, "*.wav");
    first.sort();
    REQUIRE (! first.isEmpty());
    const auto rel = first[0].getRelativePathFrom (base);
    CHECK (diffDb (readWav (first[0]), readWav (settled.getChildFile (rel))) > -40);   // the tone change is in it
}

TEST_CASE ("Sample model SA03: changed counts block export until Auto Prep runs")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa03") }).isEmpty());
    prep (m);
    REQUIRE (m.exportBlocker().isEmpty());
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.det.RR = 5; }, SampleModel::Redo::reprep));
    CHECK_FALSE (m.isPrepared());
    int n = 0;
    CHECK (m.exportTo (freshDir ("sa03"), n).isNotEmpty());
    CHECK (n == 0);
    pumpUntilIdle (m);
    CHECK (m.exportBlocker().isNotEmpty());
}

TEST_CASE ("Sample model SA07: polarity lives in the session; export settings don't touch it")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa07") }).isEmpty());
    prep (m);
    int t = 0;
    for (int i = 0; i < (int) m.tracks().size(); ++i) if (i != m.leadTrack()) { t = i; break; }
    const bool was = m.flip (t);
    m.setFlip (t, ! was);   // the Hits step's Ø
    CHECK (m.flip (t) == ! was);
    auto* a = ld::live::alignOn (*m.trackB (t));
    CHECK (a->flipValue.get() == ! was);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.exp.bits = 16; }, SampleModel::Redo::none));
    CHECK (m.flip (t) == ! was);
    CHECK (a->flipValue.get() == ! was);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.exp.bits = 24; }, SampleModel::Redo::align));
    CHECK (a->flipValue.get() == ! was);
}

TEST_CASE ("Sample model SA08: Level match follows a level change")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa08") }).isEmpty());
    prep (m);
    m.setLevelMatch (true);
    auto avgLead = [&]
    {
        double s = 0; int n = 0;
        for (size_t i = 0; i < m.hits().size(); ++i) { s += m.hitGainDb (m.leadTrack(), i); ++n; }
        return s / n;
    };
    const double before = avgLead();
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.lev.offsetDb -= 12; }, SampleModel::Redo::levels));
    pumpUntilIdle (m);
    const double after = avgLead();
    CHECK (std::abs ((after - before) + 12) < 0.5);
    const float aDb = m.trackA (m.leadTrack())->getVolumePlugin()->getVolumeDb();
    CHECK (std::abs (aDb - (m.monDb (m.leadTrack()) + after)) < 0.05);
}

TEST_CASE ("Sample model SA12: every setting undoes and redoes")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa12") }).isEmpty());
    prep (m);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.4; }, SampleModel::Redo::mix, "tone.bright"));
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.8; }, SampleModel::Redo::mix, "tone.bright"));   // same drag
    int t = m.leadTrack() == 0 ? 1 : 0;
    m.setFlip (t, ! m.flip (t));
    const bool flipped = m.flip (t);
    m.undo();   // the flip
    CHECK (m.flip (t) != flipped);
    CHECK (ld::live::alignOn (*m.trackB (t))->flipValue.get() == m.flip (t));
    m.undo();   // the whole drag
    CHECK (m.session()->tone.bright == 0.0);
    m.redo();
    CHECK (m.session()->tone.bright == 0.8);
    pumpUntilIdle (m);
    CHECK (m.exportBlocker().isEmpty());
}

TEST_CASE ("Sample model: a hit tuned far down exports alone, whatever overlaps it")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("tune") }).isEmpty());
    prep (m);
    // every hit a quarter speed: each region plays four times as long and runs over the next three hits
    REQUIRE (m.change ([] (ld::hl::Session& S)
    {
        ld::hl::TuneState ti;
        ti.ratios.assign (S.onsets.size(), 0.25);
        S.tinfo = ti;
    }, SampleModel::Redo::none));
    m.layoutHits();
    pumpUntilIdle (m);
    int n = 0;
    auto out = freshDir ("tune");
    REQUIRE (m.exportTo (out, n).isEmpty());
    auto files = out.findChildFiles (juce::File::findFiles, true, "Top.wav");
    REQUIRE (files.size() == 4);
    for (auto& f : files)
    {
        const auto b = readWav (f);
        // the burst is 0.25 s, so 1 s at quarter speed: after 1.3 s there must be nothing but its own faint noise
        double head = 0, tail = 0;
        const int cut = (int) (1.3 * SR);
        for (int i = 0; i < b.getNumSamples(); ++i) (i < cut ? head : tail) = std::max (i < cut ? head : tail, (double) std::abs (b.getSample (0, i)));
        INFO (f.getFileName() << ": peak before 1.3 s " << head << ", after " << tail);
        CHECK (tail < head * 0.01);
    }
}

TEST_CASE ("Sample model SA14: the Align score is measured where the mic sits now")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("sa14") }).isEmpty());
    prep (m);
    int t = -1;
    for (int i = 0; i < (int) m.tracks().size(); ++i)
        if (i != m.leadTrack() && m.session()->tracks[(size_t) i].alignTo != "none") { t = i; break; }
    REQUIRE (t >= 0);
    auto settle = [&]
    {
        std::pair<double, double> v;
        for (int k = 0; k < 400; ++k) { v = m.sumScore (t); juce::MessageManager::getInstance()->runDispatchLoopUntil (10); if (std::isfinite (v.first) && ! (k < 3)) break; }
        for (int k = 0; k < 50; ++k) { juce::MessageManager::getInstance()->runDispatchLoopUntil (10); v = m.sumScore (t); }
        return v;
    };
    const auto good = settle();
    REQUIRE (std::isfinite (good.first));
    m.setFlip (t, ! m.flip (t));   // a deliberately wrong flip
    const auto bad = settle();
    INFO ("aligned " << good.first << ", wrongly flipped " << bad.first);
    CHECK (bad.first < good.first - 0.5);
    CHECK (std::abs (bad.second - good.second) < 1e-9);   // as recorded doesn't move
}

TEST_CASE ("Sample model: a tuned hit's phase EQ moves with the tuning (matches the reference export)")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("apTune") }).isEmpty());
    prep (m);
    int t = std::getenv ("LD_TEST_LEAD") ? m.leadTrack() : (m.leadTrack() == 0 ? 1 : 0);
    // a phase EQ on one mic, every hit tuned 8 % down
    REQUIRE (m.change ([t] (ld::hl::Session& S)
    {
        auto& tr = S.tracks[(size_t) t];
        if (! std::getenv ("LD_TEST_NOAP")) { tr.ap = { { 250.0, 0.7 } }; tr.apOn = true; }
        ld::hl::TuneState ti;
        ti.ratios.assign (S.onsets.size(), std::getenv ("LD_TEST_RATIO") ? std::atof (std::getenv ("LD_TEST_RATIO")) : 0.92);
        S.tinfo = ti;
    }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    m.change ([] (ld::hl::Session&) {}, SampleModel::Redo::align);   // the plugin hears the new phase EQ
    m.layoutHits();
    pumpUntilIdle (m);
    const auto* st = m.engineState();
    REQUIRE (st->mix);
    REQUIRE (st->levels);
    auto S = st->session;
    S.tuneTaps = 64;   // the reference at the same filter length the hits are rendered with
    const auto& R = st->levels->regions;
    // B for that mic through Tracktion, against the reference render of every hit
    juce::TemporaryFile temp (".wav");
    auto& edit = *m.getEdit();
    juce::BigInteger which; which.setBit (te::getAllTracks (edit).indexOf (m.trackB (t)));
    te::Renderer::Parameters p (edit);
    p.destFile = temp.getFile();
    p.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32; p.sampleRateForAudio = S.sr; p.blockSizeForAudio = 512;
    p.time = te::TimeRange (te::TimePosition(), edit.getLength());
    p.tracksToDo = which; p.usePlugins = true; p.useMasterPlugins = false; p.mustRenderInMono = true;
    REQUIRE (te::Renderer::renderToFile ("Render", p) != juce::File());
    const auto b = readWav (temp.getFile());
    const long pad = (long) std::lround (SampleModel::kPad * S.sr);
    double e = 0, d = 0;
    for (size_t i = 0; i < R.size(); ++i)
    {
        const auto ref = ld::hl::renderHitMic (S, *st->mix, t, i, R[i], S.sr, std::pow (10.0, m.hitGainDb (t, i) / 20));
        const long cap = std::min<long> ((long) ref[0].size(), (long) (0.3 * S.sr));   // the hit itself
        for (long n = 0; n < cap; ++n)
        {
            const long k = pad + R[i].start + n;
            if (k >= b.getNumSamples()) break;
            const double r = ref[0][(size_t) n], x = b.getSample (0, (int) k);
            e += r * r; d += (r - x) * (r - x);
        }
    }
    if (std::getenv ("LD_TEST_VERBOSE"))
        for (size_t i = 0; i < R.size(); ++i)
        {
            const auto ref = ld::hl::renderHitMic (S, *st->mix, t, i, R[i], S.sr, std::pow (10.0, m.hitGainDb (t, i) / 20));
            const long W = (long) (0.2 * S.sr);
            double best = -1e30; int bl = 0, rpk = 0, bpk = 0; float rm = 0, bm = 0;
            for (int l = -400; l <= 400; ++l)
            {
                double c = 0;
                for (long n = 0; n < W && n < (long) ref[0].size(); ++n) { const long k = pad + R[i].start + n + l; if (k >= 0 && k < b.getNumSamples()) c += ref[0][(size_t) n] * b.getSample (0, (int) k); }
                if (c > best) { best = c; bl = l; }
            }
            for (long n = 0; n < W && n < (long) ref[0].size(); ++n) { if (std::abs (ref[0][(size_t) n]) > rm) { rm = std::abs (ref[0][(size_t) n]); rpk = (int) n; } const float x = std::abs (b.getSample (0, (int) (pad + R[i].start + n))); if (x > bm) { bm = x; bpk = (int) n; } }
            fprintf (stderr, "hit %zu: region %ld..%ld onset %ld, best lag %d, peak ref %.3f at %d, B %.3f at %d, gain %.2f\n", i, R[i].start, R[i].end, S.onsets[i], bl, rm, rpk, bm, bpk, m.hitGainDb (t, i));
        }
    const double nullDb = 10 * std::log10 (std::max (d, 1e-30) / std::max (e, 1e-30));
    INFO ("tuned hits with a phase EQ against the reference: " << nullDb << " dB");
    CHECK (nullDb < -50);   // (without the phase EQ following the tuning this is about -15 dB)
    if (std::getenv ("LD_TEST_VERBOSE")) fprintf (stderr, "null %.1f dB\n", nullDb);
}

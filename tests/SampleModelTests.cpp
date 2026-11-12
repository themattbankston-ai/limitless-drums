// The sample build's model under the scenarios the 9 Nov audit reproduced (SA01-SA03, SA07, SA08, SA12, the
// overlapping-export case) and the review that followed (CR01, CR03), plus the Hits screen's target and range logic (SA09, SA11). A small synthetic take (three mics, four hits) so it runs anywhere, CI included.
#include <random>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <complex>
#include "TestSignals.h"
#include "samples/SampleModel.h"
#include "core/Session.h"
#include "hl/Session.h"
#include "hl/Align.h"
#include "hl/Shape.h"
#include "hl/Prep.h"
#include "hl/Detect.h"
#include "samples/ui/HitsLogic.h"
#include <cmath>

namespace te = tracktion;
using ld::samples::SampleModel;
using ld::samples::HitInfo;
using Catch::Approx;

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

namespace
{
int scoreTrack (SampleModel& m)
{
    for (int i = 0; i < (int) m.tracks().size(); ++i)
        if (i != m.leadTrack() && m.session()->tracks[(size_t) i].alignTo != "none") return i;
    return -1;
}

/** Pumps until mic t's score is current (not stale) and returns it. */
std::pair<double, double> settledScore (SampleModel& m, int t)
{
    std::pair<double, double> v;
    bool stale = true;
    for (int k = 0; k < 1000 && stale; ++k) { v = m.sumScore (t, &stale); juce::MessageManager::getInstance()->runDispatchLoopUntil (5); }
    REQUIRE_FALSE (stale);
    return v;
}
} // namespace

TEST_CASE ("Sample model CR03: the Align score goes stale when any phase EQ band or Q changes")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("cr03") }).isEmpty());
    prep (m);
    const int t = scoreTrack (m);
    REQUIRE (t >= 0);
    auto setAp = [&] (double q2)
    {
        REQUIRE (m.change ([t, q2] (ld::hl::Session& S)
        {
            auto& tr = S.tracks[(size_t) t];
            tr.apOn = true; tr.ap = { { 400, 0.7 }, { 2500, q2 } };
        }, SampleModel::Redo::none, ""));
    };
    setAp (0.7);
    const auto a = settledScore (m, t);
    bool stale = false;
    m.sumScore (t, &stale);
    CHECK_FALSE (stale);
    setAp (4.0);   // only the second band's Q
    m.sumScore (t, &stale);
    CHECK (stale);   // the old score isn't passed off as current
    const auto b = settledScore (m, t);
    INFO ("Q 0.7: " << a.first << ", Q 4: " << b.first);
    CHECK (std::isfinite (b.first));
    CHECK (a.first != b.first);   // measured again with the new band
    setAp (0.7);
    CHECK (std::abs (settledScore (m, t).first - a.first) < 1e-12);   // and the same inputs give the same score
}

TEST_CASE ("Sample model CR01: scoring while Undo and Redo replace the session (run under ThreadSanitizer too)")
{
    // the workers read only what was captured when they started; undo/redo/flip churn underneath them, then the
    // shown score has to equal a fresh measurement of the session as it finally is
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("cr01") }).isEmpty());
    prep (m);
    const int t = scoreTrack (m);
    REQUIRE (t >= 0);
    for (int k = 0; k < 6; ++k)
        REQUIRE (m.change ([t, k] (ld::hl::Session& S) { S.tracks[(size_t) t].lag += 0.5 + k; }, SampleModel::Redo::align, ""));
    juce::Random rnd (11);
    for (int k = 0; k < 300; ++k)
    {
        for (int i = 0; i < (int) m.tracks().size(); ++i) m.sumScore (i);
        const int op = rnd.nextInt (4);
        if (op == 0) m.undo();
        else if (op == 1) m.redo();
        else if (op == 2) m.setFlip (t, ! m.flip (t));
        else m.change ([t, k] (ld::hl::Session& S) { auto& tr = S.tracks[(size_t) t]; tr.apOn = ! tr.apOn; tr.ap = { { 300.0 + k, 0.7 } }; }, SampleModel::Redo::none, "");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (rnd.nextInt (3));
    }
    const auto shown = settledScore (m, t);
    pumpUntilIdle (m);
    // a fresh measurement of the final session, the way the worker does it
    const auto& S = *m.session();
    const auto& tr = S.tracks[(size_t) t];
    const auto& res = tr.alignTo.size() == 1 ? tr.grp : tr.autoResult;
    REQUIRE (res);
    const int r = ld::hl::alignRef (S, t) >= 0 ? ld::hl::alignRef (S, t) : S.refIdx;
    const auto ap = tr.apOn ? tr.ap : std::vector<ld::hl::AllPass> {};
    const auto sc = ld::hl::scoreMic (ld::hl::monoOf (S.tracks[(size_t) r].ch), tr.ch, std::vector<int> (S.onsets.begin(), S.onsets.end()), S.sr, tr.lag, tr.pol, ap, res->arrival);
    REQUIRE (sc.valid);
    INFO ("shown " << shown.first << ", fresh " << sc.score);
    CHECK (std::abs (shown.first - sc.score) < 1e-12);
    CHECK (std::abs (shown.second - sc.asis) < 1e-12);
}

TEST_CASE ("Hits screen SA09: the smallest target under the pointer wins, whatever order they were added")
{
    using ld::samples::Target;
    const Target row { 0, 0, 400, 30 }, button { 350, 5, 40, 20 }, other { 500, 0, 40, 20 };
    CHECK (ld::samples::pickTarget (std::vector<Target> { row, button }, 360, 10) == 1);
    CHECK (ld::samples::pickTarget (std::vector<Target> { button, row }, 360, 10) == 0);   // added before the row: still the button
    CHECK (ld::samples::pickTarget (std::vector<Target> { button, row }, 100, 10) == 1);   // outside the button: the row
    CHECK (ld::samples::pickTarget (std::vector<Target> { row, button, other }, 600, 10) == -1);
    CHECK (ld::samples::pickTarget (std::vector<Target> { button, button }, 360, 10) == 1);   // ties: the last drawn (on top)
    CHECK (ld::samples::pickTarget (std::vector<Target> { row }, 400, 10) == -1);   // right edge is exclusive
}

TEST_CASE ("Hits screen SA11: Hit, Layer and Sweep select the right range")
{
    // two velocities x three round robins, deliberately out of time order
    std::vector<HitInfo> H;
    auto add = [&] (double time, int vel, int rr) { HitInfo h; h.time = time; h.end = time + 0.4; h.vel = vel; h.rr = rr; H.push_back (h); };
    add (1.0, 0, 0); add (2.0, 0, 1); add (0.5, 0, 2);
    add (4.0, 1, 0); add (3.0, 1, 1); add (5.0, 1, 2);
    auto r = ld::samples::auditionRange (H, 1, 0);   // Hit
    CHECK (r.first == Approx (2.0 - 0.005)); CHECK (r.second == Approx (2.4));
    r = ld::samples::auditionRange (H, 1, 1);        // Layer: velocity 0's round robins only
    CHECK (r.first == Approx (0.5 - 0.005)); CHECK (r.second == Approx (2.4));
    r = ld::samples::auditionRange (H, 3, 1);        // Layer of velocity 1
    CHECK (r.first == Approx (3.0 - 0.005)); CHECK (r.second == Approx (5.4));
    r = ld::samples::auditionRange (H, 3, 2);        // Sweep: everything
    CHECK (r.first == Approx (0.5 - 0.005)); CHECK (r.second == Approx (5.4));
    H[2].time = 0.001;
    CHECK (ld::samples::auditionRange (H, 2, 0).first == 0.0);   // never before the start
    CHECK (ld::samples::auditionRange (std::vector<HitInfo> {}, 0, 2) == std::make_pair (0.0, 0.0));
    CHECK (ld::samples::auditionRange (H, 99, 0).first == Approx (5.0 - 0.005));   // out of range clamps
}

namespace
{
/** Every .wav under dir, by relative path, as raw bytes. */
std::map<juce::String, juce::MemoryBlock> exportedBytes (const juce::File& dir)
{
    std::map<juce::String, juce::MemoryBlock> out;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.wav"))
    {
        juce::MemoryBlock mb;
        f.loadFileAsData (mb);
        out[f.getRelativePathFrom (dir)] = mb;
    }
    return out;
}
} // namespace

TEST_CASE ("Sample model M1: the same settings export the same bytes, and the files match the export list")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("m1det") }).isEmpty());
    prep (m);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.5; }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    int n1 = 0, n2 = 0;
    auto a = freshDir ("m1det_a"), b = freshDir ("m1det_b");
    REQUIRE (m.exportTo (a, n1).isEmpty());
    REQUIRE (m.exportTo (b, n2).isEmpty());
    const auto A = exportedBytes (a), B = exportedBytes (b);
    CHECK (n1 == n2);
    CHECK ((int) A.size() == n1);
    CHECK ((int) ld::hl::exportList (*m.session()).size() == n1);   // nothing in the list is skipped silently
    REQUIRE (A.size() == B.size());
    int differ = 0;
    for (auto& [k, v] : A) if (! B.count (k) || B.at (k) != v) ++differ;
    CHECK (differ == 0);
}

TEST_CASE ("Sample model M1: monitor faders, pans, mutes, solos, A/B and level match never reach the export, and come back after it")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("m1mon") }).isEmpty());
    prep (m);
    int n = 0;
    auto base = freshDir ("m1mon_base");
    REQUIRE (m.exportTo (base, n).isEmpty());
    // everything a listener might leave set
    m.setMonDb (0, -18.0f);
    m.setPan (1, -0.7f);
    m.setMute (2, true);
    m.setSolo (1, true);
    m.setProcessed (false);   // A playing
    m.setLevelMatch (true);
    pumpUntilIdle (m);
    auto with = freshDir ("m1mon_with");
    REQUIRE (m.exportTo (with, n).isEmpty());
    const auto A = exportedBytes (base), B = exportedBytes (with);
    REQUIRE (A.size() == B.size());
    int differ = 0;
    for (auto& [k, v] : A) if (! B.count (k) || B.at (k) != v) ++differ;
    CHECK (differ == 0);
    // and the monitoring is as the listener left it
    CHECK (m.monDb (0) == Approx (-18.0f));
    CHECK (m.pan (1) == Approx (-0.7f));
    CHECK (m.mute (2));
    CHECK (m.solo (1));
    CHECK_FALSE (m.processed());
    CHECK (m.levelMatch());
}

TEST_CASE ("Sample model M1: an unwritable destination fails before rendering, says where, and leaves everything as it was")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("m1dest") }).isEmpty());
    prep (m);
    m.setSolo (1, true);
    // a folder inside a file can't be made
    auto blocker = freshDir ("m1dest").getChildFile ("not-a-folder");
    REQUIRE (blocker.replaceWithText ("x"));
    const auto dest = blocker.getChildFile ("out");
    int n = -1;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const auto err = m.exportTo (dest, n);
    const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    INFO ("error: " << err << " after " << ms << " ms");
    CHECK (err.isNotEmpty());
    CHECK (err.contains (dest.getFullPathName()));
    CHECK (n == 0);
    CHECK (ms < 1000);   // told at once, not after a full render
    CHECK (m.solo (1));
    CHECK (m.exportBlocker().isEmpty());   // and nothing about the take changed
}

TEST_CASE ("Sample model M1: rapid edits while rendering settle on the last one, and that's what exports")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("m1rapid") }).isEmpty());
    prep (m);
    for (int k = 0; k <= 10; ++k)
    {
        REQUIRE (m.change ([k] (ld::hl::Session& S) { S.tone.bright = k / 10.0; }, SampleModel::Redo::mix, "tone.bright"));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (3);
    }
    pumpUntilIdle (m);
    int n = 0;
    auto rapid = freshDir ("m1rapid_a");
    REQUIRE (m.exportTo (rapid, n).isEmpty());
    // a fresh model set straight to the last value
    SampleModel m2 (ldtest::engine());
    REQUIRE (m2.open ({ makeTake ("m1rapid") }).isEmpty());
    prep (m2);
    REQUIRE (m2.change ([] (ld::hl::Session& S) { S.tone.bright = 1.0; }, SampleModel::Redo::mix));
    pumpUntilIdle (m2);
    auto direct = freshDir ("m1rapid_b");
    REQUIRE (m2.exportTo (direct, n).isEmpty());
    const auto A = exportedBytes (rapid), B = exportedBytes (direct);
    REQUIRE (A.size() == B.size());
    int differ = 0;
    for (auto& [k, v] : A) if (! B.count (k) || B.at (k) != v) ++differ;
    CHECK (differ == 0);
    m.undo();   // the whole drag is one step
    CHECK (m.session()->tone.bright == 0.0);
    // (the model is destroyed here with the undo's render still running: its destructor waits for it)
}

TEST_CASE ("Sample model M1: closing a take while it renders waits for the work, and nothing lands afterwards")
{
    bool landed = false;
    {
        SampleModel m (ldtest::engine());
        REQUIRE (m.open ({ makeTake ("m1close") }).isEmpty());
        prep (m);
        m.onChanged = [&] { landed = true; };
        landed = false;
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.7; }, SampleModel::Redo::mix));
        REQUIRE (m.isBusy());
    }   // destroyed mid-render
    for (int k = 0; k < 50; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    CHECK_FALSE (landed);
}

TEST_CASE ("Sample model: a phase EQ band set by hand is what plays, survives nudges and flips, and Reset returns to measured")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("manualap") }).isEmpty());
    prep (m);
    const int t = scoreTrack (m);
    REQUIRE (t >= 0);
    const auto measured = m.session()->tracks[(size_t) t].ap;
    using R = SampleModel::Redo;
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::setAllPass (S, t, 0, 430.0, 1.3); }, R::align));
    {
        const auto& tr = m.session()->tracks[(size_t) t];
        REQUIRE (tr.ap.size() >= 1);
        CHECK (tr.ap[0].fc == Approx (430.0)); CHECK (tr.ap[0].Q == Approx (1.3));
        CHECK (tr.manualAp);
    }
    // the plugin on that mic gets the same band
    auto* plug = ld::live::alignOn (*m.trackB (t));
    REQUIRE (plug != nullptr);
    CHECK (plug->ap1Fc.get() == Approx (430.0f));
    CHECK (plug->ap1Q.get() == Approx (1.3f));
    // a nudge and a flip re-resolve the alignment: the band stays
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::nudgeLag (S, t, 1); }, R::align));
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::flipMic (S, t); }, R::align));
    CHECK (m.session()->tracks[(size_t) t].ap[0].fc == Approx (430.0));
    // out-of-range values are kept sane; a second band can be added and removed
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::setAllPass (S, t, 1, 1e9, 0.0); }, R::align));
    {
        const auto& tr = m.session()->tracks[(size_t) t];
        REQUIRE (tr.ap.size() == 2);
        CHECK (tr.ap[1].fc <= 0.45 * SR); CHECK (tr.ap[1].Q >= 0.3);
    }
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::removeAllPass (S, t, 1); }, R::align));
    CHECK (m.session()->tracks[(size_t) t].ap.size() == 1);
    // switched off: nothing plays; back on: the hand-set band again
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::setPhaseEq (S, t, false); }, R::align));
    CHECK (m.session()->tracks[(size_t) t].ap.empty());
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::setPhaseEq (S, t, true); }, R::align));
    CHECK (m.session()->tracks[(size_t) t].ap[0].fc == Approx (430.0));
    // undo steps back through it; Reset returns to what was measured
    m.undo();
    CHECK (m.session()->tracks[(size_t) t].ap.empty());
    m.redo();
    REQUIRE (m.change ([t] (ld::hl::Session& S) { ld::hl::resetMic (S, t); }, R::align));
    {
        const auto& tr = m.session()->tracks[(size_t) t];
        CHECK_FALSE (tr.manualAp);
        REQUIRE (tr.ap.size() == measured.size());
        for (size_t i = 0; i < measured.size(); ++i) CHECK (tr.ap[i].fc == Approx (measured[i].fc));
    }
    pumpUntilIdle (m);
}

namespace
{
/** The 3-mic take plus a ROOM: the lead about 23 ms later (8 m), darker. The distance leaves the drum's 185 Hz
    fundamental roughly a third of a cycle out against the lead, which neither polarity fixes. */
juce::File makeRoomTake (const juce::String& name)
{
    auto dir = makeTake (name);
    const auto b = readWav (dir.getChildFile ("SNARE_CENTER_TOP_V01_RR04.wav"));
    const int n = b.getNumSamples(), d = (int) (0.02335 * SR);
    std::vector<float> x ((size_t) n, 0.0f);
    for (int i = d; i < n; ++i) x[(size_t) i] = 0.5f * b.getSample (0, i - d);
    juce::IIRFilter lp; lp.setCoefficients (juce::IIRCoefficients::makeLowPass (SR, 3000.0));
    lp.processSamples (x.data(), n);
    writeMono (dir.getChildFile ("SNARE_CENTER_ROOM_V01_RR04.wav"), x);
    return dir;
}
int roomTrack (SampleModel& m)
{
    for (int i = 0; i < (int) m.tracks().size(); ++i) if (juce::String (m.tracks()[(size_t) i].name).containsIgnoreCase ("room")) return i;
    return -1;
}
} // namespace

TEST_CASE ("Sample model: rooms can keep their distance and still be set in phase, and Auto Prep follows the choice")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeRoomTake ("roomphase") }).isEmpty());
    prep (m);
    const int r = roomTrack (m);
    REQUIRE (r >= 0);
    // the default: left as recorded (no shift, no phase EQ)
    {
        const auto& tr = m.session()->tracks[(size_t) r];
        CHECK (tr.alignTo == "none");
        CHECK (tr.lag == 0.0);
        CHECK (tr.ap.empty());
        REQUIRE (tr.inPlace);   // measured anyway, so switching is instant
        CHECK (tr.inPlace->valid);
        CHECK (tr.inPlace->lag == 0.0);
    }
    const auto before = settledScore (m, r);
    // switched on: still not moved in time; polarity and phase EQ as measured in place
    m.setRoomPhase (true);
    pumpUntilIdle (m);
    {
        const auto& tr = m.session()->tracks[(size_t) r];
        CHECK (tr.alignTo == "phase");
        CHECK (tr.lag == 0.0);   // its distance is untouched
        CHECK (tr.pol == tr.inPlace->pol);
        CHECK (tr.ap.size() == tr.inPlace->ap.size());
        auto* plug = ld::live::alignOn (*m.trackB (r));
        REQUIRE (plug != nullptr);
        CHECK (plug->shiftValue.get() == Approx (0.0f));
        CHECK (plug->flipValue.get() == (tr.pol < 0));
    }
    const auto after = settledScore (m, r);
    {
        const auto& ip = *m.session()->tracks[(size_t) r].inPlace;
        INFO ("sum with the lead: as recorded " << before.first << ", in phase " << after.first << "; polarity " << ip.pol << ", bands " << ip.ap.size()
              << (ip.ap.empty() ? std::string() : " first at " + std::to_string (ip.ap[0].fc) + " Hz"));
        CHECK (after.first >= before.first);            // never worse than leaving it
        CHECK_FALSE (ip.ap.empty());                    // here the phase EQ has something to fix
        CHECK (after.first > before.first + 0.02);      // and the sum is clearly better for it
    }
    // the close mics are exactly as they were
    for (int t = 0; t < (int) m.tracks().size(); ++t)
        if (t != r && t != m.leadTrack()) CHECK (m.session()->tracks[(size_t) t].alignTo == "lead");
    // Auto Prep again, and a newly opened take, keep following the choice
    prep (m);
    CHECK (m.session()->tracks[(size_t) roomTrack (m)].alignTo == "phase");
    CHECK (m.session()->tracks[(size_t) roomTrack (m)].lag == 0.0);
    REQUIRE (m.open ({ makeRoomTake ("roomphase2") }).isEmpty());
    prep (m);
    CHECK (m.roomPhase());
    CHECK (m.session()->tracks[(size_t) roomTrack (m)].alignTo == "phase");
    // off again: back to exactly as recorded; and undo restores it
    m.setRoomPhase (false);
    CHECK (m.session()->tracks[(size_t) roomTrack (m)].alignTo == "none");
    CHECK (m.session()->tracks[(size_t) roomTrack (m)].ap.empty());
    pumpUntilIdle (m);
}

TEST_CASE ("Views: Hit, Velocity layer and All layers show the right hits and never lose the selected one")
{
    // 3 velocities x 4 round robins, layer by layer
    std::vector<HitInfo> H;
    for (int v = 0; v < 3; ++v) for (int r = 0; r < 4; ++r) { HitInfo h; h.vel = v; h.rr = r; h.time = (double) H.size(); h.end = h.time + 0.5; H.push_back (h); }
    using ld::samples::inView; using ld::samples::viewHits;
    const int sel = 6;   // velocity 1, round robin 2
    CHECK (viewHits (H, sel, 0) == std::vector<int> { 6 });
    CHECK (viewHits (H, sel, 1) == std::vector<int> { 4, 5, 6, 7 });
    CHECK (viewHits (H, sel, 2).size() == 12);
    CHECK (inView (H, sel, 1, 7));
    CHECK_FALSE (inView (H, sel, 1, 8));
    CHECK_FALSE (inView (H, sel, 0, 5));
    CHECK_FALSE (inView (H, sel, 2, 12));   // out of range is never in view
    // thinned to fit: still in order, no more than asked, and the selected hit is always among them
    for (int cap = 1; cap <= 12; ++cap)
        for (int s2 = 0; s2 < 12; ++s2)
        {
            const auto v = viewHits (H, s2, 2, cap);
            CHECK ((int) v.size() <= cap);
            CHECK (std::is_sorted (v.begin(), v.end()));
            CHECK (std::find (v.begin(), v.end(), s2) != v.end());
            CHECK (std::adjacent_find (v.begin(), v.end()) == v.end());
        }
    // unequal round-robin counts and an empty set
    H.pop_back(); H.pop_back();
    CHECK (viewHits (H, 9, 1) == std::vector<int> { 8, 9 });
    CHECK (viewHits (std::vector<HitInfo> {}, 0, 2).empty());
    // the audition range follows the same membership
    const auto r = ld::samples::auditionRange (H, 9, 1);
    CHECK (r.first == Approx (8.0 - 0.005)); CHECK (r.second == Approx (9.5));
}

// ------------------------------------------------------------------------------------------------ reference match
// Fixtures for the reference Tone match (hl::loadReference). A one-mic take with noise clean-up off, so the
// "source" the match measures is the recording itself, and references built from that same recording with a known
// difference. They run on engine 2 (the sample build's); the last case pins engine 1, which must never change.
namespace
{
void writeWav (const juce::File& f, const std::vector<std::vector<float>>& chans, double sr = SR)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) chans.size(), 32, {}, 0));
    REQUIRE (w != nullptr);
    std::vector<const float*> p;
    for (auto& c : chans) p.push_back (c.data());
    w->writeFromFloatArrays (p.data(), (int) chans.size(), (int) chans[0].size());
}

struct RefFixture
{
    SampleModel m { ldtest::engine() };
    juce::File dir;
    std::vector<float> src;
    RefFixture (const juce::String& name)
    {
        dir = makeTake ("ref_" + name, 1);
        REQUIRE (m.open ({ dir }).isEmpty());
        prep (m);
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.dnOn = false; }, SampleModel::Redo::mix));
        pumpUntilIdle (m);
        const auto b = readWav (dir.getChildFile ("SNARE_CENTER_TOP_V01_RR04.wav"));
        src.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
    }
    ld::hl::Reference match (const std::vector<std::vector<float>>& chans, double sr = SR, int engine = ld::hl::kReferenceEngine)
    {
        const auto f = dir.getSiblingFile (dir.getFileName() + "_reference.wav");
        writeWav (f, chans, sr);
        REQUIRE (m.mixRender() != nullptr);
        return ld::hl::loadReference (*m.session(), *m.mixRender(), f, engine);
    }
    static std::vector<float> filtered (std::vector<float> x, const juce::IIRCoefficients& c, int passes = 1)
    {
        for (int p = 0; p < passes; ++p) { juce::IIRFilter f; f.setCoefficients (c); f.processSamples (x.data(), (int) x.size()); }
        return x;
    }
};

/** Largest |curve| over the third-octave bands between lo and hi Hz. */
double worst (const ld::hl::Reference& R, double lo = 0, double hi = 1e9)
{
    const auto& B = ld::hl::bands3();
    double w = 0;
    for (size_t j = 0; j < B.size() && j < R.curve.size(); ++j) if (B[j] >= lo && B[j] <= hi) w = std::max (w, std::abs (R.curve[j]));
    return w;
}
std::string show (const ld::hl::Reference& R)
{
    const auto& B = ld::hl::bands3();
    std::string s;
    for (size_t j = 0; j < B.size() && j < R.curve.size(); ++j) s += juce::String (B[j], 0).toStdString() + ":" + juce::String (R.curve[j], 1).toStdString() + " ";
    return s;
}
} // namespace

TEST_CASE ("Reference match: the recording as its own reference asks for no change", "[refmatch]")
{
    RefFixture fx ("identity");
    const auto R = fx.match ({ fx.src });
    REQUIRE (R.error.empty());
    INFO (show (R));
    CHECK (worst (R) <= 0.5);
}

TEST_CASE ("Reference match: a reference that is only quieter asks for the same curve as the loud one", "[refmatch]")
{
    // level alone must not colour the match (whatever the identity case's own error is, a gain change adds nothing)
    RefFixture fx ("gain");
    const auto loud = fx.match ({ fx.src });
    auto q = fx.src;
    for (auto& v : q) v *= 0.25f;   // -12 dB
    const auto quiet = fx.match ({ q });
    REQUIRE (quiet.error.empty());
    REQUIRE (quiet.curve.size() == loud.curve.size());
    double diff = 0;
    for (size_t j = 0; j < loud.curve.size(); ++j) diff = std::max (diff, std::abs (loud.curve[j] - quiet.curve[j]));
    INFO ("largest difference " << diff << " dB");
    CHECK (diff <= 0.1);
}

TEST_CASE ("Reference match: a known broad EQ on the reference is recovered", "[refmatch]")
{
    RefFixture fx ("shelf");
    const auto c = juce::IIRCoefficients::makeHighShelf (SR, 3000.0, 0.707, juce::Decibels::decibelsToGain (6.0));
    const auto R = fx.match ({ RefFixture::filtered (fx.src, c) });
    REQUIRE (R.error.empty());
    INFO (show (R));
    // the shelf's own response at each band centre, against the curve (both relative to the 200 Hz band, where the
    // drum's energy is, since the match removes the overall level)
    const auto& B = ld::hl::bands3();
    auto shelfDb = [&] (double f)
    {
        const double w = 2 * M_PI * f / SR;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2 * w);
        const auto& k = c.coefficients;   // b0 b1 b2 a1 a2 (a0 = 1)
        return 20 * std::log10 (std::abs ((double) k[0] + (double) k[1] * z1 + (double) k[2] * z2) / std::abs (1.0 + (double) k[3] * z1 + (double) k[4] * z2));
    };
    size_t j200 = 0;
    for (size_t j = 0; j < B.size(); ++j) if (std::abs (B[j] - 200) < std::abs (B[j200] - 200)) j200 = j;
    double err = 0;
    for (size_t j = 0; j < B.size() && j < R.curve.size(); ++j)
        if (B[j] >= 100 && B[j] <= 12000) err = std::max (err, std::abs ((R.curve[j] - R.curve[j200]) - (shelfDb (B[j]) - shelfDb (B[j200]))));
    INFO ("worst fit error " << err << " dB");
    CHECK (err <= 2.0);   // third-octave bands, smoothed twice: the knee is blurred, the plateau must be right
    CHECK (worst (R, 6000, 12000) >= 4.0);
}

TEST_CASE ("Reference match RM01: a stereo reference with one side inverted matches like its mono self", "[refmatch]")
{
    RefFixture fx ("inverse");
    const auto eq = RefFixture::filtered (fx.src, juce::IIRCoefficients::makeHighShelf (SR, 3000.0, 0.707, juce::Decibels::decibelsToGain (6.0)));
    const auto mono = fx.match ({ eq });
    auto inv = eq;
    for (auto& v : inv) v = -v;
    const auto wide = fx.match ({ eq, inv });   // L and R in opposite polarity: silent if averaged, a real recording if listened to
    REQUIRE (mono.error.empty());
    INFO ("mono: " << show (mono));
    INFO ("L/-R: " << show (wide));
    REQUIRE (wide.curve.size() == mono.curve.size());
    double diff = 0;
    for (size_t j = 0; j < mono.curve.size(); ++j) diff = std::max (diff, std::abs (mono.curve[j] - wide.curve[j]));
    CHECK ((wide.error.empty() && diff <= 0.5));
}

TEST_CASE ("Reference match RM04: bands the reference has nothing in are left alone", "[refmatch]")
{
    RefFixture fx ("weak");
    // nothing above about 2 kHz (96 dB/octave), and 6 dB less of the drum's fundamental: the level difference is
    // real below 2 kHz, and there is no evidence at all above it
    auto ref = RefFixture::filtered (fx.src, juce::IIRCoefficients::makeLowPass (SR, 1500.0), 8);
    ref = RefFixture::filtered (ref, juce::IIRCoefficients::makePeakFilter (SR, 185.0, 1.0, juce::Decibels::decibelsToGain (-6.0)));
    const auto R = fx.match ({ ref });
    REQUIRE (R.error.empty());
    INFO (show (R));
    CHECK (worst (R, 6000, 15000) <= 0.25);   // no boost or cut where the reference is silent
}

TEST_CASE ("Reference match RM03: the reference you audition is resampled cleanly", "[refmatch]")
{
    RefFixture fx ("src");
    // a 5 kHz tone decaying slowly, recorded at 44.1 kHz, auditioned in a 48 kHz session
    const double rsr = 44100, f0 = 5000, decay = 3.0;
    std::vector<float> tone ((size_t) (2.0 * rsr), 0.0f);
    for (size_t i = 2000; i < tone.size(); ++i) { const double t = (double) (i - 2000) / rsr; tone[i] = (float) (0.5 * std::exp (-decay * t) * std::sin (2 * M_PI * f0 * t)); }
    const auto R = fx.match ({ tone }, rsr);
    REQUIRE (R.error.empty());
    REQUIRE (! R.audio.empty());
    const auto& y = R.audio[0];
    REQUIRE (y.size() > (size_t) (0.7 * SR));
    // undo the decay, fit a 5 kHz sine, and see what's left: a clean resample leaves almost nothing
    double ss = 0, sc = 0, s2 = 0, c2 = 0, scx = 0, tot = 0;
    const size_t a = (size_t) (0.1 * SR), b = (size_t) (0.6 * SR);
    std::vector<double> z;
    for (size_t i = a; i < b; ++i) z.push_back ((double) y[i] * std::exp (decay * (double) i / SR));
    for (size_t i = 0; i < z.size(); ++i)
    {
        const double ph = 2 * M_PI * f0 * (double) i / SR, s = std::sin (ph), c = std::cos (ph);
        ss += z[i] * s; sc += z[i] * c; s2 += s * s; c2 += c * c; scx += s * c; tot += z[i] * z[i];
    }
    const double det = s2 * c2 - scx * scx, A = (ss * c2 - sc * scx) / det, Bc = (sc * s2 - ss * scx) / det;
    double res = 0;
    for (size_t i = 0; i < z.size(); ++i) { const double ph = 2 * M_PI * f0 * (double) i / SR; const double e = z[i] - A * std::sin (ph) - Bc * std::cos (ph); res += e * e; }
    const double db = 10 * std::log10 (res / std::max (1e-30, tot));
    INFO ("what isn't the tone: " << db << " dB below it");
    CHECK (db <= -50.0);
}

TEST_CASE ("Reference match: engine 1 is unchanged (the main app, parity and stored curves depend on it)", "[refmatch]")
{
    // the numbers engine 1 gave on these fixtures before engine 2 existed (10 Nov 2026), defects included
    RefFixture fx ("v1");
    const auto same = fx.match ({ fx.src }, SR, 1);
    REQUIRE (same.error.empty());
    INFO (show (same));
    CHECK (worst (same) == Approx (2.16).margin (0.005));
    auto inv = fx.src;
    for (auto& v : inv) v = -v;
    const auto wide = fx.match ({ fx.src, inv }, SR, 1);
    CHECK (worst (wide, 100, 10000) == Approx (8.0).margin (0.005));
    CHECK (fx.match ({ fx.src }).engine == 2);
    CHECK (fx.match ({ fx.src }).hitsUsed == 4);   // engine 2 measures every hit; engine 1 only the loudest
    CHECK (same.hitsUsed == 1);
}

TEST_CASE ("Rooms in phase on a real take (measurement; set LD_ROOM_TAKE to a take folder)", "[.rooms]")
{
    const auto dir = juce::File (juce::String (std::getenv ("LD_ROOM_TAKE") ? std::getenv ("LD_ROOM_TAKE") : ""));
    if (! dir.isDirectory()) { WARN ("LD_ROOM_TAKE isn't a folder: nothing measured."); return; }
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ dir }).isEmpty());
    prep (m);
    std::vector<std::pair<int, double>> before;
    for (int t = 0; t < (int) m.tracks().size(); ++t)
        if (m.session()->tracks[(size_t) t].inPlace) before.push_back ({ t, settledScore (m, t).first });
    m.setRoomPhase (true);
    pumpUntilIdle (m);
    for (auto& [t, b] : before)
    {
        const auto& tr = m.session()->tracks[(size_t) t];
        const double a = settledScore (m, t).first;
        std::string bands;
        for (auto& ap : tr.ap) bands += " " + std::to_string ((int) ap.fc) + " Hz Q" + juce::String (ap.Q, 2).toStdString();
        WARN (m.tracks()[(size_t) t].name << ": sum with the lead " << juce::String (b * 100, 0) << " -> " << juce::String (a * 100, 0)
              << ", shift " << tr.lag << " samples, polarity " << (tr.pol < 0 ? "inverted" : "normal") << ", phase EQ:" << (bands.empty() ? " none" : bands));
        CHECK (tr.lag == 0.0);
        CHECK (a >= b - 1e-9);
    }
}

// ---------------------------------------------------------------------------- projects
TEST_CASE ("Project: everything set on a take is saved, recalled, and exports the same bytes", "[project]")
{
    const auto take = makeTake ("proj1");
    const auto pf = take.getChildFile ("proj1.ldproj");
    auto a = freshDir ("proj1_a"), b = freshDir ("proj1_b");
    double lag1 = 0; int pol2 = 0; int na = 0;
    {
        SampleModel m (ldtest::engine());
        REQUIRE (m.open ({ take }).isEmpty());
        CHECK_FALSE (m.modified());
        prep (m);
        REQUIRE (m.isPrepared());
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.5; S.dnDb = 12; }, SampleModel::Redo::mix));
        REQUIRE (m.change ([] (ld::hl::Session& S) { ld::hl::nudgeLag (S, 1, 6); ld::hl::flipMic (S, 2); ld::hl::setAllPass (S, 1, 0, 240, 1.1); }, SampleModel::Redo::align));
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.tuneAnyway = true; ld::hl::runPitch (S); S.tune.on = true; S.tune.cents = -35; }, SampleModel::Redo::tune));
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.lev.curve = "even"; S.exp.bits = 24; }, SampleModel::Redo::levels));
        REQUIRE (m.renameTrack (2, "Overs").isEmpty());
        m.setMonDb (1, -6.5f); m.setPan (2, 0.4f); m.setMute (1, true); m.setLevelMatch (false);
        CHECK (m.modified());
        pumpUntilIdle (m);
        REQUIRE (m.exportTo (a, na).isEmpty());
        REQUIRE (m.saveProject (pf).isEmpty());
        CHECK_FALSE (m.modified());
        CHECK (m.projectFile() == pf);
        lag1 = m.session()->tracks[1].lag; pol2 = m.session()->tracks[2].pol;
    }
    CHECK (SampleModel::projectFor ({ take }) == pf);
    SampleModel m (ldtest::engine());
    REQUIRE (m.openProject (pf).isEmpty());
    REQUIRE (m.recallNeedsPrep());
    CHECK (m.exportBlocker().isNotEmpty());   // not until the recall has finished
    CHECK (m.tracks()[2].name == "Overs");
    prep (m);
    REQUIRE (m.isPrepared());
    CHECK_FALSE (m.recallNeedsPrep());
    CHECK (m.recallNote().isEmpty());
    CHECK_FALSE (m.modified());
    const auto& S = *m.session();
    CHECK (S.tone.bright == Approx (0.5)); CHECK (S.dnDb == Approx (12));
    CHECK (S.tracks[1].lag == lag1); CHECK (S.tracks[2].pol == pol2);   // to the last bit
    REQUIRE (S.tracks[1].ap.size() == 1); CHECK (S.tracks[1].ap[0].fc == Approx (240)); CHECK (S.tracks[1].manualAp);
    CHECK (S.tune.on); CHECK (S.tune.cents == Approx (-35));
    CHECK (S.lev.curve == "even"); CHECK (S.exp.bits == 24);
    CHECK (m.monDb (1) == Approx (-6.5f).margin (0.05)); CHECK (m.pan (2) == Approx (0.4f).margin (0.01)); CHECK (m.mute (1)); CHECK_FALSE (m.levelMatch());
    int nb = 0;
    REQUIRE (m.exportTo (b, nb).isEmpty());
    const auto A = exportedBytes (a), B = exportedBytes (b);
    REQUIRE (na == nb);
    REQUIRE (A.size() == B.size());
    int differ = 0; bool renamed = false;
    for (auto& [k, v] : A)
    {
        if (! B.count (k) || B.at (k) != v) { ++differ; WARN (k << " differs" << (B.count (k) ? " by " + std::to_string (diffDb (readWav (a.getChildFile (k)), readWav (b.getChildFile (k)))) + " dB" : std::string (": missing"))); }
        if (k.containsIgnoreCase ("Overs")) renamed = true;
    }
    CHECK (differ == 0);
    CHECK (renamed);   // the name you gave is the name on the files
}

TEST_CASE ("Project: the saved alignment is what comes back, whatever the measurement would say today", "[project]")
{
    const auto take = makeTake ("proj2");
    const auto pf = take.getChildFile ("proj2.ldproj");
    {
        SampleModel m (ldtest::engine());
        REQUIRE (m.open ({ take }).isEmpty());
        prep (m);
        REQUIRE (m.saveProject (pf).isEmpty());
    }
    // as if an older version had measured this mic a little differently
    auto p = juce::JSON::parse (pf);
    auto* tr = p["tracks"].getArray();
    REQUIRE (tr != nullptr);
    const double saved = (double) (*tr)[1]["lag"] + 3.25;
    (*tr)[1].getDynamicObject()->setProperty ("lag", saved);
    REQUIRE (pf.replaceWithText (juce::JSON::toString (p)));
    SampleModel m (ldtest::engine());
    REQUIRE (m.openProject (pf).isEmpty());
    prep (m);
    CHECK (m.session()->tracks[1].lag == Approx (saved));
    CHECK_FALSE (m.session()->tracks[1].manualLag);   // and it isn't turned into a hand setting
    CHECK (m.shiftMs (1) == Approx ((float) (saved / m.sampleRate() * 1000.0)).margin (1e-3));
}

TEST_CASE ("Project: a project that can't be recalled says why and changes nothing", "[project]")
{
    const auto take = makeTake ("proj3");
    const auto pf = take.getChildFile ("proj3.ldproj");
    {
        SampleModel m (ldtest::engine());
        REQUIRE (m.open ({ take }).isEmpty());
        prep (m);
        REQUIRE (m.saveProject (pf).isEmpty());
        // nowhere to write: the old project is still whole
        const auto before = pf.loadFileAsString();
        CHECK (m.saveProject (juce::File ("/proc/nope/x.ldproj")).isNotEmpty());
        CHECK (pf.loadFileAsString() == before);
        CHECK (m.projectFile() == pf);
    }
    SampleModel m (ldtest::engine());
    auto p = juce::JSON::parse (pf);
    p.getDynamicObject()->setProperty ("limitlessDrumsProject", 99);
    const auto newer = take.getChildFile ("newer.ldproj");
    REQUIRE (newer.replaceWithText (juce::JSON::toString (p)));
    CHECK (m.openProject (newer).containsIgnoreCase ("newer version"));
    CHECK_FALSE (m.isOpen());
    const auto junk = take.getChildFile ("junk.ldproj");
    REQUIRE (junk.replaceWithText ("not a project"));
    CHECK (m.openProject (junk).isNotEmpty());
    CHECK_FALSE (m.isOpen());
    // the project moved with its folder: found beside it; a recording gone: named
    const auto moved = take.getParentDirectory().getChildFile ("ld_model_proj3_moved");
    moved.deleteRecursively();
    REQUIRE (take.copyDirectoryTo (moved));
    take.deleteRecursively();
    REQUIRE (m.openProject (moved.getChildFile ("proj3.ldproj")).isEmpty());
    CHECK (m.isOpen());
    const auto gone = moved.findChildFiles (juce::File::findFiles, false, "*BOTTOM*.wav");
    REQUIRE (gone.size() == 1);
    gone[0].deleteFile();
    SampleModel m2 (ldtest::engine());
    CHECK (m2.openProject (moved.getChildFile ("proj3.ldproj")).contains ("BOTTOM"));
    CHECK_FALSE (m2.isOpen());
}

TEST_CASE ("Track names: a new name shows, writes the files, undoes, and can't collide", "[project]")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("rename") }).isEmpty());
    prep (m);
    const auto was = m.tracks()[1].name;
    CHECK (m.renameTrack (1, "  Snare Under ").isEmpty());
    CHECK (m.tracks()[1].name == "Snare Under");
    CHECK (m.renameTrack (2, "snare under").isNotEmpty());   // their files would overwrite each other
    CHECK (m.renameTrack (2, m.tracks()[0].name).isNotEmpty());
    CHECK (juce::String (m.session()->tracks[1].mic).equalsIgnoreCase ("bottom"));           // how it's handled still goes by the recording
    int n = 0; auto d = freshDir ("rename");
    REQUIRE (m.exportTo (d, n).isEmpty());
    CHECK (d.findChildFiles (juce::File::findFiles, true, "Snare_Under.wav").size() > 0);
    CHECK (d.findChildFiles (juce::File::findFiles, true, "*BOTTOM*").isEmpty());
    m.undo();
    CHECK (m.tracks()[1].name == was);
    CHECK (m.renameTrack (1, "").isEmpty());
    CHECK (m.tracks()[1].name == was);
}

TEST_CASE ("Export cancel: stopping part-way writes nothing, puts the monitor mix back, and the next export is whole", "[project]")
{
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ makeTake ("cancel") }).isEmpty());
    prep (m);
    m.setMonDb (1, -9.0f); m.setMute (2, true); m.setSolo (0, true);
    auto whole = freshDir ("cancel_whole"), dest = freshDir ("cancel_dest");
    int n0 = 0;
    REQUIRE (m.exportTo (whole, n0).isEmpty());
    // something already in the folder stays exactly as it was
    const auto mine = dest.getChildFile ("keep-me.txt");
    REQUIRE (mine.replaceWithText ("mine"));
    for (int stopAt : { 0, 1, 2, 1000 })   // before anything, after one render, after two, and at the very last moment
    {
        int asked = 0, n = -1;
        const auto err = m.exportTo (dest, n, {}, [&] { return ++asked > stopAt || stopAt == 1000 ? (stopAt != 1000 || asked > 3) : false; });
        INFO ("stop after " << stopAt << " renders");
        CHECK (err == SampleModel::exportCancelled());
        CHECK (n == 0);
        CHECK (dest.findChildFiles (juce::File::findFilesAndDirectories, true).size() == 1);
        CHECK (mine.loadFileAsString() == "mine");
        CHECK (m.monDb (1) == Approx (-9.0f).margin (0.05)); CHECK (m.mute (2)); CHECK (m.solo (0)); CHECK (m.processed());
        CHECK (m.exportBlocker().isEmpty());
    }
    int n1 = 0; auto again = freshDir ("cancel_again");
    REQUIRE (m.exportTo (again, n1, {}, [] { return false; }).isEmpty());
    const auto A = exportedBytes (whole), B = exportedBytes (again);
    REQUIRE (n1 == n0);
    int differ = 0;
    for (auto& [k, v] : A) if (! B.count (k) || B.at (k) != v) ++differ;
    CHECK (differ == 0);
}

// ---------------------------------------------------------------------------- reference match: your side, and the app around it
namespace
{
/** One mic, `hits` snare-ish hits a second apart at one velocity; `shape (h, x)` may change hit h's samples. */
juce::File makeHitsTake (const juce::String& name, int hits, const std::function<void (int, std::vector<float>&)>& shape = {}, int mics = 1, int secondDelay = 0, float secondGain = 1.0f)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_model_" + name);
    dir.deleteRecursively(); dir.createDirectory();
    const int n = (int) ((hits + 1.0) * SR);
    std::vector<float> lead ((size_t) n, 0.0f);
    juce::Random rnd (11);
    for (int h = 0; h < hits; ++h)
    {
        std::vector<float> one ((size_t) (0.4 * SR));
        for (size_t k = 0; k < one.size(); ++k)
        {
            const double t = (double) k / SR;
            one[k] = (float) (0.5 * std::exp (-t * 16.0) * (std::sin (2 * M_PI * 190 * t) + 0.6 * (rnd.nextFloat() * 2 - 1)));
        }
        if (shape) shape (h, one);
        const int at = (int) ((0.5 + h) * SR);
        for (size_t k = 0; k < one.size(); ++k) lead[(size_t) at + k] = one[k];
    }
    for (auto& v : lead) v += (float) (1e-5 * (rnd.nextFloat() * 2 - 1));
    const juce::String tail = "_V01_RR" + juce::String (hits).paddedLeft ('0', 2) + ".wav";
    writeMono (dir.getChildFile ("SNARE_CENTER_TOP" + tail), lead);
    if (mics > 1)
    {
        std::vector<float> x ((size_t) n, 0.0f);
        for (int i = secondDelay; i < n; ++i) x[(size_t) i] = secondGain * lead[(size_t) (i - secondDelay)];
        writeMono (dir.getChildFile ("SNARE_CENTER_BOTTOM" + tail), x);
    }
    return dir;
}

ld::hl::Reference matchAgainst (SampleModel& m, const juce::File& dir, const std::vector<float>& ref)
{
    const auto f = dir.getSiblingFile (dir.getFileName() + "_reference.wav");
    writeWav (f, { ref }, SR);
    return ld::hl::loadReference (*m.session(), *m.mixRender(), f, ld::hl::kReferenceEngine);
}
/** The lead's whole recording (every hit: the fixture's hits each carry their own noise, so one hit alone is not the take). */
std::vector<float> leadOf (const juce::File& dir)
{
    const auto b = readWav (dir.findChildFiles (juce::File::findFiles, false, "*TOP*.wav")[0]);
    return std::vector<float> (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
}
double curveGap (const ld::hl::Reference& a, const ld::hl::Reference& b)
{
    double d = 0;
    for (size_t j = 0; j < std::min (a.curve.size(), b.curve.size()); ++j) d = std::max (d, std::abs (a.curve[j] - b.curve[j]));
    return d;
}
} // namespace

TEST_CASE ("Reference match RM02: one odd round robin on your side doesn't move the match", "[refmatch]")
{
    const auto dull = juce::IIRCoefficients::makeLowPass (SR, 700.0);
    auto clean = makeHitsTake ("rm02_clean", 5);
    auto odd = makeHitsTake ("rm02_odd", 5, [&] (int h, std::vector<float>& x) { if (h == 2) x = RefFixture::filtered (x, dull, 2); });
    const auto ref = RefFixture::filtered (leadOf (clean), juce::IIRCoefficients::makeHighShelf (SR, 3000.0, 0.707, juce::Decibels::decibelsToGain (5.0)));
    ld::hl::Reference a, b;
    for (auto* pair : { &a, &b })
    {
        SampleModel m (ldtest::engine());
        const auto dir = pair == &a ? clean : odd;
        REQUIRE (m.open ({ dir }).isEmpty());
        prep (m);
        REQUIRE (m.isPrepared());
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.dnOn = false; }, SampleModel::Redo::mix));
        pumpUntilIdle (m);
        *pair = matchAgainst (m, dir, ref);
        REQUIRE (pair->error.empty());
    }
    INFO ("clean: " << show (a)); INFO ("one dull hit: " << show (b));
    WARN ("one dull round robin of five moves the match by at most " << curveGap (a, b) << " dB");
    CHECK (curveGap (a, b) <= 1.0);
    CHECK (worst (a, 6000, 12000) >= 3.0);   // and the match itself is still there
}

TEST_CASE ("Reference match RM02: one odd hit in the reference doesn't move the match", "[refmatch]")
{
    RefFixture fx ("rm02_ref");
    // five hits of the reference, 0.5 s apart; in the second set the middle one is badly dulled
    const auto hit = std::vector<float> (fx.src.begin() + (int) (0.5 * SR) - 64, fx.src.begin() + (int) (0.9 * SR));
    auto build = [&] (bool withOdd)
    {
        std::vector<float> x ((size_t) (3.0 * SR), 0.0f);
        for (int h = 0; h < 5; ++h)
        {
            auto one = withOdd && h == 2 ? RefFixture::filtered (hit, juce::IIRCoefficients::makeLowPass (SR, 600.0), 2) : hit;
            for (size_t k = 0; k < one.size(); ++k) x[(size_t) ((0.1 + 0.5 * h) * SR) + k] = one[k];
        }
        return x;
    };
    const auto a = fx.match ({ build (false) }), b = fx.match ({ build (true) });
    REQUIRE (a.error.empty()); REQUIRE (b.error.empty());
    CHECK (a.hitsUsed == 5); CHECK (b.hitsUsed == 5);
    INFO ("clean: " << show (a)); INFO ("one dull hit: " << show (b));
    CHECK (curveGap (a, b) <= 0.5);
}

TEST_CASE ("Reference match RM05: where your mics cancel in the sum, the hole is left alone, not boosted", "[refmatch]")
{
    // the second mic is the first, 24 samples late and inverted, and deliberately left that way: the sum is a comb
    // (nulls at 0, 2 kHz, 4 kHz ...). The reference is the lead alone, so it has everything the comb took out.
    auto dir = makeHitsTake ("rm05_comb", 4, {}, 2, 24, -1.0f);
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ dir }).isEmpty());
    prep (m);
    REQUIRE (m.isPrepared());
    REQUIRE (m.change ([] (ld::hl::Session& S)
    {
        S.dnOn = false;
        for (auto& tr : S.tracks) { tr.lag = 0; tr.pol = 1; tr.manualLag = tr.manualPol = true; tr.ap.clear(); }
    }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    const auto R = matchAgainst (m, dir, leadOf (dir));
    REQUIRE (R.error.empty());
    INFO (show (R));
    std::string holes; for (double f : R.cancelled) holes += juce::String (f, 0).toStdString() + " ";
    INFO ("left alone: " << holes);
    REQUIRE_FALSE (R.cancelled.empty());
    // 24 samples late and inverted takes out the bottom of the drum (20 dB at 60 Hz, easing to 6 dB by 230 Hz) and puts a
    // null every 2 kHz. Third-octave bands are wider than the comb's teeth higher up, so it's the low end and the first
    // null that read as cancelled.
    const auto& B = ld::hl::bands3();
    int low = 0;
    for (size_t j = 0; j < B.size(); ++j)
        for (double f : R.cancelled) if (f == B[j]) { CHECK (R.curve[j] == 0.0); if (B[j] >= 90 && B[j] <= 210) ++low; }
    CHECK (low >= 3);                   // 100 to 200 Hz, where the lead-only reference would otherwise ask for up to +7 dB
    CHECK (worst (R, 60, 210) == 0.0);
    // lined up and the right way round, nothing cancels and nothing is held back
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tracks[1].lag = 24; S.tracks[1].pol = -1; }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    const auto fixed = matchAgainst (m, dir, leadOf (dir));
    INFO ("aligned: " << show (fixed));
    CHECK (fixed.cancelled.empty());
    CHECK (worst (fixed) <= 0.75);
}

TEST_CASE ("Reference match RM05: a tuned take is matched for where its bands land", "[refmatch]")
{
    // the take is tuned up four semitones (one third-octave band). Its own sound, played back that much faster, is
    // what the export will be: as the reference, it should ask for no change.
    auto dir = makeHitsTake ("rm05_tuned", 4);
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ dir }).isEmpty());
    prep (m);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.dnOn = false; }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    const double ratio = std::pow (2.0, 4.0 / 12.0);
    const auto hit = leadOf (dir);
    std::vector<float> fast ((size_t) ((double) hit.size() / ratio) - 2);
    for (size_t i = 0; i < fast.size(); ++i) { const double p = (double) i * ratio; const size_t k = (size_t) p; fast[i] = (float) (hit[k] + (p - (double) k) * (hit[k + 1] - hit[k])); }
    const auto untuned = matchAgainst (m, dir, fast);
    REQUIRE (m.change ([ratio] (ld::hl::Session& S) { S.tune.on = true; S.tinfo = ld::hl::TuneState {}; S.tinfo->ratios.assign (S.onsets.size(), ratio); }, SampleModel::Redo::none));
    const auto tuned = matchAgainst (m, dir, fast);
    REQUIRE (tuned.error.empty());
    INFO ("as if untuned: " << show (untuned)); INFO ("knowing the tuning: " << show (tuned));
    CHECK (worst (tuned, 100, 8000) <= 1.5);                         // linear interpolation dulls the very top of the test signal
    CHECK (worst (untuned, 100, 8000) > worst (tuned, 100, 8000) + 1.0);   // without it the same reference asks for a real EQ
}

TEST_CASE ("Reference in the app (RM06): load, undo, stale, audition, project and export all agree", "[refmatch][project]")
{
    const auto dir = makeTake ("rm06");
    const auto pf = dir.getChildFile ("rm06.ldproj");
    const auto refFile = dir.getSiblingFile ("ld_model_rm06_reference.wav");
    {
        const auto b = readWav (dir.getChildFile ("SNARE_CENTER_TOP_V01_RR04.wav"));
        std::vector<float> x (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
        writeWav (refFile, { RefFixture::filtered (x, juce::IIRCoefficients::makeHighShelf (SR, 3000.0, 0.707, juce::Decibels::decibelsToGain (6.0))) }, SR);
    }
    auto plain = freshDir ("rm06_plain"), zero = freshDir ("rm06_zero"), matched = freshDir ("rm06_matched"), undone = freshDir ("rm06_undone"), recalled = freshDir ("rm06_recalled");
    std::vector<double> curve; int n = 0;
    {
        SampleModel m (ldtest::engine());
        REQUIRE (m.open ({ dir }).isEmpty());
        CHECK (m.loadReference (refFile).isNotEmpty());   // not before Auto Prep
        prep (m);
        REQUIRE (m.exportTo (plain, n).isEmpty());
        CHECK (m.setReferenceAudition (true).isNotEmpty());   // nothing loaded: nothing to hear
        REQUIRE (m.loadReference (refFile).isEmpty());
        pumpUntilIdle (m);
        CHECK (m.hasReference()); CHECK (m.referenceName() == refFile.getFileName()); CHECK (m.referenceHits() >= 1);
        CHECK_FALSE (m.referenceStale());
        curve = m.session()->tone.refCurve;
        REQUIRE (m.exportTo (matched, n).isEmpty());
        // the amount at zero is the take without the reference, to the byte
        const double amt = m.session()->tone.refAmt, env = m.session()->tone.refEnvAmt;
        REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.refAmt = 0; S.tone.refEnvAmt = 0; }, SampleModel::Redo::mix));
        pumpUntilIdle (m);
        REQUIRE (m.exportTo (zero, n).isEmpty());
        REQUIRE (m.change ([amt, env] (ld::hl::Session& S) { S.tone.refAmt = amt; S.tone.refEnvAmt = env; }, SampleModel::Redo::mix));
        pumpUntilIdle (m);
        // listening to it: neither A nor B plays, nothing about the take changes, and it all comes back
        m.setSolo (1, true); m.setMute (2, true);
        const int editsSaved = m.modified() ? 1 : 0; (void) editsSaved;
        REQUIRE (m.setReferenceAudition (true, 0.5).isEmpty());
        CHECK (m.referenceAudition());
        for (int t = 0; t < (int) m.tracks().size(); ++t) { CHECK (m.trackB (t)->isMuted (false)); CHECK (m.trackA (t)->isMuted (false)); }
        CHECK (m.solo (1)); CHECK (m.mute (2)); CHECK (m.processed());
        m.setProcessed (false);   // A or B takes over
        CHECK_FALSE (m.referenceAudition());
        CHECK (m.trackA (0)->isMuted (false) == false);
        m.setProcessed (true);
        REQUIRE (m.setReferenceAudition (true, 0.5).isEmpty());
        int n2 = 0; auto again = freshDir ("rm06_again");
        REQUIRE (m.exportTo (again, n2).isEmpty());        // export turns it off and writes the take, not the reference
        CHECK_FALSE (m.referenceAudition());
        m.setSolo (1, false); m.setMute (2, false);
        // the take moves after the measurement: the match says so
        REQUIRE (m.change ([] (ld::hl::Session& S) { ld::hl::nudgeLag (S, 1, 8); }, SampleModel::Redo::align));
        CHECK (m.referenceStale());
        m.undo();
        CHECK_FALSE (m.referenceStale());
        pumpUntilIdle (m);
        REQUIRE (m.saveProject (pf).isEmpty());
        const auto A = exportedBytes (matched), B2 = exportedBytes (again);
        int differ = 0; for (auto& [k, v] : A) if (! B2.count (k) || B2.at (k) != v) ++differ;
        CHECK (differ == 0);
        // undo the load itself: back to the take as it was, and nothing to listen to
        while (m.canUndo()) m.undo();
        pumpUntilIdle (m);
        CHECK_FALSE (m.hasReference());
        CHECK (m.setReferenceAudition (true).isNotEmpty());
        REQUIRE (m.exportTo (undone, n).isEmpty());
    }
    {
        const auto P = exportedBytes (plain), Z = exportedBytes (zero), M = exportedBytes (matched), U = exportedBytes (undone);
        int zDiff = 0, mDiff = 0, uDiff = 0;
        for (auto& [k, v] : P) { if (! Z.count (k) || Z.at (k) != v) ++zDiff; if (! M.count (k) || M.at (k) != v) ++mDiff; if (! U.count (k) || U.at (k) != v) ++uDiff; }
        CHECK (zDiff == 0);   // amount 0 = no reference
        CHECK (mDiff > 0);    // the match is in the export
        CHECK (uDiff == 0);   // undone = never loaded
    }
    // recalled: the same curve to the last digit (never measured again), the same bytes, and it can still be heard
    SampleModel m (ldtest::engine());
    REQUIRE (m.openProject (pf).isEmpty());
    prep (m);
    REQUIRE (m.hasReference());
    CHECK (m.session()->tone.refCurve == curve);
    CHECK (m.referenceName() == refFile.getFileName());
    CHECK_FALSE (m.referenceStale());
    REQUIRE (m.exportTo (recalled, n).isEmpty());
    const auto M = exportedBytes (matched), R = exportedBytes (recalled);
    int differ = 0; for (auto& [k, v] : M) if (! R.count (k) || R.at (k) != v) ++differ;
    CHECK (differ == 0);
    CHECK (m.setReferenceAudition (true, 0.5).isEmpty());
    CHECK (m.referenceAudition());
    m.setReferenceAudition (false);
    refFile.deleteFile();   // the file gone: the match still stands, only listening needs it
    SampleModel m3 (ldtest::engine());
    REQUIRE (m3.openProject (pf).isEmpty());
    prep (m3);
    CHECK (m3.hasReference());
    CHECK (m3.setReferenceAudition (true, 0.5).contains ("reference"));
    CHECK_FALSE (m3.referenceAudition());
}

TEST_CASE ("Timing: how long a change takes to be heard on a real take", "[.timing]")
{
    const char* dir = std::getenv ("LD_TAKE");
    REQUIRE (dir != nullptr);
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ juce::File (dir) }).isEmpty());
    prep (m);
    auto timed = [&] (const char* what, std::function<void (ld::hl::Session&)> f, SampleModel::Redo r)
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        REQUIRE (m.change (f, r));
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        pumpUntilIdle (m);
        fprintf (stderr, "[timing] %s: change() blocked %.0f ms, heard after %.0f ms\n", what, t1 - t0, juce::Time::getMillisecondCounterHiRes() - t0);
    };
    timed ("tone.bright", [] (ld::hl::Session& S) { S.tone.bright = 0.5; }, SampleModel::Redo::mix);
    timed ("tone.bright again", [] (ld::hl::Session& S) { S.tone.bright = 0.6; }, SampleModel::Redo::mix);
    timed ("levels only", [] (ld::hl::Session&) {}, SampleModel::Redo::levels);
    timed ("tune", [] (ld::hl::Session&) {}, SampleModel::Redo::tune);
    timed ("align", [] (ld::hl::Session&) {}, SampleModel::Redo::align);
}

TEST_CASE ("Preview: one hit worked out on its own is the whole render's samples", "[preview]")
{
    const char* dir = std::getenv ("LD_TAKE");
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ dir ? juce::File (dir) : makeTake ("preview1", 3) }).isEmpty());
    prep (m);
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.7; S.tone.crack = 0.5; S.shape.atk = 0.6; S.shape.sus = -0.4; }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    const auto& S = *m.session();
    const auto& M = *m.mixRender();
    const auto& R = m.engineState()->levels->regions;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    ld::hl::HitPreviewer P (S, M);
    REQUIRE (P.valid());
    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    double worst = 0, ms = 0; long differ = 0, n = 0;
    for (size_t i = 0; i < R.size(); ++i)
    {
        const auto h0 = juce::Time::getMillisecondCounterHiRes();
        for (int t = 0; t < (int) S.tracks.size(); ++t)
        {
            const auto& whole = M.bus[(size_t) t].empty() ? M.proc[(size_t) t] : M.bus[(size_t) t];
            const auto got = P.render (t, R[i].start, R[i].end);
            REQUIRE (got.size() == whole.size());
            for (size_t c = 0; c < got.size(); ++c)
                for (long q = R[i].start; q < R[i].end; ++q, ++n)
                {
                    const double d = std::abs ((double) got[c][(size_t) (q - R[i].start)] - (double) whole[c][(size_t) q]);
                    if (d != 0) ++differ;
                    worst = std::max (worst, d);
                }
        }
        ms = std::max (ms, juce::Time::getMillisecondCounterHiRes() - h0);
    }
    fprintf (stderr, "[preview] %zu hits, %d mics: set-up %.0f ms, slowest hit %.0f ms, %ld of %ld samples differ, worst %.3g\n", R.size(), (int) S.tracks.size(), t1 - t0, ms, differ, n, worst);
    CHECK (differ == 0);
    // taming is linked across hits: no per-hit preview for it
    REQUIRE (m.change ([] (ld::hl::Session& S2) { S2.tone.tame = 0.5; }, SampleModel::Redo::mix));
    pumpUntilIdle (m);
    CHECK_FALSE (ld::hl::HitPreviewer (*m.session(), *m.mixRender()).valid());
}

namespace
{
void pumpUntilPreviewed (SampleModel& m)
{
    const auto t0 = juce::Time::getMillisecondCounter();
    do juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    while ((m.isPrepping() || m.isBusy() || m.previewRunning()) && juce::Time::getMillisecondCounter() - t0 < 120000);
}
double nullDb (const ld::hl::Chs& a, const ld::hl::Chs& b)
{
    double e = 0, r = 0;
    for (size_t c = 0; c < a.size() && c < b.size(); ++c)
        for (size_t n = 0; n < std::min (a[c].size(), b[c].size()); ++n) { const double d = (double) a[c][n] - b[c][n]; e += d * d; r += (double) b[c][n] * b[c][n]; }
    return 10.0 * std::log10 ((e + 1e-30) / (r + 1e-30));
}
}

TEST_CASE ("Preview: hits swapped in from memory play what the rendered files play, for tone and for tuning", "[preview]")
{
    const char* dir = std::getenv ("LD_TAKE");
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ dir ? juce::File (dir) : makeTake ("preview2", 3) }).isEmpty());
    prep (m);
    const double sr = m.session()->sr;
    m.previewRateForTests = sr;
    const int nT = (int) m.session()->tracks.size();
    const size_t nH = m.session()->onsets.size();
    auto all = [&] { std::vector<ld::hl::Chs> v; for (int t = 0; t < nT; ++t) v.push_back (m.renderTrackForTests (t, sr)); return v; };
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.2; }, SampleModel::Redo::mix));
    pumpUntilPreviewed (m);
    CHECK (m.hitsPreviewed() == 0);          // nothing playing: the files took over
    const auto filesA = all();

    for (int pass = 0; pass < 2; ++pass)
    {
        // as if playing: the change is heard from memory, the files on the tracks stay as they were
        m.holdClipsForTests = true;
        if (pass == 0) REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.9; S.tone.crack = 0.6; S.shape.atk = 0.5; }, SampleModel::Redo::mix));
        else REQUIRE (m.change ([] (ld::hl::Session& S) { S.tuneAnyway = true; ld::hl::runPitch (S); S.tune.on = true; S.tune.cents = -35; }, SampleModel::Redo::tune));
        pumpUntilPreviewed (m);
        CHECK (m.hitsPreviewed() == (int) nH);
        CHECK (m.clipsWaiting());
        const auto heard = all();
        // stopped: the files are laid on the tracks and the memory is let go
        m.holdClipsForTests = false;
        m.stop();
        CHECK (m.hitsPreviewed() == 0);
        CHECK_FALSE (m.clipsWaiting());
        const auto files = all();
        for (int t = 0; t < nT; ++t)
        {
            INFO ("pass " << pass << ", mic " << t << ": heard vs files " << nullDb (heard[(size_t) t], files[(size_t) t]) << " dB; files vs the settings before " << nullDb (files[(size_t) t], filesA[(size_t) t]) << " dB");
            fprintf (stderr, "[preview] pass %d mic %d: heard vs files %.1f dB, files vs before %.1f dB\n", pass, t, nullDb (heard[(size_t) t], files[(size_t) t]), nullDb (files[(size_t) t], filesA[(size_t) t]));
            CHECK (nullDb (heard[(size_t) t], files[(size_t) t]) < -100.0);
        }
        if (pass == 0) { double moved = -300; for (int t = 0; t < nT; ++t) moved = std::max (moved, nullDb (files[(size_t) t], filesA[(size_t) t])); CHECK (moved > -60.0); }
    }
}

TEST_CASE ("Phrase: hits played off the timeline sound as they do in the take, on every mic", "[preview][phrase]")
{
    const char* dir = std::getenv ("LD_TAKE");
    SampleModel m (ldtest::engine());
    REQUIRE (m.open ({ dir ? juce::File (dir) : makeTake ("phrase1", 3) }).isEmpty());
    prep (m);
    const double sr = m.session()->sr;
    m.previewRateForTests = sr;
    REQUIRE (m.change ([] (ld::hl::Session& S) { S.tone.bright = 0.6; }, SampleModel::Redo::mix));
    pumpUntilPreviewed (m);
    const int nT = (int) m.session()->tracks.size();
    const auto& R = m.engineState()->levels->regions;
    REQUIRE (R.size() >= 2);
    const int hA = (int) R.size() - 1, hB = 0;
    m.playPhrase ({ { 0.0, hA }, { 1.5, hB } }, "test", false);
    pumpUntilPreviewed (m);
    REQUIRE (m.phraseOn());
    REQUIRE (m.hitsPreviewed() == (int) R.size());
    m.getEdit()->getTransport().stop (false, false);   // (the phrase stays laid out: it's rendered offline here)
    const double len = 0.25;
    for (int t = 0; t < nT; ++t)
        for (int k = 0; k < 2; ++k)
        {
            const int h = k == 0 ? hA : hB;
            const double inTake = (double) R[(size_t) h].start / sr, inPhrase = m.phraseStart() + (k == 0 ? 0.0 : 1.5);
            const auto a = m.renderTrackForTests (t, sr, inTake, inTake + len), b = m.renderTrackForTests (t, sr, inPhrase, inPhrase + len);
            REQUIRE_FALSE (a.empty()); REQUIRE_FALSE (b.empty());
            // the same audio, give or take where a sample boundary falls: best match within two samples
            double best = -1, ea = 0, eb = 0;
            for (float v : a[0]) ea += (double) v * v;
            for (float v : b[0]) eb += (double) v * v;
            for (int lag = -2; lag <= 2; ++lag)
            {
                double xy = 0;
                for (size_t n = 8; n + 8 < std::min (a[0].size(), b[0].size()); ++n) xy += (double) a[0][n] * b[0][(size_t) ((long) n + lag)];
                best = std::max (best, xy / std::sqrt (ea * eb + 1e-30));
            }
            INFO ("mic " << t << " event " << k << ": match " << best << ", level " << 10 * std::log10 ((eb + 1e-30) / (ea + 1e-30)) << " dB");
            CHECK (ea > 1e-9);
            CHECK (best > 0.98);
            CHECK (std::abs (10 * std::log10 ((eb + 1e-30) / (ea + 1e-30))) < 0.5);
        }
    m.stop();
    CHECK_FALSE (m.phraseOn());
}

TEST_CASE ("Audition phrases: layer, sweep, groove and rudiments pick the right hits", "[phrase]")
{
    struct H { double time, end; int vel, rr; };
    std::vector<H> hits;
    for (int v = 0; v < 4; ++v) for (int r = 0; r < 3; ++r) hits.push_back ({ hits.size() * 2.0, hits.size() * 2.0 + 2, v, r });
    using namespace ld::samples;
    const auto layer = layerPhrase (hits, 7);                 // hit 7: velocity 2, round robin 1
    REQUIRE (layer.size() == 3);
    for (auto& n : layer) CHECK (hits[(size_t) n.hit].vel == 2);
    const auto sweep = sweepPhrase (hits, 7);
    REQUIRE (sweep.size() == 4);
    for (size_t k = 0; k < sweep.size(); ++k) { CHECK (hits[(size_t) sweep[k].hit].vel == (int) k); CHECK (hits[(size_t) sweep[k].hit].rr == 1); CHECK (sweep[k].at == Catch::Approx (0.5 * k)); }
    std::mt19937 rng (7);
    const auto groove = groovePhrase (hits, 90.0, rng);
    REQUIRE (groove.size() == 8 + hits.size());              // two bars of quarters on the top layer, then every hit once
    for (size_t k = 0; k < 8; ++k) CHECK (hits[(size_t) groove[k].hit].vel == 3);
    std::vector<int> seen (hits.size(), 0);
    for (size_t k = 8; k < groove.size(); ++k) { ++seen[(size_t) groove[k].hit]; if (k > 8) CHECK (hits[(size_t) groove[k].hit].vel >= hits[(size_t) groove[k - 1].hit].vel); }
    for (int n : seen) CHECK (n == 1);
    for (size_t k = 1; k < 8; ++k) CHECK (groove[k].hit != groove[k - 1].hit);   // never the same round robin twice running
    const auto rudi = rudimentPhrase (hits, 100.0, rng);
    CHECK (rudi.size() > 70);
    for (size_t k = 1; k < rudi.size(); ++k) CHECK (rudi[k].at >= rudi[k - 1].at);
}

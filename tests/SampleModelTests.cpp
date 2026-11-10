// The sample build's model under the scenarios the 9 Nov audit reproduced (SA01-SA03, SA07, SA08, SA12, the
// overlapping-export case) and the review that followed (CR01, CR03), plus the Hits screen's target and range logic (SA09, SA11). A small synthetic take (three mics, four hits) so it runs anywhere, CI included.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestSignals.h"
#include "samples/SampleModel.h"
#include "core/Session.h"
#include "hl/Session.h"
#include "hl/Align.h"
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

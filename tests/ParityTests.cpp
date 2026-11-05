// Parity with Hitline 4.8: the same session through Hitline (tools/parity, answer keys in
// tests/parity) and through the C++ port must give the same results.
// Needs the session audio: set LD_PARITY_SESSIONS to the folder that holds the session folders
// named in tests/parity/sessions.json. Skipped (with a message) when it isn't set.
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "hl/Detect.h"
#include "hl/Wav.h"
#include "hl/Align.h"
#include "hl/Resolve.h"
#include "hl/Tune.h"
#include "hl/Levels.h"
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;

namespace
{
juce::File parityDir() { return juce::File (LD_SOURCE_DIR).getChildFile ("tests/parity"); }

struct Case { juce::var key; juce::File folder; juce::String name; };

std::vector<Case> cases()
{
    std::vector<Case> out;
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) return out;
    const auto index = juce::JSON::parse (parityDir().getChildFile ("sessions.json"));
    for (auto& e : *index.getArray())
    {
        Case c;
        c.name = e["key"].toString();
        c.key = juce::JSON::parse (parityDir().getChildFile (c.name))["state"];
        c.folder = juce::File (root).getChildFile (e["folder"].toString());
        if (c.folder.isDirectory()) out.push_back (c);
    }
    return out;
}

ld::hl::WavData trackWav (const Case& c, int index)
{
    const auto name = c.key["tracks"][index]["name"].toString() + ".wav";
    return ld::hl::readWav (c.folder.getChildFile (name));
}
} // namespace

TEST_CASE ("Parity: hit detection matches Hitline 4.8")
{
    auto all = cases();
    if (all.empty()) { WARN ("LD_PARITY_SESSIONS not set or no sessions found; parity skipped"); return; }

    for (auto& c : all)
    {
        INFO (c.name);
        const auto det = c.key["det"];
        const int ref = c.key["refIdx"];
        const auto wav = trackWav (c, ref);
        REQUIRE (wav.error.empty());
        CHECK ((double) c.key["sr"] == wav.sampleRate);

        ld::hl::DetectOptions o;
        o.expected = (int) det["V"] * (int) det["RR"];
        o.minGapMs = det["minGapMs"];
        const auto r = ld::hl::detectHits (ld::hl::monoOf (wav.channels), wav.sampleRate, o);

        const auto* keyOn = c.key["onsets"].getArray();
        REQUIRE ((int) r.onsets.size() == keyOn->size());
        for (int i = 0; i < keyOn->size(); ++i) { INFO ("hit " << i); CHECK (r.onsets[(size_t) i] == (int) (*keyOn)[i]); }
        CHECK (ld::hl::jsRound (r.thrDb * 2) / 2 == (double) det["thrDb"]);
    }
}

TEST_CASE ("Velocity and round-robin counts are read from file names like Hitline")
{
    auto c = ld::hl::guessCounts ({ "SNARE_CENTER_TOP_RR06_V06" });
    REQUIRE (c);
    CHECK (c->first == 6);
    CHECK (c->second == 6);
    c = ld::hl::guessCounts ({ "SNARE_CENTER_TOP_V04_RR08" });
    REQUIRE (c);
    CHECK (c->first == 4);
    CHECK (c->second == 8);
    CHECK_FALSE (ld::hl::guessCounts ({ "Kick In" }));
}

TEST_CASE ("Parity: alignment matches Hitline 4.8")
{
    auto all = cases();
    if (all.empty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }

    for (auto& c : all)
    {
        const int ref = c.key["refIdx"];
        const auto refWav = trackWav (c, ref);
        const auto refMono = ld::hl::monoOf (refWav.channels);
        std::vector<int> onsets;
        for (auto& v : *c.key["onsets"].getArray()) onsets.push_back ((int) v);
        const auto* tracks = c.key["tracks"].getArray();
        std::vector<ld::hl::MicAlign> mics ((size_t) tracks->size());
        for (int t = 0; t < tracks->size(); ++t)
        {
            auto& m = mics[(size_t) t];
            m.mic = (*tracks)[t]["mic"].toString().toStdString();
            m.name = (*tracks)[t]["name"].toString().toStdString();
            m.alignTo = ld::hl::defaultAlignTo (m.mic);
            CHECK (m.alignTo == (*tracks)[t]["alignTo"].toString().toStdString());
            if (t == ref) continue;
            const auto kt = (*tracks)[t];
            const auto ka = kt["auto"];
            INFO (c.name << " / " << kt["mic"].toString());
            REQUIRE (ka.isObject());
            const auto wav = trackWav (c, t);
            ld::hl::AlignOptions o;
            o.role = ld::hl::roleOf ((kt["mic"].toString() + " " + kt["name"].toString()).toStdString());
            CHECK (o.role == ka["role"].toString().toStdString());
            const auto r = ld::hl::alignMic (refMono, wav.channels, onsets, refWav.sampleRate, o);
            REQUIRE (r.valid);
            CHECK_THAT (r.lag, WithinAbs ((double) ka["lag"], 1e-9));
            CHECK (r.pol == (int) ka["pol"]);
            CHECK (r.kept == (bool) ka["kept"]);
            CHECK_THAT (r.asis, WithinAbs ((double) ka["asis"], 1e-9));
            CHECK_THAT (r.score, WithinAbs ((double) ka["score"], 1e-9));
            const auto* kap = ka["ap"].getArray();
            REQUIRE ((int) r.ap.size() == kap->size());
            for (int i = 0; i < kap->size(); ++i)
            {
                CHECK (r.ap[(size_t) i].fc == (double) (*kap)[i]["fc"]);
                CHECK (r.ap[(size_t) i].Q == (double) (*kap)[i]["Q"]);
            }
            const auto* kb = ka["bandsAfter"].getArray();
            for (int b = 0; b < ld::hl::numBands; ++b) CHECK_THAT (r.bandsAfter[(size_t) b], WithinAbs ((double) (*kb)[b], 1e-9));
            m.autoResult = r;
        }
        // what Hitline applies to each mic after resolving
        const bool flipped = ld::hl::resolveAlignment (mics, ref);
        CHECK (flipped == (bool) c.key["phase"]["flipped"]);
        for (int t = 0; t < tracks->size(); ++t)
        {
            const auto kt = (*tracks)[t];
            INFO (c.name << " / applied / " << kt["mic"].toString());
            CHECK_THAT (mics[(size_t) t].lag, WithinAbs ((double) kt["lag"], 1e-9));
            CHECK (mics[(size_t) t].pol == (int) kt["pol"]);
            CHECK ((int) mics[(size_t) t].ap.size() == kt["ap"].getArray()->size());
        }
    }
}

TEST_CASE ("Parity: pitch reading matches Hitline 4.8")
{
    auto all = cases();
    if (all.empty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    for (auto& c : all)
    {
        INFO (c.name);
        const auto tune = c.key["tune"];
        REQUIRE (tune["mic"].toString() == "ref");
        const int ref = c.key["refIdx"];
        const auto wav = trackWav (c, ref);
        std::vector<long> onsets;
        for (auto& v : *c.key["onsets"].getArray()) onsets.push_back ((long) (int) v);
        ld::hl::PitchOptions o;
        o.skipMs = tune["skipMs"]; o.winMs = tune["winMs"]; o.fmin = tune["fmin"]; o.fmax = tune["fmax"];
        const auto sp = ld::hl::sessionPitch (ld::hl::monoOf (wav.channels), onsets, wav.sampleRate, o);
        CHECK_THAT (sp.est, WithinAbs ((double) c.key["pitchEst"], 1e-9));
        const auto* kp = c.key["pitch"].getArray();
        REQUIRE ((int) sp.hits.size() == kp->size());
        for (int i = 0; i < kp->size(); ++i)
        {
            INFO ("hit " << i);
            const auto k = (*kp)[i];
            REQUIRE (sp.hits[(size_t) i].has_value() == k.isObject());
            if (! k.isObject()) continue;
            CHECK_THAT (sp.hits[(size_t) i]->f, WithinAbs ((double) k["f"], 1e-9));
            CHECK_THAT (sp.hits[(size_t) i]->conf, WithinAbs ((double) k["conf"], 1e-9));
        }
        // computeTune
        const auto det = c.key["det"];
        const int V = det["V"], RR = det["RR"];
        std::vector<int> velOf;
        for (int i = 0; i < kp->size(); ++i) { const int g = i / RR; velOf.push_back (det["order"].toString() == "loud" ? V - 1 - g : g); }
        ld::hl::TuneSettings ts;
        ts.on = tune["on"]; ts.mode = tune["mode"].toString().toStdString(); ts.target = tune["target"].toString().toStdString();
        ts.cents = tune["cents"]; ts.a4 = tune["a4"];
        const auto ti = ld::hl::computeTune (sp, ts, V, velOf);
        const auto kt = c.key["tinfo"];
        REQUIRE (ti.valid == kt.isObject());
        if (! ti.valid) continue;
        CHECK_THAT (ti.medF, WithinAbs ((double) kt["medF"], 1e-9));
        CHECK (ti.nearest == (int) kt["nearest"]);
        CHECK_THAT (ti.targetF, WithinAbs ((double) kt["targetF"], 1e-9));
        for (int v = 0; v < V; ++v)
        {
            const auto km = kt["layerMed"][v];
            if (km.isVoid() || ! std::isfinite ((double) km)) continue;
            CHECK_THAT (ti.layerMed[(size_t) v], WithinAbs ((double) km, 1e-9));
        }
        INFO ("Hitline reads this drum at " << ti.medF << " Hz, nearest " << ld::hl::noteName (ti.nearest));
        CHECK (true);
    }
}

TEST_CASE ("Parity: tuned renders match Hitline 4.8's resampler")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    const auto meta = juce::JSON::parse (parityDir().getChildFile ("tuned.json"));
    const auto wav = ld::hl::readWav (juce::File (root).getChildFile ("SNARE 2 - BB/Center/SNARE_CENTER_TOP_RR06_V06.wav"));
    REQUIRE (wav.error.empty());
    for (auto& cs : *meta["cases"].getArray())
    {
        INFO (cs["name"].toString());
        juce::MemoryBlock mb;
        REQUIRE (parityDir().getChildFile (cs["name"].toString() + ".f32").loadFileAsData (mb));
        const auto* golden = static_cast<const float*> (mb.getData());
        const auto out = ld::hl::renderTuned (wav.channels[0], (double) meta["onset"], (long) (int) meta["pre"], (long) (int) meta["outLen"],
                                              (double) meta["lag"], (int) meta["pol"], (double) meta["g"], (double) cs["step"],
                                              (long) (int) meta["lo"], (long) (int) meta["hi"], (long) (int) meta["fadeN"], 32);
        REQUIRE (out.size() * 4 == mb.getSize());
        double worst = 0;
        for (size_t i = 0; i < out.size(); ++i) worst = std::max (worst, (double) std::abs (out[i] - golden[i]));
        INFO ("largest difference " << worst);
        CHECK (worst < 1e-6);   // float rounding only (-120 dB)
    }
}

TEST_CASE ("Parity: levels match Hitline 4.8")
{
    auto all = cases();
    if (all.empty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    for (auto& c : all)
    {
        INFO (c.name);
        const auto& k = c.key;
        const int ref = k["refIdx"];
        const auto* tracks = k["tracks"].getArray();
        std::vector<ld::hl::LevelTrack> lt;
        double sr = 0;
        for (int t = 0; t < tracks->size(); ++t)
        {
            const auto kt = (*tracks)[t];
            auto wav = trackWav (c, t);
            sr = wav.sampleRate;
            ld::hl::LevelTrack tr;
            tr.mic = kt["mic"].toString().toStdString(); tr.name = kt["name"].toString().toStdString();
            tr.mono = ld::hl::monoOf (wav.channels);
            tr.ch = std::move (wav.channels);
            tr.lagInt = (long) ld::hl::jsRound ((double) kt["lag"]);
            tr.pol = kt["pol"];
            lt.push_back (std::move (tr));
        }
        std::vector<long> onsets;
        for (auto& v : *k["onsets"].getArray()) onsets.push_back ((long) (int) v);
        ld::hl::Det det; det.V = k["det"]["V"]; det.RR = k["det"]["RR"]; det.order = k["det"]["order"].toString().toStdString(); det.prerollMs = k["det"]["prerollMs"];
        const auto kl = k["lev"];
        ld::hl::LevelSettings lev;
        lev.mode = kl["mode"].toString().toStdString(); lev.source = kl["source"].toString().toStdString(); lev.winMs = kl["winMs"];
        lev.curve = kl["curve"].toString().toStdString(); lev.shape = kl["shape"]; lev.midiRange = kl["midiRange"].toString().toStdString();
        lev.stepDb = kl["stepDb"]; lev.rangeDb = kl["rangeDb"]; lev.anchor = kl["anchor"].toString().toStdString(); lev.offsetDb = kl["offsetDb"];
        lev.std = kl["std"].toString().toStdString(); lev.steps = kl["steps"].toString().toStdString();
        ld::hl::ExportLevelSettings ex; ex.maxLenS = k["exp"]["maxLenS"]; ex.trim = k["exp"]["trim"].toString().toStdString(); ex.ceilDb = k["exp"]["ceilDb"];
        ld::hl::F64 ratios;
        if (k["tinfo"].isObject()) for (auto& r : *k["tinfo"]["ratios"].getArray()) ratios.push_back ((double) r);

        const auto R = ld::hl::computeLevels (lt, ref, onsets, sr, det, lev, ex, k["drum"].toString().toStdString(), ratios);
        REQUIRE (R.valid);
        const auto kr = k["res"];
        for (int t = 0; t < tracks->size(); ++t)
        {
            INFO ("mic " << (*tracks)[t]["mic"].toString());
            for (int i = 0; i < (int) onsets.size(); ++i)
            {
                CHECK_THAT (R.lvlRaw[(size_t) t][(size_t) i], WithinAbs ((double) kr["lvlRaw"][t][i], 1e-9));
                CHECK_THAT (R.gainsDbT[(size_t) t][(size_t) i], WithinAbs ((double) kr["gainsDbT"][t][i], 1e-9));
            }
            for (int v = 0; v < det.V; ++v) CHECK_THAT (R.badge[(size_t) t][(size_t) v], WithinAbs ((double) kr["badge"][t][v], 1e-9));
            CHECK_THAT (R.peakAfter[(size_t) t], WithinAbs ((double) kr["peakAfter"][t], 1e-9));
            CHECK_THAT (R.spreadAfter[(size_t) t], WithinAbs ((double) kr["spreadAfter"][t], 1e-9));
        }
        for (int v = 0; v < det.V; ++v) CHECK_THAT (R.targetsOut[(size_t) v], WithinAbs ((double) kr["targetsOut"][v], 1e-9));
        CHECK_THAT (R.trimDb, WithinAbs ((double) kr["trimDb"], 1e-12));
    }
}

#include "hl/FxInserts.h"
#include "hl/FxSat.h"
TEST_CASE ("Parity: inserts match Hitline 4.8")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    const auto meta = juce::JSON::parse (parityDir().getChildFile ("fx.json"));
    juce::MemoryBlock mb;
    REQUIRE (parityDir().getChildFile ("fx.f32").loadFileAsData (mb));
    const auto* golden = static_cast<const float*> (mb.getData());
    const auto dir = juce::File (root).getChildFile ("SNARE 2 - BB/Center");
    const auto top = ld::hl::readWav (dir.getChildFile ("SNARE_CENTER_TOP_RR06_V06.wav")), oh = ld::hl::readWav (dir.getChildFile ("SNARE_CENTER_OH_RR06_V06.wav"));
    const long start = (int) meta["start"], len = (int) meta["len"];
    auto cut = [&] (const ld::hl::WavData& w) { ld::hl::Chs c; for (auto& x : w.channels) c.push_back (ld::hl::F32 (x.begin() + start, x.begin() + start + len)); return c; };
    const auto T = cut (top), O = cut (oh);
    const double sr = meta["sr"];
    ld::hl::setReverbLibrary (juce::JSON::parse (juce::File (LD_SOURCE_DIR).getChildFile ("assets/irlib.json")));
    for (auto& cs : *meta["cases"].getArray())
    {
        const auto name = cs["name"].toString(), fn = cs["fn"].toString();
        INFO (name);
        const auto& in = cs["input"].toString() == "oh" ? O : T;
        const auto p = cs["p"];
        ld::hl::Chs out;
        if (fn == "eqRender") out = ld::hl::eqRender (in, sr, p);
        else if (fn == "pultecRender") out = ld::hl::pultecRender (in, sr, p);
        else if (fn == "widthRender") out = ld::hl::widthRender (in, sr, p);
        else if (fn == "gateRender") out = ld::hl::gateRender (in, sr, p);
        else if (fn == "compRender") out = ld::hl::compRender (in, sr, p);
        else if (fn == "satRender") out = ld::hl::satRender (in, sr, p);
        else if (fn == "verbRender") out = ld::hl::verbRender (in, sr, p);
        else if (fn == "insRender") { ld::hl::InsertOptions io; io.trim = -2; out = ld::hl::insRender (in, sr, p, io); }
        REQUIRE ((int) out.size() == (int) cs["channels"]);
        double worst = 0;
        long off = (int) cs["offset"];
        for (auto& y : out) { for (long i = 0; i < len; ++i) worst = std::max (worst, (double) std::abs (y[(size_t) i] - golden[off + i])); off += len; }
        INFO ("largest difference " << worst << " (" << 20 * std::log10 (worst + 1e-300) << " dB)");
        CHECK (worst < 1e-5);   // float rounding (-100 dB)
    }
}

#include "hl/Clean.h"
#include "hl/Shape.h"
namespace
{
// Compares a render with a golden stored as every STEP-th sample plus total energy.
double packDiff (const ld::hl::Chs& got, const juce::var& gold, int step)
{
    double worst = 0;
    REQUIRE ((int) got.size() == gold.getArray()->size());
    for (int c = 0; c < (int) got.size(); ++c)
    {
        const auto g = gold[c];
        REQUIRE ((int) got[(size_t) c].size() == (int) g["len"]);
        double e = 0;
        for (float v : got[(size_t) c]) e += (double) v * v;
        const double ge = g["energy"];
        worst = std::max (worst, std::abs (e - ge) / std::max (1e-30, ge) * 1e-3);
        const auto* s = g["s"].getArray();
        for (int i = 0; i < s->size(); ++i) worst = std::max (worst, std::abs ((double) got[(size_t) c][(size_t) (i * step)] - (double) (*s)[i]));
    }
    return worst;
}
}

TEST_CASE ("Parity: clean-up, tone/shape and tails match Hitline 4.8")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    const auto G = juce::JSON::parse (parityDir().getChildFile ("clean.json"));
    const auto C = G["cases"];
    const int step = G["STEP"];
    const double sr = G["sr"];
    const long end = (int) G["end"];
    std::vector<long> ons;
    for (auto& v : *G["ons"].getArray()) ons.push_back ((long) (int) v);
    const auto dir = juce::File (root).getChildFile ("SNARE 2 - BB/Center");
    auto load = [&] (const char* f) { auto w = ld::hl::readWav (dir.getChildFile (f)); ld::hl::Chs c; for (auto& x : w.channels) c.push_back (ld::hl::F32 (x.begin(), x.begin() + end)); return c; };
    const auto top = load ("SNARE_CENTER_TOP_RR06_V06.wav"), oh = load ("SNARE_CENTER_OH_RR06_V06.wav"), far = load ("SNARE_CENTER_ROOM-FAR_RR06_V06.wav");

    // noise print and denoise
    const auto np = ld::hl::noisePrint (far[0], sr, ons);
    REQUIRE (np.has_value() == C["noisePrint"].isObject());
    CHECK (np->runs == (int) C["noisePrint"]["runs"]);
    CHECK_THAT (np->floorDb, WithinAbs ((double) C["noisePrint"]["floorDb"], 1e-9));
    CHECK_THAT (np->trust, WithinAbs ((double) C["noisePrint"]["trust"], 1e-9));
    const auto* psd = C["noisePrint"]["psdHead"].getArray();
    for (int k = 0; k < psd->size(); ++k) CHECK_THAT (np->psd[(size_t) k] / (double) (*psd)[k], WithinAbs (1.0, 1e-9));
    { INFO ("denoise"); CHECK (packDiff ({ ld::hl::denoise (far[0], sr, &*np, 14, 1.6, 0.08) }, C["denoise"], step) < 1e-6); }

    // tails
    const long h = 3, a = ons[(size_t) h] - 88, b = ons[(size_t) h + 1] - 88;
    ld::hl::Chs seg;
    for (auto* set : { &top, &oh, &far }) for (auto& x : *set) seg.push_back (ld::hl::F32 (x.begin() + a, x.begin() + b));
    const auto plan = ld::hl::tailPlan (seg, sr, 0, -60);
    CHECK (plan.end == (long) (int) C["tailPlan"]["end"]);
    CHECK (plan.F == (long) (int) C["tailPlan"]["F"]);
    CHECK_THAT (plan.need, WithinAbs ((double) C["tailPlan"]["need"], 1e-9));
    { INFO ("applyTail"); CHECK (packDiff (ld::hl::applyTail (seg, plan, sr), C["applyTail"], step) < 1e-6); }

    // click repair
    auto clicked = top[0];
    for (long k = 0; k < (long) ons.size(); ++k) { const long p = ons[(size_t) k] + (long) ld::hl::jsRound (0.3 * sr); clicked[(size_t) p] = (float) (clicked[(size_t) p] + 0.3); clicked[(size_t) p + 1] = (float) (clicked[(size_t) p + 1] - 0.25); }
    CHECK (ld::hl::repairClicks (clicked, sr, ons) == (int) C["repairCount"]);
    { INFO ("repair"); CHECK (packDiff ({ clicked }, C["repair"], step) < 1e-6); }

    // bleed gate
    { INFO ("bleedGate"); CHECK (packDiff (ld::hl::bleedGate (oh, sr, 12), C["bleedGate"], step) < 1e-6); }

    // tone and shape pass
    {
        const auto P = C["fxPlan"];
        ld::hl::FxPlan fp;
        fp.K = P["K"]; fp.ons = ons; fp.spare = P["spare"]; fp.rampMs = P["rampMs"]; fp.envStepMs = P["envStepMs"]; fp.envT0 = P["envT0"];
        for (auto& k : *P["kern"].getArray()) { ld::hl::F64 v; for (auto& x : *k.getArray()) v.push_back ((double) x); fp.kern.push_back (v); }
        for (auto& x : *P["tone"].getArray()) fp.tone.push_back ((int) x);
        for (auto& x : *P["cut"].getArray()) fp.cut.push_back ((int) x);
        for (auto& x : *P["hitK"].getArray()) fp.hitK.push_back ((double) x);
        for (auto& x : *P["env"].getArray()) fp.env.push_back ((double) x);
        fp.hasComp = true; fp.compWin = (int) P["comp"]["win"]; fp.compLag = (int) P["comp"]["lag"]; fp.repair = P["repair"];
        INFO ("fxRenderMulti");
        CHECK (packDiff (ld::hl::fxRenderMulti (oh, sr, fp), C["fx"], step) < 1e-6);
    }

    // linked taming
    {
        ld::hl::TameOptions to;
        to.ons = ons; for (size_t i = 1; i < ons.size(); ++i) to.bnd.push_back (ons[i]); to.bnd.push_back (end);
        to.lag = { 0, 22.7, 1254 }; to.depth = 0.6; to.sharp = 0.4;
        const auto r = ld::hl::tameMulti ({ top, oh, far }, sr, to);
        const auto* gk = C["tame"]["gains"].getArray();
        for (int k = 0; k < gk->size(); ++k) CHECK_THAT (r.gains[(size_t) k], WithinAbs ((double) (*gk)[k], 1e-9));
        for (int t = 0; t < 3; ++t) { INFO ("tame track " << t); CHECK (packDiff (r.tracks[(size_t) t], C["tame"]["tracks"][t], step) < 1e-6); }
    }

    // resonance scan and library measurements
    {
        ld::hl::ResScanOptions ro; ro.ring = true; ro.autoProtect = true;
        std::vector<long> ends; for (size_t i = 1; i < ons.size(); ++i) ends.push_back (ons[i]); ends.push_back (end);
        const auto rs = ld::hl::resScan (top[0], ons, ends, sr, ro);
        const auto K = C["resScan"];
        CHECK_THAT (rs.f0, WithinAbs ((double) K["f0"], 1e-9));
        CHECK_THAT (rs.thr, WithinAbs ((double) K["thr"], 1e-9));
        REQUIRE ((int) rs.peaks.size() == K["peaks"].getArray()->size());
        for (int i = 0; i < (int) rs.peaks.size(); ++i) { CHECK_THAT (rs.peaks[(size_t) i].f, WithinAbs ((double) K["peaks"][i]["f"], 1e-9)); CHECK_THAT (rs.peaks[(size_t) i].q, WithinAbs ((double) K["peaks"][i]["q"], 1e-9)); }
        const auto bs = ld::hl::bandSpec (top[0], ons[2], sr);
        for (int i = 0; i < (int) bs.size(); ++i) CHECK_THAT (bs[(size_t) i], WithinAbs ((double) C["bandSpec"][i], 1e-9));
        const auto es = ld::hl::envStats (top[0], ons[2], sr);
        CHECK (es.atk == (long) (int) C["envStats"]["atk"]);
        CHECK (es.t60 == (long) (int) C["envStats"]["t60"]);
        CHECK_THAT (es.body, WithinAbs ((double) C["envStats"]["body"], 1e-9));
    }
}

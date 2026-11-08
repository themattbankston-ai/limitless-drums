// Regression tests for the independent audit's findings (LD-01 ... LD-15). Each test fails on the audited commit
// (e365e61) and passes with its fix. LD-06 (export at another sample rate) is in SongTests; LD-05 (a plugin load
// that times out) needs a stalled message thread and isn't covered here.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/hl/Song.h"
#include "../src/hl/FxInserts.h"
#include "../src/hl/FxSat.h"
#include <cmath>
#include <thread>

using namespace ld::hl;

namespace
{
constexpr double SR = 48000;
F32 sine (double f, double amp, long n) { F32 x ((size_t) n); for (long i = 0; i < n; ++i) x[(size_t) i] = (float) (amp * std::sin (2 * M_PI * f * (double) i / SR)); return x; }
double peak (const F32& x, size_t a = 0, size_t b = SIZE_MAX) { double m = 0; for (size_t i = a; i < std::min (b, x.size()); ++i) m = std::max (m, (double) std::abs (x[i])); return m; }
double db (double r) { return 20 * std::log10 (r); }
juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> kv)
{
    auto* o = new juce::DynamicObject();
    for (auto& [k, v] : kv) o->setProperty (k, v);
    return juce::var (o);
}
juce::var arr (std::initializer_list<juce::var> v) { juce::Array<juce::var> a; for (auto& x : v) a.add (x); return a; }
}

TEST_CASE ("Audit LD-01: Song edits that move nothing leave the audio exactly as it was")
{
    // a drum-like signal: decaying 80 Hz plus a little 2 kHz
    const long n = (long) SR;
    F32 x ((size_t) n);
    for (long i = 0; i < n; ++i) { const double t = (i % 12000) / SR; x[(size_t) i] = (float) (0.8 * std::exp (-t * 8) * (std::sin (2 * M_PI * 80 * t) + 0.2 * std::sin (2 * M_PI * 2000 * t))); }
    // boundaries with no move on either side
    const auto y = applyEdits (x, { { 6000, 0 }, { 18000, 0 }, { 30000, 0 } }, SR);
    REQUIRE (y.size() == x.size());
    for (size_t i = 0; i < x.size(); ++i) REQUIRE (y[i] == x[i]);
    // the same shift on both sides of a boundary: a clean shift, no swell where the boundary was
    const auto z = applyEdits (x, { { 6000, 40 }, { 18000, 40 } }, SR);
    for (size_t i = 6000 + 200; i < x.size(); ++i) REQUIRE (z[i] == x[i - 40]);
}

TEST_CASE ("Audit LD-01: a small move doesn't swell the level across the crossfade")
{
    // a steady low tone: both sides of the cut are nearly identical (2 samples apart), the case where equal-power
    // fades added up to 3 dB
    const auto x = sine (100, 0.5, (long) SR);
    const auto y = applyEdits (x, { { 24120, 2 } }, SR);   // on a peak of the tone, where a swell shows
    CHECK (db (peak (y, 23000, 25000) / peak (x, 23000, 25000)) < 0.05);
}

TEST_CASE ("Audit LD-02: Prism dynamic band with range 0 makes no change")
{
    const auto x = sine (1000, 0.5, 24000);
    const auto band = obj ({ { "type", "bell" }, { "f", 1000 }, { "g", 0 }, { "q", 1 },
                             { "dyn", obj ({ { "on", true }, { "range", 0 }, { "thr", -60 } }) } });
    const auto y = eqRender ({ x }, SR, obj ({ { "bands", arr ({ band }) } }));
    CHECK (std::abs (db (peak (y[0], 12000) / peak (x, 12000))) < 0.05);
    // unset still means the old -6 dB default
    const auto band6 = obj ({ { "type", "bell" }, { "f", 1000 }, { "g", 0 }, { "q", 1 }, { "dyn", obj ({ { "on", true }, { "thr", -60 } }) } });
    const auto y6 = eqRender ({ x }, SR, obj ({ { "bands", arr ({ band6 }) } }));
    CHECK (db (peak (y6[0], 12000) / peak (x, 12000)) < -3);
}

TEST_CASE ("Audit LD-03: VCA compressor reduces a polarity-flipped stereo pair like an in-phase one")
{
    const auto l = sine (200, 0.9, 24000);
    F32 r (l.size()); for (size_t i = 0; i < l.size(); ++i) r[i] = -l[i];
    const auto p = obj ({ { "model", "160" } });
    const auto same = compRender ({ l, l }, SR, p), opp = compRender ({ l, r }, SR, p);
    const double grSame = db (peak (same[0], 12000) / peak (l, 12000)), grOpp = db (peak (opp[0], 12000) / peak (l, 12000));
    CHECK (grSame < -3);                                   // it is compressing
    CHECK (std::abs (grOpp - grSame) < 0.1);               // and the flipped pair the same amount
}

TEST_CASE ("Audit LD-04: the gate opens for a polarity-flipped stereo pair")
{
    const auto l = sine (200, 0.3, 24000);
    F32 r (l.size()); for (size_t i = 0; i < l.size(); ++i) r[i] = -l[i];
    const auto p = obj ({ { "thr", -40 }, { "range", -60 } });
    const auto same = gateRender ({ l, l }, SR, p), opp = gateRender ({ l, r }, SR, p);
    CHECK (std::abs (db (peak (opp[0], 12000) / peak (same[0], 12000))) < 0.1);
    CHECK (db (peak (opp[0], 12000) / peak (l, 12000)) > -1);
}

TEST_CASE ("Audit LD-14: Skull Cutter MULTI Transformer's Tone changes the sound")
{
    const auto x = sine (3000, 0.4, 12000);
    auto run = [&] (double tone) { return satRender ({ x }, SR, obj ({ { "model", "saturn" }, { "saturn", obj ({ { "style", "transformer" }, { "drive", 6 }, { "tone", tone } }) } })); };
    const auto a = run (0), b = run (5);
    // the output is level-matched, so compare the sound itself: it differs (it was bit-identical)
    F32 d (a[0].size()); for (size_t i = 0; i < d.size(); ++i) d[i] = a[0][i] - b[0][i];
    CHECK (db (peak (d, 6000) / peak (a[0], 6000)) > -30);
}

TEST_CASE ("Audit LD-15: reverbs built at the same time come out the same as built one at a time")
{
    std::vector<juce::var> ps;
    for (auto prog : { "Plate", "Room", "Chamber", "Concert Hall" }) ps.push_back (obj ({ { "model", "vv" }, { "prog", prog }, { "decay", 0.5 } }));
    clearReverbCache();
    std::vector<std::pair<F32, F32>> seq;
    for (auto& p : ps) seq.push_back (verbIR (p, SR));
    clearReverbCache();
    std::vector<std::pair<F32, F32>> par (ps.size());
    std::vector<std::thread> th;
    for (size_t i = 0; i < ps.size(); ++i) th.emplace_back ([&, i] { par[i] = verbIR (ps[i], SR); });
    for (auto& t : th) t.join();
    for (size_t i = 0; i < ps.size(); ++i) { CHECK (par[i].first == seq[i].first); CHECK (par[i].second == seq[i].second); }
}

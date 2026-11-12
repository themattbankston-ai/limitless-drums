#include <sstream>
#include <mutex>
#include "Session.h"
#include "Detect.h"
#include "FxInserts.h"
#include "Resolve.h"
#include "Tune.h"
#include <algorithm>
#include <atomic>
#include <future>
#include <thread>
#include <map>
#include <regex>

namespace ld::hl
{
namespace
{
juce::var LIBREF;

/** JavaScript's clamp (Math.max(a, Math.min(b, v))): NaN stays NaN. */
double jclamp (double v, double a, double b) { return std::isnan (v) ? v : std::max (a, std::min (b, v)); }

/** Hitline's median1: the finite values, sorted; NaN when there are none. */
double median1 (F64 a)
{
    a.erase (std::remove_if (a.begin(), a.end(), [] (double v) { return ! std::isfinite (v); }), a.end());
    if (a.empty()) return NAN;
    std::sort (a.begin(), a.end());
    const size_t n = a.size();
    return n % 2 ? a[n >> 1] : (a[n / 2 - 1] + a[n / 2]) / 2;
}
double bell (double f, double fc, double w) { const double l = std::log2 (f / fc); return std::exp (-(l * l) / (2 * w * w)); }
double shelf (double f, double fc, double w) { return 0.5 + 0.5 * std::tanh (std::log2 (f / fc) / w); }

bool reTest (const std::string& s, const char* re) { return std::regex_search (s, std::regex (re, std::regex::icase)); }
std::string lower (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }

const std::map<std::string, std::pair<double, double>> FAM_W = {
    { "lead", { 1, 1 } }, { "bottom", { 0.8, 0.6 } }, { "sub", { 0, 0 } }, { "oh", { 1, 0.7 } }, { "roomClose", { 0.8, 0.45 } },
    { "roomFar", { 0.7, 0.35 } }, { "roomMono", { 0.8, 0.45 } }, { "other", { 0.8, 0.6 } } };
const std::map<std::string, double> CRACK_HZ = { { "snare", 2800 }, { "sidestick", 2200 }, { "kick", 3600 }, { "rack", 3000 }, { "floor", 2600 },
    { "hat", 7500 }, { "hatOpen", 7000 }, { "ride", 4500 }, { "crash", 5000 }, { "china", 5500 } };
const std::map<std::string, std::array<double, 5>> SHAPE_T = {
    { "snare", { 10, 35, 150, 450, 1300 } }, { "sidestick", { 6, 25, 110, 350, 900 } }, { "kick", { 15, 50, 200, 600, 1600 } },
    { "rack", { 12, 60, 300, 1100, 3500 } }, { "floor", { 14, 70, 380, 1500, 4500 } }, { "hat", { 4, 15, 60, 220, 700 } },
    { "hatOpen", { 6, 40, 350, 1600, 5000 } }, { "ride", { 6, 45, 450, 2200, 8000 } }, { "crash", { 6, 45, 450, 2200, 8000 } },
    { "china", { 6, 40, 400, 1800, 6500 } } };
const std::map<std::string, double> TAME_SPARE = { { "snare", 4 }, { "sidestick", 3 }, { "kick", 6 }, { "rack", 5 }, { "floor", 5 },
    { "hat", 2 }, { "ride", 2 }, { "crash", 2 }, { "china", 2 }, { "splash", 2 } };
const std::map<std::string, std::array<double, 3>> TAME_SENS = {
    { "snare", { 250, 600, 1200 } }, { "sidestick", { 250, 600, 1200 } }, { "kick", { 400, 1000, 2000 } }, { "rack", { 300, 800, 1500 } },
    { "floor", { 250, 700, 1400 } }, { "hat", { 500, 900, 1500 } }, { "ride", { 500, 900, 1500 } }, { "crash", { 500, 900, 1500 } },
    { "china", { 500, 900, 1500 } }, { "splash", { 500, 900, 1500 } } };
const std::map<std::string, std::string> DRUM_NAMES = { { "kit", "Full kit" }, { "kick", "Kick" }, { "snare", "Snare" }, { "tom", "Tom" },
    { "hat", "Hi-hat" }, { "ride", "Ride" }, { "crash", "Crash" }, { "china", "China" }, { "splash", "Splash" }, { "other", "Other" } };
const char* INS_TYPES[] = { "gate", "eq", "pultec", "comp", "sat", "verb", "width", "ext" };
constexpr double ROOM_WIN_MS = 400;

/** Runs fn(0..n-1) across the cores. Every job is independent, so the results are the same as in order. */
void parallelFor (size_t n, const std::function<void (size_t)>& fn)
{
    const size_t nT = std::min<size_t> (n, std::max (1u, std::thread::hardware_concurrency()));
    if (nT <= 1) { for (size_t i = 0; i < n; ++i) fn (i); return; }
    std::atomic<size_t> next { 0 };
    std::vector<std::future<void>> jobs;
    for (size_t w = 0; w < nT; ++w)
        jobs.push_back (std::async (std::launch::async, [&] { for (size_t i; (i = next++) < n;) fn (i); }));
    for (auto& j : jobs) j.get();
}

bool toneAny (const ToneMoves& T) { return T.match != 0 || T.bright != 0 || T.crack != 0 || T.smooth != 0 || T.tame != 0; }
bool shapeAny (const ShapeMoves& H) { return H.atk != 0 || H.dec != 0 || H.sus != 0 || H.rel != 0 || H.match != 0; }
bool refLive (const Session& S) { return S.tone.hasRef && S.tone.refAmt != 0; }
bool refEnvOn (const Session& S) { return S.tone.hasRefEnv && S.tone.refEnvAmt != 0; }

bool insLive (const juce::var& s)
{
    if (! s.isObject() || isFalse (s, "on")) return false;
    const auto ty = s["type"].toString();
    for (auto* k : INS_TYPES) if (ty == k) return true;
    return false;
}
bool insActive (const Session& S, int t)
{
    // (Song mode uses the mixer too)
    const auto& st = S.tracks[(size_t) t].strip;
    if (auto* a = st.inserts.getArray()) for (auto& s : *a) if (insLive (s)) return true;
    return st.trim != 0;
}
int polR (const Session& S, int t) { return S.tracks[(size_t) t].pol * (S.tracks[(size_t) t].strip.pol ? -1 : 1); }
double lagOf (const Session& S, int t) { return t == S.refIdx ? 0.0 : S.tracks[(size_t) t].lag; }

Chs baseCh (const SessionTrack& tr)
{
    if (tr.ch.size() != 2 || ! tr.lrLock || ! tr.lr || tr.lr->kept) return tr.ch;
    const auto& R = tr.ch[1];
    return { tr.ch[0], renderSeg (R, 0, (long) R.size(), tr.lr->lag, tr.lr->pol, 1, true) };
}

/** fxMono: one channel, or the channels averaged into a Float32Array. */
F32 fxMono (const Chs& chs)
{
    if (chs.size() == 1) return chs[0];
    F32 m (chs[0].size(), 0.0f);
    for (auto& x : chs)
        for (size_t n = 0; n < m.size(); ++n) m[n] = (float) ((double) m[n] + (double) x[n] / (double) chs.size());
    return m;
}
/** Hitline's micKind, for the kit bleed gate: which mics are close mics. */
std::string micKind (const SessionTrack& tr)
{
    const auto n = lower (tr.mic + " " + tr.name);
    auto h = [&] (const char* re) { return std::regex_search (n, std::regex (re)); };
    if (h (R"(\bsub\b|subkick)")) return "sub";
    if (h (R"(kick|\bbd\b)") && h (R"(\bin\b|inside)")) return "kickIn";
    if (h (R"(kick|\bbd\b)") && h (R"(\bout\b|front|reso)")) return "kickOut";
    if (h (R"(kick in|^in\b)") || (h (R"(\bin\b)") && ! h ("room"))) return "kickIn";
    if (h (R"(\bout\b)")) return "kickOut";
    if (h ("crush|crsh|trash|smash")) return "crush";
    if (h ("mono") && h ("room|rm|mono")) return "roomMono";
    if (h ("far")) return "roomFar";
    if (h (R"(room|\brm\b|amb|hall)")) return "roomClose";
    if (h (R"(\boh\b|overhead|ovh)")) return "oh";
    if (h (R"(bottom|btm|\bbot\b|reso)")) return "bottom";
    if (h (R"(hat|hh\b)")) return "hat";
    if (h ("ride|crash|china|splash|cym")) return "cym";
    if (h ("tom|floor|rack")) return "tom";
    if (h ("spot|clip")) return "spot";
    return "top";
}
bool isCloseKind (const SessionTrack& tr)
{
    const auto k = micKind (tr);
    return k == "kickIn" || k == "kickOut" || k == "top" || k == "bottom" || k == "tom" || k == "spot";
}

LevelTrack levelTrack (const Session& S, int t)
{
    const auto& tr = S.tracks[(size_t) t];
    LevelTrack lt;
    lt.mic = tr.mic; lt.name = tr.name; lt.mono = tr.mono;
    lt.lagInt = (long) jsRound (lagOf (S, t)); lt.pol = tr.pol; lt.clipDb = tr.clip; lt.autoAsis = tr.autoAsis;
    return lt;
}
/** Hitline's firstArrivals: per hit, the first sample at 2 % of the hit's own peak (or 4x the noise before it). */
F64 firstArrivals (const Session& S, const F32& x, const std::vector<long>& ends)
{
    const double sr = S.sr;
    const long pre = (long) jsRound (0.003 * sr), X = (long) x.size();
    F64 out;
    for (size_t k = 0; k < S.onsets.size(); ++k)
    {
        const long on = S.onsets[k], a = std::max (0L, on - pre), e = std::min ({ X, ends[k], on + (long) jsRound (0.5 * sr) });
        double pk = 0, nz = 0; long nn = 0;
        for (long n = a; n < e; ++n) { const double v = std::abs ((double) x[(size_t) n]); if (v > pk) pk = v; }
        for (long n = std::max (0L, a - (long) jsRound (0.02 * sr)); n < a; ++n) { nz += (double) x[(size_t) n] * x[(size_t) n]; nn++; }
        const double thr = std::max (0.02 * pk, 4 * std::sqrt (nz / std::max (1L, nn)));
        double r = NAN;
        for (long n = a; n < e; ++n) if (std::abs ((double) x[(size_t) n]) >= thr) { r = (double) n; break; }
        out.push_back (r);
    }
    return out;
}
std::vector<long> regionEnds (const Session& S)
{
    std::vector<long> e;
    for (size_t k = 0; k < S.onsets.size(); ++k) e.push_back (k + 1 < S.onsets.size() ? S.onsets[k + 1] : S.maxLen());
    return e;
}
} // namespace

bool studioReferenceAvailable() { return LIBREF["panda"].isObject(); }
void setLibraryReference (const juce::var& libref) { LIBREF = libref; }

long Session::maxLen() const { long m = 0; for (auto& t : tracks) m = std::max (m, t.len); return m; }
int Session::velOfHit (size_t i) const
{
    const int g = (int) (i / (size_t) det.RR);
    return det.order == "loud" ? det.V - 1 - g : g;
}

bool fxOn (const Session& S)
{
    if (S.mode == "kit") return false;
    if (S.tone.repair || refLive (S) || refEnvOn (S)) return true;
    for (int t = 0; t < (int) S.tracks.size(); ++t) if (trackFxOn (S, t)) return true;
    return false;
}

//==============================================================================
ToneEngine::ToneEngine (const Session& s, const std::vector<std::optional<MixRender::Scan>>& sc)
    : S (s), scans (sc), arrCache (s.tracks.size(), NAN), houseCache (s.tracks.size()), famCache (s.tracks.size()) {}

bool ToneEngine::isCymbal() const { return S.drum == "hat" || S.drum == "ride" || S.drum == "crash" || S.drum == "china" || S.drum == "splash"; }

std::string ToneEngine::libDrum() const
{
    if (! drumCache) drumCache = libDrumNow (S, scans);
    return *drumCache;
}

std::string ToneEngine::libDrumNow (const Session& S, const std::vector<std::optional<MixRender::Scan>>& scans)
{
    const auto& d = S.drum; const auto a = lower (S.artic);
    if (d == "snare") return reTest (a, "side|x-?stick|cross") ? "sidestick" : "snare";
    if (d == "tom")
    {
        double f = 120;
        if (S.tinfo) f = S.tinfo->medF;
        else if (S.refIdx < (int) scans.size() && scans[(size_t) S.refIdx]) f = scans[(size_t) S.refIdx]->res.f0;
        return f < 105 ? "floor" : "rack";
    }
    if (d == "hat") return reTest (a, "open|loose|half|wash") ? "hatOpen" : "hat";
    if (d == "splash") return "crash";
    return LIBREF["drums"].hasProperty (juce::Identifier (d)) ? d : std::string();
}

std::string ToneEngine::houseKeyOf (int t) const
{
    auto& c = houseCache[(size_t) t];
    if (c.empty()) c = houseKeyNow (t);
    return c;
}

std::string ToneEngine::houseKeyNow (int t) const
{
    auto lt = levelTrack (S, t);
    if (t != S.refIdx && ! S.onsets.empty()) { const double a = arrOf (t) / S.sr * 1000; lt.arrivalMs = a; }
    return houseKey (lt, t == S.refIdx);
}

std::string ToneEngine::libFam (int t) const
{
    auto& c = famCache[(size_t) t];
    if (c.empty()) c = libFamNow (t);
    return c;
}

std::string ToneEngine::libFamNow (int t) const
{
    const auto k = houseKeyOf (t);
    if (k != "room") return k;
    const auto n = lower (S.tracks[(size_t) t].mic + " " + S.tracks[(size_t) t].name);
    if (reTest (n, "far|hall|amb")) return "roomFar";
    if (reTest (n, "mono")) return "roomMono";
    return "roomClose";
}

ToneEngine::Ref ToneEngine::libRef (int t) const
{
    const auto d = libDrum(), f = libFam (t);
    auto pick = [&] (const juce::var& D) -> juce::var
    {
        if (! D.isObject()) return {};
        const auto v = D[juce::Identifier (f)];
        if (v.isObject()) return v;
        if (f == "bottom") return {};
        if (f.rfind ("room", 0) == 0) return D["roomClose"];
        return {};
    };
    const bool panda = S.tone.proSrc == "panda" && LIBREF["panda"].isObject();
    const auto P = panda && ! d.empty() ? pick (LIBREF["panda"]["drums"][juce::Identifier (d)]) : juce::var();
    const auto L = ! d.empty() ? pick (LIBREF["drums"][juce::Identifier (d)]) : juce::var();
    Ref r;
    auto arrOfV = [] (const juce::var& v) { const auto a = v["arr"]; return a.isVoid() || a.isUndefined() ? NAN : (double) a; };
    if (P.isObject()) { r.v = P; r.ok = true; r.arr = arrOfV (P); const auto pa = P["arr"]; if ((pa.isVoid() || pa.isUndefined()) && L.isObject()) r.arr = arrOfV (L); return r; }
    if (L.isObject()) { r.v = L; r.ok = true; r.arr = arrOfV (L); }
    return r;
}

ToneMoves ToneEngine::moves (int t) const
{
    const auto& a = S.tone; const auto& b = S.tracks[(size_t) t].strip.tone;
    ToneMoves o;
    o.match = jclamp (a.match + b.match, 0, 1); o.bright = jclamp (a.bright + b.bright, -1, 1); o.crack = jclamp (a.crack + b.crack, -1, 1);
    o.smooth = jclamp (a.smooth + b.smooth, 0, 1); o.tame = jclamp (a.tame + b.tame, 0, 1);
    return o;
}
ShapeMoves ToneEngine::shapeMoves (int t) const
{
    const auto& a = S.shape; const auto& b = S.tracks[(size_t) t].strip.shape;
    ShapeMoves o;
    o.atk = jclamp (a.atk + b.atk, -1, 1); o.dec = jclamp (a.dec + b.dec, -1, 1); o.sus = jclamp (a.sus + b.sus, -1, 1);
    o.rel = jclamp (a.rel + b.rel, -1, 1); o.match = jclamp (a.match + b.match, 0, 1);
    return o;
}

static F64 tierOf (const juce::var& spec, int i)
{
    auto* a = spec.getArray();
    juce::var s = (*a)[i];
    if (! s.isArray()) for (auto& v : *a) if (v.isArray()) { s = v; break; }
    F64 o; if (auto* q = s.getArray()) for (auto& v : *q) o.push_back ((double) v);
    return o;
}

F64 ToneEngine::matchBands (int t, int v) const
{
    F64 out (30, 0.0);
    const auto& A = scans[(size_t) t];
    const auto ref = libRef (t);
    if (! A || ! ref.ok || ! ref.v["spec"].isArray()) return out;
    const int V = S.det.V;
    const double pos = V > 1 ? 2.0 * v / (V - 1) : 2;
    const int lo = (int) std::floor (std::min (pos, 1.999));
    const double fr = pos - lo;
    const auto t0 = tierOf (ref.v["spec"], lo), t1 = tierOf (ref.v["spec"], lo + 1);
    F64 R (30);
    for (size_t j = 0; j < 30; ++j) R[j] = t0[j] + (t1[j] - t0[j]) * fr;
    std::vector<size_t> hits;
    for (size_t k = 0; k < S.onsets.size(); ++k) if (S.velOfHit (k) == v) hits.push_back (k);
    if (hits.empty()) for (size_t k = 0; k < S.onsets.size(); ++k) hits.push_back (k);
    const auto& B = bands3();
    F64 U (30);
    for (size_t j = 0; j < 30; ++j) { F64 col; for (auto k : hits) col.push_back (k < A->bands.size() ? A->bands[k][j] : NAN); U[j] = median1 (col); }
    const double f0 = A->res.f0;
    const bool cym = isCymbal();
    double sw = 0, sd = 0;
    F64 w (30, 0.0);
    for (size_t j = 0; j < 30; ++j)
    {
        const double f = B[j];
        double wt = f < 90 ? 0 : f < 140 ? (f - 90) / 50 : f > 15000 ? 0 : 1;
        if (! cym && f0 != 0 && ! std::isnan (f0) && std::abs (std::log2 (f / f0)) < 0.5) wt *= 0.3;
        if (! (R[j] > -70) || ! (U[j] > -90)) wt = 0;
        w[j] = wt; out[j] = (R[j] - U[j]) * wt;
        const double e = std::pow (10.0, U[j] / 10) * wt;
        sw += e; sd += e * out[j];
    }
    const double mean = (sw != 0 && ! std::isnan (sw)) ? sd / sw : 0;
    for (size_t j = 0; j < 30; ++j) out[j] = w[j] != 0 ? out[j] - mean * w[j] : 0;
    for (int it = 0; it < 2; ++it) { const F64 c = out; for (size_t j = 1; j < 29; ++j) out[j] = 0.25 * c[j - 1] + 0.5 * c[j] + 0.25 * c[j + 1]; }
    const double lim = cym ? 4.5 : 6, m = moves (t).match;
    for (size_t j = 0; j < 30; ++j) out[j] = jclamp (out[j], -lim, lim) * m;
    return out;
}

double ToneEngine::refAt (double f) const
{
    if (! refLive (S)) return 0;
    const auto& B = bands3(); const auto& c = S.tone.refCurve;
    const double x = std::log2 (f / B[0]) * 3;
    double m;
    if (x <= 0) m = c[0]; else if (x >= 29) m = c[29];
    else { const int j = (int) std::floor (x); const double u = x - j; m = c[(size_t) j] + (c[(size_t) j + 1] - c[(size_t) j]) * u; }
    return m * S.tone.refAmt;
}

double ToneEngine::toneAt (int t, const F64* mb, double f) const
{
    const auto fam = libFam (t);
    const auto wi = FAM_W.find (fam);
    const auto W = wi != FAM_W.end() ? wi->second : FAM_W.at ("other");
    const auto T = moves (t);
    auto d = libDrum(); if (d.empty()) d = "snare";
    const auto& B = bands3();
    double m = 0;
    if (mb)
    {
        const double x = std::log2 (f / B[0]) * 3;
        if (x <= 0) m = (*mb)[0]; else if (x >= 29) m = (*mb)[29];
        else { const int j = (int) std::floor (x); const double u = x - j; m = (*mb)[(size_t) j] + ((*mb)[(size_t) j + 1] - (*mb)[(size_t) j]) * u; }
    }
    const double br = T.bright * W.first * (isCymbal() ? 4.5 * shelf (f, 5500, 1.1) - 1 * (1 - shelf (f, 300, 1)) : 6 * shelf (f, 4500, 1.1) - 1.5 * (1 - shelf (f, 220, 1)));
    const auto ci = CRACK_HZ.find (d);
    const double fc = ci != CRACK_HZ.end() ? ci->second : 3000;
    const double cr = T.crack * W.second * (5 * bell (f, fc, 0.75) - (T.crack > 0 ? 1.5 * bell (f, 520, 0.6) : 0));
    return m + br + cr + refAt (f);
}

double ToneEngine::cutAt (int t, double f, double harsh) const
{
    const auto& A = scans[(size_t) t];
    const double sm = moves (t).smooth;
    if (! A || sm == 0) return 0;
    double g = 0;
    for (auto& p : A->res.peaks)
    {
        const double dep = sm * std::min (12.0, std::max (0.0, p.ex - 2)) * 0.85, y = p.q * (f / p.f - p.f / f);
        g -= dep / (1 + y * y);
    }
    if (harsh != 0) g -= sm * harsh * bell (f, 4000, 0.55);
    return std::max (-15.0, g);
}

double ToneEngine::harshOf (int t) const
{
    if (! isCymbal()) return 0;
    const auto& A = scans[(size_t) t];
    const auto ref = libRef (t);
    if (! A || ! ref.ok || ! ref.v["spec"].isArray()) return 0;
    const auto R = tierOf (ref.v["spec"], 2);
    std::vector<size_t> top;
    for (size_t k = 0; k < S.onsets.size(); ++k) if (S.velOfHit (k) >= S.det.V - 2) top.push_back (k);
    double s = 0; int c = 0;
    const auto& B = bands3();
    for (size_t j = 0; j < 30; ++j)
        if (B[j] >= 2400 && B[j] <= 6400)
        {
            F64 col; for (auto k : top) col.push_back (k < A->bands.size() ? A->bands[k][j] : NAN);
            const double u = median1 (col);
            if (std::isfinite (u) && R[j] > -70) { s += u - R[j]; c++; }
        }
    return c ? jclamp (s / c, 0, 6) * 0.8 : 0;
}

std::array<double, 3> ToneEngine::proEnvDelta (int t) const
{
    const auto& A = scans[(size_t) t];
    const auto ref = libRef (t);
    if (! A || ! ref.ok) return { 0, 0, 0 };
    std::vector<size_t> top;
    for (size_t k = 0; k < S.onsets.size(); ++k) if (S.velOfHit (k) >= S.det.V - 2) top.push_back (k);
    if (top.empty()) for (size_t k = 0; k < S.onsets.size(); ++k) top.push_back (k);
    auto u = [&] (double EnvStats::* key) { F64 col; for (auto k : top) if (k < A->env.size()) col.push_back (A->env[k].*key); return median1 (col); };
    auto num = [] (const juce::var& v) { return v.isVoid() || v.isUndefined() ? NAN : (double) v; };
    auto c = [&] (double r, double x) { return std::isfinite (r) && std::isfinite (x) && r > -100 && x > -100 ? jclamp (r - x, -6, 6) : 0.0; };
    return { c (num (ref.v["body"]), u (&EnvStats::body)), c (num (ref.v["sus"]), u (&EnvStats::sus)), c (num (ref.v["tail"]), u (&EnvStats::tail)) };
}

F32 ToneEngine::shapeTable (int t) const
{
    const auto H = shapeMoves (t);
    if (! shapeAny (H) && ! refEnvOn (S)) return {};
    auto d = libDrum(); if (d.empty()) d = "snare";
    const auto si = SHAPE_T.find (d);
    const auto T0 = si != SHAPE_T.end() ? si->second : SHAPE_T.at ("snare");
    const auto& A = scans[(size_t) t];
    const auto ref = libRef (t);
    const double Ta = T0[0], Tm = T0[1], Ts = T0[2], Tr = T0[3], Te = T0[4];
    const double Atk = H.atk * 10, hold = std::min (5.0, 0.4 * Ta), tau = 1.4 * Ta, D = H.dec * 6, Su = H.sus * 8, R = H.rel > 0 ? H.rel * 6 : H.rel * 12;
    double mB = 0, mS = 0, mT = 0;
    if (H.match != 0) { const auto d = proEnvDelta (t); mB = d[0] * H.match; mS = d[1] * H.match; mT = d[2] * H.match; }
    if (refEnvOn (S)) { mB += S.tone.refEnv[0] * S.tone.refEnvAmt; mS += S.tone.refEnv[1] * S.tone.refEnvAmt; mT += S.tone.refEnv[2] * S.tone.refEnvAmt; }
    auto lt = [] (double ms) { return std::log (1 + std::max (0.0, ms)); };
    using Pts = std::vector<std::pair<double, double>>;
    auto seg = [&] (const Pts& pts, double ms)
    {
        if (ms <= pts[0].first) return pts[0].second;
        for (size_t j = 1; j < pts.size(); ++j)
            if (ms <= pts[j].first)
            {
                const auto [a0, v0] = pts[j - 1]; const auto [a1, v1] = pts[j];
                const double u = (lt (ms) - lt (a0)) / (lt (a1) - lt (a0));
                return v0 + (v1 - v0) * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * u));
            }
        return pts.back().second;
    };
    const Pts body = { { 0, 0 }, { Ta, 0 }, { Tm, D }, { Ts, Su }, { Tr, Su + R * 0.5 }, { Te, Su + R } };
    const Pts match = { { 3, 0 }, { 30, mB }, { 130, mS }, { 600, mT } };
    const double step = 0.25;
    const size_t n = (size_t) std::ceil ((Te + 1) / step) + 1;
    F32 tab (n);
    for (size_t j = 0; j < n; ++j)
    {
        const double ms = -1 + (double) j * step;
        double a = 0;
        if (ms > -0.5 && Atk != 0) { if (ms < 0.5) a = Atk * (ms + 0.5); else if (ms < hold) a = Atk; else a = Atk * std::exp (-(ms - hold) / tau); }
        tab[j] = (float) std::pow (10.0, jclamp (a + seg (body, ms) + seg (match, ms), -30, 12) / 20);
    }
    return tab;
}

FxPlan ToneEngine::plan (int t) const
{
    const int V = S.det.V;
    const int K = S.sr > 64000 ? 8192 : 4096;
    const auto T = moves (t);
    const bool toneLive = T.match != 0 || T.bright != 0 || T.crack != 0 || refLive (S);
    const auto& A = scans[(size_t) t];
    auto grid = [&] (const std::function<double (double)>& fn)
    {
        F64 d ((size_t) K / 2 + 1);
        for (int k = 0; k <= K / 2; ++k) d[(size_t) k] = (double) (float) fn (std::max (1, k) * S.sr / K);
        return d;
    };
    FxPlan P;
    P.K = K;
    P.ons = S.onsets;
    const double harsh = T.smooth != 0 ? harshOf (t) : 0;
    const bool haveCut = T.smooth != 0 && A && (! A->res.peaks.empty() || harsh > 0.3);
    F64 cutG;
    if (haveCut) cutG = grid ([&] (double f) { return cutAt (t, f, harsh); });
    std::map<int, std::pair<int, int>> byLayer;
    for (size_t k = 0; k < S.onsets.size(); ++k)
    {
        const int v = std::clamp (S.velOfHit (k), 0, V - 1);
        if (! byLayer.count (v))
        {
            F64 mb; const bool haveMb = toneLive && T.match != 0;
            if (haveMb) mb = matchBands (t, v);
            int iT = -1, iC = -1;
            F64 base;
            if (toneLive) { base = grid ([&] (double f) { return toneAt (t, haveMb ? &mb : nullptr, f); }); P.kern.push_back (base); iT = (int) P.kern.size() - 1; }
            if (haveCut)
            {
                if (toneLive) { F64 s (base.size()); for (size_t j = 0; j < s.size(); ++j) s[j] = (double) (float) (base[j] + cutG[j]); P.kern.push_back (s); }
                else P.kern.push_back (cutG);
                iC = (int) P.kern.size() - 1;
            }
            byLayer[v] = { iT, iC };
        }
        P.tone.push_back (byLayer[v].first); P.cut.push_back (byLayer[v].second);
    }
    if (! haveCut) P.cut.clear();
    for (size_t k = 0; k < S.onsets.size(); ++k)
    {
        if (! haveCut || A->res.peaks.empty()) { P.hitK.push_back (1); continue; }
        double s = 0, w = 0;
        for (auto& p : A->res.peaks) { s += (k < p.hit.size() ? p.hit[k] : 1.0) * p.ex; w += p.ex; }
        P.hitK.push_back (w != 0 ? jclamp (s / w, 0, 1.25) : 1);
    }
    const bool cym = isCymbal();
    P.spare = cym ? (S.drum == "hat" ? 0.4 : 0.5) : 0.25;
    P.rampMs = cym ? (S.drum == "hat" ? 15 : 60) : 25;
    const auto tab = shapeTable (t);
    P.env.assign (tab.begin(), tab.end());
    P.envT0 = -1; P.envStepMs = 0.25;
    const bool roomStd = t != S.refIdx && houseKeyOf (t) == "room" && S.lev.std == "house";
    P.hasComp = true;
    P.compWin = (long) jsRound ((roomStd ? ROOM_WIN_MS : S.lev.winMs) * S.sr / 1000);
    P.compLag = (long) jsRound (lagOf (S, t));
    P.repair = S.tone.repair;
    return P;
}

double ToneEngine::arrOf (int t) const
{
    if (! std::isnan (arrCache[(size_t) t])) return arrCache[(size_t) t];
    const auto ends = regionEnds (S);
    const auto ra = firstArrivals (S, S.tracks[(size_t) S.refIdx].mono, ends), ma = firstArrivals (S, S.tracks[(size_t) t].mono, ends);
    F64 d; for (size_t k = 0; k < ra.size(); ++k) d.push_back (ma[k] - ra[k]);
    return arrCache[(size_t) t] = median1 (d);
}
double ToneEngine::proTarget (int t) const
{
    if (t == S.refIdx) return 0;
    const auto k = houseKeyOf (t);
    if (k != "oh" && k != "room") return NAN;
    const auto r = libRef (t);
    return r.ok && std::isfinite (r.arr) ? r.arr * S.sr / 1000 : NAN;
}
double ToneEngine::proLag (int t) const
{
    const double a = arrOf (t), T = proTarget (t);
    return std::isfinite (a) && std::isfinite (T) ? a - T : NAN;
}

bool trackFxOn (const Session& S, int t)
{
    if (S.mode == "kit") return false;
    if (S.tone.repair || refLive (S) || refEnvOn (S)) return true;
    static const std::vector<std::optional<MixRender::Scan>> none;
    ToneEngine e (S, none);
    return toneAny (e.moves (t)) || shapeAny (e.shapeMoves (t));
}
bool busLive (const Session& S)
{
    // (Song mode uses the mixer too)
    if (auto* a = S.busIns.getArray()) for (auto& s : *a) if (insLive (s)) return true;
    return false;
}
static double sendOf (const SessionTrack& tr) { return tr.strip.send; }
static double verbSendOf (const SessionTrack& tr, int k) { return k == 0 ? tr.strip.r1 : tr.strip.r2; }
static double busVerbSendOf (const Session& S, int k) { return k == 0 ? S.busR1 : S.busR2; }
bool refEnvLive (const Session& S) { return refEnvOn (S); }
bool verbLive (const Session& S, int k)
{
    // (Song mode uses the mixer too)
    if (busVerbSendOf (S, k) > -59.5) return true;
    for (auto& tr : S.tracks) if (verbSendOf (tr, k) > -59.5) return true;
    return false;
}
juce::var verbParams (const VerbUnit& v)
{
    // the four types on the measured models: Room = Lexicon Room A, Plate = EMT plate B, Hall = Lexicon Small Hall A, Chamber = Lexicon Chamber
    auto* o = new juce::DynamicObject();
    const auto& t = v.type;
    o->setProperty ("model", t == "plate" ? "emt" : "lex");
    o->setProperty ("prog", t == "plate" ? "B" : t == "hall" ? "Small Hall A" : t == "chamber" ? "Chamber" : "Room A");
    o->setProperty ("decay", clampD (v.decay, 0.25, 3));
    o->setProperty ("pre", clampD (v.pre, 0, 250));
    o->setProperty ("early", clampD (v.size, 0, 2));
    o->setProperty ("era", "Now");
    // tone: dark pulls the top down to 3 kHz, bright opens it to 20 kHz and lifts the low cut to 300 Hz
    const double tn = clampD (v.tone, -1, 1);
    o->setProperty ("lpf", tn < 0 ? 14000 * std::pow (3000.0 / 14000, -tn) : 14000 + 6000 * tn);
    o->setProperty ("hpf", tn > 0 ? 100 * std::pow (3.0, tn) : 100);
    o->setProperty ("width", clampD (v.width, 0, 2));
    o->setProperty ("mix", 1.0);
    o->setProperty ("out", 0.0);
    return juce::var (o);
}
bool roomLive (const Session& S)
{
    if (S.mode == "kit" || ! S.room.on) return false;
    for (auto& tr : S.tracks) if (sendOf (tr) > -59.5) return true;
    return false;
}
static double roomLin (const Session& S) { return roomLive (S) ? std::pow (10.0, S.room.ret / 20) : 0; }

//==============================================================================
static F32 tameSens (const Session& S, const std::string& drum)
{
    const double sr = S.sr;
    const size_t N = sr > 64000 ? 2048 : 1024, B = N / 2 + 1;
    const auto it = TAME_SENS.find (drum);
    const auto abc = it != TAME_SENS.end() ? it->second : TAME_SENS.at ("snare");
    const double a = abc[0], b = abc[1], c = abc[2];
    F32 s (B);
    for (size_t k = 0; k < B; ++k)
    {
        const double f = (double) k * sr / (double) N;
        s[k] = (float) (f < a ? 0 : f < b ? 0.6 * (f - a) / (b - a) : f < c ? 0.6 + 0.4 * (f - b) / (c - b) : f > 14000 ? std::max (0.5, 1 - (f - 14000) / 12000) : 1);
    }
    return s;
}

static void runTame (const Session& S, const ToneEngine& E, MixRender& M, const std::vector<Chs*>& rawPtr)
{
    const double sr = S.sr;
    const int nT = (int) S.tracks.size();
    const size_t K = S.onsets.size();
    auto raw = [&] (int t) -> const Chs& { return *rawPtr[(size_t) t]; };
    long Lmax = 0; for (int t = 0; t < nT; ++t) Lmax = std::max (Lmax, (long) raw (t)[0].size());
    F64 lag; std::vector<std::vector<long>> mons;
    for (int t = 0; t < nT; ++t)
    {
        lag.push_back (jsRound (lagOf (S, t)));
        const auto& x = M.dn[(size_t) t][0];
        std::vector<long> o; for (auto v : S.onsets) o.push_back (std::max (0L, v + (long) lag.back()));
        mons.push_back (fxOnsets (x, sr, o, (long) x.size()));
    }
    F64 depthT; for (int t = 0; t < nT; ++t) depthT.push_back (E.moves (t).tame);
    auto d = E.libDrum(); if (d.empty()) d = "snare";
    TameOptions base;
    base.lag = lag; base.depth = *std::max_element (depthT.begin(), depthT.end()); base.depthT = depthT; base.sharp = 0.5;
    const auto sp = TAME_SPARE.find (d); base.spareMs = sp != TAME_SPARE.end() ? sp->second : 4;
    base.sens = tameSens (S, d); base.win = (long) jsRound (S.lev.winMs * sr / 1000); base.noGain = true;

    const int nW = std::clamp (S.tameWorkers, 1, 6);
    const size_t per = (size_t) std::ceil ((double) K / nW);
    struct Part { TameResult r; size_t k0, k1; long S0; };
    std::vector<Part> parts;
    std::vector<std::pair<std::vector<Chs>, TameOptions>> jobs;
    for (size_t k0 = 0; k0 < K; k0 += per)
    {
        const size_t k1 = std::min (K, k0 + per);
        long m0 = LONG_MAX; for (auto& m : mons) m0 = std::min (m0, m[k0]);
        const long S0 = std::max (0L, m0 - (long) jsRound (0.03 * sr));
        long S1 = Lmax;
        if (k1 < K) { long m1 = LONG_MAX; for (auto& m : mons) m1 = std::min (m1, m[k1]); S1 = std::max (S0 + 1, m1 - (long) jsRound (0.002 * sr)); }
        std::vector<Chs> src;
        for (int t = 0; t < nT; ++t)
        {
            Chs c;
            for (auto& x : raw (t)) { const long e = std::min (S1, (long) x.size()); c.emplace_back (x.begin() + std::min (S0, e), x.begin() + e); }
            src.push_back (std::move (c));
        }
        TameOptions o = base;
        for (size_t k = k0; k < k1; ++k) o.ons.push_back (S.onsets[k] - S0);
        const long pre = (long) jsRound (std::max (1.0, S.det.prerollMs) * sr / 1000);
        o.bnd = { 0 };
        for (size_t k = 1; k < o.ons.size(); ++k) o.bnd.push_back (std::max (o.bnd[k - 1], o.ons[k] - pre));
        o.bnd.push_back (S1 - S0);
        for (auto& m : mons) { std::vector<long> q; for (size_t k = k0; k < k1; ++k) q.push_back (m[k] - S0); o.mons.push_back (q); }
        parts.push_back ({ {}, k0, k1, S0 });
        jobs.push_back ({ std::move (src), std::move (o) });
    }
    parallelFor (parts.size(), [&] (size_t j) { parts[j].r = tameMulti (jobs[j].first, sr, jobs[j].second); });
    std::vector<Chs> out; for (int t = 0; t < nT; ++t) out.push_back (raw (t));
    F64 gk (K, 1.0);
    const long pad = (long) jsRound (0.001 * sr);
    for (auto& p : parts)
    {
        for (size_t j = 0; j < p.r.gains.size(); ++j) gk[p.k0 + j] = p.r.gains[j];
        for (int t = 0; t < nT; ++t)
        {
            const long from = std::max (0L, mons[(size_t) t][p.k0] - pad - p.S0);
            for (size_t c = 0; c < out[(size_t) t].size(); ++c)
            {
                const auto& seg = p.r.tracks[(size_t) t][c];
                auto& y = out[(size_t) t][c];
                for (long q = from; q < (long) seg.size(); ++q) y[(size_t) (p.S0 + q)] = seg[(size_t) q];
            }
        }
    }
    std::vector<Chs> on; std::vector<std::vector<long>> monsOn; std::vector<int> idx;
    for (int t = 0; t < nT; ++t) if (depthT[(size_t) t] > 0) { on.push_back (std::move (out[(size_t) t])); monsOn.push_back (mons[(size_t) t]); idx.push_back (t); }
    tameGain (on, monsOn, gk, sr);
    for (size_t j = 0; j < idx.size(); ++j) M.tone[(size_t) idx[j]] = std::move (on[j]);
}

namespace
{
std::string cleanKeyOf (const Session& S)
{
    std::ostringstream k;
    k.precision (17);
    k << S.mode << '|' << S.dnOn << '|' << S.dnDb << '|' << S.sr << '|';
    for (auto v : S.onsets) k << v << ',';
    for (auto& tr : S.tracks)
    {
        const Chs& c = tr.ch;
        k << '|' << (const void*) &c << ':' << c.size() << ':' << (c.empty() ? 0 : c[0].size()) << ':' << tr.lrLock;
        if (tr.lr) k << ':' << tr.lr->lag << ':' << tr.lr->pol << ':' << tr.lr->kept;
    }
    return k.str();
}
std::string scanKeyOf (const Session& S, const std::string& cleanKey)
{
    std::ostringstream k;
    k.precision (17);
    k << cleanKey << "|scan|" << S.drum << '|' << S.maxLen() << '|' << (S.tinfo ? S.tinfo->medF : -1.0);
    return k.str();
}
/** What the tone pass hears in each mic's cleaned audio (per hit: bands and envelope; per mic: its resonances). */
std::vector<std::optional<MixRender::Scan>> scanTracks (const Session& S, const std::vector<Chs>& dn, bool cym)
{
    const int nT = (int) S.tracks.size();
    const double sr = S.sr;
    const auto ends = regionEnds (S);
    std::vector<std::optional<MixRender::Scan>> out ((size_t) nT);
    parallelFor ((size_t) nT, [&] (size_t ti)
    {
        const int t = (int) ti;
        const auto x = fxMono (dn[(size_t) t]);
        ResScanOptions o;
        if (cym) { o.lo = 900; o.hi = 15000; o.lenMs = 900; o.skipMs = 20; o.consistency = 0.6; }
        else
        {
            o.lo = S.drum == "kick" ? 180 : 250; o.hi = 14000; o.lenMs = 700; o.skipMs = 12; o.ring = true; o.autoProtect = true;
            o.f0hi = S.drum == "kick" ? 120 : 400;
            if (S.tinfo) o.protect = { S.tinfo->medF, 1.59 * S.tinfo->medF };
        }
        MixRender::Scan sc;
        for (size_t k = 0; k < S.onsets.size(); ++k)
        {
            sc.bands.push_back (bandSpec (x, S.onsets[k], sr, (double) (ends[k] - S.onsets[k] + 64)));
            sc.env.push_back (envStats (x, S.onsets[k], sr, (double) (ends[k] - S.onsets[k])));
        }
        sc.res = resScan (x, S.onsets, ends, sr, o);
        out[(size_t) t] = std::move (sc);
    });
    return out;
}
} // namespace

bool HitPreviewer::possible (const Session& S, const MixRender& base)
{
    const int nT = (int) S.tracks.size();
    if (S.mode == "kit" || S.onsets.empty() || base.dn.size() != (size_t) nT || base.cleanKey != cleanKeyOf (S)) return false;
    if (S.tone.repair || busLive (S) || roomLive (S)) return false;
    for (int t = 0; t < nT; ++t) if (insActive (S, t)) return false;
    if (fxOn (S))
    {
        static const std::vector<std::optional<MixRender::Scan>> none;
        ToneEngine E (S, none);
        for (int t = 0; t < nT; ++t) if (E.moves (t).tame > 0) return false;   // linked across mics and across hits: not a per-hit job
    }
    return true;
}

std::string HitPreviewer::listeningKey (const Session& S, const MixRender& base) { return scanKeyOf (S, base.cleanKey); }

std::shared_ptr<const Listening> HitPreviewer::listen (const Session& S, const MixRender& base)
{
    auto L = std::make_shared<Listening>();
    L->key = listeningKey (S, base);
    static const std::vector<std::optional<MixRender::Scan>> none;
    ToneEngine probe (S, none);
    L->scans = scanTracks (S, base.dn, probe.isCymbal());
    return L;
}

HitPreviewer::HitPreviewer (const Session& s, const MixRender& b, std::shared_ptr<const Listening> known) : S (s), base (b)
{
    if (! possible (S, base)) return;
    const int nT = (int) S.tracks.size();
    on.assign ((size_t) nT, 0);
    plans.resize ((size_t) nT);
    if (fxOn (S))
    {
        bool kept = base.scanKey == scanKeyOf (S, base.cleanKey) && base.scan.size() == (size_t) nT;
        if (kept) for (auto& sc : base.scan) if (! sc) kept = false;
        if (kept) scans = base.scan;
        else if (known && known->key == listeningKey (S, base) && known->scans.size() == (size_t) nT) scans = known->scans;
        else { ToneEngine probe (S, scans); scans = scanTracks (S, base.dn, probe.isCymbal()); }
        ToneEngine E (S, scans);
        for (int t = 0; t < nT; ++t) if (trackFxOn (S, t)) { plans[(size_t) t] = E.plan (t); on[(size_t) t] = 1; }
    }
    ok = true;
}

Chs HitPreviewer::render (int t, long from, long to) const
{
    const auto& src = base.dn[(size_t) t];
    const long L = src.empty() ? 0 : (long) src[0].size();
    from = std::clamp (from, 0L, L); to = std::clamp (to, from, L);
    Chs out;
    if (! on[(size_t) t])
    {
        for (auto& x : src) out.emplace_back (x.begin() + from, x.begin() + to);
        return out;
    }
    // the hits whose stretches reach into [from, to): a stretch starts within a few ms before its onset to 50 ms after
    const auto& plan = plans[(size_t) t];
    const auto& ons = plan.ons;
    const int nH = (int) ons.size();
    const long late = (long) std::ceil (0.06 * S.sr), early = (long) std::ceil (0.008 * S.sr);
    int h0 = 0; for (int h = 0; h < nH; ++h) if (ons[(size_t) h] + late <= from) h0 = h;
    int h1 = nH; for (int h = nH - 1; h > h0; --h) if (ons[(size_t) h] - early >= to) h1 = h;
    const auto r = fxRenderRange (src, S.sr, plan, h0, h1);
    for (auto& y : r.audio)
    {
        F32 o ((size_t) (to - from), 0.0f);
        for (long n = std::max (from, r.start), e = std::min (to, r.start + (long) y.size()); n < e; ++n) o[(size_t) (n - from)] = y[(size_t) (n - r.start)];
        out.push_back (std::move (o));
    }
    return out;
}


MixRender renderMix (const Session& S, const MixRender* prev)
{
    MixRender M;
    const int nT = (int) S.tracks.size();
    const double sr = S.sr;
    const bool tlog = std::getenv ("LD_TIMING") != nullptr;
    double tl = juce::Time::getMillisecondCounterHiRes();
    auto lap = [&] (const char* w) { if (! tlog) return; const double n = juce::Time::getMillisecondCounterHiRes(); fprintf (stderr, "[timing]   %s %.0f ms\n", w, n - tl); tl = n; };
    M.scan.resize ((size_t) nT);
    // clean-up (and what it was made from: turning a tone knob doesn't run it again)
    M.cleanKey = cleanKeyOf (S);
    const bool cleanKept = prev && S.mode != "kit" && prev->cleanKey == M.cleanKey && prev->dn.size() == S.tracks.size();
    if (cleanKept) M.dn = prev->dn;
    else for (auto& tr : S.tracks) M.dn.push_back (baseCh (tr));
    // Song mode: tightening and swing, every mic cut at the same places so phase is untouched
    if (S.mode == "kit" && S.song.an.done)
    {
        if (S.song.set.consistency > 0)
        {
            std::vector<std::string> names; for (auto& tr : S.tracks) names.push_back (tr.mic + " " + tr.name);
            applyConsistency (M.dn, names, S.song.an, S.song.set.consistency);
        }
        const auto E = planEdits (S.song.an, S.song.set);
        if (! E.empty()) parallelFor ((size_t) nT, [&] (size_t t) { M.dn[t] = applyEdits (M.dn[t], E, sr); });
    }
    const bool kit = S.mode == "kit";
    const auto& dnOns = kit ? S.kitOn : S.onsets;
    if (! cleanKept && S.dnOn && ! dnOns.empty())
    {
        std::vector<F32*> chans; for (auto& c : M.dn) for (auto& x : c) chans.push_back (&x);
        parallelFor (chans.size(), [&] (size_t j) { auto& x = *chans[j]; const auto p = noisePrint (x, sr, dnOns); x = denoise (x, sr, p ? &*p : nullptr, S.dnDb); });
    }
    lap ("clean-up");
    if (kit)
    {
        // whole kit: close mics can be gated against bleed from the rest of the kit; nothing else is touched
        // (a song is long: the clean-up stage moves on rather than staying as a copy; nothing reads it afterwards)
        M.tone = std::move (M.dn); M.dn.clear();
        if (S.kitBleed != 0)
            parallelFor ((size_t) nT, [&] (size_t t) { if (isCloseKind (S.tracks[t])) M.tone[t] = bleedGate (M.tone[t], sr, S.kitBleed); });
        // Song mode: the mixer still applies (inserts, the bus, the reverbs); Tone and Shape work per sampled hit and
        // don't apply to a performance
    }
    // tone and shape, then tame (one-shot sessions)
    if (! kit) M.tone = M.dn;
    if (! kit)
    {
        ToneEngine E (S, M.scan);
        if (! S.onsets.empty() && S.mode != "kit" && fxOn (S))
        {
            const auto ends = regionEnds (S);
            const bool cym = E.isCymbal();
            M.scanKey = scanKeyOf (S, M.cleanKey);
            bool scanKept = prev && prev->scanKey == M.scanKey && prev->scan.size() == (size_t) nT;
            if (scanKept) for (auto& sc : prev->scan) if (! sc) scanKept = false;
            if (scanKept) M.scan = prev->scan;
            else M.scan = scanTracks (S, M.dn, cym);
            lap ("scan");
            std::vector<FxPlan> plans ((size_t) nT);
            for (int t = 0; t < nT; ++t) if (trackFxOn (S, t)) plans[(size_t) t] = E.plan (t);
            parallelFor ((size_t) nT, [&] (size_t t) { if (trackFxOn (S, (int) t)) M.tone[t] = fxRenderMulti (M.dn[t], sr, plans[t]); });
            lap ("tone+shape");
            bool tame = false;
            for (int t = 0; t < nT; ++t) if (E.moves (t).tame > 0) tame = true;
            if (tame)
            {
                std::vector<Chs> rawCopy = M.tone;   // chTRaw where rendered, else the cleaned audio
                std::vector<Chs*> ptr; for (auto& c : rawCopy) ptr.push_back (&c);
                runTame (S, E, M, ptr);
            }
        }
    }
    lap ("tame");
    // channel inserts
    M.ins = M.tone;
    std::mutex grLock;
    parallelFor ((size_t) nT, [&] (size_t ti)
    {
        const int t = (int) ti;
        if (! insActive (S, t)) return;
        const auto& st = S.tracks[(size_t) t].strip;
        InsertOptions o;
        o.trim = st.trim;
        if (auto* a = st.inserts.getArray())
            for (auto& s : *a)
                if (insLive (s) && s["type"].toString() == "gate")
                {
                    const auto k = s["p"]["keyT"];
                    if (! k.isVoid() && ! k.isUndefined() && (int) k >= 0 && (int) k < nT) o.keys[(int) k] = M.tone[(size_t) (int) k];
                }
        for (auto v : S.onsets) o.ons.push_back (std::max (0L, (long) jsRound ((double) v + lagOf (S, t))));
        std::map<int, F32> grs; o.grCurves = &grs;
        M.ins[(size_t) t] = insRender (M.tone[(size_t) t], sr, st.inserts, o);
        const std::lock_guard<std::mutex> lk (grLock);
        for (auto& [k, c] : grs) M.gr[std::to_string (t) + ":" + std::to_string (k)] = std::move (c);
    });
    lap ("inserts");
    // memory: tone isn't needed past the inserts, and ins only feeds the shared room; move rather than copy
    std::vector<Chs>().swap (M.tone);
    if (roomLive (S)) M.bus = M.ins; else M.bus = std::move (M.ins);
    // the bus: its EQs on every channel; its compressor driven by the whole sum, one gain curve on every channel
    if (busLive (S))
    {
        auto& cur = M.bus;
        int slot = -1;
        for (auto& s : *S.busIns.getArray())
        {
            ++slot;
            if (! insLive (s)) continue;
            if (s["type"].toString() == "comp")
            {
                size_t n = 0; for (auto& c : cur) n = std::max (n, c[0].size());
                F32 det (n, 0.0f);
                const auto o = compModel (s["p"]);
                const double gin = std::pow (10.0, o.inDb / 20);
                std::vector<long> lagT; for (int t = 0; t < nT; ++t) lagT.push_back ((long) jsRound (lagOf (S, t)));
                for (int t = 0; t < nT; ++t)
                {
                    const auto& tr = S.tracks[(size_t) t]; const auto& c = cur[(size_t) t];
                    const double g = tr.mute ? 0 : polR (S, t) * std::pow (10.0, tr.fader / 20) / (double) c.size() * gin;
                    const long l = lagT[(size_t) t];
                    if (g != 0)
                        for (auto& x : c)
                            for (size_t q = 0; q < n; ++q) { const long m2 = (long) q + l; if (m2 >= 0 && m2 < (long) x.size()) det[q] = (float) ((double) det[q] + (double) x[(size_t) m2] * g); }
                }
                const auto gdb = compGainCurve (det, sr, o, std::max (orr (s["p"], "scHpf", 0), o.hp));
                M.gr["bus:" + std::to_string (slot)] = grBlocks (gdb, sr);
                const double mix = clampD (nn (s["p"], "mix", 1), 0, 1), out = std::pow (10.0, o.out / 20);
                F32 G (n);
                for (size_t q = 0; q < n; ++q) G[q] = (float) (gin * ((1 - mix) + mix * std::pow (10.0, (double) gdb[q] / 20)) * out);
                for (int t = 0; t < nT; ++t)
                    for (auto& x : cur[(size_t) t])
                    {
                        const long l = lagT[(size_t) t];
                        for (size_t q = 0; q < x.size(); ++q) { const long k = std::clamp ((long) q - l, 0L, (long) n - 1); x[q] = (float) ((double) x[q] * (double) G[(size_t) k]); }
                    }
            }
            else
            {
                juce::Array<juce::var> one; one.add (s);
                const juce::var ins (one);
                parallelFor (cur.size(), [&] (size_t t) { cur[t] = insRender (cur[t], sr, ins, {}); });
            }
        }
    }
    // the shared room: every send summed as it plays, one reverb, a stereo return on the lead's timeline
    if (roomLive (S))
    {
        size_t n = 0; for (auto& c : M.ins) n = std::max (n, c[0].size());
        F32 send (n, 0.0f);
        for (int t = 0; t < nT; ++t)
        {
            const double db = sendOf (S.tracks[(size_t) t]);
            if (db <= -59.5) continue;
            const auto& c = M.ins[(size_t) t];
            const double g = polR (S, t) * std::pow (10.0, db / 20) / (double) c.size();
            const long l = (long) jsRound (lagOf (S, t));
            for (auto& x : c)
                for (size_t q = 0; q < n; ++q) { const long m = (long) q + l; if (m >= 0 && m < (long) x.size()) send[q] = (float) ((double) send[q] + (double) x[(size_t) m] * g); }
        }
        M.room = roomRender (send, sr, S.room.p);
    }
    // phase EQ. Where a track has none, its bus stage moves into proc (bus left empty, see busOf) instead of copying
    M.proc.assign ((size_t) nT, {});
    parallelFor ((size_t) nT, [&] (size_t t)
    {
        const auto& tr = S.tracks[t];
        if (tr.ap.empty()) { M.proc[t] = std::move (M.bus[t]); M.bus[t].clear(); return; }
        M.proc[t] = M.bus[t];
        for (auto& x : M.proc[t]) x = applyAllpass (x, sr, tr.ap);
    });
    // the two reverb units: every channel's R1/R2 send (post-fader, panned, mute/solo respected) and the drum bus send
    // (the channel sum before any return, so nothing feeds back), each into a 100 % wet reverb, stereo return
    for (int k = 0; k < 2; ++k)
    {
        if (! verbLive (S, k)) continue;
        size_t n = 0; for (auto& c : M.proc) n = std::max (n, c[0].size());
        F32 sL (n, 0.0f), sR (n, 0.0f), bL (n, 0.0f), bR (n, 0.0f);
        const bool anySolo = std::any_of (S.tracks.begin(), S.tracks.end(), [] (auto& t) { return t.solo; });
        const double busSend = busVerbSendOf (S, k);
        for (int t = 0; t < nT; ++t)
        {
            const auto& tr = S.tracks[(size_t) t];
            const bool audible = anySolo ? tr.solo : ! tr.mute;
            if (! audible || tr.fader <= -60) continue;
            const double fad = std::pow (10.0, tr.fader / 20), db = verbSendOf (tr, k);
            const double gs = db > -59.5 ? fad * std::pow (10.0, db / 20) : 0, gb = busSend > -59.5 ? fad : 0;
            if (gs == 0 && gb == 0) continue;
            const auto& c = M.proc[(size_t) t];
            const bool st = c.size() > 1;
            const auto P = trackPanGains (tr.strip.pan, tr.panL, tr.panR, st);
            const long l = (long) jsRound (lagOf (S, t));
            const double pol = polR (S, t);
            for (size_t q = 0; q < n; ++q)
            {
                const long m = (long) q + l; if (m < 0 || m >= (long) c[0].size()) continue;
                const double x = c[0][(size_t) m] * pol, y = st ? c[1][(size_t) m] * pol : 0;
                const double Lq = x * P[0] + (st ? y * P[1] : 0), Rq = x * P[2] + (st ? y * P[3] : 0);
                if (gs != 0) { sL[q] = (float) (sL[q] + Lq * gs); sR[q] = (float) (sR[q] + Rq * gs); }
                if (gb != 0) { bL[q] = (float) (bL[q] + Lq * gb); bR[q] = (float) (bR[q] + Rq * gb); }
            }
        }
        if (busSend > -59.5)
        {
            const double g = std::pow (10.0, busSend / 20);
            for (size_t q = 0; q < n; ++q) { sL[q] = (float) (sL[q] + bL[q] * g); sR[q] = (float) (sR[q] + bR[q] * g); }
        }
        M.rev[(size_t) k] = verbWet ({ sL, sR }, S.sr, verbParams (S.verbs[(size_t) k]));
    }
    // the in-between stages aren't read after the render: free them (each is a full copy of every mic)
    std::vector<Chs>().swap (M.tone); std::vector<Chs>().swap (M.ins);
    lap ("bus, phase EQ, reverbs");
    return M;
}

std::pair<ToneMoves, ShapeMoves> standardMoves (const Session& S0, const MixRender& M, int sel)
{
    // the target: the pro libraries' curve and envelope at Hitline's standard amounts (tone 60 %, envelope 50 %)
    Session S = S0;
    S.tone.match = 0.6; S.tone.bright = S.tone.crack = 0; S.tone.refAmt = 0; S.shape = {};
    for (auto& tr : S.tracks) { tr.strip.tone = {}; tr.strip.shape = {}; }
    ToneEngine E (S, M.scan);
    const bool cym = E.isCymbal() || ! (S0.drum == "kick" || S0.drum == "snare" || S0.drum == "tom");
    std::vector<int> ts;
    if (sel >= 0) ts.push_back (sel); else for (int t = 0; t < (int) S.tracks.size(); ++t) ts.push_back (t);
    const int v = std::max (0, S.det.V - 1);
    // Tone and Crack: the pair whose curve lands closest to the target's, weighted toward where drums live
    const auto& B = bands3();
    F64 target (30, 0.0); int n = 0;
    for (int t : ts)
    {
        if (t >= (int) M.scan.size() || ! M.scan[(size_t) t]) continue;
        const auto mb = E.matchBands (t, v);
        for (size_t j = 0; j < 30; ++j) target[j] += mb[j];
        ++n;
    }
    ToneMoves T; T.smooth = cym ? 0.5 : 0.35; T.tame = cym ? 0.4 : 0.25;
    ShapeMoves H;
    if (n == 0) return { T, H };
    for (auto& x : target) x /= n;
    const int t0 = ts.front();
    double best = 1e18;
    S.tone.match = 0;
    for (int bi = -20; bi <= 20; ++bi)
        for (int ci = -20; ci <= 20; ++ci)
        {
            S.tone.bright = bi / 20.0; S.tone.crack = ci / 20.0;
            ToneEngine Eq (S, M.scan);
            double err = 0;
            for (size_t j = 0; j < 30; ++j)
            {
                if (B[j] < 100 || B[j] > 12000) continue;
                const double d = Eq.toneAt (t0, nullptr, B[j]) - target[j];
                err += d * d;
            }
            err += 0.5 * (bi * bi + ci * ci) / 400.0;   // a little pull toward small moves
            if (err < best) { best = err; T.bright = bi / 20.0; T.crack = ci / 20.0; }
        }
    // Attack, Decay, Sustain, Release from the envelope target (body at decay, sustain, tail at release)
    std::array<double, 3> d { 0, 0, 0 }; int m = 0;
    for (int t : ts) { const auto e = E.proEnvDelta (t); if (e[0] != 0 || e[1] != 0 || e[2] != 0) { for (int k = 0; k < 3; ++k) d[(size_t) k] += e[(size_t) k]; ++m; } }
    if (m) for (auto& x : d) x = x / m * 0.5;
    H.dec = jclamp (d[0] / 6, -1, 1); H.sus = jclamp (d[1] / 8, -1, 1);
    const double R = d[2] - d[1];
    H.rel = jclamp (R > 0 ? R / 6 : R / 12, -1, 1);
    H.atk = 0;
    auto r2 = [] (double x) { return std::round (x * 100) / 100; };
    T.bright = r2 (T.bright); T.crack = r2 (T.crack); H.dec = r2 (H.dec); H.sus = r2 (H.sus); H.rel = r2 (H.rel);
    return { T, H };
}

LevelResult measureLevels (const Session& S, const MixRender& M)
{
    std::vector<LevelTrack> lt;
    static const std::vector<std::optional<MixRender::Scan>> none;
    ToneEngine E (S, none);
    for (int t = 0; t < (int) S.tracks.size(); ++t)
    {
        auto tr = levelTrack (S, t);
        tr.ch = M.bus[(size_t) t].empty() ? M.proc[(size_t) t] : M.bus[(size_t) t];   // bus is empty where proc took it over
        tr.raw = S.tracks[(size_t) t].ch;
        if (t != S.refIdx && ! S.onsets.empty()) tr.arrivalMs = E.arrOf (t) / S.sr * 1000;
        lt.push_back (std::move (tr));
    }
    ExportLevelSettings ex; ex.maxLenS = S.exp.maxLenS; ex.trim = S.exp.trim; ex.ceilDb = S.exp.ceilDb;
    return computeLevels (lt, S.refIdx, S.onsets, S.sr, S.det, S.lev, ex, S.drum, S.tinfo ? S.tinfo->ratios : F64 {}, S.rejects);
}

//==============================================================================
static double ratioOf (const Session& S, size_t i) { return S.tinfo && i < S.tinfo->ratios.size() ? S.tinfo->ratios[i] : 1.0; }
/** Keep the stick attack at its real pitch on shifts of a semitone or more: 3 ms as recorded, then a 2 ms glide. */
static AttackKeep attackKeep (const Session& S, double ratio, double step0, double outRate)
{
    if (! S.keepAttack || std::abs (12 * std::log2 (ratio)) < 1) return {};
    return { (long) jsRound (0.003 * outRate), (long) jsRound (0.002 * outRate), step0 };
}

Chs renderHitMic (const Session& S, const MixRender& M, int t, size_t i, const Region& r, double srOut, double gain)
{
    const double ratio = ratioOf (S, i), step = ratio * S.sr / srOut, on = (double) S.onsets[i], lag = lagOf (S, t);
    const int pol = polR (S, t);
    const auto keep = attackKeep (S, ratio, S.sr / srOut, srOut);
    const long preOut = (long) jsRound ((on - r.start) / (keep.keepN ? keep.step0 : step)), outLen = std::max (1L, (long) jsRound ((double) (r.end - r.start) / step));
    const long lo = r.start + (long) std::floor (lag), hi = r.end + (long) std::ceil (lag);
    Chs out;
    for (auto& x : M.proc[(size_t) t]) out.push_back (renderTuned (x, on, preOut, outLen, lag, pol, gain, step, lo, hi, 0, S.tuneTaps, keep));
    return out;
}

Chs renderRoomHit (const Session& S, const MixRender& M, size_t i, const Region& r, double srOut, double gain)
{
    if (M.room.empty() || ! roomLive (S)) return {};
    const double ratio = ratioOf (S, i), step = ratio * S.sr / srOut, on = (double) S.onsets[i];
    const auto keep = attackKeep (S, ratio, S.sr / srOut, srOut);
    const long preOut = (long) jsRound ((on - r.start) / (keep.keepN ? keep.step0 : step)), outLen = std::max (1L, (long) jsRound ((double) (r.end - r.start) / step));
    Chs out;
    for (auto& x : M.room) out.push_back (renderTuned (x, on, preOut, outLen, 0, 1, gain, step, r.start, r.end, 0, S.tuneTaps, keep));
    return out;
}

Chs renderFull (const Session& S, const MixRender& M, const LevelResult& L, int t, double trimLin, double srOut)
{
    const auto& tr = S.tracks[(size_t) t];
    const double sr = S.sr, k2o = srOut / sr;
    const auto& regions = L.regions;
    const size_t N = regions.size();
    const auto& src = M.proc[(size_t) t];
    std::vector<long> bnd { 0 };
    for (size_t k = 1; k < N; ++k) bnd.push_back (regions[k].start);
    bnd.push_back (tr.len);
    const long total = (long) jsRound ((double) tr.len * k2o);
    const double lag = lagOf (S, t);
    const int pol = polR (S, t);
    F64 gl; for (size_t k = 0; k < N; ++k) gl.push_back (std::pow (10.0, L.gainsDbT[(size_t) t][k] / 20) * trimLin);
    Chs outs (src.size(), F32 ((size_t) total, 0.0f));
    for (size_t k = 0; k < N; ++k)
    {
        const double ratio = ratioOf (S, k), step = ratio / k2o;
        const double on = (double) S.onsets[k];
        const long o0 = (long) jsRound ((double) bnd[k] * k2o), o1 = k == N - 1 ? total : (long) jsRound ((double) bnd[k + 1] * k2o), slot = o1 - o0;
        const auto keep = attackKeep (S, ratio, 1 / k2o, sr * k2o);
        const long preOut = (long) jsRound ((on - (double) bnd[k]) / (keep.keepN ? keep.step0 : step));
        const bool tuned = std::abs (ratio - 1) > 1e-9;
        const long fadeSrc = tuned ? (long) jsRound (0.006 * sr) : 0;
        for (size_t c = 0; c < src.size(); ++c)
        {
            const long lo = bnd[k] + (long) std::floor (lag) - (k == 0 ? 1000000000L : 0L);
            const long hi = bnd[k + 1] + (long) std::ceil (lag) + (k == N - 1 ? 1000000000L : 0L);
            auto y = renderTuned (src[c], on, preOut, slot, lag, pol, gl[k], step, lo, hi, k == N - 1 ? 0 : fadeSrc, S.tuneTaps, keep);
            if (k > 0)
            {
                const long rN = std::min (preOut, (long) jsRound (0.002 * srOut));
                const double from = gl[k - 1] / gl[k];
                for (long n = 0; n < rN; ++n) y[(size_t) n] = (float) ((double) y[(size_t) n] * (from + (1 - from) * ((double) n / rN)));
            }
            if (tuned && ratio < 1 && k < N - 1)
            {
                const long f = std::min (slot, (long) jsRound (0.006 * srOut));
                for (long n = 0; n < f; ++n)
                {
                    const double u = (double) (n + 1) / f, cs = std::cos (u * juce::MathConstants<double>::pi / 2);
                    y[(size_t) (slot - f + n)] = (float) ((double) y[(size_t) (slot - f + n)] * (cs * cs));
                }
            }
            for (long n = 0; n < (long) y.size() && o0 + n < total; ++n) outs[c][(size_t) (o0 + n)] = y[(size_t) n];
        }
    }
    return outs;
}

//==============================================================================
static std::string cleanTok (const std::string& s)
{
    auto r = std::regex_replace (s, std::regex (R"([\\:*?"<>|]+)"), "");
    r = juce::String (r).trim().toStdString();
    return std::regex_replace (r, std::regex (R"(\s+)"), "_");
}
static std::string upper (std::string s) { return juce::String (s).toUpperCase().toStdString(); }
static std::string p2 (int n) { auto s = std::to_string (n); return s.size() < 2 ? "0" + s : s; }

std::string fillTokens (const Session& S, const std::string& tpl, int t, std::optional<size_t> hit)
{
    const auto di = DRUM_NAMES.find (S.drum);
    const auto drumTok = upper (cleanTok (di != DRUM_NAMES.end() ? di->second : S.drum));
    const auto micTok = t < 0 ? std::string ("Room_Return") : cleanTok (! S.tracks[(size_t) t].rename.empty() ? S.tracks[(size_t) t].rename : S.tracks[(size_t) t].mic.empty() ? "Mic" : S.tracks[(size_t) t].mic);
    std::string hitTok;
    if (hit) { hitTok = std::to_string (*hit + 1); while (hitTok.size() < 3) hitTok = "0" + hitTok; }
    auto p = tpl;
    auto rep = [&] (const char* tok, const std::string& v) { p = std::regex_replace (p, std::regex (tok), v); };
    // $ in a value would be read as a back-reference: escape it
    auto esc = [] (std::string v) { std::string o; for (char c : v) { if (c == '$') o += "$$"; else o += c; } return o; };
    rep (R"(\{DRUM\})", esc (drumTok));
    rep (R"(\{ARTIC\})", esc (upper (cleanTok (S.artic))));
    rep (R"(\{MIC\})", esc (micTok));
    rep (R"(\{VEL\})", hit ? p2 (S.velOfHit (*hit) + 1) : "");
    rep (R"(\{RR\})", hit ? p2 ((int) (*hit % (size_t) S.det.RR) + 1) : "");
    rep (R"(\{HIT\})", hitTok);
    p = std::regex_replace (p, std::regex (R"(_+(?=[/.]))"), "");
    p = std::regex_replace (p, std::regex ("_{2,}"), "_");
    // finishPath: trim each folder, drop empty ones, add .wav
    juce::StringArray parts; parts.addTokens (juce::String (p), "/", "");
    juce::StringArray keep; for (auto& s : parts) { const auto q = s.trim(); if (q.isNotEmpty()) keep.add (q); }
    auto out = keep.joinIntoString ("/").toStdString();
    if (! std::regex_search (out, std::regex (R"(\.wav$)", std::regex::icase))) out += ".wav";
    return out;
}

std::vector<ExportItem> exportList (const Session& S)
{
    std::vector<ExportItem> list;
    const size_t N = S.onsets.size();
    const int T = (int) S.tracks.size();
    const bool room = roomLive (S) && S.mode != "kit";
    if (S.exp.mode != "full")
        for (size_t i = 0; i < N; ++i)
        {
            for (int t = 0; t < T; ++t) list.push_back ({ "hit", i, t, fillTokens (S, S.exp.hitTemplate, t, i) });
            if (room) list.push_back ({ "room", i, -1, fillTokens (S, S.exp.hitTemplate, -1, i) });
        }
    if (S.exp.mode != "hits")
        for (int t = 0; t < T; ++t) list.push_back ({ "full", 0, t, fillTokens (S, S.exp.fullTemplate, t, std::nullopt) });
    return list;
}

juce::MemoryBlock encodeWav (const Chs& chs, double sr, int bits)
{
    const size_t nch = chs.size(), n = chs[0].size();
    const bool isF = bits == 32;
    const size_t bps = (size_t) bits / 8, ba = nch * bps, dataLen = n * ba;
    const size_t hdr = 12 + 24 + (isF ? 12 : 0) + 8;
    juce::MemoryBlock mb (hdr + dataLen + (dataLen & 1), true);
    auto* u8 = static_cast<uint8_t*> (mb.getData());
    size_t p = 0;
    auto w4 = [&] (const char* s) { for (int i = 0; i < 4; ++i) u8[p++] = (uint8_t) s[i]; };
    auto u32 = [&] (uint32_t v) { for (int i = 0; i < 4; ++i) u8[p++] = (uint8_t) (v >> (8 * i)); };
    auto u16 = [&] (uint32_t v) { u8[p++] = (uint8_t) v; u8[p++] = (uint8_t) (v >> 8); };
    w4 ("RIFF"); u32 ((uint32_t) (mb.getSize() - 8)); w4 ("WAVE");
    w4 ("fmt "); u32 (16); u16 (isF ? 3 : 1); u16 ((uint32_t) nch); u32 ((uint32_t) sr); u32 ((uint32_t) (sr * (double) ba)); u16 ((uint32_t) ba); u16 ((uint32_t) bits);
    if (isF) { w4 ("fact"); u32 (4); u32 ((uint32_t) n); }
    w4 ("data"); u32 ((uint32_t) dataLen);
    if (isF)
    {
        for (size_t i = 0; i < n; ++i)
            for (size_t c = 0; c < nch; ++c) { float f = chs[c][i]; std::memcpy (u8 + p, &f, 4); p += 4; }
    }
    else
    {
        for (size_t i = 0; i < n; ++i)
            for (size_t c = 0; c < nch; ++c)
            {
                double v = jsRound ((double) chs[c][i] * 8388607);
                if (v > 8388607) v = 8388607; else if (v < -8388608) v = -8388608;
                const int32_t q = (int32_t) v;
                u8[p++] = (uint8_t) (q & 255); u8[p++] = (uint8_t) ((q >> 8) & 255); u8[p++] = (uint8_t) ((q >> 16) & 255);
            }
    }
    return mb;
}

void applyFades (Chs& chs, long inN, long outN)
{
    for (auto& x : chs)
    {
        const long n = (long) x.size();
        const long fi = std::min (inN, n);
        for (long i = 0; i < fi; ++i) x[(size_t) i] = (float) ((double) x[(size_t) i] * ((double) i / fi));
        const long fo = std::min (outN, n);
        for (long i = 0; i < fo; ++i)
        {
            const double g = std::cos ((double) (i + 1) / fo * juce::MathConstants<double>::pi / 2);
            x[(size_t) (n - fo + i)] = (float) ((double) x[(size_t) (n - fo + i)] * (g * g));
        }
    }
}

int writeSession (const Session& S, const MixRender& M, const LevelResult& L,
                  const std::function<void (const std::string&, const juce::MemoryBlock&)>& sink)
{
    if (! L.valid) throw std::runtime_error ("Every hit has to be found before export: the levels need the full count.");
    const auto list = exportList (S);
    const double srOut = S.exp.rate == "same" ? S.sr : std::stod (S.exp.rate);
    const double trimLin = std::pow (10.0, L.trimDb / 20);
    const long fin = (long) jsRound (S.exp.fadeInMs * srOut / 1000);
    const auto& regions = L.regions;
    const bool fade = S.exp.tailMode == "fade" && S.exp.mode != "full";
    const bool natural = S.exp.tailMode == "natural" && S.exp.mode != "full";
    const long fout = (long) jsRound ((fade ? 30 : S.exp.fadeOutMs) * srOut / 1000);
    auto gainOf = [&] (int t, size_t i) { return std::pow (10.0, L.gainsDbT[(size_t) t][i] / 20) * trimLin; };
    const int T = (int) S.tracks.size();
    std::vector<long> cuts;
    if (fade)
    {
        const double thr = std::pow (10.0, S.exp.tailDb / 20);
        const long hold = (long) jsRound (0.02 * srOut);
        for (size_t i = 0; i < regions.size(); ++i)
        {
            long last = 0;
            for (int t = 0; t < T; ++t)
                for (auto& x : renderHitMic (S, M, t, i, regions[i], srOut, gainOf (t, i)))
                    for (long n = (long) x.size() - 1; n > last; --n) if (std::abs (x[(size_t) n]) >= thr) { last = n; break; }
            cuts.push_back (last + hold);
        }
    }
    std::map<size_t, TailPlan> plans;
    auto planFor = [&] (size_t i) -> const TailPlan&
    {
        auto it = plans.find (i);
        if (it != plans.end()) return it->second;
        Chs all;
        for (int t = 0; t < T; ++t) for (auto& x : renderHitMic (S, M, t, i, regions[i], srOut, gainOf (t, i))) all.push_back (std::move (x));
        return plans[i] = tailPlan (all, srOut, 0, S.exp.tailFloor);
    };
    int done = 0;
    for (auto& it : list)
    {
        Chs chs;
        if (it.kind == "hit" || it.kind == "room")
        {
            if (it.kind == "hit") chs = renderHitMic (S, M, it.t, it.i, regions[it.i], srOut, gainOf (it.t, it.i));
            else
            {
                chs = renderRoomHit (S, M, it.i, regions[it.i], srOut, gainOf (S.refIdx, it.i) * roomLin (S));
                if (chs.empty()) chs = { F32 (1, 0.0f), F32 (1, 0.0f) };
            }
            if (natural)
            {
                chs = applyTail (chs, it.kind == "hit" ? planFor (it.i) : tailPlan (chs, srOut, 0, S.exp.tailFloor), srOut);
                applyFades (chs, fin, 0);
            }
            else
            {
                if (it.i < cuts.size() && cuts[it.i] != 0 && cuts[it.i] < (long) chs[0].size())
                    for (auto& x : chs) x.resize ((size_t) cuts[it.i]);
                applyFades (chs, fin, fout);
            }
        }
        else chs = renderFull (S, M, L, it.t, trimLin, srOut);
        sink (it.path, encodeWav (chs, srOut, S.exp.bits));
        ++done;
    }
    return done;
}

int writeKit (const Session& S, const MixRender& M, const std::function<void (const std::string&, const juce::MemoryBlock&)>& sink)
{
    const double srOut = S.exp.rate == "same" ? S.sr : std::stod (S.exp.rate), k = srOut / S.sr;
    int n = 0;
    for (int t = 0; t < (int) S.tracks.size(); ++t)
    {
        const auto& tr = S.tracks[(size_t) t];
        const auto path = fillTokens (S, S.exp.kitTemplate.empty() ? "{ARTIC}/{MIC}.wav" : S.exp.kitTemplate, t, std::nullopt);
        Chs chs;
        for (auto& x : M.proc[(size_t) t])
            chs.push_back (std::abs (k - 1) < 1e-9 ? renderSeg (x, 0, tr.len, lagOf (S, t), polR (S, t), 1, true)
                                                   : renderTuned (x, 0, 0, (long) jsRound ((double) tr.len * k), lagOf (S, t), polR (S, t), 1, 1 / k, 0, (long) x.size(), 0));
        sink (path, encodeWav (chs, srOut, S.exp.bits));
        ++n;
    }
    // the drum bus and the reverb returns, full length, on the same timeline as the mics: mixed at the session's
    // rate, then resampled the same way as the mics when exporting at another rate (they used to be left out then)
    if (S.exp.kitBus)
    {
        auto atRate = [k] (const F32& x)
        {
            if (std::abs (k - 1) < 1e-9) return x;
            return renderTuned (x, 0, 0, (long) jsRound ((double) x.size() * k), 0, 1, 1, 1 / k, 0, (long) x.size(), 0);
        };
        HitStems h;
        long len = 0;
        for (int t = 0; t < (int) S.tracks.size(); ++t)
        {
            const auto& tr = S.tracks[(size_t) t];
            Chs chs; for (auto& x : M.proc[(size_t) t]) chs.push_back (renderSeg (x, 0, tr.len, lagOf (S, t), polR (S, t), 1, true));
            len = std::max (len, tr.len);
            h.stems.push_back (std::move (chs));
        }
        h.len = len;
        auto fit = [len] (Chs c) { for (auto& x : c) x.resize ((size_t) len, 0.0f); return c; };
        if (! M.room.empty()) h.room = fit (M.room);
        const bool print = S.exp.verbs == "print";
        if (print) for (int r = 0; r < 2; ++r) if (! M.rev[(size_t) r].empty()) h.rev[(size_t) r] = fit (M.rev[(size_t) r]);
        const auto lr = mixToStereo (S, h);
        const auto dir = fillTokens (S, "{ARTIC}/", 0, std::nullopt);
        sink (dir + "Drum Bus.wav", encodeWav ({ atRate (lr.first), atRate (lr.second) }, srOut, S.exp.bits)); ++n;
        if (! print)
        {
            const double bus = S.busMute ? 0 : S.busFader <= -60 ? 0 : std::pow (10.0, S.busFader / 20);
            for (int r = 0; r < 2; ++r)
            {
                if (M.rev[(size_t) r].size() != 2) continue;
                Chs rv = fit (M.rev[(size_t) r]);
                for (auto& x : rv) { for (auto& v : x) v = (float) ((double) v * bus); x = atRate (x); }
                sink (dir + (r ? "R2 Return.wav" : "R1 Return.wav"), encodeWav (rv, srOut, S.exp.bits)); ++n;
            }
        }
    }
    return n;
}

HitStems renderHitStems (const Session& S, const MixRender& M, const LevelResult& L, size_t i, const StemOptions& o)
{
    // Hitline's renderHitStems0: processed (B) or the recording as it is (A), one shared tail for all mics
    HitStems h;
    const long pre = (long) jsRound (S.det.prerollMs * S.sr / 1000), cap = (long) jsRound (S.exp.maxLenS * S.sr);
    const auto r = sliceRegions (S.onsets, pre, cap, S.maxLen())[i];
    const double ratio = o.processed ? ratioOf (S, i) : 1.0;
    const long len = std::max (1L, (long) jsRound ((double) (r.end - r.start) / ratio));
    auto clipLin = [&] (int t) { return std::pow (10.0, (i < S.tracks[(size_t) t].clip.size() ? S.tracks[(size_t) t].clip[i] : 0.0) / 20); };
    auto gainAt = [&] (int t) { return o.processed && L.valid ? std::pow (10.0, L.gainsDbT[(size_t) t][i] / 20) : clipLin (t); };
    auto fit = [] (F32 c, long n) { c.resize ((size_t) n, 0.0f); return c; };
    for (int t = 0; t < (int) S.tracks.size(); ++t)
    {
        if (! o.only.empty() && std::find (o.only.begin(), o.only.end(), t) == o.only.end()) { h.stems.emplace_back(); continue; }
        const double step = ratio, on = (double) S.onsets[i], lag = o.processed ? lagOf (S, t) : 0.0;
        const int pol = o.processed ? polR (S, t) : 1;
        const auto keep = attackKeep (S, ratio, 1, S.sr);
        const long preOut = (long) jsRound ((on - r.start) / (keep.keepN ? 1.0 : step)), outLen = std::max (1L, (long) jsRound ((double) (r.end - r.start) / step));
        const long lo = r.start + (long) std::floor (lag), hi = r.end + (long) std::ceil (lag);
        const auto& src = o.processed ? M.proc[(size_t) t] : S.tracks[(size_t) t].ch.get();
        Chs s;
        for (auto& x : src) s.push_back (fit (renderTuned (x, on, preOut, outLen, lag, pol, gainAt (t), step, lo, hi, 0, o.taps, keep), len));
        h.stems.push_back (std::move (s));
    }
    Chs all; for (auto& s : h.stems) for (auto& c : s) all.push_back (c);
    if (! all.empty())
    {
        const auto plan = tailPlan (all, S.sr, 0, S.exp.tailFloor);
        for (auto& s : h.stems) if (! s.empty()) s = applyTail (s, plan, S.sr);
    }
    long n = len;
    for (auto& s : h.stems) if (! s.empty()) { n = (long) s[0].size(); break; }
    // mono for the mono check; mono mics sit -3 dB in each side (equal-power centre)
    for (auto& s : h.stems)
        if (! s.empty() && (o.mono || s.size() == 1))
        {
            const double k = s.size() == 1 ? 0.7071 : 0.5;
            F32 m ((size_t) n, 0.0f);
            for (auto& c : s) for (long q = 0; q < n; ++q) m[(size_t) q] = (float) ((double) m[(size_t) q] + (double) c[(size_t) q] * k);
            s = { m };
        }
    if (o.processed && o.only.empty() && ! M.room.empty() && roomLive (S))
    {
        const double on = (double) S.onsets[i];
        const auto keep = attackKeep (S, ratio, 1, S.sr);
        const long preOut = (long) jsRound ((on - r.start) / (keep.keepN ? 1.0 : ratio)), outLen = std::max (1L, (long) jsRound ((double) (r.end - r.start) / ratio));
        for (auto& x : M.room) h.room.push_back (fit (renderTuned (x, on, preOut, outLen, 0, 1, gainAt (S.refIdx), ratio, r.start, r.end, 0, o.taps, keep), n));
    }
    if (o.processed && o.only.empty())
        for (int k = 0; k < 2; ++k)
        {
            if (M.rev[(size_t) k].empty() || ! verbLive (S, k)) continue;
            const double on = (double) S.onsets[i];
            const auto keep = attackKeep (S, ratio, 1, S.sr);
            const long preOut = (long) jsRound ((on - r.start) / (keep.keepN ? 1.0 : ratio)), outLen = std::max (1L, (long) jsRound ((double) (r.end - r.start) / ratio));
            for (auto& x : M.rev[(size_t) k]) h.rev[(size_t) k].push_back (fit (renderTuned (x, on, preOut, outLen, 0, 1, gainAt (S.refIdx), ratio, r.start, r.end, 0, o.taps, keep), n));
        }
    h.i = i; h.ratio = ratio; h.start = r.start; h.len = n;
    return h;
}

std::array<double, 4> panGains (double p, bool stereo)
{
    const double pi = juce::MathConstants<double>::pi;
    if (! stereo) { const double x = (p + 1) * pi / 4; return { std::cos (x), 0, std::sin (x), 0 }; }
    if (p <= 0) { const double x = (p + 1) * pi / 2; return { 1, std::cos (x), 0, std::sin (x) }; }
    const double x = p * pi / 2;
    return { std::cos (x), 0, std::sin (x), 1 };
}

std::array<double, 4> panGainsLR (double pl, double pr)
{
    const double pi = juce::MathConstants<double>::pi;
    const double a = (jclamp (pl, -1, 1) + 1) * pi / 4, b = (jclamp (pr, -1, 1) + 1) * pi / 4;
    return { std::cos (a), std::cos (b), std::sin (a), std::sin (b) };
}

std::array<double, 4> trackPanGains (double pan, double pl, double pr, bool stereo)
{
    if (stereo && (pl != -1 || pr != 1)) return panGainsLR (pl, pr);
    return panGains (jclamp (pan, -1, 1), stereo);
}

std::pair<F32, F32> mixToStereo (const Session& S, const HitStems& h)
{
    const size_t n = (size_t) h.len;
    F32 L (n, 0.0f), R (n, 0.0f);
    const double bus = S.busMute ? 0 : S.busFader <= -60 ? 0 : std::pow (10.0, S.busFader / 20);
    const bool anySolo = std::any_of (S.tracks.begin(), S.tracks.end(), [] (auto& t) { return t.solo; });
    for (size_t t = 0; t < h.stems.size(); ++t)
    {
        const auto& s = h.stems[t];
        if (s.empty()) continue;
        const auto& tr = S.tracks[t];
        const bool audible = anySolo ? tr.solo : ! tr.mute;
        const double g = (audible ? (tr.fader <= -60 ? 0 : std::pow (10.0, tr.fader / 20)) : 0) * bus;
        if (g == 0) continue;
        const bool st = s.size() > 1;
        const auto P = trackPanGains (tr.strip.pan, tr.panL, tr.panR, st);
        const auto& x = s[0]; const auto& y = st ? s[1] : s[0];
        for (size_t q = 0; q < n; ++q)
        {
            const double l = x[q], r = y[q];
            L[q] = (float) ((double) L[q] + (l * P[0] + (st ? r * P[1] : 0)) * g);
            R[q] = (float) ((double) R[q] + (l * P[2] + (st ? r * P[3] : 0)) * g);
        }
    }
    for (auto& rv : h.rev)
        if (rv.size() == 2 && bus != 0)
            for (size_t q = 0; q < n && q < rv[0].size(); ++q) { L[q] = (float) ((double) L[q] + (double) rv[0][q] * bus); R[q] = (float) ((double) R[q] + (double) rv[1][q] * bus); }
    if (! h.room.empty())
    {
        const double g = roomLin (S) * bus;
        if (g != 0) for (size_t q = 0; q < n; ++q) { L[q] = (float) ((double) L[q] + (double) h.room[0][q] * g); R[q] = (float) ((double) R[q] + (double) h.room[1][q] * g); }
    }
    return { std::move (L), std::move (R) };
}

size_t defaultOneShot (const Session& S, const LevelResult& L)
{
    const bool ok = S.mode != "kit" && ! S.onsets.empty() && (int) S.onsets.size() == S.det.V * S.det.RR;
    if (! L.valid || ! ok) return 0;
    const auto& lv = L.lvlRaw[(size_t) S.refIdx];
    std::vector<size_t> top;
    for (size_t i = 0; i < S.onsets.size(); ++i) if (S.velOfHit (i) == S.det.V - 1) top.push_back (i);
    F64 v; for (auto i : top) v.push_back (lv[i]);
    const double med = median1 (v);
    size_t b = top[0];
    for (auto i : top) if (std::abs (lv[i] - med) < std::abs (lv[b] - med)) b = i;
    return b;
}

OneShot exportOneShot (const Session& S, const MixRender& M, const LevelResult& L)
{
    OneShot o;
    if (S.tracks.empty() || S.onsets.empty()) return o;
    const size_t i = S.oneShot >= 0 && S.oneShot < (int) S.onsets.size() ? (size_t) S.oneShot : defaultOneShot (S, L);
    const auto h = renderHitStems (S, M, L, i, StemOptions {});
    auto [Lc, Rc] = mixToStereo (S, h);
    double pk = 0;
    for (size_t q = 0; q < Lc.size(); ++q) pk = std::max ({ pk, (double) std::abs (Lc[q]), (double) std::abs (Rc[q]) });
    if (! (pk > 0)) return o;   // the bus is silent
    const double ceil = std::pow (10.0, S.exp.ceilDb / 20), k = S.oneShotNorm || pk > ceil ? ceil / pk : 1;
    if (k != 1) for (size_t q = 0; q < Lc.size(); ++q) { Lc[q] = (float) ((double) Lc[q] * k); Rc[q] = (float) ((double) Rc[q] * k); }
    const bool ok = (int) S.onsets.size() == S.det.V * S.det.RR && S.mode != "kit";
    const int v = ok ? S.velOfHit (i) + 1 : 0, rr = ok ? (int) (i % (size_t) S.det.RR) + 1 : 0;
    const auto di = DRUM_NAMES.find (S.drum);
    std::string base = ! S.artic.empty() ? S.artic : di != DRUM_NAMES.end() ? di->second : "Drum";
    base = std::regex_replace (base, std::regex (R"([^\w-]+)"), "_");
    o.name = base + "_OneShot" + (v ? "_V" + p2 (v) + "_RR" + p2 (rr) : std::string()) + ".wav";
    o.wav = encodeWav ({ Lc, Rc }, S.sr, S.exp.bits);
    o.hit = i; o.scale = k;
    return o;
}

//==============================================================================
static ToneMoves toneOf (const juce::var& v)
{
    ToneMoves o; o.match = nn (v, "match", 0); o.bright = nn (v, "bright", 0); o.crack = nn (v, "crack", 0); o.smooth = nn (v, "smooth", 0); o.tame = nn (v, "tame", 0);
    return o;
}
static ShapeMoves shapeOf (const juce::var& v)
{
    ShapeMoves o; o.atk = nn (v, "atk", 0); o.dec = nn (v, "dec", 0); o.sus = nn (v, "sus", 0); o.rel = nn (v, "rel", 0); o.match = nn (v, "match", 0);
    return o;
}
static std::string s_ (const juce::var& v, const char* k, const char* d) { return str (v, k, d).toStdString(); }

Session sessionFromHitline (const juce::var& st, std::vector<Chs> audio)
{
    Session S;
    S.sr = st["sr"]; S.drum = s_ (st, "drum", "snare"); S.artic = s_ (st, "artic", ""); S.mode = s_ (st, "mode", "drum");
    S.refIdx = st["refIdx"];
    for (auto& v : *st["onsets"].getArray()) S.onsets.push_back ((long) (double) v);
    if (auto* r = st["rejects"].getArray()) for (auto& v : *r) S.rejects.push_back ((int) v);
    const auto d = st["det"];
    S.det.V = d["V"]; S.det.RR = d["RR"]; S.det.order = s_ (d, "order", "soft"); S.det.prerollMs = d["prerollMs"];
    const auto kl = st["lev"];
    auto& lev = S.lev;
    lev.mode = s_ (kl, "mode", "permic"); lev.source = s_ (kl, "source", "ref"); lev.winMs = kl["winMs"];
    lev.curve = s_ (kl, "curve", "midi"); lev.shape = kl["shape"]; lev.midiRange = s_ (kl, "midiRange", "natural");
    lev.stepDb = kl["stepDb"]; lev.rangeDb = kl["rangeDb"]; lev.anchor = s_ (kl, "anchor", "loud"); lev.offsetDb = kl["offsetDb"];
    lev.std = s_ (kl, "std", "house"); lev.steps = s_ (kl, "steps", "even");
    const auto e = st["exp"];
    auto& ex = S.exp;
    ex.mode = s_ (e, "mode", "hits"); ex.hitTemplate = s_ (e, "template", ex.hitTemplate.c_str()); ex.fullTemplate = s_ (e, "fullTemplate", ex.fullTemplate.c_str());
    ex.kitTemplate = s_ (e, "kitTemplate", ex.kitTemplate.c_str()); ex.bits = (int) nn (e, "bits", 32); ex.rate = s_ (e, "rate", "same");
    ex.tailMode = s_ (e, "tailMode", "natural"); ex.tailFloor = nn (e, "tailFloor", -90); ex.maxLenS = nn (e, "maxLenS", 12);
    ex.fadeOutMs = nn (e, "fadeOutMs", 40); ex.fadeInMs = nn (e, "fadeInMs", 0.5); ex.tailDb = nn (e, "tailDb", -70);
    ex.trim = s_ (e, "trim", "off"); ex.ceilDb = nn (e, "ceilDb", -1);
    S.dnOn = truthy (st["dn"], "on"); S.dnDb = nn (st["dn"], "db", 18);
    const auto tn = st["tone"];
    static_cast<ToneMoves&> (S.tone) = toneOf (tn);
    S.tone.repair = truthy (tn, "repair"); S.tone.proSrc = s_ (tn, "proSrc", "");
    if (tn["ref"].isObject()) { S.tone.hasRef = true; for (auto& v : *tn["ref"]["curve"].getArray()) S.tone.refCurve.push_back ((double) v); }
    S.tone.refAmt = nn (tn, "refAmt", 0);
    S.shape = shapeOf (st["shape"]);
    const auto mix = st["mix"];
    S.busIns = mix["busIns"].isArray() ? mix["busIns"] : juce::var (juce::Array<juce::var>());
    S.busFader = nn (mix, "fader", 0); S.busMute = truthy (mix, "mute"); S.oneShot = (int) nn (mix, "oneShot", -1);
    S.oneShotNorm = mix.hasProperty ("norm") ? truthy (mix, "norm") : true;
    S.kitBleed = nn (st, "kitBleed", 0);
    if (auto* k = st["kitOn"].getArray()) for (auto& v : *k) S.kitOn.push_back ((long) (double) v);
    if (mix["room"].isObject()) { S.room.on = truthy (mix["room"], "on"); S.room.ret = nn (mix["room"], "ret", 0); S.room.p = mix["room"]["p"]; }
    if (st["tinfo"].isObject())
    {
        TuneState ti; for (auto& v : *st["tinfo"]["ratios"].getArray()) ti.ratios.push_back ((double) v);
        ti.medF = nn (st["tinfo"], "medF", NAN);
        S.tinfo = ti;
    }
    const auto* tracks = st["tracks"].getArray();
    for (int t = 0; t < tracks->size(); ++t)
    {
        const auto kt = (*tracks)[t];
        SessionTrack tr;
        tr.mic = s_ (kt, "mic", ""); tr.name = s_ (kt, "name", "");
        tr.ch = std::move (audio[(size_t) t]);
        tr.mono = monoOf (tr.ch);
        tr.len = (long) tr.ch[0].size();
        tr.alignTo = s_ (kt, "alignTo", "lead");
        tr.lag = nn (kt, "lag", 0); tr.pol = (int) nn (kt, "pol", 1);
        if (auto* a = kt["ap"].getArray()) for (auto& b : *a) tr.ap.push_back ({ (double) b["fc"], (double) b["Q"] });
        if (auto* c = kt["clip"].getArray()) for (auto& v : *c) tr.clip.push_back (v.isVoid() ? 0.0 : (double) v);
        tr.fader = nn (kt, "fader", 0); tr.mute = truthy (kt, "mute"); tr.solo = truthy (kt, "solo"); tr.lrLock = truthy (kt, "lrLock");
        if (kt["lr"].isObject()) tr.lr = LrFix { (double) kt["lr"]["lag"], (int) kt["lr"]["pol"], truthy (kt["lr"], "kept") };
        if (kt["auto"].isObject()) tr.autoAsis = nn (kt["auto"], "asis", NAN);
        const auto sv = kt["strip"];
        if (sv.isObject())
        {
            tr.strip.trim = nn (sv, "trim", 0); tr.strip.pan = nn (sv, "pan", 0); tr.strip.pol = truthy (sv, "pol");
            tr.strip.send = nn (sv, "send", -60); tr.strip.r1 = nn (sv, "r1", -60); tr.strip.r2 = nn (sv, "r2", -60);
            tr.strip.tone = toneOf (sv["tone"]); tr.strip.shape = shapeOf (sv["shape"]);
            tr.strip.inserts = sv["inserts"].isArray() ? sv["inserts"] : juce::var (juce::Array<juce::var>());
        }
        S.tracks.push_back (std::move (tr));
    }
    return S;
}

namespace
{
juce::var obj() { return juce::var (new juce::DynamicObject()); }
void put (juce::var& o, const char* k, const juce::var& v) { o.getDynamicObject()->setProperty (k, v); }
juce::var toneVar (const ToneMoves& t)
{
    auto o = obj(); put (o, "match", t.match); put (o, "bright", t.bright); put (o, "crack", t.crack); put (o, "smooth", t.smooth); put (o, "tame", t.tame);
    return o;
}
juce::var shapeVar (const ShapeMoves& t)
{
    auto o = obj(); put (o, "atk", t.atk); put (o, "dec", t.dec); put (o, "sus", t.sus); put (o, "rel", t.rel); put (o, "match", t.match);
    return o;
}
} // namespace

juce::var settingsToVar (const Session& S)
{
    auto st = obj();
    put (st, "version", 1);
    put (st, "drum", juce::String (S.drum)); put (st, "artic", juce::String (S.artic));
    auto d = obj(); put (d, "V", S.det.V); put (d, "RR", S.det.RR); put (d, "order", juce::String (S.det.order)); put (d, "prerollMs", S.det.prerollMs);
    put (d, "minGapMs", S.minGapMs); put (d, "thrDb", S.thrDb); put (d, "thrAuto", S.thrAuto);
    put (st, "det", d);
    auto l = obj(); const auto& lv = S.lev;
    put (l, "mode", juce::String (lv.mode)); put (l, "source", juce::String (lv.source)); put (l, "winMs", lv.winMs); put (l, "curve", juce::String (lv.curve));
    put (l, "shape", lv.shape); put (l, "midiRange", juce::String (lv.midiRange)); put (l, "stepDb", lv.stepDb); put (l, "rangeDb", lv.rangeDb);
    put (l, "anchor", juce::String (lv.anchor)); put (l, "offsetDb", lv.offsetDb); put (l, "std", juce::String (lv.std)); put (l, "steps", juce::String (lv.steps));
    put (st, "lev", l);
    auto e = obj(); const auto& ex = S.exp;
    put (e, "mode", juce::String (ex.mode)); put (e, "template", juce::String (ex.hitTemplate)); put (e, "fullTemplate", juce::String (ex.fullTemplate));
    put (e, "kitTemplate", juce::String (ex.kitTemplate)); put (e, "bits", ex.bits); put (e, "rate", juce::String (ex.rate)); put (e, "tailMode", juce::String (ex.tailMode));
    put (e, "kitBus", ex.kitBus); put (e, "verbs", juce::String (ex.verbs));
    put (e, "tailFloor", ex.tailFloor); put (e, "maxLenS", ex.maxLenS); put (e, "fadeOutMs", ex.fadeOutMs); put (e, "fadeInMs", ex.fadeInMs);
    put (e, "tailDb", ex.tailDb); put (e, "trim", juce::String (ex.trim)); put (e, "ceilDb", ex.ceilDb);
    put (st, "exp", e);
    auto dn = obj(); put (dn, "on", S.dnOn); put (dn, "db", S.dnDb); put (st, "dn", dn);
    auto tn = toneVar (S.tone);
    put (tn, "repair", S.tone.repair); put (tn, "proSrc", juce::String (S.tone.proSrc)); put (tn, "refAmt", S.tone.refAmt); put (tn, "refEnvAmt", S.tone.refEnvAmt);
    if (S.tone.hasRefEnv) { juce::Array<juce::var> e; for (double x : S.tone.refEnv) e.add (x); put (tn, "refEnv", e); }
    if (S.tone.hasRef) { auto r = obj(); juce::Array<juce::var> c; for (double v : S.tone.refCurve) c.add (v); put (r, "curve", c); put (tn, "ref", r); }
    put (st, "tone", tn);
    put (st, "shape", shapeVar (S.shape));
    auto mix = obj();
    put (mix, "busIns", S.busIns.isArray() ? S.busIns : juce::var (juce::Array<juce::var>()));
    put (mix, "fader", S.busFader); put (mix, "mute", S.busMute); put (mix, "norm", S.oneShotNorm);
    auto room = obj(); put (room, "on", S.room.on); put (room, "ret", S.room.ret); put (room, "p", S.room.p); put (mix, "room", room);
    juce::Array<juce::var> vu;
    for (auto& v : S.verbs) { auto o = obj(); put (o, "type", juce::String (v.type)); put (o, "decay", v.decay); put (o, "pre", v.pre); put (o, "size", v.size); put (o, "tone", v.tone); put (o, "width", v.width); vu.add (o); }
    put (mix, "verbs", vu); put (mix, "busR1", S.busR1); put (mix, "busR2", S.busR2);
    put (st, "mix", mix);
    put (st, "kitBleed", S.kitBleed);
    {
        auto sg = obj(); const auto& ss = S.song.set;
        put (sg, "tempoMode", juce::String (ss.tempoMode)); put (sg, "tightness", ss.tightness); put (sg, "swing", ss.swing); put (sg, "keepFeel", ss.keepFeel);
        put (sg, "swingHatsOnly", ss.swingHatsOnly); put (sg, "division", ss.division); put (sg, "divisionAuto", ss.divisionAuto); put (sg, "bpmScale", ss.bpmScale); put (sg, "flatBpm", ss.flatBpm); put (sg, "consistency", ss.consistency);
        put (st, "song", sg);
    }
    juce::Array<juce::var> tracks;
    for (auto& t : S.tracks)
    {
        auto kt = obj();
        put (kt, "mic", juce::String (t.mic)); put (kt, "alignTo", juce::String (t.alignTo)); put (kt, "amount", t.amount); put (kt, "apOn", t.apOn);
        put (kt, "fader", t.fader); put (kt, "panL", t.panL); put (kt, "panR", t.panR); put (kt, "mute", t.mute); put (kt, "lrLock", t.lrLock);
        auto sv = obj();
        put (sv, "trim", t.strip.trim); put (sv, "pan", t.strip.pan); put (sv, "pol", t.strip.pol); put (sv, "send", t.strip.send); put (sv, "r1", t.strip.r1); put (sv, "r2", t.strip.r2);
        put (sv, "tone", toneVar (t.strip.tone)); put (sv, "shape", shapeVar (t.strip.shape));
        put (sv, "inserts", t.strip.inserts.isArray() ? t.strip.inserts : juce::var (juce::Array<juce::var>()));
        put (kt, "strip", sv);
        tracks.add (kt);
    }
    put (st, "tracks", tracks);
    return st;
}

void applySettings (Session& S, const juce::var& st)
{
    if (! st.isObject()) return;
    const auto d = st["det"];
    if (d.isObject())
    {
        S.det.order = s_ (d, "order", S.det.order.c_str()); S.det.prerollMs = nn (d, "prerollMs", S.det.prerollMs);
        S.minGapMs = nn (d, "minGapMs", S.minGapMs); S.thrDb = nn (d, "thrDb", S.thrDb);
        if (d.hasProperty ("thrAuto")) S.thrAuto = truthy (d, "thrAuto");
    }
    const auto kl = st["lev"];
    if (kl.isObject())
    {
        auto& lev = S.lev;
        lev.mode = s_ (kl, "mode", lev.mode.c_str()); lev.source = s_ (kl, "source", lev.source.c_str()); lev.winMs = nn (kl, "winMs", lev.winMs);
        lev.curve = s_ (kl, "curve", lev.curve.c_str()); lev.shape = nn (kl, "shape", lev.shape); lev.midiRange = s_ (kl, "midiRange", lev.midiRange.c_str());
        lev.stepDb = nn (kl, "stepDb", lev.stepDb); lev.rangeDb = nn (kl, "rangeDb", lev.rangeDb); lev.anchor = s_ (kl, "anchor", lev.anchor.c_str());
        lev.offsetDb = nn (kl, "offsetDb", lev.offsetDb); lev.std = s_ (kl, "std", lev.std.c_str()); lev.steps = s_ (kl, "steps", lev.steps.c_str());
    }
    const auto e = st["exp"];
    if (e.isObject())
    {
        auto& ex = S.exp;
        ex.mode = s_ (e, "mode", ex.mode.c_str()); ex.hitTemplate = s_ (e, "template", ex.hitTemplate.c_str()); ex.fullTemplate = s_ (e, "fullTemplate", ex.fullTemplate.c_str());
        ex.kitTemplate = s_ (e, "kitTemplate", ex.kitTemplate.c_str()); ex.bits = (int) nn (e, "bits", ex.bits); ex.rate = s_ (e, "rate", ex.rate.c_str());
        ex.tailMode = s_ (e, "tailMode", ex.tailMode.c_str()); ex.tailFloor = nn (e, "tailFloor", ex.tailFloor); ex.maxLenS = nn (e, "maxLenS", ex.maxLenS);
        if (e.hasProperty ("kitBus")) ex.kitBus = truthy (e, "kitBus");
        ex.verbs = s_ (e, "verbs", ex.verbs.c_str());
        ex.fadeOutMs = nn (e, "fadeOutMs", ex.fadeOutMs); ex.fadeInMs = nn (e, "fadeInMs", ex.fadeInMs); ex.tailDb = nn (e, "tailDb", ex.tailDb);
        ex.trim = s_ (e, "trim", ex.trim.c_str()); ex.ceilDb = nn (e, "ceilDb", ex.ceilDb);
    }
    if (st["dn"].isObject()) { S.dnOn = truthy (st["dn"], "on"); S.dnDb = nn (st["dn"], "db", S.dnDb); }
    const auto tn = st["tone"];
    if (tn.isObject())
    {
        static_cast<ToneMoves&> (S.tone) = toneOf (tn);
        S.tone.repair = truthy (tn, "repair"); S.tone.proSrc = s_ (tn, "proSrc", ""); S.tone.refAmt = nn (tn, "refAmt", 0); S.tone.refEnvAmt = nn (tn, "refEnvAmt", 0);
        S.tone.hasRefEnv = false; if (auto* e = tn["refEnv"].getArray()) if (e->size() == 3) { S.tone.hasRefEnv = true; for (int k = 0; k < 3; ++k) S.tone.refEnv[(size_t) k] = (double) (*e)[k]; }
        S.tone.hasRef = false; S.tone.refCurve.clear();
        if (tn["ref"].isObject()) if (auto* c = tn["ref"]["curve"].getArray()) { S.tone.hasRef = true; for (auto& v : *c) S.tone.refCurve.push_back ((double) v); }
    }
    if (st["shape"].isObject()) S.shape = shapeOf (st["shape"]);
    const auto mix = st["mix"];
    if (mix.isObject())
    {
        if (mix["busIns"].isArray()) S.busIns = mix["busIns"];
        S.busFader = nn (mix, "fader", S.busFader); S.busMute = truthy (mix, "mute");
        if (mix.hasProperty ("norm")) S.oneShotNorm = truthy (mix, "norm");
        if (mix["room"].isObject()) { S.room.on = truthy (mix["room"], "on"); S.room.ret = nn (mix["room"], "ret", 0); S.room.p = mix["room"]["p"]; }
        if (auto* vu = mix["verbs"].getArray())
            for (int k = 0; k < std::min (2, vu->size()); ++k)
            {
                const auto& o = vu->getReference (k); auto& v = S.verbs[(size_t) k];
                v.type = s_ (o, "type", v.type.c_str()); v.decay = nn (o, "decay", v.decay); v.pre = nn (o, "pre", v.pre);
                v.size = nn (o, "size", v.size); v.tone = nn (o, "tone", v.tone); v.width = nn (o, "width", v.width);
            }
        S.busR1 = nn (mix, "busR1", S.busR1); S.busR2 = nn (mix, "busR2", S.busR2);
    }
    S.kitBleed = nn (st, "kitBleed", S.kitBleed);
    if (st["song"].isObject())
    {
        const auto& sg = st["song"]; auto& ss = S.song.set;
        ss.tempoMode = s_ (sg, "tempoMode", ss.tempoMode.c_str()); ss.tightness = nn (sg, "tightness", 0); ss.swing = nn (sg, "swing", 0);
        ss.keepFeel = ! sg.hasProperty ("keepFeel") || truthy (sg, "keepFeel"); ss.swingHatsOnly = truthy (sg, "swingHatsOnly");
        ss.division = (int) nn (sg, "division", 16); ss.divisionAuto = sg.getProperty ("divisionAuto", true); ss.bpmScale = nn (sg, "bpmScale", 1); ss.flatBpm = nn (sg, "flatBpm", 0); ss.consistency = nn (sg, "consistency", 0);
    }
    if (auto* tracks = st["tracks"].getArray())
        for (auto& t : S.tracks)
            for (auto& kt : *tracks)
            {
                if (juce::String (t.mic).compareIgnoreCase (kt["mic"].toString()) != 0) continue;
                t.alignTo = s_ (kt, "alignTo", t.alignTo.c_str()); t.amount = nn (kt, "amount", t.amount);
                if (kt.hasProperty ("apOn")) t.apOn = truthy (kt, "apOn");
                t.fader = nn (kt, "fader", t.fader); t.panL = nn (kt, "panL", t.panL); t.panR = nn (kt, "panR", t.panR); t.mute = truthy (kt, "mute"); t.lrLock = truthy (kt, "lrLock");
                const auto sv = kt["strip"];
                if (sv.isObject())
                {
                    t.strip.trim = nn (sv, "trim", 0); t.strip.pan = nn (sv, "pan", 0); t.strip.pol = truthy (sv, "pol"); t.strip.send = nn (sv, "send", -60); t.strip.r1 = nn (sv, "r1", -60); t.strip.r2 = nn (sv, "r2", -60);
                    t.strip.tone = toneOf (sv["tone"]); t.strip.shape = shapeOf (sv["shape"]);
                    if (sv["inserts"].isArray()) t.strip.inserts = sv["inserts"];
                }
                break;
            }
}
double trackLag (const Session& S, int t) { return lagOf (S, t); }
int trackPol (const Session& S, int t) { return polR (S, t); }
} // namespace ld::hl

#include "FxSat.h"
#include "FxInserts.h"
#include "Fft.h"
#include <map>
#include <mutex>

namespace ld::hl
{
namespace
{
    // ------------------------------------------------ 4x oversampled memoryless curves
    const Kernel& osk()
    {
        static const Kernel K = []
        {
            Kernel k;
            const int L = 256, c = 127;
            const double fc = 0.5 / 4 * 0.92;
            k.h.assign (L, 0.0);
            double s = 0;
            for (int n = 0; n < 255; ++n)
            {
                const int t = n - c;
                const double w = 0.42 - 0.5 * std::cos (2 * M_PI * n / 254) + 0.08 * std::cos (4 * M_PI * n / 254);
                k.h[(size_t) n] = (t == 0 ? 2 * fc : std::sin (2 * M_PI * fc * t) / (M_PI * t)) * w;
                s += k.h[(size_t) n];
            }
            for (auto& v : k.h) v /= s;
            k.delay = c;
            return k;
        }();
        return K;
    }

    F32 os4 (const F32& x, const std::function<double (double)>& f)
    {
        const long n = (long) x.size(), C = 1 << 18, P = 96;
        F32 y ((size_t) n, 0.0f);
        for (long a = 0; a < n; a += C)
        {
            const long s = std::max (0L, a - P), e = std::min (n, a + C + P), m = e - s;
            F32 up ((size_t) (4 * m), 0.0f);
            for (long i = 0; i < m; ++i) up[(size_t) (4 * i)] = (float) (4.0 * x[(size_t) (s + i)]);
            auto u = convolve (up, osk());
            for (auto& v : u) v = (float) f (v);
            const auto d = convolve (u, osk());
            for (long i = a; i < std::min (n, a + C); ++i) y[(size_t) i] = d[(size_t) (4 * (i - s))];
        }
        return y;
    }

    double satTanh (double u) { if (u > 15) return 1; if (u < -15) return -1; const double e = std::exp (2 * u); return (e - 1) / (e + 1); }
    double satHard (double u, double p) { return p == 4 ? u / std::sqrt (std::sqrt (1 + u * u * u * u)) : u / std::sqrt (1 + u * u); }

    std::function<double (double)> curveFn (double g, double b, const std::string& kind, double p = 0)
    {
        std::function<double (double)> s;
        if (kind == "hard") { const double pp = p != 0 ? p : 2.5; s = [pp] (double u) { return satHard (u, pp); }; }
        else s = satTanh;
        const double s0 = s (b), d = 1e-4;
        double slope = (s (b + d) - s (b - d)) / (2 * d);
        if (slope == 0 || std::isnan (slope)) slope = 1;
        return [=] (double v) { return (s (g * v + b) - s0) / (g * slope); };
    }

    struct Style { double k, b; std::string kind; double p, post; bool lf = false, lfx = false; };
    const std::map<std::string, Style> decapStyles { { "A", { 0.23, 0.35, "tanh", 0, 0 } }, { "E", { 0.22, 0.16, "tanh", 0, 2.8 } }, { "N", { 0.13, 0.22, "tanh", 0, -2.8 } },
                                                     { "T", { 0.36, 0.08, "hard", 2, 0.8 } }, { "P", { 1.0, 0.25, "tanh", 0, 0.4 } } };
    const std::map<std::string, Style> saturnStyles { { "gentle", { 1.5, 0, "tanh", 0, 0 } }, { "tape", { 1.4, 0.12, "tanh", 0, -1.5, true } }, { "tube", { 1.5, 0.35, "tanh", 0, 0 } },
                                                      { "transformer", { 1.4, 0.05, "tanh", 0, 0, false, true } }, { "heavy", { 4, 0.15, "hard", 4, 0 } } };

    juce::var bandV (const char* type, double f, double g, double q = 0.71)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", type); o->setProperty ("f", f); o->setProperty ("g", g); o->setProperty ("q", q);
        return juce::var (o);
    }
    juce::var bandNoQ (const char* type, double f, double g)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", type); o->setProperty ("f", f); o->setProperty ("g", g);
        return juce::var (o);
    }

    F32 eqMin (const F32& x, double sr, const std::function<double (double)>& fn)
    {
        const size_t N = kernN (sr, true);
        return convolve (x, magToKernel (magGrid (sr, N, fn), N, true));
    }
    F32 hpN (const F32& x, double f, double sr, int ord) { return eqMin (x, sr, [=] (double q) { return 1 / std::sqrt (1 + std::pow (f / q, 2 * ord)); }); }
    F32 lpN (const F32& x, double f, double sr, int ord) { return eqMin (x, sr, [=] (double q) { return 1 / std::sqrt (1 + std::pow (q / f, 2 * ord)); }); }

    F32 leveller (const F32& det, double sr, double thrDb, double atkMs, double relMs, double ratio)
    {
        const double aA = std::exp (-1 / (atkMs * sr / 1000)), aR = std::exp (-1 / (relMs * sr / 1000)), k = 1 / ratio - 1;
        F32 g (det.size());
        double v = 0;
        for (size_t q = 0; q < det.size(); ++q)
        {
            const double L = 20 * std::log10 (det[q] + 1e-9), t = L > thrDb ? (L - thrDb) * k : 0;
            v = t < v ? aA * v + (1 - aA) * t : aR * v + (1 - aR) * t;
            g[q] = (float) std::pow (10.0, v / 20);
        }
        return g;
    }
}

Chs satRender (const Chs& chs, double sr, const juce::var& P)
{
    auto m = str (P, "model", "decap").toStdString();
    if (m.empty()) m = "decap";
    const auto pv = P.getProperty (juce::Identifier (m), juce::var());
    const juce::var p = pv.isObject() ? pv : juce::var (new juce::DynamicObject());
    const size_t n = chs[0].size();
    const double mix = clampD (nn (P, "mix", 1), 0, 1), outG = dbl (orr (P, "out", 0));
    F32 det (n, 0.0f);
    for (auto& x : chs) for (size_t q = 0; q < n; ++q) { const double v = std::abs ((double) x[q]); if (v > det[q]) det[q] = (float) v; }
    Chs wet;
    if (m == "decap")
    {
        auto styleName = str (p, "style", "A").toStdString(); if (styleName.empty()) styleName = "A";
        const auto& st = decapStyles.count (styleName) ? decapStyles.at (styleName) : decapStyles.at ("A");
        const double dr = clampD (nn (p, "drive", 3), 0, 10), g = st.k * dbl (dr * 3.2 + (truthy (p, "punish") ? 20 : 0));
        const auto f = curveFn (g, st.b, st.kind, st.p);
        const double tone = clampD (nn (p, "tone", 0), -5, 5), lc = orr (p, "lowcut", 0), hc = orr (p, "highcut", 0);
        const int ord = truthy (p, "steep") ? 5 : 1;
        const auto shelf = bandV ("highshelf", 9000, st.post, 0.7), tilt = bandNoQ ("tilt", 900, tone * 2.4), thump = bandV ("bell", lc * 1.15, 4, 1.4);
        for (auto& x : chs)
        {
            auto y = os4 (x, f);
            if (st.post != 0 || tone != 0) y = eqMin (y, sr, [&] (double q) { return bandMag (shelf, q) * bandMag (tilt, q); });
            if (lc > 20) { y = hpN (y, lc, sr, ord); if (truthy (p, "thump")) y = eqMin (y, sr, [&] (double q) { return bandMag (thump, q); }); }
            if (hc > 0 && hc < 20000) y = lpN (y, hc, sr, ord);
            wet.push_back (std::move (y));
        }
    }
    else if (m == "devil")
    {
        const double crush = clampD (nn (p, "crush", 4), 0, 10), thr = -36 - crush * 2, R = 4.6;
        const auto g = leveller (det, sr, thr, 1.2, 95, R);
        const double mk = dbl (-6 - thr * (1 - 1 / R));
        const auto f = curveFn (0.6 + crush * 0.06, 0.2, "tanh");
        const double dark = clampD (nn (p, "darkness", 2), 0, 10);
        for (auto& x : chs)
        {
            F32 l (n);
            for (size_t q = 0; q < n; ++q) l[q] = (float) ((double) x[q] * g[q] * mk);
            auto y = os4 (l, f);
            if (truthy (p, "lowcut")) y = hpN (y, 50, sr, 4);
            y = lpN (y, 22000 * std::pow (1500.0 / 22000, dark / 10), sr, 2);
            wet.push_back (std::move (y));
        }
    }
    else if (m == "saturn")
    {
        auto styleName = str (p, "style", "gentle").toStdString(); if (styleName.empty()) styleName = "gentle";
        const auto& st = saturnStyles.count (styleName) ? saturnStyles.at (styleName) : saturnStyles.at ("gentle");
        const double g = st.k * dbl (clampD (nn (p, "drive", 0), -12, 30));
        const auto f = curveFn (g, st.b, st.kind, st.p);
        const double tone = clampD (nn (p, "tone", 0), -5, 5);
        const auto shelf = bandV ("highshelf", 10000, st.post, 0.7), lfb = bandV ("bell", 70, 1.2, 0.8), tilt = bandNoQ ("tilt", 900, tone * 2.4);
        for (auto& x : chs)
        {
            if (st.lfx)
            {
                const auto lo = lpN (x, 180, sr, 1);
                F32 hi (n);
                for (size_t q = 0; q < n; ++q) hi[q] = (float) ((double) x[q] - lo[q]);
                const auto yl = os4 (lo, curveFn (g * 2.2, st.b, "tanh"));
                for (size_t q = 0; q < n; ++q) hi[q] = (float) (hi[q] + (double) yl[q]);
                wet.push_back (std::move (hi));
                continue;
            }
            auto y = os4 (x, f);
            if (st.post != 0 || st.lf || tone != 0)
                y = eqMin (y, sr, [&] (double q) { return bandMag (shelf, q) * (st.lf ? bandMag (lfb, q) : 1) * bandMag (tilt, q); });
            wet.push_back (std::move (y));
        }
    }
    else
    {
        const double inDb = clampD (nn (p, "input", 0), -12, 24), gi = dbl (inDb), warm = clampD (nn (p, "warmth", 0), 0, 7);
        const auto mode = str (p, "comp", "off");
        F32 gc;
        if (mode != "off" && mode.isNotEmpty())
        {
            F32 d (n);
            for (size_t q = 0; q < n; ++q) d[q] = (float) (det[q] * gi);
            gc = mode == "buss" ? leveller (d, sr, -16, 30, 400, 2) : mode == "gp" ? leveller (d, sr, -18, 6, 180, 4) : leveller (d, sr, -22, 0.5, 60, 20);
        }
        const auto fw = curveFn (0.45 + warm * 0.25, 0.005, "tanh"), ft = curveFn (1.6, 0.02, "tanh");
        const auto preB = bandV ("highshelf", 4000, 4 + warm, 0.6), loShelf = bandV ("lowshelf", 45, 2, 0.7);
        auto pre = [&] (double q) { return bandMag (preB, q); };
        for (auto& x : chs)
        {
            F32 y (n);
            for (size_t q = 0; q < n; ++q) y[q] = (float) ((double) x[q] * gi * (gc.empty() ? 1.0 : (double) gc[q]));
            y = eqMin (os4 (eqMin (y, sr, pre), fw), sr, [&] (double q) { return 1 / pre (q); });
            if (! isFalse (p, "tranny"))
            {
                const auto lo = lpN (y, 150, sr, 1), yl = os4 (lo, ft);
                for (size_t q = 0; q < n; ++q) y[q] = (float) (y[q] + ((double) yl[q] - lo[q]));
                y = eqMin (y, sr, [&] (double q) { return bandMag (loShelf, q); });
            }
            wet.push_back (std::move (y));
        }
    }
    const double dcA = std::exp (-2 * M_PI * 5 / sr);
    for (auto& w : wet)
    {
        double px = 0, py = 0;
        for (auto& v : w) { py = dcA * (py + v - px); px = v; v = (float) py; }
    }
    double k = 1;
    if (! isFalse (P, "auto"))
    {
        double a = 0, b = 0;
        for (size_t c = 0; c < chs.size(); ++c) for (size_t q = 0; q < n; ++q) { a += (double) chs[c][q] * chs[c][q]; b += (double) wet[c][q] * wet[c][q]; }
        k = b > 0 ? std::sqrt (a / b) : 1;
    }
    Chs res;
    for (size_t c = 0; c < chs.size(); ++c)
    {
        F32 y (n);
        for (size_t q = 0; q < n; ++q) y[q] = (float) ((chs[c][q] * (1 - mix) + wet[c][q] * k * mix) * outG);
        res.push_back (std::move (y));
    }
    return res;
}

// ------------------------------------------------------------------ reverb
namespace
{
    juce::var irLibrary;
    uint32_t rngS = 1;
    double rnd() { rngS = (uint32_t) (((uint64_t) rngS * 1664525u + 1013904223u) & 0xffffffffu); return rngS / 4294967296.0; }
    double gau() { double u = 0; while (u == 0) u = rnd(); return std::sqrt (-2 * std::log (u)) * std::cos (2 * M_PI * rnd()); }
    const double IRB[9] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

    F32 synthTail (long n, double sr, const std::vector<std::pair<double, double>>& bands, uint32_t seed, double gateS)
    {
        rngS = seed;
        const int N = 2048, H = 512;
        F32 out ((size_t) (n + N), 0.0f);
        F64 re (N), im (N), win (N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = std::sqrt (0.5 - 0.5 * std::cos (2 * M_PI * i / N));
        double lf[9];
        for (int j = 0; j < 9; ++j) lf[j] = std::log (IRB[j]);
        F64 binB (N / 2 + 1);
        for (int k = 0; k <= N / 2; ++k)
        {
            const double l = std::log (std::max (20.0, k * sr / N));
            int j = 0;
            while (j < 9 - 2 && l > lf[j + 1]) ++j;
            binB[(size_t) k] = j + clampD ((l - lf[j]) / (lf[j + 1] - lf[j]), 0, 1);
        }
        std::vector<double> dens;
        for (size_t j = 0; j < bands.size(); ++j) dens.push_back (std::pow (10.0, bands[j].second / 10) / std::max (1.0, IRB[j] * 0.707 * N / sr));
        std::vector<double> pw (bands.size());
        for (long s = 0; s < n; s += H)
        {
            const double t = (s + N / 2.0) / sr;
            for (size_t j = 0; j < bands.size(); ++j)
                pw[j] = dens[j] * std::pow (10.0, (gateS != 0 ? (t < gateS ? 0 : -60 * (t - gateS) / 0.06) : -60 * (t - 0.08) / bands[j].first) / 10);
            std::fill (re.begin(), re.end(), 0.0); std::fill (im.begin(), im.end(), 0.0);
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = binB[(size_t) k];
                const int j = (int) std::floor (f);
                const double u = f - j, p = pw[(size_t) j] * (1 - u) + pw[(size_t) std::min (j + 1, (int) pw.size() - 1)] * u, a = std::sqrt (p);
                re[(size_t) k] = a * gau(); im[(size_t) k] = a * gau();
                re[(size_t) (N - k)] = re[(size_t) k]; im[(size_t) (N - k)] = -im[(size_t) k];
            }
            fft (re, im, true);
            for (int i = 0; i < N; ++i) out[(size_t) (s + i)] = (float) (out[(size_t) (s + i)] + re[(size_t) i] * win[(size_t) i]);
        }
        out.resize ((size_t) n);
        return out;
    }

    F32 sincResample (const F32& x, double from, double to)
    {
        if (from == to) return x;
        const double r = to / from, fc = std::min (1.0, r);
        const long n = (long) jsRound (x.size() * r);
        const int W = 16;
        F32 y ((size_t) n);
        for (long i = 0; i < n; ++i)
        {
            const double c = i / r;
            const long c0 = (long) std::floor (c);
            double s = 0;
            for (long k = c0 - W; k <= c0 + W; ++k)
            {
                if (k < 0 || k >= (long) x.size()) continue;
                const double d = c - k, w = 0.5 + 0.5 * std::cos (M_PI * d / (W + 1));
                s += x[(size_t) k] * w * (std::abs (d) < 1e-9 ? fc : std::sin (M_PI * fc * d) / (M_PI * d));
            }
            y[(size_t) i] = (float) s;
        }
        return y;
    }

    struct VV { double rt, lo, hi, er, pd, lf, gate; };
    const std::map<juce::String, VV> vvPrograms {
        { "Concert Hall", { 2.8, 1.25, 0.55, 60, 20, 0, 0 } }, { "Bright Hall", { 2.4, 1.0, 0.85, 50, 15, 0, 0 } }, { "Plate", { 1.8, 0.9, 0.8, 4, 0, 0, 0 } },
        { "Room", { 0.9, 1.1, 0.6, 25, 5, 0, 0 } }, { "Chamber", { 1.4, 1.0, 0.7, 35, 8, 0, 0 } }, { "Random Space", { 2.2, 1.1, 0.7, 70, 10, 0, 0 } },
        { "Ambience", { 0.45, 1.0, 0.7, 30, 0, 0, 0 } }, { "Sanctuary", { 4.5, 1.3, 0.5, 90, 30, 0, 0 } }, { "Dirty Hall", { 2.5, 1.2, 0.5, 60, 15, 6000, 0 } },
        { "Dirty Plate", { 1.6, 0.9, 0.7, 4, 0, 7000, 0 } }, { "Smooth Plate", { 2.2, 0.9, 0.7, 8, 0, 0, 0 } }, { "Smooth Room", { 1.0, 1.0, 0.65, 20, 4, 0, 0 } },
        { "Nonlin", { 0.35, 1, 0.8, 10, 0, 0, 1 } } };

    F32 decodeEarly (const juce::String& b64)
    {
        juce::MemoryOutputStream mo;
        juce::Base64::convertFromBase64 (mo, b64);
        const auto* d = static_cast<const uint8_t*> (mo.getData());
        const size_t n = mo.getDataSize() / 2;
        F32 e (n);
        for (size_t i = 0; i < n; ++i) e[i] = (float) ((int16_t) (d[2 * i] | (d[2 * i + 1] << 8)) / 32767.0);
        return e;
    }

    std::map<std::string, std::pair<F32, F32>> irCache;
    std::mutex irMutex;
}

void setReverbLibrary (const juce::var& irlib) { irLibrary = irlib; }

std::pair<F32, F32> verbIR (const juce::var& p, double sr)
{
    const auto model = str (p, "model"), prog = str (p, "prog");
    const std::string key = (model + "|" + prog + "|" + juce::String (nn (p, "decay", 1)) + "|" + juce::String (nn (p, "pre", 0)) + "|"
                             + juce::String (nn (p, "early", 1)) + "|" + str (p, "era") + "|" + juce::String (sr)).toStdString();
    {
        std::lock_guard<std::mutex> lock (irMutex);
        if (irCache.count (key)) return irCache[key];
    }
    const double dm = clampD (nn (p, "decay", 1), 0.25, 3), eL = clampD (nn (p, "early", 1), 0, 2);
    const long pre = jsRoundI (clampD (nn (p, "pre", 0), 0, 250) * sr / 1000);
    F32 early;
    bool haveEarly = false;
    std::vector<std::pair<double, double>> bands;
    double gateS = 0, lofi = 0;
    long len;
    if (model == "vv")
    {
        const auto& v = vvPrograms.count (prog) ? vvPrograms.at (prog) : vvPrograms.at ("Plate");
        const double sh[9] = { v.lo, v.lo, (v.lo + 1) / 2, 1, 1, (1 + v.hi) / 2, v.hi, v.hi * 0.85, v.hi * 0.7 }, lv[9] = { 0, 0, 0, 0, -1, -2, -4, -7, -11 };
        for (int j = 0; j < 9; ++j) bands.push_back ({ std::max (0.08, v.rt * sh[j] * dm), lv[j] });
        if (v.gate != 0) gateS = v.rt * dm;
        const auto era = str (p, "era", "Now");
        const double eraLp = era == "1970s" ? 8000 : era == "1980s" ? 14000 : 0;
        lofi = v.lf != 0 ? v.lf : eraLp;
        rngS = (uint32_t) (7 + prog.length());
        early.assign ((size_t) jsRound (0.08 * sr), 0.0f);
        const int taps = v.er < 6 ? 0 : 24;
        for (int k = 0; k < taps; ++k)
        {
            const long at = jsRoundI ((v.pd + rnd() * v.er) * sr / 1000);
            if (at < (long) early.size())
            {
                const double sign = rnd() < 0.5 ? -1 : 1;   // evaluated only when the tap lands, as in Hitline
                const double amp = std::pow (0.8, k / 3.0) * (0.5 + rnd());
                early[(size_t) at] = (float) (early[(size_t) at] + sign * amp);
            }
        }
        haveEarly = taps != 0;
        double mx = 0; for (auto& b : bands) mx = std::max (mx, b.first);
        len = jsRoundI (std::min (10.0, v.gate != 0 ? v.rt * dm + 0.3 : mx * 1.1 + 0.2) * sr);
    }
    else
    {
        static const std::map<juce::String, std::vector<std::pair<juce::String, juce::String>>> names {
            { "emt", { { "A", "EMT140_02" }, { "B", "EMT140_01" }, { "C", "EMT140_03" } } },
            { "lex", { { "Small Hall A", "L224_01" }, { "Chamber", "L224_02" }, { "Perc Plate", "L224_03" }, { "Room A", "L224_04" } } },
            { "city", { { "Re-mic", "SndC_01" }, { "Rooms", "SndC_02" }, { "Reverb", "SndC_03" } } } };
        juce::String irName = "EMT140_01";
        if (names.count (model))
        {
            const auto& list = names.at (model);
            irName = list[0].second;
            for (auto& [k, v] : list) if (k == prog) irName = v;
        }
        const auto L = irLibrary[juce::Identifier (irName)];
        if (! L.isObject() || ! L["bands"].isArray()) return {};   // the library isn't loaded: no reverb rather than a crash
        early = sincResample (decodeEarly (L["early"].toString()), (double) L["sr"], sr);
        haveEarly = true;
        double mx = 0;
        for (auto& b : *L["bands"].getArray()) { bands.push_back ({ std::max (0.08, (double) b[0] * dm), (double) b[1] }); mx = std::max (mx, bands.back().first); }
        len = jsRoundI (std::min (10.0, mx * 1.1 + 0.2) * sr);
    }
    const long f0 = jsRoundI (0.06 * sr), f1 = jsRoundI (0.08 * sr);
    auto mk = [&] (uint32_t seed)
    {
        const auto tail = synthTail (len, sr, bands, seed, gateS);
        F32 h ((size_t) len);
        if (haveEarly)
        {
            double ea = 0, ta = 0;
            for (long i = f0; i < f1 && i < (long) early.size(); ++i) { ea += (double) early[(size_t) i] * early[(size_t) i]; ta += (double) tail[(size_t) i] * tail[(size_t) i]; }
            const double k = ta > 0 && ea > 0 ? std::sqrt (ea / ta) : 1;
            for (long i = 0; i < len; ++i)
            {
                const double w = i < f0 ? 0 : i >= f1 ? 1 : 0.5 - 0.5 * std::cos (M_PI * (i - f0) / (double) (f1 - f0));
                h[(size_t) i] = (float) (tail[(size_t) i] * k * w + (i < (long) early.size() ? early[(size_t) i] * (1 - w) * eL : 0));
            }
        }
        else h = tail;
        return h;
    };
    F32 Lh = mk (11), Rh = mk (29);
    if (haveEarly)
    {
        const long d = jsRoundI (0.0031 * sr);
        const double g = 0.55;
        F32 y ((size_t) len);
        for (long i = 0; i < len; ++i)
        {
            const double xi = i < (long) early.size() ? early[(size_t) i] : 0, yd = i >= d ? y[(size_t) (i - d)] : 0;
            const double xd = i >= d && i - d < (long) early.size() ? early[(size_t) (i - d)] : 0;
            y[(size_t) i] = (float) (-g * xi + xd + g * yd);
        }
        for (long i = 0; i < f1; ++i)
        {
            const double w = i < f0 ? 0 : 0.5 - 0.5 * std::cos (M_PI * (i - f0) / (double) (f1 - f0));
            Rh[(size_t) i] = (float) (Rh[(size_t) i] * w + y[(size_t) i] * (1 - w) * eL);
        }
    }
    auto fin = [&] (F32 h)
    {
        if (lofi != 0) h = lpN (h, lofi, sr, 2);
        F32 o (h.size() + (size_t) pre, 0.0f);
        std::copy (h.begin(), h.end(), o.begin() + pre);
        double e = 0;
        for (float v : o) e += (double) v * v;
        const double k = 1 / std::sqrt (e != 0 ? e : 1);
        for (auto& v : o) v = (float) (v * k);
        return o;
    };
    std::pair<F32, F32> res { fin (Lh), fin (Rh) };
    std::lock_guard<std::mutex> lock (irMutex);
    if (irCache.size() > 6) irCache.erase (irCache.begin());
    irCache[key] = res;
    return res;
}

static Kernel irKernel (const F32& h)
{
    size_t L = 1;
    while (L < h.size()) L <<= 1;
    Kernel k;
    k.h.assign (L, 0.0);
    for (size_t i = 0; i < h.size(); ++i) k.h[i] = h[i];
    return k;
}

Chs verbWet (const Chs& chs, double sr, const juce::var& p)
{
    const auto [IL, IR] = verbIR (p, sr);
    if (IL.empty()) return Chs (chs.size(), F32 (chs.empty() ? 0 : chs[0].size(), 0.0f));
    const bool st = chs.size() > 1;
    F32 L = convolve (chs[0], irKernel (IL)), R;
    if (st) R = convolve (chs[1], irKernel (IR));
    const double hp = orr (p, "hpf", 0), lp = orr (p, "lpf", 0);
    auto filt = [&] (F32 y) { if (hp > 20) y = hpN (y, hp, sr, 2); if (lp > 0 && lp < 19900) y = lpN (y, lp, sr, 2); return y; };
    L = filt (L);
    if (st) R = filt (R);
    if (st && nn (p, "width", 1) != 1)
    {
        const double w = clampD (nn (p, "width", 1), 0, 2);
        for (size_t q = 0; q < L.size(); ++q) { const double m = 0.5 * ((double) L[q] + R[q]), s = 0.5 * ((double) L[q] - R[q]) * w; L[q] = (float) (m + s); R[q] = (float) (m - s); }
    }
    return st ? Chs { L, R } : Chs { L };
}

Chs verbRender (const Chs& chs, double sr, const juce::var& p)
{
    const auto w = verbWet (chs, sr, p);
    const double mix = clampD (nn (p, "mix", 0.2), 0, 1), out = dbl (orr (p, "out", 0));
    Chs res;
    for (size_t c = 0; c < chs.size(); ++c)
    {
        const auto& x = chs[c];
        const auto& v = w[c];
        double ex = 0, ev = 0;
        for (size_t q = 0; q < x.size(); ++q) { ex += (double) x[q] * x[q]; ev += (double) v[q] * v[q]; }
        const double k = ev > 0 ? std::sqrt (ex / ev) : 0;
        F32 y (x.size());
        for (size_t q = 0; q < x.size(); ++q) y[q] = (float) ((x[q] * (1 - mix) + v[q] * k * mix) * out);
        res.push_back (std::move (y));
    }
    return res;
}

Chs roomRender (const F32& send, double sr, const juce::var& p) { return verbWet ({ send, send }, sr, p); }

Chs insRender (const Chs& chs, double sr, const juce::var& inserts, const InsertOptions& o, std::vector<F64>* compGr)
{
    Chs y = chs;
    const double tr = dbl (o.trim);
    if (tr != 1) for (auto& x : y) x = scale (x, tr);
    int slot = -1;
    if (auto* list = inserts.getArray())
        for (auto& s : *list)
        {
            ++slot;
            if (! s.isObject() || isFalse (s, "on")) continue;
            const auto pv = s.getProperty ("p", juce::var());
            const juce::var p = pv.isObject() ? pv : juce::var (new juce::DynamicObject());
            const auto type = str (s, "type");
            if (type == "eq") y = eqRender (y, sr, p);
            else if (type == "pultec") y = pultecRender (y, sr, p);
            else if (type == "width") y = widthRender (y, sr, p);
            else if (type == "gate")
            {
                const auto kt = p.getProperty ("keyT", juce::var());
                const Chs* key = nullptr;
                if (! kt.isVoid() && o.keys.count ((int) kt)) key = &o.keys.at ((int) kt);
                y = gateRender (y, sr, p, key, o.grCurves ? &(*o.grCurves)[slot] : nullptr);
            }
            else if (type == "comp") y = compRender (y, sr, p, o.ons.empty() ? nullptr : &o.ons, compGr, o.grCurves ? &(*o.grCurves)[slot] : nullptr);
            else if (type == "sat") y = satRender (y, sr, p);
            else if (type == "verb") y = verbRender (y, sr, p);
        }
    return y;
}
} // namespace ld::hl

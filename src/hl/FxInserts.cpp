#include "FxInserts.h"
#include <algorithm>
#include <map>

namespace ld::hl
{
// ---------------------------------------------------------------- EQ
Chs eqRender (const Chs& chs, double sr, const juce::var& p)
{
    std::vector<juce::var> bands;
    if (auto* a = p.getProperty ("bands", juce::var()).getArray())
        for (auto& b : *a) if (! isFalse (b, "on")) bands.push_back (b);
    const bool minP = str (p, "phase") == "min";
    const size_t N = kernN (sr, minP);
    const bool st = chs.size() > 1;
    const double out = dbl (orr (p, "out", 0));
    if (bands.empty()) { Chs r; for (auto& x : chs) r.push_back (out == 1 ? x : scale (x, out)); return r; }
    static const juce::StringArray places { "st", "mid", "side", "left", "right" };
    auto place = [&] (const juce::var& b) { const auto pl = str (b, "place"); return st ? (places.contains (pl) && pl.isNotEmpty() ? pl : juce::String ("st")) : juce::String ("st"); };
    auto isDyn = [] (const juce::var& b) { const auto d = b.getProperty ("dyn", juce::var()); return d.isObject() && truthy (d, "on"); };

    auto runPath = [&] (const F32& x, const juce::StringArray& which) -> F32
    {
        std::vector<juce::var> mine, stat, dyn;
        for (auto& b : bands) if (which.contains (place (b))) mine.push_back (b);
        if (mine.empty()) return x;
        for (auto& b : mine) (isDyn (b) ? dyn : stat).push_back (b);
        bool dynG = false;
        for (auto& b : dyn) if (orr (b, "g", 0) != 0) dynG = true;
        F32 y = (! stat.empty() || dynG)
            ? convolve (x, magToKernel (magGrid (sr, N, [&] (double f) { double m = 1; for (auto& b : stat) m *= bandMag (b, f); for (auto& b : dyn) if (orr (b, "g", 0) != 0) m *= bandMag (b, f); return m; }), N, minP))
            : x;
        for (auto& b : dyn)
        {
            const auto d = b.getProperty ("dyn", juce::var());
            const double range = orr (d, "range", -6), g = orr (b, "g", 0), g0 = dbl (g), gr = std::abs (g) > 0.5 ? g : range;
            auto* refObj = new juce::DynamicObject();
            for (auto& prop : b.getDynamicObject()->getProperties()) refObj->setProperty (prop.name, prop.value);
            const auto t = str (b, "type");
            refObj->setProperty ("type", t == "lowcut" || t == "highcut" || t == "notch" ? juce::String ("bell") : t);
            refObj->setProperty ("g", gr);
            const juce::var refBand (refObj);
            const double den = dbl (gr) - 1;
            const auto side = convolve (x, magToKernel (magGrid (sr, N, [&] (double f) { return std::max (0.0, (bandMag (refBand, f) - 1) / den); }), N, minP));
            const auto e = envDb (side, sr, nn (d, "atk", 3), nn (d, "rel", 80));
            const double thr = nn (d, "thr", -30), span = std::max (6.0, std::abs (range));
            for (size_t n = 0; n < y.size(); ++n)
            {
                const double over = clampD ((e[n] - thr) / span, 0, 1), sm = over * over * (3 - 2 * over), gt = dbl (g + range * sm);
                y[n] = (float) (y[n] + (gt - g0) * side[n]);
            }
        }
        return y;
    };

    F32 L = chs[0];
    if (! st) { auto y = runPath (L, { "st" }); return { out == 1 ? y : scale (y, out) }; }
    F32 R = chs[1];
    bool ms = false;
    for (auto& b : bands) if (place (b) == "mid" || place (b) == "side") ms = true;
    if (ms)
    {
        const size_t n = L.size();
        F32 M (n), Sd (n);
        for (size_t i = 0; i < n; ++i) { M[i] = (float) (0.5 * ((double) L[i] + R[i])); Sd[i] = (float) (0.5 * ((double) L[i] - R[i])); }
        const auto M2 = runPath (M, { "st", "mid" }), S2 = runPath (Sd, { "st", "side" });
        for (size_t i = 0; i < n; ++i) { L[i] = (float) ((double) M2[i] + S2[i]); R[i] = (float) ((double) M2[i] - S2[i]); }
    }
    else { L = runPath (L, { "st" }); R = runPath (R, { "st" }); }
    L = runPath (L, { "left" });
    R = runPath (R, { "right" });
    if (out != 1) { L = scale (L, out); R = scale (R, out); }
    return { L, R };
}

// ---------------------------------------------------------------- Tube EQ (Pultec EQP-1A / MEQ-5)
static double pulLaw (double v) { return std::pow (clampD (v, 0, 10) / 10, 1.15); }
static juce::var band (const char* type, double f, double g, double q)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", type); o->setProperty ("f", f); o->setProperty ("g", g); o->setProperty ("q", q);
    return juce::var (o);
}

double insertCurveDb (const juce::String& type, const juce::var& p, double f, int band)
{
    double m = 1;
    if (type == "pultec") m = pultecMag (p, f) * dbl (orr (p, "out", 0));
    else if (type == "eq")
    {
        if (auto* a = p.getProperty ("bands", juce::var()).getArray())
            for (int i = 0; i < a->size(); ++i)
                if ((band < 0 || band == i) && ! isFalse ((*a)[i], "on")) m *= bandMag ((*a)[i], f);
        if (band < 0) m *= dbl (orr (p, "out", 0));
    }
    return 20 * std::log10 (std::max (1e-9, m));
}

double pultecMag (const juce::var& p, double f)
{
    double m = 1;
    if (! isFalse (p, "eqp"))
    {
        const double F = orr (p, "lf", 60), b = pulLaw (orr (p, "lfBoost", 0)), a = pulLaw (orr (p, "lfAtten", 0)), x = b * a;
        if (b > 0) m *= bandMag (band ("lowshelf", F * (3.4 + 1.25 * x), 12.7 * b * (1 + 1.0 * x), 0.5), f);
        if (a > 0) m *= bandMag (band ("lowshelf", F * (3.8 + 10.1 * x), -19.3 * a * (1 - 0.3 * x), 0.42 + 0.24 * x), f);
        const double hb = pulLaw (orr (p, "hfBoost", 0));
        if (hb > 0) m *= bandMag (band ("bell", orr (p, "hf", 10) * 1000, 13.3 * hb, 0.3 + 0.45 * (1 - clampD (nn (p, "bw", 5), 0, 10) / 10)), f);
        const double ha = pulLaw (orr (p, "hfAtten", 0));
        if (ha > 0) m *= bandMag (band ("highshelf", 0.64 * orr (p, "hfa", 10) * 1000, -17 * ha, 0.44), f);
    }
    if (truthy (p, "meq"))
    {
        const double lp = pulLaw (orr (p, "lpk", 0));
        if (lp > 0) m *= bandMag (band ("bell", orr (p, "lpf", 300), 10.6 * lp, 0.5), f);
        const double dp = pulLaw (orr (p, "dip", 0)), df = orr (p, "dipf", 1000);
        if (dp > 0) m *= bandMag (band ("bell", df, -11.5 * dp, std::pow (df / 200, 0.26)), f);
        const double hp = pulLaw (orr (p, "hpk", 0));
        if (hp > 0) m *= bandMag (band ("bell", orr (p, "hpf", 3) * 1000, 10 * hp, 0.9), f);
    }
    if (! isFalse (p, "tube")) m /= std::sqrt (1 + (f / 64500) * (f / 64500));
    return m;
}

Chs pultecRender (const Chs& chs, double sr, const juce::var& p)
{
    // Precompute the curve once per bin (bandMag on vars is slow per call).
    const bool minP = str (p, "phase") != "lin";
    const size_t N = kernN (sr, minP);
    const auto K = magToKernel (magGrid (sr, N, [&] (double f) { return pultecMag (p, f); }), N, minP);
    const double out = dbl (orr (p, "out", 0));
    const double drive = isFalse (p, "tube") ? 0 : nn (p, "drive", 1), k2 = 0.0071 * drive, k3 = 0.029 * drive;
    const double lpA = std::exp (-2 * M_PI * 250 / sr), hpA = std::exp (-2 * M_PI * 5 / sr);
    Chs res;
    for (auto& x : chs)
    {
        auto y = convolve (x, K);
        if (drive > 0)
        {
            double l = 0, dcx = 0, dcy = 0;
            for (size_t n = 0; n < y.size(); ++n)
            {
                l = lpA * l + (1 - lpA) * y[n];
                const double h = k2 * l * l + k3 * l * l * l, hp = h - dcx + hpA * dcy;
                dcx = h; dcy = hp;
                y[n] = (float) (y[n] + hp);
            }
        }
        if (out != 1) for (auto& v : y) v = (float) (v * out);
        res.push_back (std::move (y));
    }
    return res;
}

// ---------------------------------------------------------------- Width
Chs widthRender (const Chs& chs, double sr, const juce::var& p, F32* corrCurve)
{
    if (chs.size() < 2) return chs;
    const auto& L = chs[0];
    const auto& R = chs[1];
    const size_t n = L.size();
    F32 M (n), Sd (n);
    for (size_t q = 0; q < n; ++q) { M[q] = (float) (0.5 * ((double) L[q] + R[q])); Sd[q] = (float) (0.5 * ((double) L[q] - R[q])); }
    const double fc = orr (p, "bassMono", 0);
    double ps0 = 0;
    for (size_t q = 0; q < n; ++q) ps0 += (double) Sd[q] * Sd[q];
    if (fc > 0)
    {
        const size_t N = kernN (sr, true);
        Sd = convolve (Sd, magToKernel (magGrid (sr, N, [&] (double f) { return 1 / std::sqrt (1 + std::pow (fc / f, 8)); }), N, true));
    }
    const double w = clampD (nn (p, "width", 1), 0, 2.5);
    // width by frequency: the side's highs (above highFrom) get their own width
    const double hf = orr (p, "highFrom", 0), hw = clampD (nn (p, "highWidth", w), 0, 2.5);
    if (hf > 0 && std::abs (hw - w) > 1e-9)
    {
        const size_t N = kernN (sr, true);
        // Linkwitz-Riley low part (24 dB/oct), the high part is what's left, so low + high is the side exactly
        const F32 lo = convolve (Sd, magToKernel (magGrid (sr, N, [&] (double f) { return 1 / (1 + std::pow (f / hf, 4)); }), N, true));
        for (size_t q = 0; q < n; ++q) Sd[q] = (float) ((double) lo[q] * w + ((double) Sd[q] - lo[q]) * hw);
    }
    const double ws = hf > 0 && std::abs (hw - w) > 1e-9 ? 1.0 : w;   // the width still to apply (already in Sd when split)
    double pm = 0, ps = 0;
    for (size_t q = 0; q < n; ++q) { pm += (double) M[q] * M[q]; ps += (double) Sd[q] * Sd[q]; }
    const double c = ! isFalse (p, "keep") && pm + ps0 > 0 ? std::sqrt ((pm + ps0) / (pm + ws * ws * ps + 1e-30)) : 1;
    F32 oL (n), oR (n);
    for (size_t q = 0; q < n; ++q) { const double m = M[q] * c, s = Sd[q] * ws * c; oL[q] = (float) (m + s); oR[q] = (float) (m - s); }
    if (corrCurve)
    {
        const size_t B = std::max<size_t> (1, (size_t) std::lround (sr / GR_RATE)), win = (size_t) (0.03 * sr);
        corrCurve->assign ((n + B - 1) / B, 1.0f);
        for (size_t b = 0; b < corrCurve->size(); ++b)
        {
            const size_t e = std::min (n, b * B + B), s0 = e > win ? e - win : 0;
            double a = 0, x = 0, y = 0;
            for (size_t q = s0; q < e; ++q) { a += (double) oL[q] * oR[q]; x += (double) oL[q] * oL[q]; y += (double) oR[q] * oR[q]; }
            (*corrCurve)[b] = x * y > 1e-14 ? (float) (a / std::sqrt (x * y)) : 1.0f;
        }
    }
    return { oL, oR };
}

double corrOf (const Chs& chs)
{
    if (chs.size() < 2) return 1;
    double a = 0, b = 0, c = 0;
    for (size_t q = 0; q < chs[0].size(); ++q) { a += (double) chs[0][q] * chs[1][q]; b += (double) chs[0][q] * chs[0][q]; c += (double) chs[1][q] * chs[1][q]; }
    return a / std::sqrt (b * c + 1e-30);
}

// ---------------------------------------------------------------- Gate / expander
static void biquadInPlace (F32& x, double f, bool hp, double sr)
{
    const double w = 2 * M_PI * f / sr, cs = std::cos (w), al = std::sin (w) / (2 * 0.707), a0 = 1 + al;
    const double b0 = (hp ? (1 + cs) : (1 - cs)) / 2 / a0, b1 = (hp ? -(1 + cs) : (1 - cs)) / a0, a1 = -2 * cs / a0, a2 = (1 - al) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (auto& s : x) { const double v = s, y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; s = (float) y; }
}

F32 grBlocks (const F32& gdb, double sr)
{
    const size_t B = std::max<size_t> (1, (size_t) std::lround (sr / GR_RATE));
    F32 r ((gdb.size() + B - 1) / B, 0.0f);
    for (size_t q = 0; q < gdb.size(); ++q) r[q / B] = std::max (r[q / B], -gdb[q]);
    return r;
}

Chs gateRender (const Chs& chs, double sr, const juce::var& p, const Chs* keyChs, F32* grCurve)
{
    const Chs& kc = keyChs ? *keyChs : chs;
    const size_t n = chs[0].size();
    F32 key (n, 0.0f);
    for (auto& x : kc) for (size_t q = 0; q < n; ++q) key[q] = (float) (key[q] + (double) x[q] / kc.size());
    const double hpf = orr (p, "hpf", 0), lpf = orr (p, "lpf", 0);
    if (hpf > 20) biquadInPlace (key, hpf, true, sr);
    if (lpf != 0 && lpf < sr / 2.2) biquadInPlace (key, lpf, false, sr);
    if (truthy (p, "listen"))
    {
        // the key as the gate hears it, on every channel
        Chs res; for (size_t c = 0; c < chs.size(); ++c) res.push_back (key);
        if (grCurve) grCurve->assign ((size_t) std::ceil ((double) n * GR_RATE / sr), 0.0f);
        return res;
    }
    const bool flip = truthy (p, "flip");
    const auto e = envDb (key, sr, 0.05, 8);
    const double thr = nn (p, "thr", -40), hy = nn (p, "hyst", 4), range = std::min (0.0, nn (p, "range", -40));
    const double ratio = nn (p, "ratio", 0) > 1.05 ? nn (p, "ratio", 0) : 0;
    const long hold = jsRoundI (nn (p, "hold", 40) * sr / 1000), look = jsRoundI (nn (p, "look", 1.5) * sr / 1000);
    F32 tgt (n);
    bool open = false;
    long hc = 0;
    for (size_t q = 0; q < n; ++q)
    {
        if (ratio != 0)
        {
            const double d = e[q] >= thr ? 0 : std::max (range, (e[q] - thr) * (ratio - 1));
            tgt[q] = (float) (flip ? range - d : d);
            continue;
        }
        if (e[q] >= thr) { open = true; hc = hold; }
        else if (open && e[q] < thr - hy) { if (hc > 0) --hc; else open = false; }
        else if (open && hc < hold) hc = hold;
        tgt[q] = (float) ((open != flip) ? 0 : range);
    }
    const double aA = std::exp (-1 / (std::max (0.02, nn (p, "atk", 0.3)) * sr / 1000)), aR = std::exp (-1 / (std::max (1.0, nn (p, "rel", 120)) * sr / 1000));
    F32 g (n);
    double v = range;
    for (size_t q = 0; q < n; ++q)
    {
        const double t = tgt[std::min (n - 1, q + (size_t) look)];
        // the gate's attack is how fast it reacts to a hit: opening normally, closing when flipped
        const bool fast = flip ? t < v : t > v;
        v = fast ? aA * v + (1 - aA) * t : aR * v + (1 - aR) * t;
        g[q] = (float) dbl (v);
    }
    if (grCurve)
    {
        F32 gd (n); for (size_t q = 0; q < n; ++q) gd[q] = (float) (20 * std::log10 (std::max (1e-9, (double) g[q])));
        *grCurve = grBlocks (gd, sr);
    }
    Chs res;
    for (auto& x : chs) { F32 y (n); for (size_t q = 0; q < n; ++q) y[q] = (float) ((double) x[q] * g[q]); res.push_back (std::move (y)); }
    return res;
}

// ---------------------------------------------------------------- Compressors
CompModel compModel (const juce::var& P)
{
    CompModel o;
    o.m = str (P, "model", "dist").toStdString();
    if (o.m.empty()) o.m = "dist";
    const auto pv = P.getProperty (juce::Identifier (o.m), juce::var());
    const juce::var p = pv.isObject() ? pv : juce::var (new juce::DynamicObject());
    o.out = orr (P, "out", 0);
    auto ratioStr = [&] (const char* d) { const auto v = p.getProperty ("ratio", juce::var()); return v.isVoid() ? juce::String (d) : v.toString(); };
    if (o.m == "dist")
    {
        static const std::map<juce::String, double> R { { "1", 1 }, { "2", 2 }, { "3", 3 }, { "4", 4 }, { "6", 6 }, { "10", 10 }, { "20", 20 }, { "nuke", 60 } };
        static const std::map<juce::String, double> K { { "1", 0 }, { "2", 14 }, { "3", 11 }, { "4", 9 }, { "6", 7 }, { "10", 4 }, { "20", 2 }, { "nuke", 0 } };
        const auto r = ratioStr ("4");
        o.inDb = (nn (p, "input", 5) - 5) * 4 + 6.3; o.thr = -14.5;
        o.ratio = R.count (r) && R.at (r) != 0 ? R.at (r) : 4; o.knee = K.count (r) ? K.at (r) : 6;
        o.atk = 0.05 * std::pow (600.0, clampD (nn (p, "attack", 5), 0, 10) / 10);
        o.rel = 30 * std::pow (3500.0 / 30, std::pow (clampD (nn (p, "release", 5), 0, 10) / 10, 1.4));
        o.h2 = -68; o.h3 = -61;
        const auto dist = str (p, "dist");
        if (dist == "d2") o.h2 = -47;
        if (dist == "d3") o.h3 = -47;
        if (r == "nuke") { o.over = 1; o.rel *= 0.8; }
        if (truthy (p, "hp")) o.hp = 100;
    }
    else if (o.m == "76")
    {
        static const std::map<juce::String, double> R { { "4", 4 }, { "8", 8 }, { "12", 12 }, { "20", 20 }, { "all", 24 } };
        static const std::map<juce::String, double> K { { "4", 6 }, { "8", 4 }, { "12", 3 }, { "20", 2 }, { "all", 0 } };
        const auto r = ratioStr ("4");
        o.inDb = nn (p, "input", 6.2); o.thr = -14;
        o.ratio = R.count (r) ? R.at (r) : 4; o.knee = K.count (r) ? K.at (r) : 4;
        o.atk = 0.8 * std::pow (0.02 / 0.8, (clampD (nn (p, "attack", 3), 1, 7) - 1) / 6) + 3;
        o.rel = 1500 * std::pow (50.0 / 1500, (clampD (nn (p, "release", 3), 1, 7) - 1) / 6);
        o.h2 = -49; o.h3 = -59; o.hiShelf = 0.4;
        if (r == "all") { o.over = 1.5; o.rel *= 0.4; o.h3 = -45; }
    }
    else if (o.m == "fair")
    {
        static const double TC[6][3] = { { 2, 25, 0 }, { 2, 90, 0 }, { 4, 150, 0 }, { 4, 300, 0 }, { 4, 90, 600 }, { 2, 150, 1500 } };
        const auto* tc = TC[(int) clampD (jsRound (nn (p, "tc", 2)), 1, 6) - 1];
        const double d = std::max (0.0, nn (p, "input", 0));
        o.inDb = nn (p, "input", 0); o.thr = -4 - 3 * clampD (nn (p, "thresh", 5), 0, 10); o.ratio = 3.2; o.knee = 20;
        o.atk = tc[0]; o.rel = tc[1]; o.rel2 = tc[2]; o.h2 = -64 + 4.5 * d; o.h3 = -67 + 3 * d; o.hiShelf = 0.8;
    }
    else if (o.m == "160")
    {
        const double r = nn (p, "ratio", 4);
        o.thr = nn (p, "thresh", -27); o.ratio = r >= 40 ? 60 : std::max (1.0, r); o.knee = truthy (p, "overEasy") ? 12 : 0;
        o.atk = 12; o.rel = 120; o.rms = 0.005; o.h3 = -46;
    }
    else
    {
        o.thr = nn (p, "thresh", -24); o.ratio = std::max (1.0, nn (p, "ratio", 3)); o.knee = nn (p, "knee", 6);
        o.atk = nn (p, "attack", 10); o.rel = nn (p, "release", 120);
    }
    return o;
}

double compCurve (const CompModel& o, double L)
{
    if (o.ratio <= 1.001) return 0;
    const double over = L - o.thr, s = 1 / o.ratio - 1, W = o.knee;
    double g = 2 * over < -W ? 0 : W > 0 && 2 * std::abs (over) <= W ? s * (over + W / 2) * (over + W / 2) / (2 * W) : s * over;
    if (o.over != 0 && over > 0) g -= o.over * 0.08 * over * over / (6 + over);
    return std::min (0.0, g);
}

F32 compGainCurve (const F32& det, double sr, const CompModel& o, double scHpf)
{
    const size_t n = det.size();
    F32 x = det;
    if (scHpf > 20) biquadInPlace (x, scHpf, true, sr);
    F32 lv (n);
    if (o.rms != 0)
    {
        const double a = std::exp (-1 / (o.rms * sr));
        double e = 0;
        for (size_t q = 0; q < n; ++q) { e = a * e + (1 - a) * (double) x[q] * x[q]; lv[q] = (float) (10 * std::log10 (2 * e + 1e-12)); }
    }
    else for (size_t q = 0; q < n; ++q) lv[q] = (float) (20 * std::log10 (std::abs ((double) x[q]) + 1e-9));
    const double aA = std::exp (-1 / (std::max (0.01, o.atk) * sr / 1000)), aR = std::exp (-1 / (std::max (1.0, o.rel) * sr / 1000));
    const double aR2 = o.rel2 != 0 ? std::exp (-1 / (o.rel2 * sr / 1000)) : 0;
    F32 g (n);
    double v = 0, slow = 0;
    for (size_t q = 0; q < n; ++q)
    {
        const double t = compCurve (o, lv[q]);
        if (t < v) v = aA * v + (1 - aA) * t; else v = aR * v + (1 - aR) * t;
        if (aR2 != 0) { slow = t < slow ? aA * slow + (1 - aA) * t : aR2 * slow + (1 - aR2) * t; slow = std::max (slow, v - 6); v = std::min (v, 0.6 * v + 0.4 * slow); }
        g[q] = (float) v;
    }
    return g;
}

Chs compRender (const Chs& chs, double sr, const juce::var& p, const std::vector<long>* ons, std::vector<F64>* grOut, F32* grCurve)
{
    const auto o = compModel (p);
    const size_t n = chs[0].size();
    const double gin = dbl (o.inDb);
    F32 det (n, 0.0f);
    for (auto& x : chs) for (size_t q = 0; q < n; ++q) { const double v = std::abs ((double) x[q] * gin); if (v > det[q]) det[q] = (float) v; }
    if (o.rms != 0)
    {
        std::fill (det.begin(), det.end(), 0.0f);
        for (auto& x : chs) for (size_t q = 0; q < n; ++q) det[q] = (float) (det[q] + (double) x[q] * gin / chs.size());
    }
    const auto gdb = compGainCurve (det, sr, o, std::max (orr (p, "scHpf", 0), o.hp));
    if (grCurve) *grCurve = grBlocks (gdb, sr);
    const double mix = clampD (nn (p, "mix", 1), 0, 1), out = dbl (o.out);
    const double a2 = o.h2 > -150 ? 2 * dbl (o.h2) / 0.25 : 0, a3 = o.h3 > -150 ? 4 * dbl (o.h3) / 0.0625 : 0, hpA = std::exp (-2 * M_PI * 8 / sr);
    std::optional<Kernel> shelf;
    if (o.hiShelf != 0)
    {
        const size_t N = kernN (sr, true);
        const auto b = band ("highshelf", 9000, o.hiShelf, 0.7);
        shelf = magToKernel (magGrid (sr, N, [&] (double f) { return bandMag (b, f); }), N, true);
    }
    Chs res;
    for (auto& x : chs)
    {
        F32 y (n);
        double dx = 0, dy = 0;
        for (size_t q = 0; q < n; ++q)
        {
            double w = x[q] * gin * dbl (gdb[q]);
            const double h = a2 * w * w, hp = h - dx + hpA * dy;
            dx = h; dy = hp;
            w += hp + a3 * w * w * w;
            y[q] = (float) w;
        }
        if (shelf) y = convolve (y, *shelf);
        for (size_t q = 0; q < n; ++q) y[q] = (float) ((mix * y[q] + (1 - mix) * x[q] * gin) * out);
        res.push_back (std::move (y));
    }
    if (ons && grOut && ! ons->empty())
    {
        F64 per;
        for (size_t k = 0; k < ons->size(); ++k)
        {
            const long on = (*ons)[k], e = k + 1 < ons->size() ? (*ons)[k + 1] : (long) n;
            double m = 0;
            for (long q = on; q < std::min (e, on + jsRoundI (0.3 * sr)); ++q) m = std::min (m, (double) gdb[(size_t) q]);
            per.push_back (-m);
        }
        grOut->push_back (per);
    }
    return res;
}
} // namespace ld::hl

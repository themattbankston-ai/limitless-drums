#include "Studio.h"
#include "BinaryData.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ld::ui
{
using visage::Brush;
using visage::Canvas;
using visage::Color;
using visage::Font;
using visage::Point;

//==============================================================================
// Tokens
namespace neon
{
    constexpr unsigned cyan = 0xff2ee6ff, magenta = 0xffff3da8, violet = 0xff8b5cff, amber = 0xffffb23e,
                       lime = 0xff7cff6b, blue = 0xff4c8dff, orange = 0xffff6a3d, gold = 0xffffd85a, red = 0xffff4d5e;
}

struct Chrome
{
    unsigned bgTop, bgBottom, plateTop, plateBottom, plateEdge, ink, inkQuiet, inkDim, well;
};

static Chrome chrome (bool day)
{
    if (day)
        return { 0xffc9ccd2, 0xffaeb2b9, 0xffeef0f3, 0xffcdd1d7, 0xff9ea3ab, 0xff15171b, 0xff474c55, 0xff7b818b, 0xffd9dce1 };
    return { 0xff101216, 0xff07080a, 0xff24272d, 0xff17191d, 0xff0a0b0d, 0xfff2f3f5, 0xffa3a9b3, 0xff5f6670, 0xff121418 };
}

//==============================================================================
// Fonts (embedded)
static Font fontOf (const char* data, int size, float pts)
{
    return Font (pts, reinterpret_cast<const unsigned char*> (data), size);
}
static Font uiFont (float s)   { return fontOf (BinaryData::InterDisplaySemiBold_otf, BinaryData::InterDisplaySemiBold_otfSize, s); }
static Font boldFont (float s) { return fontOf (BinaryData::InterDisplayBold_otf, BinaryData::InterDisplayBold_otfSize, s); }
static Font bodyFont (float s) { return fontOf (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize, s); }
static Font monoFont (float s) { return fontOf (BinaryData::DejaVuSansMono_ttf, BinaryData::DejaVuSansMono_ttfSize, s); }
static Font monoBold (float s) { return fontOf (BinaryData::DejaVuSansMonoBold_ttf, BinaryData::DejaVuSansMonoBold_ttfSize, s); }

static float textWidth (const Font& f, const std::string& s)
{
    return f.stringWidth (visage::String::convertToUtf32 (s));
}

//==============================================================================
// Drawing helpers: machined plates, screws, glass, LEDs, knobs
namespace draw
{
    static Color withAlpha (unsigned argb, float a) { return Color (argb).withAlpha (a); }

    static void text (Canvas& c, const std::string& s, const Font& f, Font::Justification j,
                      float x, float y, float w, float h)
    {
        c.text (visage::String (visage::String::convertToUtf32 (s)), f, j, x, y, w, h);
    }

    /** Letter-spaced caps, like silkscreen on a faceplate. */
    static float spaced (Canvas& c, const std::string& s, const Font& f, float x, float y, float h, float tracking)
    {
        for (char32_t ch : visage::String::convertToUtf32 (s))
        {
            const std::u32string one (1, ch);
            const float w = f.stringWidth (one);
            c.text (visage::String (one), f, Font::kLeft, x, y, w + 2, h);
            x += w + tracking;
        }
        return x;
    }

    static void screw (Canvas& c, float cx, float cy, float r = 4.5f)
    {
        c.setColor (0x88000000);
        c.circle (cx - r - 0.5f, cy - r + 0.5f, 2 * r + 1);
        c.setColor (Brush::linear (Color (0xffd3d7dd), Color (0xff4d5158), Point (cx - r, cy - r), Point (cx + r, cy + r)));
        c.circle (cx - r, cy - r, 2 * r);
        c.setColor (0xff1d1f23);
        c.segment (cx - r * 0.6f, cy - r * 0.6f, cx + r * 0.6f, cy + r * 0.6f, 1.4f, true);
    }

    /** A machined faceplate: brushed gradient, bevel highlight, dark edge, optional screws. */
    static void plate (Canvas& c, float x, float y, float w, float h, unsigned top, unsigned bottom,
                       unsigned edge, float radius = 10.0f, bool screws = true)
    {
        c.setColor (0x66000000);
        c.roundedRectangleShadow (x, y + 3, w, h, radius, 18);
        c.setColor (edge);
        c.roundedRectangle (x - 1, y - 1, w + 2, h + 2, radius + 1);
        c.setColor (Brush::linear (Color (top), Color (bottom), Point (x, y), Point (x, y + h)));
        c.roundedRectangle (x, y, w, h, radius);
        // brushed streaks
        unsigned seed = 7;
        for (float yy = y + 3; yy < y + h - 3; yy += 2.0f)
        {
            seed = seed * 1103515245u + 12345u;
            const bool light = (seed >> 16) & 1;
            c.setColor (light ? 0x07ffffff : 0x09000000);
            c.fill (x + radius * 0.5f, yy, w - radius, 1.0f);
        }
        c.setColor (0x1fffffff);
        c.fill (x + radius, y + 0.5f, w - 2 * radius, 1.0f);
        if (screws)
        {
            screw (c, x + 11, y + 11);
            screw (c, x + w - 11, y + 11);
            screw (c, x + 11, y + h - 11);
            screw (c, x + w - 11, y + h - 11);
        }
    }

    /** A recessed glass display. */
    static void glass (Canvas& c, float x, float y, float w, float h, float radius = 6.0f)
    {
        c.setColor (0x40ffffff);
        c.roundedRectangle (x - 1, y - 1, w + 2, h + 2.5f, radius + 1);
        c.setColor (0xff1d2028);
        c.roundedRectangle (x - 1, y - 1, w + 2, h + 2, radius + 1);
        c.setColor (Brush::linear (Color (0xff05060a), Color (0xff0b0d13), Point (x, y), Point (x, y + h)));
        c.roundedRectangle (x, y, w, h, radius);
        c.setColor (Brush::linear (Color (0x99000000), Color (0x00000000), Point (x, y), Point (x, y + 10)));
        c.roundedRectangle (x, y, w, std::min (h, 12.0f), radius);
    }

    static void led (Canvas& c, float cx, float cy, unsigned colour, bool on, float r = 3.0f)
    {
        if (on)
        {
            c.setColor (Brush::radial (withAlpha (colour, 0.55f), withAlpha (colour, 0.0f), Point (cx, cy), r * 4.0f));
            c.circle (cx - r * 4, cy - r * 4, r * 8);
            c.setColor (colour);
        }
        else
            c.setColor (0xff2a2d33);
        c.circle (cx - r, cy - r, 2 * r);
        if (on)
        {
            c.setColor (0xccffffff);
            c.circle (cx - r * 0.45f, cy - r * 0.7f, r * 0.8f);
        }
    }

    /** A machined knob with a glowing value ring. value in [0, 1]. */
    static void knob (Canvas& c, float cx, float cy, float r, float value, unsigned colour, bool light = false)
    {
        constexpr float pi = 3.14159265f;
        const float sweep = 1.5f * pi;               // 270 degrees
        // ring track
        c.setColor (light ? 0x33000000 : 0xff0c0d10);
        c.roundedArc (cx - r - 6, cy - r - 6, 2 * r + 12, 4.0f, pi, sweep);
        // value glow and ring
        const float v = std::clamp (value, 0.0f, 1.0f);
        const float startA = pi - sweep * 0.5f;
        const float valueSweep = std::max (0.02f, sweep * v);
        const float centre = startA + valueSweep * 0.5f;
        c.setColor (withAlpha (colour, 0.28f));
        c.roundedArc (cx - r - 9, cy - r - 9, 2 * r + 18, 10.0f, centre, valueSweep);
        c.setColor (colour);
        c.roundedArc (cx - r - 6, cy - r - 6, 2 * r + 12, 4.0f, centre, valueSweep);
        // body: knurled edge then a brushed silver cap
        c.setColor (0x99000000);
        c.circle (cx - r - 1, cy - r + 2, 2 * r + 2);
        c.setColor (Brush::linear (Color (0xff5a5e66), Color (0xff16181c), Point (cx, cy - r), Point (cx, cy + r)));
        c.circle (cx - r, cy - r, 2 * r);
        const float cr = r * 0.78f;
        c.setColor (Brush::radial (Color (0xfff4f6f8), Color (0xff8e939b), Point (cx - cr * 0.3f, cy - cr * 0.45f), cr * 1.5f));
        c.circle (cx - cr, cy - cr, 2 * cr);
        // pointer
        const float a = startA + sweep * v;   // radians, 0 = up, clockwise
        const float px = cx + std::sin (a) * cr * 0.85f, py = cy - std::cos (a) * cr * 0.85f;
        c.setColor (0xff1b1d21);
        c.segment (cx + std::sin (a) * cr * 0.25f, cy - std::cos (a) * cr * 0.25f, px, py, 2.4f, true);
    }

    static unsigned layerColour (int k, int n)
    {
        static const unsigned ramp[] = { neon::blue, neon::cyan, neon::lime, neon::gold, neon::orange, neon::magenta };
        if (n <= 1) return ramp[1];
        const float pos = (float) k / (float) (n - 1) * 5.0f;
        const int i = std::min (4, (int) pos);
        return Color (ramp[i]).interpolateWith (Color (ramp[i + 1]), pos - (float) i).toARGB();
    }

    static std::string fmt (const char* f, double v)
    {
        char b[64];
        std::snprintf (b, sizeof (b), f, v);
        return b;
    }

    static std::string clock (double t)
    {
        t = std::max (0.0, t);
        char b[32];
        std::snprintf (b, sizeof (b), "%d:%06.3f", (int) (t / 60.0), std::fmod (t, 60.0));
        return b;
    }

    static void micIcon (Canvas& c, float x, float y, unsigned colour)
    {
        c.setColor (colour);
        c.roundedRectangle (x + 3, y, 6, 9, 3);
        c.roundedRectangleBorder (x + 1, y + 3, 10, 9, 5, 1.3f);
        c.fill (x + 5.4f, y + 12, 1.3f, 3);
        c.fill (x + 2.5f, y + 14.5f, 7, 1.3f);
    }

    static void drumIcon (Canvas& c, float x, float y, unsigned colour)
    {
        c.setColor (colour);
        c.roundedRectangleBorder (x, y + 4, 13, 9, 2, 1.3f);
        c.fill (x, y + 4, 13, 2.2f);
        c.segment (x + 2, y + 1, x + 6, y + 4, 1.2f, true);
        c.segment (x + 11, y + 1, x + 7, y + 4, 1.2f, true);
    }
}

//==============================================================================
// Header unit: logo, steps, the movable transport group and the reference picker.
struct Studio::Header : visage::Frame
{
    // Laid out on the 1652 x 286 rendered faceplate (assets/ui/header-plate.png) and scaled to the window.
    explicit Header (Studio& s) : studio (s) {}
    Studio& studio;
    static constexpr float DW = 1652, DH = 286;
    float k = 1;   // scale
    std::map<std::string, visage::Svg> svgs;

    static constexpr unsigned stepColour[7] = { 0xff2ee6ff, 0xffff3da8, 0xff9b6bff, 0xff3dffd2, 0xffffb23e, 0xff7cff9b, 0xffff7ad9 };
    static constexpr const char* stepName[7] = { "Session", "Hits", "Align", "Tune", "Mix", "Level", "Export" };
    static constexpr const char* modeId[6] = { "hit", "layer", "sweep", "groove", "midi", "mono" };
    static constexpr const char* modeName[6] = { "HIT", "LAYER", "SWEEP", "GROOVE", "MIDI", "MONO" };
    static constexpr unsigned modeColour[6] = { 0xff2ee6ff, 0xff9b6bff, 0xffff3da8, 0xff3dffd2, 0xff7cff9b, 0xffff7ad9 };

    static std::string glowPath (const std::string& d, const std::string& stroke, float w)
    {
        return "<path d='" + d + "' fill='none' stroke='" + stroke + "' stroke-opacity='0.22' stroke-width='" + std::to_string (w * 2.6f) + "' stroke-linecap='round' stroke-linejoin='round'/>"
             + "<path d='" + d + "' fill='none' stroke='" + stroke + "' stroke-width='" + std::to_string (w) + "' stroke-linecap='round' stroke-linejoin='round'/>";
    }
    static std::string grad (const char* id, const char* a, const char* b, int w)
    {
        return std::string ("<linearGradient id='") + id + "' gradientUnits='userSpaceOnUse' x1='0' y1='0' x2='" + std::to_string (w) + "' y2='0'><stop offset='0' stop-color='" + a + "'/><stop offset='1' stop-color='" + b + "'/></linearGradient>";
    }
    static std::string stepSvg (int i)
    {
        auto wrap = [] (int w, int h, const std::string& body) { return "<svg xmlns='http://www.w3.org/2000/svg' width='" + std::to_string (w) + "' height='" + std::to_string (h) + "' viewBox='0 0 " + std::to_string (w) + " " + std::to_string (h) + "'>" + body + "</svg>"; };
        switch (i)
        {
            case 0: return wrap (64, 54, "<defs>" + grad ("g", "#2ee6ff", "#4c8dff", 64) + "</defs>" + glowPath ("M10 15a22 7 0 1 0 44 0a22 7 0 1 0 -44 0M10 15v20c0 4 10 7 22 7s22-3 22-7V15", "url(#g)", 2.6f) + "<path d='M14 20l6 20M50 20l-6 20M32 22v20' stroke='url(#g)' stroke-opacity='0.55' stroke-width='2.4' stroke-linecap='round' fill='none'/>");
            case 1: return wrap (74, 54, "<defs>" + grad ("g", "#ff3da8", "#ff7ad9", 74) + "</defs>" + glowPath ("M3 34h8l3-22 4 34 3-18 3 8 3-4h6l3-14 3 22 2-10 2 4h4l2-8 2 12 2-6h8", "url(#g)", 2.6f) + "<path d='M14 6l-3-4h6zM42 6l-3-4h6zM60 6l-3-4h6z' fill='#ff3da8'/>");
            case 2: return wrap (74, 58, "<defs>" + grad ("g", "#9b6bff", "#2ee6ff", 74) + "</defs><circle cx='14' cy='29' r='4' fill='#9b6bff'/>" + glowPath ("M24 19a14 14 0 0 1 0 20M33 12a24 24 0 0 1 0 34M43 6a34 34 0 0 1 0 46", "url(#g)", 2.4f) + "<path d='M53 2a44 44 0 0 1 0 54' fill='none' stroke='url(#g)' stroke-opacity='0.45' stroke-width='2.4' stroke-linecap='round'/>");
            case 3: return wrap (70, 54, "<defs>" + grad ("g", "#3dffd2", "#2ee6ff", 70) + "</defs>" + glowPath ("M26 6v14a9 9 0 0 0 18 0V6M35 29v19", "url(#g)", 2.6f) + "<path d='M4 27c4-8 8 8 12 0M54 27c4-8 8 8 12 0' fill='none' stroke='url(#g)' stroke-opacity='0.6' stroke-width='2.6' stroke-linecap='round'/>");
            case 4: return wrap (66, 54, "<defs>" + grad ("g", "#ffb23e", "#ff6a3d", 66) + "</defs><path d='M13 4v46M33 4v46M53 4v46' fill='none' stroke='url(#g)' stroke-opacity='0.45' stroke-width='2.6' stroke-linecap='round'/><rect x='7' y='28' width='12' height='7' rx='2' fill='#ffb23e'/><rect x='27' y='12' width='12' height='7' rx='2' fill='#ff8a3d'/><rect x='47' y='34' width='12' height='7' rx='2' fill='#ff6a3d'/>");
            case 5: return wrap (70, 54, "<defs>" + grad ("g", "#7cff9b", "#3dffd2", 70) + "</defs><g fill='url(#g)'><rect x='4' y='40' width='8' height='10' rx='2'/><rect x='15' y='34' width='8' height='16' rx='2'/><rect x='26' y='27' width='8' height='23' rx='2'/><rect x='37' y='20' width='8' height='30' rx='2'/><rect x='48' y='12' width='8' height='38' rx='2'/><rect x='59' y='4' width='8' height='46' rx='2'/></g>");
            default: return wrap (70, 54, "<defs>" + grad ("g", "#ff7ad9", "#9b6bff", 70) + "</defs>" + glowPath ("M4 28c5-14 9 14 14 0s9 14 14 0M40 28h24M54 18l10 10-10 10", "url(#g)", 2.6f));
        }
    }
    static std::string modeSvg (int i)
    {
        auto wrap = [] (const std::string& body) { return "<svg xmlns='http://www.w3.org/2000/svg' width='32' height='22' viewBox='0 0 32 22'>" + body + "</svg>"; };
        switch (i)
        {
            case 0: return wrap (glowPath ("M2 14h7l3-12 3 18 3-10 2 4h8", "#2ee6ff", 2.2f));
            case 1: return wrap (glowPath ("M4 4v14M10 4v14M16 4v14M22 4v14M28 4v14", "#9b6bff", 2.2f));
            case 2: return wrap (glowPath ("M3 17v3M9 13v7M15 9v11M21 5v15M27 2v18", "#ff3da8", 2.2f));
            case 3: return wrap ("<g fill='#3dffd2'><circle cx='4' cy='16' r='2.6'/><circle cx='11' cy='8' r='2'/><circle cx='15' cy='16' r='2.6'/><circle cx='22' cy='12' r='1.6'/><circle cx='26' cy='16' r='2.6'/></g>");
            case 4: return wrap ("<circle cx='16' cy='11' r='9' fill='none' stroke='#7cff9b' stroke-width='2'/><g fill='#7cff9b'><circle cx='11' cy='10' r='1.5'/><circle cx='21' cy='10' r='1.5'/><circle cx='13' cy='6' r='1.5'/><circle cx='19' cy='6' r='1.5'/><circle cx='16' cy='5' r='1.5'/></g>");
            default: return wrap ("<circle cx='11' cy='11' r='8' fill='none' stroke='#ff7ad9' stroke-width='2'/><circle cx='21' cy='11' r='8' fill='none' stroke='#ff7ad9' stroke-width='2'/>");
        }
    }
    void icon (Canvas& c, const std::string& key, const std::string& svgText, float x, float y, float w, float h, float alpha = 1)
    {
        const std::string id = key + "@" + std::to_string ((int) (w * 10)) + "x" + std::to_string ((int) (h * 10));
        auto it = svgs.find (id);
        if (it == svgs.end())
        {
            visage::Svg svg (reinterpret_cast<const unsigned char*> (svgText.data()), (int) svgText.size());
            svg.setDimensions ((int) std::round (w), (int) std::round (h), 1.0f);
            it = svgs.emplace (id, svg).first;
        }
        (void) alpha;
        c.svg (it->second, x, y, w, h);
    }

    float X (float v) const { return v * k; }
    void glowBox (Canvas& c, float x, float y, float w, float h, float r, unsigned col, float a, float blur)
    {
        c.setColor (draw::withAlpha (col, a));
        c.roundedRectangleShadow (X (x), X (y), X (w), X (h), X (r), X (blur));
    }

    void draw (Canvas& c) override
    {
        k = width() / DW;
        const auto& H = studio.hdr;
        c.image (reinterpret_cast<const unsigned char*> (BinaryData::headerplate_png), BinaryData::headerplate_pngSize, 0, 0, width(), height());

        // logo and session
        c.setColor (Color (0xffffb23e));
        c.ring (X (28), X (30), X (36), X (2));
        draw::text (c, "\xe2\x88\x9e", boldFont (X (26)), Font::kCenter, X (28), X (28), X (36), X (36));
        c.setColor (Color (0xfff2efe8));
        draw::spaced (c, "LIMITLESS", boldFont (X (21)), X (78), X (30), X (24), X (5.6f));
        c.setColor (Color (0xff9aa0a8));
        draw::spaced (c, "DRUMS", bodyFont (X (10)), X (79), X (54), X (14), X (7.8f));
        c.setColor (Color (0xff5f6670));
        draw::spaced (c, "SESSION", boldFont (X (10)), X (28), X (94), X (12), X (2));
        c.setColor (Color (0xffe6e3dc));
        draw::text (c, H.sessionLine, uiFont (X (15)), Font::kLeft, X (28), X (110), X (230), X (20));
        c.setColor (Color (0xff9aa0a8));
        draw::text (c, H.detailLine, bodyFont (X (12)), Font::kLeft, X (28), X (131), X (240), X (18));

        // the steps
        const int cur = (int) studio.currentStep;
        for (int i = 0; i < 7; ++i)
        {
            const float x = 272 + 138 * i, y = 18, w = 128, h = 142;
            const unsigned col = stepColour[i];
            const bool on = i == cur, dim = i == 6 && ! H.exportReady;
            if (on)
            {
                c.setColor (Brush::radial (draw::withAlpha (col, 0.34f), draw::withAlpha (col, 0.0f), Point (X (x + w / 2), X (y + h + 16)), X (w * 0.9f), X (h * 0.7f)));
                c.roundedRectangle (X (x), X (y), X (w), X (h), X (12));
                c.setColor (draw::withAlpha (col, 0.75f));
                c.roundedRectangleBorder (X (x), X (y), X (w), X (h), X (12), X (1.5f));
                c.setColor (col);
                c.roundedRectangle (X (x + 30), X (y), X (w - 60), X (2), X (1));
                glowBox (c, x + 28, y - 2, w - 56, 6, 3, col, 0.6f, 10);
            }
            const float a = dim ? 0.45f : 1.0f;
            c.setColor (draw::withAlpha (col, a));
            draw::text (c, stepName[i], uiFont (X (17)), Font::kCenter, X (x), X (y + 14), X (w), X (22));
            icon (c, "step" + std::to_string (i) + (dim ? "d" : ""), stepSvg (i), X (x + w / 2 - 36), X (y + 46), X (72), X (56));
            c.setColor (draw::withAlpha (col, dim ? 0.4f : 0.85f));
            const auto st = H.status[i];
            draw::spaced (c, st, monoBold (X (10.5f)), X (x + w / 2) - textWidth (monoBold (X (10.5f)), st) / 2 - st.size() * X (0.7f), X (y + h - 26), X (14), X (1.4f));
            if (H.done[i]) { glowBox (c, x + w - 17, y + 11, 6, 6, 3, col, 0.8f, 8); c.setColor (col); c.circle (X (x + w - 17), X (y + 11), X (6)); }
        }

        // Auto Prep: the selected mic as a heart monitor while it plays, a slow wave otherwise
        {
            const float x = 1244, y = 18, w = 214, h = 142, base = y + 84;
            c.setColor (draw::withAlpha (0xff2ee6ff, 0.06f));
            for (float gx = x + 14; gx < x + w; gx += 14) c.fill (X (gx), X (y + 2), X (1), X (h - 4));
            for (float gy = y + 14; gy < y + h; gy += 14) c.fill (X (x + 2), X (gy), X (w - 4), X (1));
            c.setColor (draw::withAlpha (0xff2ee6ff, 0.12f));
            c.fill (X (x + 2), X (base), X (w - 4), X (1));
            const auto& L = studio.monLevel;
            if (H.playing && ! L.empty())
            {
                const int n = (int) L.size(), head = studio.monHead;
                for (int col = 1; col < n; ++col)
                {
                    const int age = (head - col + n) % n;             // 0 at the head
                    if (age > n - 8) continue;                         // the gap ahead of the head
                    const float fade = age < n * 0.85f ? 1.0f - 0.55f * age / (float) n : 0.3f;
                    const float l0 = L[(size_t) col - 1], l1 = L[(size_t) col];
                    const auto amp = [&] (float v) { return std::min (66.0f, std::pow (std::max (0.0f, v), 0.5f) * 80.0f); };
                    const int vel = studio.monVel[(size_t) col];
                    const unsigned vc = draw::layerColour (vel, std::max (1, H.velocities));
                    const float px0 = x + 4 + (w - 8) * (col - 1) / (float) n, px1 = x + 4 + (w - 8) * col / (float) n;
                    const float y0 = base - amp (l0) * ((col & 1) ? 1.0f : -0.55f), y1 = base - amp (l1) * ((col & 1) ? -0.55f : 1.0f);
                    c.setColor (draw::withAlpha (vc, 0.25f * fade));
                    c.segment (X (px0), X (y0), X (px1), X (y1), X (4.5f), true);
                    c.setColor (draw::withAlpha (vc, fade));
                    c.segment (X (px0), X (y0), X (px1), X (y1), X (1.7f), true);
                }
                const float hx = x + 4 + (w - 8) * head / (float) n;
                glowBox (c, hx - 5, base - 5, 10, 10, 5, 0xffff7ad9, 0.7f, 12);
                c.setColor (Color (0xffffffff));
                c.circle (X (hx - 2.6f), X (base - 2.6f), X (5.2f));
            }
            else
            {
                // idle: a slow flowing wave, cyan to violet to pink
                const float tt = (float) studio.now;
                for (int pass = 0; pass < 2; ++pass)
                    for (int q = 0; q < 80; ++q)
                    {
                        const float u0 = q / 80.0f, u1 = (q + 1) / 80.0f;
                        auto yAt = [&] (float u) { return base + 2 + 14 * std::sin (u * 9.4f + tt * 1.3f) * std::sin (u * 3.1f + tt * 0.4f) + (pass ? 6 * std::sin (u * 13 + tt * 2.1f) : 0); };
                        const unsigned col = u0 < 0.55f ? 0xff2ee6ff : 0xffff3da8;
                        const float mix = u0 < 0.55f ? u0 / 0.55f : (u0 - 0.55f) / 0.45f;
                        const Color cc = u0 < 0.55f ? Color (col).interpolateWith (Color (0xff9b6bff), mix) : Color (0xff9b6bff).interpolateWith (Color (col), mix);
                        c.setColor (cc.withAlpha (pass ? 0.45f : 0.95f));
                        c.segment (X (x + 4 + (w - 8) * u0), X (yAt (u0)), X (x + 4 + (w - 8) * u1), X (yAt (u1)), X (pass ? 1.2f : 2.0f), true);
                    }
            }
            c.setColor (Color (0xffd9d4ea));
            draw::spaced (c, H.prepping ? "PREPARING\xe2\x80\xa6" : "AUTO PREP", boldFont (X (11)), X (x + 14), X (y + 10), X (14), X (2.2f));
            if (! H.monitorMic.empty())
            {
                const auto mic = "\xe2\x97\x8f " + H.monitorMic;
                c.setColor (Color (0xff2ee6ff));
                draw::text (c, mic, boldFont (X (10)), Font::kRight, X (x + 14), X (y + 10), X (w - 28), X (14));
            }
            c.setColor (Color (0xff7d7891));
            const std::string foot = H.playing && studio.hdr.vel >= 0 ? "VEL " + std::to_string (H.vel + 1) + (H.vel + 1 == H.velocities ? " \xc2\xb7 HARDEST" : "") : "EVERY STEP, FROM YOUR STANDARD";
            draw::spaced (c, foot, monoFont (X (10)), X (x + 14), X (y + h - 22), X (12), X (1.1f));
        }

        // undo / redo rings, history
        auto ringLight = [&] (float cx, float cy, float d, unsigned col, float a)
        {
            glowBox (c, cx - d / 2, cy - d / 2, d, d, d / 2, col, 0.45f * a, 18);
            c.setColor (draw::withAlpha (col, 0.9f * a));
            c.ring (X (cx - d / 2), X (cy - d / 2), X (d), X (2.5f));
        };
        if (H.canUndo) ringLight (1506, 62, 64, 0xff2ee6ff, 1);
        if (H.canRedo) ringLight (1578, 62, 64, 0xff2ee6ff, 1);
        auto arrow = [&] (float cx, float cy, bool redo, bool on)
        {
            c.setColor (on ? Color (0xffbff6ff) : Color (0xff4c5058));
            const float d = redo ? 1.0f : -1.0f;
            c.segment (X (cx - 6 * d), X (cy - 4), X (cx + 2 * d), X (cy - 4), X (2.2f), true);
            c.segment (X (cx - 6 * d), X (cy - 4), X (cx - 2 * d), X (cy - 8), X (2.2f), true);
            c.segment (X (cx - 6 * d), X (cy - 4), X (cx - 2 * d), X (cy), X (2.2f), true);
            c.segment (X (cx + 2 * d), X (cy - 4), X (cx + 6 * d), X (cy + 1), X (2.2f), true);
            c.segment (X (cx + 6 * d), X (cy + 1), X (cx + 2 * d), X (cy + 6), X (2.2f), true);
        };
        arrow (1506, 62, false, H.canUndo);
        arrow (1578, 62, true, H.canRedo);
        c.setColor (Color (0xff6a707a));
        draw::text (c, "UNDO", boldFont (X (9.5f)), Font::kCenter, X (1466), X (102), X (80), X (12));
        draw::text (c, "REDO", boldFont (X (9.5f)), Font::kCenter, X (1538), X (102), X (80), X (12));
        pill (c, 1496, 128, 110, "HISTORY \xc2\xb7 " + std::to_string (H.historyCount), studio.isHistoryVisible(), 0xff9b6bff);

        // transport
        c.setColor (Color (0xff8b9099));
        c.triangleLeft (X (44), X (226), X (12));
        c.fill (X (42), X (226), X (2), X (12));
        c.triangleRight (X (182), X (226), X (12));
        c.fill (X (194), X (226), X (2), X (12));
        ringLight (118, 232, 78, 0xff3dffd2, H.playing ? 1.0f : 0.7f);
        c.setColor (Color (0xffc9fff2));
        if (H.playing) { c.roundedRectangle (X (109), X (223), X (7), X (18), X (1.5f)); c.roundedRectangle (X (121), X (223), X (7), X (18), X (1.5f)); }
        else c.triangleRight (X (110), X (220), X (24));

        // readout: velocity and round robin first
        {
            const float x = 222, y = 200;
            c.setColor (Color (0xffa87a33));
            draw::spaced (c, "VEL", monoBold (X (13)), X (x + 16), X (y + 16), X (16), X (1.8f));
            draw::spaced (c, "RR", monoBold (X (13)), X (x + 96), X (y + 16), X (16), X (1.8f));
            c.setColor (Color (0xffffb23e));
            draw::text (c, H.vel >= 0 ? std::to_string (H.vel + 1) : "\xe2\x80\x93", monoBold (X (27)), Font::kLeft, X (x + 52), X (y + 6), X (40), X (32));
            draw::text (c, H.rr >= 0 ? std::to_string (H.rr + 1) : "\xe2\x80\x93", monoBold (X (27)), Font::kLeft, X (x + 124), X (y + 6), X (40), X (32));
            c.setColor (Color (0xff7b8190));
            std::string line = H.hit >= 0 ? "HIT " + std::to_string (H.hit + 1) + " / " + std::to_string (H.hits) : "NO HIT SELECTED";
            if (H.midiHi > 0) line += " \xc2\xb7 MIDI " + std::to_string (H.midiLo) + "\xe2\x80\x93" + std::to_string (H.midiHi);
            draw::spaced (c, line, monoFont (X (10)), X (x + 18), X (y + 46), X (12), X (1.2f));
            const auto& wv = H.hitWave;
            if (! wv.empty())
            {
                const float wx = x + 186, wy = y + 32, ww = 94;
                for (size_t q = 1; q < wv.size(); ++q)
                {
                    const float u = q / (float) wv.size();
                    const Color cc = u < 0.5f ? Color (0xff2ee6ff).interpolateWith (Color (0xff9b6bff), u * 2) : Color (0xff9b6bff).interpolateWith (Color (0xffff3da8), (u - 0.5f) * 2);
                    c.setColor (cc.withAlpha (1.0f - 0.6f * u));
                    const float a0 = wv[q - 1] * 21 * ((q & 1) ? 1 : -1), a1 = wv[q] * 21 * ((q & 1) ? -1 : 1);
                    c.segment (X (wx + ww * (q - 1) / wv.size()), X (wy + a0), X (wx + ww * q / wv.size()), X (wy + a1), X (1.6f), true);
                }
            }
        }

        // A / B / A then B
        {
            const float x = 528, y = 210, w = 150, h = 44;
            const int sel = H.chain ? 2 : H.processed ? 1 : 0;
            const char* lab[3] = { "A", "B", "A\xe2\x86\x92" "B" };
            for (int i = 0; i < 3; ++i)
            {
                const float sx = x + w / 3 * i;
                if (i == sel)
                {
                    c.setColor (Brush::radial (draw::withAlpha (0xff9b6bff, 0.6f), draw::withAlpha (0xff9b6bff, 0.0f), Point (X (sx + w / 6), X (y + h + 8)), X (w / 3 * 0.8f), X (h * 0.9f)));
                    c.fill (X (sx + 1), X (y + 1), X (w / 3 - 2), X (h - 2));
                }
                c.setColor (i == sel ? Color (0xffffffff) : Color (0xff5d636c));
                draw::text (c, lab[i], boldFont (X (i == 2 ? 12 : 15)), Font::kCenter, X (sx), X (y), X (w / 3), X (h));
            }
        }

        // play modes
        for (int i = 0; i < 6; ++i)
        {
            const float x = 694 + 84 * i, y = 201, w = 76, h = 62;
            const bool on = H.mode == modeId[i];
            const unsigned col = modeColour[i];
            if (on)
            {
                c.setColor (Brush::radial (draw::withAlpha (col, 0.36f), draw::withAlpha (col, 0.0f), Point (X (x + w / 2), X (y + h + 10)), X (w * 0.9f), X (h * 0.8f)));
                c.roundedRectangle (X (x), X (y), X (w), X (h), X (9));
                c.setColor (draw::withAlpha (col, 0.75f));
                c.roundedRectangleBorder (X (x), X (y), X (w), X (h), X (9), X (1.5f));
                if (H.playing) glowBox (c, x - 4, y - 4, w + 8, h + 8, 12, col, 0.25f, 16);
            }
            icon (c, std::string ("mode") + modeId[i], modeSvg (i), X (x + w / 2 - 16), X (y + 12), X (32), X (22));
            c.setColor (draw::withAlpha (col, on ? 1.0f : 0.65f));
            draw::text (c, modeName[i], boldFont (X (9.5f)), Font::kCenter, X (x), X (y + 40), X (w), X (12));
        }

        pill (c, 1208, 200, 92, "LOOP", H.loop, 0xff3dffd2);
        pill (c, 1306, 200, 92, "FOLLOW", H.follow, 0xff3dffd2);
        pill (c, 1208, 236, 92, "LM", H.lm, 0xff3dffd2);
        pill (c, 1306, 236, 92, "PADS", H.pads, 0xff3dffd2);

        // phase meter and master meter
        {
            const float x = 1418, y = 214, w = 150;
            c.setColor (Brush::horizontal (visage::Gradient (Color (0xffff4d5e), Color (0xffffb23e), Color (0xff1c1e23), Color (0xff3dffd2), Color (0xff2ee6ff))));
            c.roundedRectangle (X (x), X (y), X (w), X (8), X (4));
            if (std::isfinite (H.phase))
            {
                const float px = x + w * (0.5f + 0.5f * std::clamp (H.phase, -1.0f, 1.0f));
                glowBox (c, px - 2, y - 5, 4, 18, 2, 0xff3dffd2, 0.7f, 10);
                c.setColor (Color (0xffffffff));
                c.roundedRectangle (X (px - 1.5f), X (y - 5), X (3), X (18), X (1.5f));
            }
            c.setColor (Color (0xff5c626b));
            draw::text (c, "\xe2\x88\x92" "1", boldFont (X (9.5f)), Font::kLeft, X (x), X (y + 14), X (30), X (12));
            draw::text (c, "PHASE", boldFont (X (9.5f)), Font::kCenter, X (x), X (y + 14), X (w), X (12));
            draw::text (c, "+1", boldFont (X (9.5f)), Font::kRight, X (x), X (y + 14), X (w), X (12));
            auto bar = [&] (float bx, float v)
            {
                const float hgt = 52 * std::clamp ((20 * std::log10 (v + 1e-9f) + 60) / 60, 0.0f, 1.0f);
                if (hgt <= 0) return;
                c.setColor (Brush::vertical (Color (0xffff3da8), Color (0xff2ee6ff)));
                c.roundedRectangle (X (bx), X (258 - hgt), X (9), X (hgt), X (2));
            };
            bar (1596, H.meterL);
            bar (1610, H.meterR);
        }
    }

    void pill (Canvas& c, float x, float y, float w, const std::string& label, bool on, unsigned col)
    {
        c.setColor (Color (0xff0b0b0e));
        c.roundedRectangle (X (x), X (y), X (w), X (28), X (14));
        c.setColor (draw::withAlpha (0xffffffff, 0.05f));
        c.roundedRectangleBorder (X (x), X (y), X (w), X (28), X (14), X (1));
        const auto f = boldFont (X (10));
        const float tw = textWidth (f, label) + label.size() * X (1.4f), dx = x + w / 2 - (tw / k + 13) / 2;
        if (on) glowBox (c, dx - 3, y + 11, 6, 6, 3, col, 0.8f, 8);
        c.setColor (on ? Color (col) : Color (0xff24272d));
        c.circle (X (dx), X (y + 11), X (6));
        c.setColor (on ? Color (0xffe9fff9) : Color (0xff646a73));
        draw::spaced (c, label, f, X (dx + 13), X (y + 8), X (12), X (1.4f));
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        const float px = e.position.x / k, py = e.position.y / k;
        auto in = [&] (float x, float y, float w, float h) { return px >= x && px < x + w && py >= y && py < y + h; };
        auto near = [&] (float cx, float cy, float r) { return (px - cx) * (px - cx) + (py - cy) * (py - cy) < r * r; };
        for (int i = 0; i < 7; ++i)
            if (in (272 + 138 * i, 18, 128, 142)) { if (studio.onStepClicked) studio.onStepClicked ((Step) i); return; }
        if (in (1244, 18, 214, 142)) { if (studio.onAutoPrep) studio.onAutoPrep(); return; }
        if (near (1506, 62, 32)) { if (studio.onUndo) studio.onUndo(); return; }
        if (near (1578, 62, 32)) { if (studio.onRedo) studio.onRedo(); return; }
        if (in (1496, 128, 110, 28)) { studio.setHistoryVisible (! studio.isHistoryVisible()); return; }
        if (near (52, 232, 22)) { if (studio.onPrevHit) studio.onPrevHit(); return; }
        if (near (118, 232, 38)) { if (studio.onPlay) studio.onPlay(); return; }
        if (near (184, 232, 22)) { if (studio.onNextHit) studio.onNextHit(); return; }
        for (int i = 0; i < 3; ++i) if (in (528 + 50 * i, 210, 50, 44)) { if (studio.onAB) studio.onAB (i); return; }
        for (int i = 0; i < 6; ++i) if (in (694 + 84 * i, 201, 76, 62)) { if (studio.onMode) studio.onMode (modeId[i]); return; }
        const char* tg[4] = { "loop", "follow", "lm", "pads" };
        const float tx[4] = { 1208, 1306, 1208, 1306 }, ty[4] = { 200, 200, 236, 236 };
        for (int i = 0; i < 4; ++i) if (in (tx[i], ty[i], 92, 28)) { if (studio.onToggle) studio.onToggle (tg[i]); return; }
    }
};

//==============================================================================
// Glass screen: ruler, velocity strip, zones, glowing waveforms, hit markers, level chips.
struct Studio::Screen : visage::Frame
{
    struct Overlay : visage::Frame
    {
        explicit Overlay (Screen& s) : screen (s) { setIgnoresMouseEvents (true, false); }
        Screen& screen;
        void draw (Canvas& c) override { screen.drawOverlay (c); }
    };

    explicit Screen (Studio& s) : studio (s), overlay (*this)
    {
        addChild (&overlay);
        bloom.setBloomSize (18.0f);
        bloom.setBloomIntensity (0.9f);
        setPostEffect (&bloom);
    }

    Studio& studio;
    Overlay overlay;
    visage::BloomPostEffect bloom;
    bool scrubbing = false;

    static constexpr float rulerH = 24, stripH = 26;
    float top() const { return rulerH + (studio.data.layers.empty() ? 0.0f : stripH); }
    float timeToX (double t) const { return (float) ((t - studio.view.start) * studio.view.pixelsPerSecond); }
    double xToTime (float x) const { return studio.view.start + x / studio.view.pixelsPerSecond; }

    void resized() override { overlay.setBounds (0, 0, width(), height()); }

    void draw (Canvas& c) override
    {
        draw::glass (c, 0, 0, width(), height(), 8);
        if (! studio.data.hasSession) return drawEmpty (c);

        const auto& d = studio.data;
        const float lh = studio.laneHeight(), t0 = top();
        const int n = (int) d.tracks.size();

        // velocity zones
        for (int k = 0; k < (int) d.layers.size(); ++k)
        {
            const auto& L = d.layers[(size_t) k];
            const unsigned col = draw::layerColour (k, (int) d.layers.size());
            const float x0 = std::max (0.0f, timeToX (L.start)), x1 = std::min (width(), timeToX (L.end));
            if (x1 <= x0) continue;
            c.setColor (Brush::linear (draw::withAlpha (col, 0.10f), draw::withAlpha (col, 0.03f), Point (0, t0), Point (0, height())));
            c.fill (x0, rulerH, x1 - x0, height() - rulerH - 2);
            c.setColor (draw::withAlpha (col, 0.45f));
            c.fill (x0, rulerH, 1, height() - rulerH - 2);
        }

        drawRuler (c);

        // lanes
        c.setClampBounds (0, t0, width(), height() - t0 - 2);
        for (int i = 0; i < n; ++i)
        {
            const float y = t0 + i * lh - studio.view.scrollY;
            if (y + lh < t0 || y > height()) continue;
            drawLane (c, i, y, lh);
        }
        c.setClampBounds (0, 0, width(), height());

        if (! d.layers.empty()) drawLayerStrip (c);
    }

    void drawEmpty (Canvas& c)
    {
        const auto ch = chrome (false);
        const float cx = width() * 0.5f, cy = height() * 0.42f;
        if (studio.dropHover)
        {
            c.setColor (draw::withAlpha (neon::cyan, 0.10f));
            c.roundedRectangle (14, 14, width() - 28, height() - 28, 10);
            c.setColor (draw::withAlpha (neon::cyan, 0.8f));
            c.roundedRectangleBorder (14, 14, width() - 28, height() - 28, 10, 2);
        }
        c.setColor (Brush::radial (draw::withAlpha (neon::amber, 0.18f), draw::withAlpha (neon::amber, 0.0f), Point (cx, cy), 160));
        c.circle (cx - 160, cy - 160, 320);
        c.setColor (neon::amber);
        c.svg (reinterpret_cast<const unsigned char*> (BinaryData::marknight_svg), BinaryData::marknight_svgSize, cx - 56, cy - 56, 112, 112);
        c.setColor (ch.ink);
        draw::text (c, "Drop a folder of drum tracks here", uiFont (20), Font::kCenter, 0, cy + 76, width(), 30);
        c.setColor (ch.inkQuiet);
        draw::text (c, "WAV or AIFF from any DAW. Tracks sort into kick, snare, toms, overheads, then rooms, and get named for you.",
                    bodyFont (13), Font::kCenter, 0, cy + 108, width(), 22);
    }

    double gridStep() const
    {
        static const double steps[] = { 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120 };
        for (auto s : steps) if (s * studio.view.pixelsPerSecond >= 80) return s;
        return 300;
    }

    void drawRuler (Canvas& c)
    {
        const double step = gridStep();
        const auto f = monoFont (10.5f);
        double t = std::floor (studio.view.start / step) * step;
        for (; t < xToTime (width()); t += step)
        {
            const float x = timeToX (t);
            if (x < 0) continue;
            c.setColor (0x0dffffff);
            c.fill (x, rulerH, 1, height() - rulerH - 2);
            c.setColor (0x40ffffff);
            c.fill (x, rulerH - 7, 1, 7);
            c.setColor (0xff7f8894);
            const auto label = step >= 1 ? draw::fmt ("%.0fs", t) : draw::fmt (step >= 0.1 ? "%.1fs" : "%.2fs", t);
            draw::text (c, label, f, Font::kLeft, x + 4, 2, 60, rulerH - 6);
        }
        c.setColor (0x22ffffff);
        c.fill (0, rulerH - 1, width(), 1);
    }

    void drawLayerStrip (Canvas& c)
    {
        const auto& d = studio.data;
        const auto f = boldFont (11);
        for (int k = 0; k < (int) d.layers.size(); ++k)
        {
            const auto& L = d.layers[(size_t) k];
            const unsigned col = draw::layerColour (k, (int) d.layers.size());
            const float x0 = std::max (1.0f, timeToX (L.start) + 2), x1 = std::min (width() - 2, timeToX (L.end) - 2);
            if (x1 - x0 < 8) continue;
            c.setColor (draw::withAlpha (col, 0.16f));
            c.roundedRectangle (x0, rulerH + 2, x1 - x0, stripH - 9, 3);
            c.setColor (draw::withAlpha (col, 0.9f));
            c.fill (x0, rulerH + 2, 2, stripH - 9);
            std::string label = "VEL " + std::to_string (k + 1) + "  " + std::to_string (L.midiLow) + "–" + std::to_string (L.midiHigh)
                                + "  " + std::to_string (L.numHits) + " RR";
            if (! L.note.empty()) label += "  " + L.note;
            c.setClampBounds (x0, rulerH, x1 - x0, stripH);
            c.setColor (col);
            draw::text (c, label, f, Font::kLeft, x0 + 8, rulerH + 2, x1 - x0, stripH - 9);
            c.setClampBounds (0, 0, width(), height());
        }
        // hit markers
        for (int h = 0; h < (int) d.hits.size(); ++h)
        {
            const float x = timeToX (d.hits[(size_t) h].time);
            if (x < -6 || x > width() + 6) continue;
            const unsigned col = draw::layerColour (d.hits[(size_t) h].layer, (int) d.layers.size());
            const bool sel = h == studio.selectedHit;
            c.setColor (sel ? 0xffffffff : col);
            const float s = sel ? 9.0f : 7.0f;
            c.triangleDown (x - s * 0.5f, rulerH + stripH - 7, s);
        }
    }

    void drawLane (Canvas& c, int i, float y, float lh)
    {
        const auto& d = studio.data;
        const auto& tr = d.tracks[(size_t) i];
        const float mid = y + lh * 0.5f;

        c.setColor (0x0affffff);
        c.fill (0, y + lh - 1, width(), 1);
        c.setColor (draw::withAlpha (tr.colourA, 0.28f));
        c.fill (0, mid, width(), 1);

        float lanePeak = 0.0f;
        for (float p : tr.peaks) lanePeak = std::max (lanePeak, p);
        const float vz = lanePeak > 0 ? std::clamp (0.9f / lanePeak, 1.0f, 12.0f) : 1.0f;
        const float amp = lh * 0.44f;
        const double bps = tr.blocksPerSecond, pps = studio.view.pixelsPerSecond;
        const int nb = (int) tr.peaks.size();

        const Brush core = Brush::linear (Color (tr.colourA), Color (tr.colourB), Point (0, y + lh * 0.1f), Point (0, y + lh * 0.9f));
        const Color glow = Color (tr.colourA).withAlpha (0.22f);
        const float dim = tr.mute ? 0.25f : 1.0f;

        for (int x = 0; x < (int) width(); ++x)
        {
            const double t = studio.view.start + x / pps;
            int b0 = (int) (t * bps), b1 = (int) ((t + 1.0 / pps) * bps);
            if (b1 < 0 || b0 >= nb) continue;
            b0 = std::max (0, b0);
            b1 = std::clamp (b1, b0 + 1, nb);
            float p = 0.0f;
            for (int b = b0; b < b1; ++b) p = std::max (p, tr.peaks[(size_t) b]);
            const float a = std::min (1.0f, p * vz) * amp;
            if (a < 0.6f) continue;
            c.setColor (glow.withAlpha (0.22f * dim));
            c.fill ((float) x - 1, mid - a * 1.06f, 3, a * 2.12f);
            c.setColor (core);
            if (tr.mute) c.setColor (Color (0xff5a606a));
            c.fill ((float) x, mid - a, 1, a * 2);
        }

        // level chips at the end of each velocity zone
        if (i < (int) (d.layers.empty() ? 0 : d.layers[0].micDb.size()))
        {
            const auto f = monoBold (11);
            for (auto& L : d.layers)
            {
                const float x1 = timeToX (L.end);
                if (x1 < 40 || x1 > width() + 40) continue;
                const auto txt = draw::fmt ("%.1f", (double) L.micDb[(size_t) i]);
                const float w = textWidth (f, txt) + 10;
                c.setColor (0xb3000000);
                c.roundedRectangle (x1 - w - 6, y + lh - 22, w, 16, 3);
                c.setColor (i == d.referenceTrack ? neon::amber : 0xffc9ced6);
                draw::text (c, txt, f, Font::kCenter, x1 - w - 6, y + lh - 22, w, 16);
            }
        }
    }

    void drawOverlay (Canvas& c)
    {
        if (! studio.data.hasSession) return;
        const auto& d = studio.data;
        const float lh = studio.laneHeight(), t0 = top();

        // transient pulses: each hit flares as the playhead crosses it
        c.setBlendMode (visage::BlendMode::Add);
        c.setClampBounds (0, t0, width(), height() - t0 - 2);
        for (int h = 0; h < (int) d.hits.size(); ++h)
        {
            const double age = studio.now - studio.pulseStart[(size_t) h];
            if (age < 0 || age > 0.45) continue;
            const float k = 1.0f - (float) (age / 0.45);
            const float e = k * k;
            const float x = timeToX (d.hits[(size_t) h].time);
            for (int i = 0; i < (int) d.tracks.size(); ++i)
            {
                const float y = t0 + i * lh - studio.view.scrollY;
                if (y + lh < t0 || y > height()) continue;
                const auto& tr = d.tracks[(size_t) i];
                if (tr.mute) continue;
                const float r = lh * (0.55f + 0.35f * (1.0f - k));
                c.setColor (Brush::radial (Color (tr.colourA).withAlpha (0.55f * e), Color (tr.colourB).withAlpha (0.0f), Point (x, y + lh * 0.5f), r));
                c.circle (x - r, y + lh * 0.5f - r, 2 * r);
                c.setColor (Color (0xffffffff).withAlpha (0.55f * e));
                c.fill (x - 0.5f, y + lh * 0.12f, 1.5f, lh * 0.76f);
            }
        }
        c.setClampBounds (0, 0, width(), height());
        c.setBlendMode (visage::BlendMode::Alpha);

        // playhead
        const float px = timeToX (studio.playhead);
        if (px >= -10 && px <= width() + 10)
        {
            c.setColor (Brush::horizontal (Color (neon::amber).withAlpha (0.0f), Color (neon::amber).withAlpha (0.0f)));
            c.setColor (Color (neon::amber).withAlpha (0.18f));
            c.fill (px - 5, rulerH, 10, height() - rulerH - 2);
            c.setColor (Color (neon::amber).withAlpha (0.4f));
            c.fill (px - 2, rulerH, 4, height() - rulerH - 2);
            c.setColor (0xfffff0d0);
            c.fill (px - 0.75f, rulerH, 1.5f, height() - rulerH - 2);
            c.setColor (neon::amber);
            c.triangleDown (px - 6, rulerH - 9, 12);
        }
    }

    int hitNear (float x) const
    {
        int best = -1;
        float bestD = 7.0f;
        for (int h = 0; h < (int) studio.data.hits.size(); ++h)
        {
            const float dx = std::abs (timeToX (studio.data.hits[(size_t) h].time) - x);
            if (dx < bestD) { bestD = dx; best = h; }
        }
        return best;
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        if (! studio.data.hasSession) return;
        if (e.position.y < rulerH)
        {
            scrubbing = true;
            if (studio.onSeek) studio.onSeek (std::max (0.0, xToTime (e.position.x)));
            return;
        }
        const int h = hitNear (e.position.x);
        studio.selectedHit = h;
        if (studio.onSeek) studio.onSeek (std::max (0.0, h >= 0 ? studio.data.hits[(size_t) h].time : xToTime (e.position.x)));
        studio.redrawAll();
    }

    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (scrubbing && studio.onSeek) studio.onSeek (std::max (0.0, xToTime (e.position.x)));
    }

    void mouseUp (const visage::MouseEvent&) override { scrubbing = false; }

    bool mouseWheel (const visage::MouseEvent& e) override
    {
        if (! studio.data.hasSession) return false;
        const float dx = e.precise_wheel_delta_x != 0 ? e.precise_wheel_delta_x : e.wheel_delta_x * 40;
        const float dy = e.precise_wheel_delta_y != 0 ? e.precise_wheel_delta_y : e.wheel_delta_y * 40;
        if (e.isMainModifier())
            studio.zoomBy (std::pow (1.01, (double) dy), xToTime (e.position.x));
        else if (std::abs (dx) > std::abs (dy) || e.isShiftDown())
        {
            studio.view.start -= (std::abs (dx) > std::abs (dy) ? dx : dy) / studio.view.pixelsPerSecond;
            studio.zoomBy (1.0, 0.0);
        }
        else
        {
            const float content = (float) studio.data.tracks.size() * studio.laneHeight();
            studio.view.scrollY = std::clamp (studio.view.scrollY - dy, 0.0f, std::max (0.0f, content - (height() - top())));
            studio.redrawAll();
        }
        return true;
    }
};

//==============================================================================
// Track strips, lined up with the lanes.
struct Studio::Strips : visage::Frame
{
    explicit Strips (Studio& s) : studio (s) {}
    Studio& studio;

    struct Box { float x, y, w, h; bool contains (Point p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; } };
    Box strip (int i) const
    {
        const float lh = studio.laneHeight();
        return { 0, studio.screenContentTop() + i * lh - studio.view.scrollY + 3, width(), lh - 6 };
    }
    Box soloBox (int i) const { auto b = strip (i); return { b.w - 54, b.y + b.h - 24, 22, 17 }; }
    Box muteBox (int i) const { auto b = strip (i); return { b.w - 28, b.y + b.h - 24, 22, 17 }; }

    void draw (Canvas& c) override
    {
        const auto ch = chrome (studio.day);
        const auto& d = studio.data;
        c.setClampBounds (0, studio.screenContentTop(), width(), height() - studio.screenContentTop());
        for (int i = 0; i < (int) d.tracks.size(); ++i)
        {
            const auto b = strip (i);
            if (b.y + b.h < 0 || b.y > height()) continue;
            const auto& tr = d.tracks[(size_t) i];

            c.setColor (0x55000000);
            c.roundedRectangleShadow (b.x, b.y + 2, b.w, b.h, 7, 10);
            c.setColor (Brush::linear (Color (studio.day ? 0xffe6e8ec : 0xff23262c), Color (studio.day ? 0xffcfd3d9 : 0xff181a1e),
                                       Point (0, b.y), Point (0, b.y + b.h)));
            c.roundedRectangle (b.x, b.y, b.w, b.h, 7);
            c.setColor (studio.day ? 0x30000000 : 0x12ffffff);
            c.roundedRectangleBorder (b.x, b.y, b.w, b.h, 7, 1);

            // colour bar with glow
            c.setColor (draw::withAlpha (tr.colourA, 0.35f));
            c.roundedRectangle (b.x + 5, b.y + 8, 7, b.h - 16, 3);
            c.setColor (Brush::linear (Color (tr.colourA), Color (tr.colourB), Point (0, b.y + 8), Point (0, b.y + b.h - 8)));
            c.roundedRectangle (b.x + 7, b.y + 9, 3, b.h - 18, 1.5f);

            c.setColor (ch.ink);
            draw::text (c, tr.name, uiFont (14.5f), Font::kLeft, b.x + 20, b.y + 6, b.w - 26, 20);
            if (b.h > 44)
            {
                if (tr.isDrumIcon) draw::drumIcon (c, b.x + 20, b.y + 29, draw::withAlpha (tr.colourA, 0.9f).toARGB());
                else draw::micIcon (c, b.x + 21, b.y + 28, draw::withAlpha (tr.colourA, 0.9f).toARGB());
                c.setColor (ch.inkQuiet);
                draw::text (c, tr.model.empty() ? "—" : tr.model, bodyFont (12), Font::kLeft, b.x + 38, b.y + 27, b.w - 44, 18);
            }
            if (b.h > 66)
            {
                c.setColor (ch.inkDim);
                draw::text (c, tr.stereo ? "stereo" : "mono", bodyFont (10.5f), Font::kLeft, b.x + 20, b.y + 47, 60, 14);
            }

            auto button = [&] (Box r, const char* label, bool on, unsigned onColour)
            {
                if (on)
                {
                    c.setColor (draw::withAlpha (onColour, 0.45f));
                    c.roundedRectangleShadow (r.x, r.y, r.w, r.h, 4, 10);
                    c.setColor (onColour);
                }
                else
                    c.setColor (studio.day ? 0xffbfc3ca : 0xff2c2f36);
                c.roundedRectangle (r.x, r.y, r.w, r.h, 4);
                c.setColor (on ? 0xff1a1306 : ch.inkQuiet);
                draw::text (c, label, boldFont (10.5f), Font::kCenter, r.x, r.y, r.w, r.h);
            };
            button (soloBox (i), "S", tr.solo, neon::gold);
            button (muteBox (i), "M", tr.mute, neon::red);
        }
        c.setClampBounds (0, 0, width(), height());
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        auto& d = studio.data;
        for (int i = 0; i < (int) d.tracks.size(); ++i)
        {
            if (soloBox (i).contains (e.position)) { if (studio.onSolo) studio.onSolo (i, ! d.tracks[(size_t) i].solo); return; }
            if (muteBox (i).contains (e.position)) { if (studio.onMute) studio.onMute (i, ! d.tracks[(size_t) i].mute); return; }
            if (strip (i).contains (e.position) && e.repeatClickCount() >= 2 && studio.onRename) { studio.onRename (i); return; }
            if (strip (i).contains (e.position)) { studio.selectedTrack = i; if (studio.onSelectTrack) studio.onSelectTrack (i); studio.redrawAll(); return; }
        }
    }

    bool mouseWheel (const visage::MouseEvent& e) override
    {
        const float dy = e.precise_wheel_delta_y != 0 ? e.precise_wheel_delta_y : e.wheel_delta_y * 40;
        const float content = (float) studio.data.tracks.size() * studio.laneHeight();
        const float visible = height() - studio.screenContentTop();
        studio.view.scrollY = std::clamp (studio.view.scrollY - dy, 0.0f, std::max (0.0f, content - visible));
        studio.redrawAll();
        return true;
    }
};

//==============================================================================
// Mix: the full console, one strip per mic with a real fader and pan.
struct Studio::Console : visage::Frame
{
    explicit Console (Studio& s) : studio (s) {}
    Studio& studio;
    std::vector<float> meters;
    int dragFader = -1, dragPan = -1;
    float dragStartY = 0, dragStartValue = 0;

    static float dbToPos (float db) { return db <= -100 ? 0.0f : std::cbrt (std::pow (10.0f, db / 20.0f) / 2.0f); }
    static float posToDb (float p) { return p <= 0.001f ? -100.0f : 20.0f * std::log10 (2.0f * p * p * p); }

    float stripW() const { return std::clamp ((width() - 24) / std::max (1.0f, (float) studio.data.tracks.size()), 78.0f, 150.0f); }

    void draw (Canvas& c) override
    {
        const auto ch = chrome (studio.day);
        draw::glass (c, 0, 0, width(), height(), 8);
        const auto& d = studio.data;
        meters.resize (d.tracks.size(), 0.0f);
        const float sw = stripW();
        for (int i = 0; i < (int) d.tracks.size(); ++i)
        {
            const auto& tr = d.tracks[(size_t) i];
            const float x = 12 + i * sw, y = 12, w = sw - 8, h = height() - 24;
            c.setColor (Brush::linear (Color (0xff1e2126), Color (0xff121418), Point (0, y), Point (0, y + h)));
            c.roundedRectangle (x, y, w, h, 7);
            c.setColor (Brush::linear (Color (tr.colourA), Color (tr.colourB), Point (x, 0), Point (x + w, 0)));
            c.roundedRectangle (x + 6, y + 6, w - 12, 4, 2);
            c.setColor (ch.ink);
            draw::text (c, tr.name, uiFont (12), Font::kCenter, x, y + 14, w, 18);
            c.setColor (0xff8a919c);
            draw::text (c, tr.model, bodyFont (10.5f), Font::kCenter, x, y + 31, w, 14);

            draw::knob (c, x + w * 0.5f, y + 72, 13, tr.pan * 0.5f + 0.5f, tr.colourA);
            c.setColor (0xff8a919c);
            draw::text (c, std::abs (tr.pan) < 0.01f ? "C" : draw::fmt (tr.pan < 0 ? "L%.0f" : "R%.0f", std::abs (tr.pan) * 100),
                        monoFont (10), Font::kCenter, x, y + 94, w, 14);

            // fader slot and meter
            const float fy = y + 120, fh = h - 190;
            const float slotX = x + w * 0.5f - 14;
            c.setColor (0xff050608);
            c.roundedRectangle (slotX, fy, 4, fh, 2);
            const float m = meters[(size_t) i];
            const float mx = x + w * 0.5f + 12;
            c.setColor (0xff050608);
            c.roundedRectangle (mx, fy, 6, fh, 2);
            if (m > 0.001f)
            {
                const float mh = fh * std::min (1.0f, m);
                c.setColor (Brush::linear (Color (neon::lime), Color (neon::red), Point (0, fy + fh), Point (0, fy)));
                c.roundedRectangle (mx, fy + fh - mh, 6, mh, 2);
            }
            const float pos = dbToPos (tr.volumeDb);
            const float capY = fy + fh * (1.0f - pos);
            c.setColor (0x99000000);
            c.roundedRectangle (slotX - 14, capY - 9, 32, 20, 3);
            c.setColor (Brush::linear (Color (0xffe9ecf0), Color (0xff8d939b), Point (0, capY - 10), Point (0, capY + 10)));
            c.roundedRectangle (slotX - 14, capY - 10, 32, 20, 3);
            c.setColor (0xff1b1d21);
            c.fill (slotX - 12, capY - 0.5f, 28, 1.5f);

            c.setColor (ch.ink);
            draw::text (c, tr.volumeDb <= -99 ? "-inf" : draw::fmt ("%+.1f", (double) tr.volumeDb), monoFont (11), Font::kCenter, x, fy + fh + 8, w, 16);

            auto button = [&] (float bx, const char* label, bool on, unsigned col)
            {
                c.setColor (on ? col : 0xff2c2f36);
                c.roundedRectangle (bx, y + h - 34, 24, 20, 4);
                c.setColor (on ? 0xff1a1306 : 0xffa3a9b3);
                draw::text (c, label, boldFont (11), Font::kCenter, bx, y + h - 34, 24, 20);
            };
            button (x + w * 0.5f - 27, "S", tr.solo, neon::gold);
            button (x + w * 0.5f + 3, "M", tr.mute, neon::red);
        }
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        const auto& d = studio.data;
        const float sw = stripW();
        const int i = (int) ((e.position.x - 12) / sw);
        if (i < 0 || i >= (int) d.tracks.size()) return;
        const float x = 12 + i * sw, w = sw - 8, y = 12, h = height() - 24;
        const auto& tr = d.tracks[(size_t) i];
        if (e.position.y > y + h - 34 && e.position.y < y + h - 14)
        {
            if (e.position.x < x + w * 0.5f) { if (studio.onSolo) studio.onSolo (i, ! tr.solo); }
            else if (studio.onMute) studio.onMute (i, ! tr.mute);
            return;
        }
        if (e.position.y > y + 52 && e.position.y < y + 96)
        {
            if (e.repeatClickCount() >= 2) { if (studio.onPan) studio.onPan (i, 0.0f); return; }
            dragPan = i; dragStartY = e.position.y; dragStartValue = tr.pan;
            return;
        }
        if (e.position.y > y + 110 && e.position.y < height() - 70)
        {
            if (e.repeatClickCount() >= 2) { if (studio.onVolume) studio.onVolume (i, 0.0f); return; }
            dragFader = i; dragStartY = e.position.y; dragStartValue = dbToPos (tr.volumeDb);
        }
    }

    void mouseDrag (const visage::MouseEvent& e) override
    {
        const float fh = height() - 24 - 190;
        if (dragFader >= 0 && studio.onVolume)
            studio.onVolume (dragFader, posToDb (std::clamp (dragStartValue - (e.position.y - dragStartY) / fh, 0.0f, 1.0f)));
        if (dragPan >= 0 && studio.onPan)
            studio.onPan (dragPan, std::clamp (dragStartValue - (e.position.y - dragStartY) / 100.0f, -1.0f, 1.0f));
    }

    void mouseUp (const visage::MouseEvent&) override { dragFader = dragPan = -1; }
};

//==============================================================================
// The rack: units for the current step, room and master meters always at the bottom.
struct Studio::Rack : visage::Frame
{
    explicit Rack (Studio& s) : studio (s) {}
    Studio& studio;
    std::map<std::string, float> knobs { { "curve", 0.62f }, { "rrvary", 0.35f }, { "target", 0.5f }, { "lock", 1.0f },
                                         { "freq", 0.42f }, { "gain", 0.58f }, { "input", 0.55f }, { "ratio", 0.33f },
                                         { "send", 0.4f }, { "decay", 0.55f }, { "predelay", 0.3f }, { "mixr", 0.5f } };
    struct KnobHit { std::string id; float cx, cy, r; };
    std::vector<KnobHit> knobHits;
    std::string dragging;
    float dragY = 0, dragV = 0;
    float roomMeter = 0, masterMeter = 0;

    struct Style { unsigned top, bottom, title, accent; bool light = false; };
    static Style style (const std::string& kind)
    {
        if (kind == "align")    return { 0xff2a2d33, 0xff1a1c20, neon::cyan, neon::cyan };
        if (kind == "velocity") return { 0xff1a1a1c, 0xff0e0e10, neon::amber, neon::amber };
        if (kind == "tune")     return { 0xffe9ebee, 0xffb9bdc4, 0xff1b1d21, neon::lime, true };
        if (kind == "eq")       return { 0xff1d2a52, 0xff121a36, neon::cyan, neon::magenta };
        if (kind == "comp")     return { 0xff3a2a1c, 0xff21170f, neon::amber, neon::orange };
        if (kind == "room")     return { 0xff3a1f3c, 0xff221224, neon::magenta, neon::violet };
        return { 0xff202226, 0xff141518, 0xffd8dce2, neon::lime };
    }

    float unitTop (Canvas& c, float y, float h, const std::string& kind, const std::string& title, const std::string& sub)
    {
        const auto st = style (kind);
        draw::plate (c, 24, y, width() - 48, h, st.top, st.bottom, 0xff060708, 9, false);
        c.setColor (st.title);
        draw::spaced (c, title, boldFont (13), 42, y + 10, 20, 2.2f);
        c.setColor (st.light ? 0xff4a4f57 : 0xffa3a9b3);
        const auto f = boldFont (10);
        float w = 0; for (char32_t ch : visage::String::convertToUtf32 (sub)) w += f.stringWidth (std::u32string (1, ch)) + 1.4f;
        draw::spaced (c, sub, f, width() - 40 - w, y + 12, 18, 1.4f);
        return y + 38;
    }

    void knob (Canvas& c, const std::string& id, float cx, float cy, const std::string& label, const std::string& value, unsigned col, bool light = false)
    {
        draw::knob (c, cx, cy, 17, knobs[id], col, light);
        knobHits.push_back ({ id, cx, cy, 24 });
        c.setColor (light ? 0xff3b4047 : 0xffa3a9b3);
        draw::spaced (c, label, boldFont (9.5f), cx - textWidth (boldFont (9.5f), label) * 0.5f - label.size() * 0.6f, cy + 26, 14, 1.2f);
        c.setColor (col);
        draw::text (c, value, monoFont (10.5f), Font::kCenter, cx - 40, cy + 41, 80, 14);
    }

    void draw (Canvas& c) override
    {
        knobHits.clear();
        // rails
        for (float rx : { 4.0f, width() - 16.0f })
        {
            c.setColor (0xff0c0d10);
            c.roundedRectangle (rx, 0, 12, height(), 3);
            for (float yy = 24; yy < height() - 10; yy += 54)
            {
                c.setColor (0xff2a2d33);
                c.roundedRectangle (rx + 3, yy, 6, 9, 3);
            }
        }

        const float meterH = 128;
        float y = 0;
        const float avail = height() - meterH - 10;
        switch (studio.currentStep)
        {
            case Step::session:    y = session (c, y); break;
            case Step::hits:       y = velocity (c, y); break;
            case Step::align:      y = align (c, y, avail - 10); break;
            case Step::level:      y = align (c, y, 186); y = velocity (c, y + 10); break;
            case Step::tune:       y = tune (c, y); y = velocity (c, y + 10); break;
            case Step::mix:        y = eq (c, y); y = comp (c, y + 10); if (y + 170 < avail) y = roomAux (c, y + 10); break;
            case Step::exportStep: y = exportUnit (c, y); break;
        }
        meterUnit (c, height() - meterH, meterH);
    }

    float session (Canvas& c, float y)
    {
        const auto& d = studio.data;
        float b = unitTop (c, y, 190, "align", "SESSION", "IMPORT");
        const auto f = bodyFont (12.5f);
        auto row = [&] (const std::string& k, const std::string& v)
        {
            c.setColor (0xff8a919c); draw::text (c, k, f, Font::kLeft, 42, b, 120, 20);
            c.setColor (0xffe8ebef); draw::text (c, v, monoFont (12), Font::kRight, 42, b, width() - 84, 20);
            b += 24;
        };
        row ("Take", d.hasSession ? d.title : "—");
        row ("Mics", d.hasSession ? std::to_string (d.tracks.size()) : "—");
        row ("Length", d.hasSession ? draw::clock (d.length) : "—");
        row ("Hits", d.hasSession ? std::to_string (d.hits.size()) : "—");
        row ("Velocities", d.hasSession ? std::to_string (d.layers.size()) : "—");
        return y + 190;
    }

    float align (Canvas& c, float y, float h)
    {
        const auto& d = studio.data;
        const float b = unitTop (c, y, h, "align", "ALIGN", "ARRIVAL · PHASE");
        // the arrival graphic: rings spreading from the source
        const float gx = 40, gy = b, gw = 118, gh = h - 54;
        draw::glass (c, gx, gy, gw, gh);
        c.setClampBounds (gx, gy, gw, gh);
        const float sx = gx + 18, sy = gy + gh * 0.5f;
        for (int i = 0; i < 5; ++i)
        {
            const float r = 18 + i * 22.0f;
            const unsigned col = Color (neon::cyan).interpolateWith (Color (neon::magenta), i / 4.0f).toARGB();
            c.setColor (draw::withAlpha (col, 0.9f - i * 0.12f));
            c.flatArc (sx - r, sy - r, 2 * r, 2.0f, 3.14159265f * 0.5f, 3.14159265f * 0.62f);
        }
        draw::led (c, sx, sy, neon::amber, true, 4);
        c.setClampBounds (0, 0, width(), height());

        const float lx = gx + gw + 12, lw = width() - 40 - lx;
        const float rowH = std::min (20.0f, (gh) / std::max (1.0f, (float) d.tracks.size()));
        const auto f = monoFont (std::min (11.5f, rowH * 0.62f));
        for (int i = 0; i < (int) d.tracks.size(); ++i)
        {
            const float ry = gy + i * rowH;
            if (ry + rowH > gy + gh + 2) break;
            const auto& tr = d.tracks[(size_t) i];
            draw::led (c, lx + 4, ry + rowH * 0.5f, tr.colourA, true, 2.4f);
            c.setColor (0xffc4c9d1);
            draw::text (c, tr.name, f, Font::kLeft, lx + 12, ry, lw - 60, rowH);
            const float ms = i < (int) d.arrivalMs.size() ? d.arrivalMs[(size_t) i] : NAN;
            c.setColor (neon::cyan);
            draw::text (c, std::isnan (ms) ? "—" : draw::fmt ("%.2f ms", (double) ms), f, Font::kRight, lx, ry, lw, rowH);
        }
        return y + h;
    }

    float velocity (Canvas& c, float y)
    {
        const auto& d = studio.data;
        const float h = 200;
        const float b = unitTop (c, y, h, "velocity", "VELOCITY · RR", "SELECTED HIT");
        draw::glass (c, 40, b, width() - 80, 56);
        const int sel = studio.selectedHit;
        const bool has = sel >= 0 && sel < (int) d.hits.size();
        const auto& hit = has ? d.hits[(size_t) sel] : HitInfo {};
        const auto* L = has && hit.layer < (int) d.layers.size() ? &d.layers[(size_t) hit.layer] : nullptr;
        struct Cell { const char* label; std::string big, small; unsigned col; };
        const Cell cells[] = {
            { "HIT", has ? draw::fmt ("%02.0f", sel + 1.0) : "--", "/" + std::to_string (d.hits.size()), 0xffe8ebef },
            { "VELOCITY", has ? draw::fmt ("%02.0f", hit.layer + 1.0) : "--", "/" + std::to_string (d.layers.size()), neon::amber },
            { "RR", has ? draw::fmt ("%02.0f", hit.roundRobin + 1.0) : "--", L ? "/" + std::to_string (L->numHits) : "", neon::amber },
            { "MIDI", L ? std::to_string (L->midiLow) + "–" + std::to_string (L->midiHigh) : "--", "", 0xffe8ebef },
            { "NOTE", L && ! L->note.empty() ? L->note : "--", "", neon::lime },
        };
        const float cw = (width() - 96) / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float cx = 48 + i * cw;
            c.setColor (0xff8a919c);
            draw::spaced (c, cells[i].label, boldFont (8), cx, b + 6, 12, 0.9f);
            c.setColor (cells[i].col);
            const auto big = monoFont (19);
            draw::text (c, cells[i].big, big, Font::kLeft, cx, b + 20, cw, 30);
            if (! cells[i].small.empty())
            {
                c.setColor (0xff7b818b);
                draw::text (c, cells[i].small, monoFont (10.5f), Font::kLeft, cx + textWidth (big, cells[i].big) + 1, b + 29, cw, 18);
            }
        }
        knob (c, "curve", 74, b + 98, "CURVE", draw::fmt ("%+.1f dB", (knobs["curve"] - 0.5) * 12), neon::amber);
        knob (c, "rrvary", 150, b + 98, "RR VARY", draw::fmt ("%.1f dB", knobs["rrvary"] * 1.0), neon::amber);
        return y + h;
    }

    float tune (Canvas& c, float y)
    {
        const float h = 170;
        const float b = unitTop (c, y, h, "tune", "TUNE", "SNARE · PITCH");
        draw::glass (c, 40, b, 150, 110);
        std::string note = "--";
        for (auto& L : studio.data.layers) if (! L.note.empty()) { note = L.note; break; }
        c.setColor (neon::lime);
        draw::text (c, note, monoFont (34), Font::kCenter, 40, b + 18, 150, 50);
        c.setColor (0xff8a919c);
        draw::text (c, "detected", bodyFont (11), Font::kCenter, 40, b + 72, 150, 16);
        knob (c, "target", 226, b + 34, "TARGET", note, 0xff3fd13a, true);
        knob (c, "lock", width() - 74, b + 34, "LOCK", draw::fmt ("%.0f %%", knobs["lock"] * 100), 0xff3fd13a, true);
        return y + h;
    }

    float eq (Canvas& c, float y)
    {
        const float h = 150;
        const float b = unitTop (c, y, h, "eq", "EQ", "TONE");
        const float gw = width() - 210;
        draw::glass (c, 40, b, gw, 92);
        visage::Path p;
        for (int i = 0; i <= 60; ++i)
        {
            const float x = 40 + gw * i / 60.0f;
            const float t = i / 60.0f;
            const float yy = b + 46 - 18 * std::exp (-std::pow ((t - knobs["freq"]) * 6, 2)) * (knobs["gain"] - 0.5f) * 4 - 6 * std::sin (t * 3);
            if (i == 0) p.moveTo (x, yy); else p.lineTo (x, yy);
        }
        c.setColor (Brush::horizontal (Color (neon::magenta), Color (neon::orange)));
        c.stroke (p, 0, 0, width(), height(), 2.0f);
        knob (c, "freq", width() - 136, b + 30, "FREQ", draw::fmt ("%.0f", 40 + knobs["freq"] * 900), neon::magenta);
        knob (c, "gain", width() - 70, b + 30, "GAIN", draw::fmt ("%+.1f", (knobs["gain"] - 0.5) * 24), neon::violet);
        return y + h;
    }

    float comp (Canvas& c, float y)
    {
        const float h = 150;
        const float b = unitTop (c, y, h, "comp", "COMP · 76", "DYNAMICS");
        const float mw = width() - 210;
        // VU meter
        c.setColor (Brush::linear (Color (0xfff6d58a), Color (0xffd9a440), Point (0, b), Point (0, b + 92)));
        c.roundedRectangle (40, b, mw, 92, 6);
        c.setColor (0xff3a2a10);
        for (int i = 0; i <= 10; ++i)
        {
            const float a = -0.9f + 1.8f * i / 10.0f;
            const float cx = 40 + mw * 0.5f, cy = b + 110;
            c.segment (cx + std::sin (a) * 80, cy - std::cos (a) * 80, cx + std::sin (a) * 90, cy - std::cos (a) * 90, i > 7 ? 2.0f : 1.2f, true);
        }
        const float gr = studio.playing ? studio.data.hasSession ? 0.25f + 0.5f * masterMeter : 0.0f : 0.0f;
        const float a = -0.9f + 1.8f * (1.0f - gr);
        c.setColor (0xff1b1408);
        c.segment (40 + mw * 0.5f, b + 110, 40 + mw * 0.5f + std::sin (a) * 92, b + 110 - std::cos (a) * 92, 1.6f, true);
        knob (c, "input", width() - 136, b + 30, "INPUT", draw::fmt ("%.0f dB", knobs["input"] * 24 - 12), neon::amber);
        knob (c, "ratio", width() - 70, b + 30, "RATIO", draw::fmt ("%.0f:1", 2 + knobs["ratio"] * 18), neon::orange);
        return y + h;
    }

    float roomAux (Canvas& c, float y)
    {
        const float h = 150;
        const float b = unitTop (c, y, h, "room", "ROOM AUX", "SENDS · RETURN");
        const float sp = (width() - 80) / 4.0f;
        knob (c, "send", 40 + sp * 0.5f, b + 30, "SEND", draw::fmt ("%.0f %%", knobs["send"] * 100), neon::violet);
        knob (c, "decay", 40 + sp * 1.5f, b + 30, "DECAY", draw::fmt ("%.1f s", 0.2 + knobs["decay"] * 3), neon::magenta);
        knob (c, "predelay", 40 + sp * 2.5f, b + 30, "PRE", draw::fmt ("%.0f ms", knobs["predelay"] * 80), neon::cyan);
        knob (c, "mixr", 40 + sp * 3.5f, b + 30, "MIX", draw::fmt ("%.0f %%", knobs["mixr"] * 100), 0xffd8dce2);
        return y + h;
    }

    float exportUnit (Canvas& c, float y)
    {
        const float h = 250;
        float b = unitTop (c, y, h, "align", "EXPORT", "FILES");
        const auto f = bodyFont (12.5f);
        auto row = [&] (const std::string& k, const std::string& v, unsigned col = 0xffe8ebef)
        {
            c.setColor (0xff8a919c); draw::text (c, k, f, Font::kLeft, 42, b, 120, 20);
            c.setColor (col); draw::text (c, v, monoFont (12), Font::kRight, 42, b, width() - 84, 20);
            b += 26;
        };
        row ("Naming", "SPLIT · MY DRUM CORE", neon::cyan);
        row ("Pattern", "DRUM_ART_RRnn_VELnn");
        row ("Format", "WAV 24-bit");
        row ("Time stamp", "BWF");
        row ("Folders", "One per drum");
        row ("Audio", "Edits only", neon::amber);
        return y + h;
    }

    void meterUnit (Canvas& c, float y, float h)
    {
        draw::plate (c, 24, y, width() - 48, h, 0xff1c1e22, 0xff101114, 0xff060708, 9, false);
        auto meter = [&] (float x, const char* label, float level, unsigned col)
        {
            c.setColor (0xffa3a9b3);
            draw::spaced (c, label, boldFont (10.5f), x, y + 10, 18, 2);
            const float mx = x, my = y + 36, mw = (width() - 48) * 0.5f - 34, mh = 14;
            for (int k = 0; k < 2; ++k)
            {
                const float yy = my + k * (mh + 8);
                c.setColor (0xff050608);
                c.roundedRectangle (mx, yy, mw, mh, 3);
                const int segs = 24;
                const int lit = (int) std::round (std::min (1.0f, level * (k ? 0.96f : 1.0f)) * segs);
                for (int s = 0; s < segs; ++s)
                {
                    const unsigned sc = s >= segs - 3 ? neon::red : s >= segs - 7 ? neon::gold : col;
                    c.setColor (s < lit ? Color (sc) : Color (sc).withAlpha (0.10f));
                    c.roundedRectangle (mx + 2 + s * (mw - 4) / segs, yy + 2, (mw - 4) / segs - 2, mh - 4, 1.5f);
                }
            }
            c.setColor (0xff8a919c);
            draw::text (c, level > 0.0005f ? draw::fmt ("%.1f dB", 20.0 * std::log10 (level)) : "-inf", monoFont (10.5f), Font::kLeft, x, y + h - 30, 90, 16);
        };
        meter (42, "ROOM", roomMeter, neon::violet);
        meter (24 + (width() - 48) * 0.5f + 10, "MASTER", masterMeter, neon::lime);
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        for (auto& k : knobHits)
            if (std::hypot (e.position.x - k.cx, e.position.y - k.cy) < k.r)
            {
                dragging = k.id; dragY = e.position.y; dragV = knobs[k.id];
                return;
            }
    }
    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (dragging.empty()) return;
        knobs[dragging] = std::clamp (dragV - (e.position.y - dragY) / 150.0f, 0.0f, 1.0f);
        redraw();
    }
    void mouseUp (const visage::MouseEvent&) override { dragging.clear(); }
};

//==============================================================================
// Footer: status chips, zoom, Night/Day, History.
struct Studio::Footer : visage::Frame
{
    explicit Footer (Studio& s) : studio (s) {}
    Studio& studio;
    float zoomX = 0, moonX = 0, sunX = 0, bookX = 0;
    float zoomDragY = 0;
    double zoomStart = 1.0;
    bool zooming = false;

    void draw (Canvas& c) override
    {
        const auto ch = chrome (studio.day);
        const auto& d = studio.data;
        const float h = height();
        float x = 0;
        auto chip = [&] (const std::string& txt, unsigned col, bool ledOn, unsigned ledCol)
        {
            const auto f = bodyFont (12);
            const float w = textWidth (f, txt) + (ledOn ? 34 : 22);
            c.setColor (studio.day ? 0xffcfd3d9 : 0xff15171b);
            c.roundedRectangle (x, 4, w, h - 8, 6);
            c.setColor (studio.day ? 0x22000000 : 0x10ffffff);
            c.roundedRectangleBorder (x, 4, w, h - 8, 6, 1);
            if (ledOn) draw::led (c, x + 14, h * 0.5f, ledCol, true, 3);
            c.setColor (col);
            draw::text (c, txt, f, Font::kLeft, x + (ledOn ? 26 : 11), 4, w, h - 8);
            x += w + 8;
        };
        if (d.hasSession)
        {
            chip (d.title, ch.ink, true, neon::cyan);
            chip (std::to_string (d.tracks.size()) + " mics", ch.inkQuiet, false, 0);
            if (! d.layers.empty())
            {
                int rr = 0; for (auto& L : d.layers) rr = std::max (rr, L.numHits);
                chip ("▲ " + std::to_string (d.hits.size()) + " hits · " + std::to_string (d.layers.size()) + " velocities × " + std::to_string (rr) + " RR",
                      studio.day ? 0xff2c8a1c : neon::lime, false, 0);
            }
        }
        const std::string status = ! studio.notice.empty() ? studio.notice : studio.device;
        c.setColor (ch.inkDim);
        c.setClampBounds (x + 4, 0, std::max (0.0f, width() - 280 - x), h);
        draw::text (c, status, bodyFont (12), Font::kLeft, x + 6, 0, width(), h);
        c.setClampBounds (0, 0, width(), h);

        // right side: zoom, moon/sun, book
        float rx = width() - 40;
        bookX = rx;
        c.setColor (studio.isHistoryVisible() ? 0xff2a2e36 : (studio.day ? 0xffcfd3d9 : 0xff15171b));
        c.roundedRectangle (rx, 4, 36, h - 8, 6);
        const unsigned bookCol = studio.isHistoryVisible() ? neon::cyan : ch.inkQuiet;
        c.setColor (bookCol);
        c.roundedRectangleBorder (rx + 10, h * 0.5f - 8, 8, 16, 2, 1.4f);
        c.roundedRectangleBorder (rx + 18, h * 0.5f - 8, 8, 16, 2, 1.4f);

        rx -= 86;
        c.setColor (studio.day ? 0xffcfd3d9 : 0xff15171b);
        c.roundedRectangle (rx, 4, 78, h - 8, 6);
        moonX = rx; sunX = rx + 39;
        const unsigned sel = 0xff2a2e36;
        c.setColor (sel);
        c.roundedRectangle (studio.day ? sunX + 2 : moonX + 2, 6, 35, h - 12, 5);
        // moon
        const float mx = moonX + 19, my = h * 0.5f;
        c.setColor (studio.day ? ch.inkQuiet : 0xffb9a6ff);
        c.circle (mx - 7, my - 7, 14);
        c.setColor (studio.day ? 0xffcfd3d9 : sel);
        c.circle (mx - 3, my - 10, 14);
        // sun
        const float sx = sunX + 19;
        const unsigned sunCol = studio.day ? neon::amber : ch.inkQuiet;
        c.setColor (sunCol);
        c.circle (sx - 4, my - 4, 8);
        for (int i = 0; i < 8; ++i)
        {
            const float a = i * 3.14159265f / 4;
            c.segment (sx + std::cos (a) * 6.5f, my + std::sin (a) * 6.5f, sx + std::cos (a) * 9, my + std::sin (a) * 9, 1.4f, true);
        }

        rx -= 54;
        zoomX = rx;
        const double fit = studio.data.length > 0 ? (studio.screenWidthForZoom() / studio.data.length) : 20.0;
        const float zv = (float) std::clamp (std::log (studio.view.pixelsPerSecond / std::max (1.0, fit)) / std::log (400.0), 0.0, 1.0);
        draw::knob (c, rx + 22, h * 0.5f, 10, zv, neon::cyan);
        c.setColor (ch.inkQuiet);
        draw::spaced (c, "ZOOM", boldFont (10.5f), rx - 50, 0, h, 1.6f);
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        const float px = e.position.x;
        if (px >= bookX) { if (studio.onHistoryToggled) studio.onHistoryToggled(); return; }
        if (px >= moonX && px < moonX + 39) { if (studio.day && studio.onThemeToggled) studio.onThemeToggled(); return; }
        if (px >= sunX && px < sunX + 39) { if (! studio.day && studio.onThemeToggled) studio.onThemeToggled(); return; }
        if (px >= zoomX && px < zoomX + 44)
        {
            if (e.repeatClickCount() >= 2) { studio.zoomToFit(); return; }
            zooming = true; zoomDragY = e.position.y; zoomStart = studio.view.pixelsPerSecond;
        }
    }
    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (! zooming) return;
        const double target = zoomStart * std::pow (1.03, (double) (zoomDragY - e.position.y));
        studio.zoomBy (target / studio.view.pixelsPerSecond, studio.playhead);
    }
    void mouseUp (const visage::MouseEvent&) override { zooming = false; }
};

//==============================================================================
// History: the undo list; click a row to go back (or forward) to it.
struct Studio::HistoryPanel : visage::Frame
{
    explicit HistoryPanel (Studio& s) : studio (s) {}
    Studio& studio;
    static constexpr float rowH = 28;

    void draw (Canvas& c) override
    {
        c.setColor (0x88000000);
        c.roundedRectangleShadow (0, 0, width(), height(), 10, 24);
        c.setColor (Brush::linear (Color (0xf21a1c21), Color (0xf20f1013), Point (0, 0), Point (0, height())));
        c.roundedRectangle (0, 0, width(), height(), 10);
        c.setColor (0x18ffffff);
        c.roundedRectangleBorder (0, 0, width(), height(), 10, 1);
        c.setColor (neon::cyan);
        draw::spaced (c, "HISTORY", boldFont (12), 16, 10, 22, 2.2f);
        for (int i = 0; i < (int) studio.history.size(); ++i)
        {
            const float y = 42 + i * rowH;
            if (y > height() - rowH) break;
            const bool cur = i == studio.historyIndex;
            const bool future = i > studio.historyIndex;
            if (cur)
            {
                c.setColor (draw::withAlpha (neon::cyan, 0.16f));
                c.roundedRectangle (8, y, width() - 16, rowH - 2, 5);
                draw::led (c, 20, y + rowH * 0.5f - 1, neon::cyan, true, 2.6f);
            }
            c.setColor (future ? 0xff5f6670 : 0xffe1e4e8);
            draw::text (c, studio.history[(size_t) i], bodyFont (12.5f), Font::kLeft, 32, y, width() - 40, rowH - 2);
        }
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        const int i = (int) ((e.position.y - 42) / rowH);
        if (i >= 0 && i < (int) studio.history.size() && studio.onHistoryJump) studio.onHistoryJump (i);
    }
};

//==============================================================================
Studio::Studio()
    : header (std::make_unique<Header> (*this)), strips (std::make_unique<Strips> (*this)),
      screen (std::make_unique<Screen> (*this)), console (std::make_unique<Console> (*this)),
      rack (std::make_unique<Rack> (*this)), footer (std::make_unique<Footer> (*this)),
      historyPanel (std::make_unique<HistoryPanel> (*this))
{
    setAcceptsKeystrokes (true);
    addChild (header.get());
    addChild (strips.get());
    addChild (screen.get());
    addChild (console.get(), false);
    addChild (rack.get());
    addChild (footer.get());
    addChild (historyPanel.get(), false);
    historyPanel->setOnTop (true);
}

Studio::~Studio()
{
    for (visage::Frame* f : { (visage::Frame*) historyPanel.get(), (visage::Frame*) footer.get(), (visage::Frame*) rack.get(),
                              (visage::Frame*) console.get(), (visage::Frame*) screen.get(), (visage::Frame*) strips.get(), (visage::Frame*) header.get() })
        removeChild (f);
}

float Studio::laneHeight() const
{
    const int n = std::max (1, (int) data.tracks.size());
    const float avail = screen->height() - screen->top() - 4;
    return std::max (64.0f, avail / (float) n);
}

float Studio::screenContentTop() const { return screen->top(); }

double Studio::screenWidthForZoom() const { return std::max (100.0f, screen->width() - 8); }

void Studio::draw (Canvas& c)
{
    const auto ch = chrome (day);
    c.setColor (Brush::vertical (Color (ch.bgTop), Color (ch.bgBottom)));
    c.fill (0, 0, width(), height());
    // a warm, low studio glow behind the gear
    c.setColor (Brush::radial (Color (day ? 0x30ffffff : 0x22ff9a3c), Color (0x00ff9a3c), Point (width() * 0.42f, 40), width() * 0.6f, 260));
    c.fill (0, 0, width(), height());

    // the main faceplate that holds the strips, the screen and the footer
    const float x = screen->x() - 0, mainX = strips->x() - 16, mainY = header->bottom() + 14;
    (void) x;
    const float mainR = (currentStep == Step::mix ? console->right() : screen->right()) + 16;
    draw::plate (c, mainX, mainY, mainR - mainX, height() - 12 - mainY, ch.plateTop, ch.plateBottom, ch.plateEdge, 10);
}

void Studio::resized() { layoutPanels(); }

void Studio::layoutPanels()
{
    const float W = width(), H = height(), m = 12;
    const float hk = std::min (1.15f, (W - 2 * m) / 1652.0f), hw = 1652 * hk, hh = 286 * hk;
    header->setBounds ((W - hw) / 2, m, hw, hh);
    const float top = m + hh + 10;
    const float rackW = std::clamp (W * 0.22f, 300.0f, 380.0f);
    rack->setBounds (W - m - rackW, top, rackW, H - top - m);
    const float mainX = m, mainW = W - 2 * m - rackW - 14, mainH = H - top - m;
    const float pad = 16, footerH = 38;
    const float stripW = 156;
    strips->setBounds (mainX + pad, top + pad, stripW, mainH - 2 * pad - footerH - 6);
    screen->setBounds (mainX + pad + stripW + 10, top + pad, mainW - 2 * pad - stripW - 10, mainH - 2 * pad - footerH - 6);
    console->setBounds (mainX + pad, top + pad, mainW - 2 * pad, mainH - 2 * pad - footerH - 6);
    footer->setBounds (mainX + pad, top + mainH - pad - footerH, mainW - 2 * pad, footerH);
    historyPanel->setBounds (mainX + mainW - pad - 270, top + pad + 30, 260, std::min (420.0f, mainH - 120));
    if (data.hasSession && atFit) zoomToFit();
    redrawAll();
}

void Studio::setModel (StudioModel m)
{
    data = std::move (m);
    pulseStart.assign (data.hits.size(), -10.0);
    selectedHit = data.hits.empty() ? -1 : 0;
    view.scrollY = 0;
    zoomToFit();
    redrawAll();
}

void Studio::setTrackState (int i, bool solo, bool mute, float volumeDb, float pan)
{
    if (i < 0 || i >= (int) data.tracks.size()) return;
    auto& t = data.tracks[(size_t) i];
    if (t.solo == solo && t.mute == mute && t.volumeDb == volumeDb && t.pan == pan) return;
    t.solo = solo; t.mute = mute; t.volumeDb = volumeDb; t.pan = pan;
    strips->redraw();
    screen->redraw();
    console->redraw();
}

void Studio::setTransport (double seconds, bool isPlaying)
{
    if (isPlaying && seconds >= lastPlayhead && seconds - lastPlayhead < 3.0)
        for (int h = 0; h < (int) data.hits.size(); ++h)
            if (data.hits[(size_t) h].time > lastPlayhead && data.hits[(size_t) h].time <= seconds)
            {
                pulseStart[(size_t) h] = now;
                // keep the selected-hit LCD following playback
                selectedHit = h;
                rack->redraw();
                screen->redraw();
            }
    const bool changed = playing != isPlaying;
    playhead = seconds;
    playing = isPlaying;
    lastPlayhead = seconds;

    // follow the playhead: page when it runs off the right edge
    if (playing && data.hasSession)
    {
        const double visible = screen->width() / view.pixelsPerSecond;
        if (seconds > view.start + visible * 0.95 || seconds < view.start)
        {
            view.start = std::max (0.0, seconds - visible * 0.05);
            redrawAll();
        }
    }
    if (changed) { header->redraw(); rack->redraw(); }
}

void Studio::tick (double t)
{
    now = t;
    header->redraw();
    screen->overlay.redraw();

    // meters follow the audio under the playhead
    auto levelAt = [&] (const TrackInfo& tr)
    {
        if (! playing || tr.mute) return 0.0f;
        const int b = (int) (playhead * tr.blocksPerSecond);
        float p = 0.0f;
        for (int k = std::max (0, b - 20); k < std::min ((int) tr.peaks.size(), b + 4); ++k) p = std::max (p, tr.peaks[(size_t) k]);
        return p * std::pow (10.0f, tr.volumeDb / 20.0f);
    };
    float room = 0.0f, master = 0.0f;
    console->meters.resize (data.tracks.size(), 0.0f);
    for (size_t i = 0; i < data.tracks.size(); ++i)
    {
        const float l = levelAt (data.tracks[i]);
        console->meters[i] = std::max (l, console->meters[i] * 0.88f);
        if (data.tracks[i].isRoom) room = std::max (room, l);
        master = std::max (master, l);
    }
    rack->roomMeter = std::max (room, rack->roomMeter * 0.9f);
    rack->masterMeter = std::max (master, rack->masterMeter * 0.9f);
    rack->redraw();
    if (currentStep == Step::mix) console->redraw();
}

void Studio::setDayMode (bool d) { day = d; redrawAll(); }

void Studio::setHistory (std::vector<std::string> entries, int current)
{
    history = std::move (entries);
    historyIndex = current;
    historyPanel->redraw();
}

void Studio::setHistoryVisible (bool v) { historyPanel->setVisible (v); footer->redraw(); }
bool Studio::isHistoryVisible() const { return historyPanel->isVisible(); }

void Studio::setHeader (HeaderState h) { hdr = std::move (h); header->redraw(); }

void Studio::pushMonitor (const float* level, const int* vel, int n)
{
    if (monLevel.empty()) { monLevel.assign (300, 0.0f); monVel.assign (300, 0); }
    for (int q = 0; q < n; ++q)
    {
        // two monitor points per column, keeping the louder (the spike survives)
        static int half = 0;
        if (half == 0) { monHead = (monHead + 1) % (int) monLevel.size(); monLevel[(size_t) monHead] = 0; }
        monLevel[(size_t) monHead] = std::max (monLevel[(size_t) monHead], level[q]);
        monVel[(size_t) monHead] = vel[q];
        half = (half + 1) % 2;
    }
}

void Studio::setStep (Step s)
{
    currentStep = s;
    const bool mix = s == Step::mix;
    console->setVisible (mix);
    strips->setVisible (! mix);
    screen->setVisible (! mix);
    redrawAll();
}

void Studio::zoomToFit()
{
    if (data.length <= 0) return;
    view.pixelsPerSecond = screenWidthForZoom() / data.length;
    view.start = 0;
    atFit = true;
    redrawAll();
}

void Studio::zoomBy (double factor, double anchor)
{
    if (! data.hasSession) return;
    const double fit = screenWidthForZoom() / std::max (0.01, data.length);
    if (factor != 1.0) atFit = false;
    const double anchorX = (anchor - view.start) * view.pixelsPerSecond;
    view.pixelsPerSecond = std::clamp (view.pixelsPerSecond * factor, fit * 0.5, 48000.0);
    view.start = anchor - anchorX / view.pixelsPerSecond;
    const double visible = screen->width() / view.pixelsPerSecond;
    view.start = std::clamp (view.start, 0.0, std::max (0.0, data.length - visible * 0.5));
    redrawAll();
}

bool Studio::keyPress (const visage::KeyEvent& e) { return onKey && onKey (e); }

void Studio::mouseDown (const visage::MouseEvent&) { requestKeyboardFocus(); }

void Studio::dropFiles (const std::vector<std::string>& paths)
{
    dropHover = false;
    redrawAll();
    if (onFilesDropped) onFilesDropped (paths);
}
} // namespace ld::ui

#pragma once
// The shared drawing kit: tokens, embedded fonts, plates, screws, glass, LEDs, knobs. Header-only (each file
// that draws gets its own copy of these small helpers).
#include "BinaryData.h"
#include <visage/ui.h>
#include <algorithm>
#include <cmath>
#include <string>

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

} // namespace ld::ui

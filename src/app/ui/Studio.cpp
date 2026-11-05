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
    explicit Header (Studio& s) : studio (s) {}
    Studio& studio;

    struct Group { int id; float w; float x = 0; };   // 0 logo, 1 steps, 2 transport, 3 ref
    std::vector<Group> groups;
    float transportX = -1.0f;                          // where you dragged it; -1 = default
    bool draggingTransport = false;
    float dragOffset = 0.0f;
    std::vector<std::pair<float, float>> stepX;        // x, width per step
    float gripX = 0, rewindX = 0, playX = 0, recX = 0;

    static constexpr const char* stepNames[] = { "IMPORT", "ALIGN", "LEVELS", "TUNE", "MIX", "EXPORT" };

    float stepsWidth() const
    {
        float w = 0;
        const auto f = boldFont (11.5f);
        for (auto* n : stepNames) w += textWidth (f, n) + 14 * 0.0f + 46;
        return w + 5 * 6;
    }

    void layoutGroups()
    {
        const float logoW = 210, stepsW = stepsWidth(), transportW = 470, refW = 200;
        std::vector<Group> others { { 0, logoW }, { 1, stepsW }, { 3, refW } };
        // Default spot: centred in the space between the steps and the reference picker.
        const float defaultX = 16 + logoW + 14 + stepsW + std::max (14.0f, (width() - 16 - refW - 14 - (16 + logoW + 14 + stepsW) - transportW) * 0.5f);
        const float tx = transportX < 0 ? defaultX : transportX;

        // Insert the transport where you dropped it, then pack everything else around it.
        float natural = 16;
        size_t insertAt = others.size();
        for (size_t i = 0; i < others.size(); ++i)
        {
            if (tx + transportW * 0.5f < natural + others[i].w * 0.5f) { insertAt = i; break; }
            natural += others[i].w + 14;
        }
        others.insert (others.begin() + (long) insertAt, Group { 2, transportW });
        groups = others;

        float cursor = 16;
        for (size_t i = 0; i < groups.size(); ++i)
        {
            auto& g = groups[i];
            if (g.id == 2) g.x = std::clamp (tx, cursor, width() - 16 - g.w);
            else if (i == groups.size() - 1) g.x = std::max (cursor, width() - 16 - g.w);
            else g.x = cursor;
            cursor = g.x + g.w + 14;
        }
    }

    void resized() override { layoutGroups(); }

    void draw (Canvas& c) override
    {
        const auto ch = chrome (studio.day);
        draw::plate (c, 0, 0, width(), height(), ch.plateTop, ch.plateBottom, ch.plateEdge, 10);
        layoutGroups();
        for (auto& g : groups)
        {
            if (g.id == 0) drawLogo (c, g.x);
            if (g.id == 1) drawSteps (c, g.x);
            if (g.id == 2) drawTransport (c, g.x);
            if (g.id == 3) drawRef (c, g.x, g.w);
        }
    }

    void drawLogo (Canvas& c, float x)
    {
        const float cy = height() * 0.5f;
        c.setColor (neon::amber);
        c.svg (reinterpret_cast<const unsigned char*> (BinaryData::marknight_svg), BinaryData::marknight_svgSize,
               x + 10, cy - 17, 34, 34);
        const auto ch = chrome (studio.day);
        c.setColor (ch.ink);
        draw::spaced (c, "LIMITLESS", boldFont (19), x + 54, cy - 19, 26, 5.2f);
        c.setColor (ch.inkQuiet);
        draw::spaced (c, "DRUMS", uiFont (9.5f), x + 56, cy + 4, 14, 8.5f);
    }

    void drawSteps (Canvas& c, float x)
    {
        const auto f = boldFont (11.5f);
        const auto ch = chrome (studio.day);
        stepX.clear();
        const float h = 30, y = (height() - h) * 0.5f;
        for (int i = 0; i < 6; ++i)
        {
            const float w = textWidth (f, stepNames[i]) + 46;
            const bool on = (int) studio.currentStep == i;
            stepX.push_back ({ x, w });
            if (on)
            {
                c.setColor (draw::withAlpha (neon::cyan, 0.35f));
                c.roundedRectangleShadow (x, y, w, h, 7, 16);
                c.setColor (Brush::linear (Color (0xff7ff3ff), Color (0xff19c6e6), Point (x, y), Point (x, y + h)));
                c.roundedRectangle (x, y, w, h, 7);
                c.setColor (0x66ffffff);
                c.fill (x + 6, y + 1, w - 12, 1);
                draw::led (c, x + 15, y + h * 0.5f, 0xff053039, true, 2.6f);
                c.setColor (0xff06222a);
            }
            else
            {
                c.setColor (studio.day ? 0xffd5d8dd : 0xff1b1d22);
                c.roundedRectangle (x, y, w, h, 7);
                c.setColor (studio.day ? 0x22000000 : 0x14ffffff);
                c.roundedRectangleBorder (x, y, w, h, 7, 1);
                const bool reachable = studio.data.hasSession || i == 0;
                draw::led (c, x + 15, y + h * 0.5f, neon::lime, reachable && i < (int) studio.currentStep, 2.6f);
                c.setColor (reachable ? ch.inkQuiet : ch.inkDim);
            }
            draw::spaced (c, stepNames[i], f, x + 25, y, h, 1.6f);
            x += w + 6;
        }
    }

    void drawTransport (Canvas& c, float x)
    {
        const float h = height() - 14, y = 7;
        c.setColor (studio.day ? 0xffc3c7cd : 0xff0e0f12);
        c.roundedRectangle (x, y, 470, h, 9);
        c.setColor (studio.day ? 0x30000000 : 0x18ffffff);
        c.roundedRectangleBorder (x, y, 470, h, 9, 1);

        // grip
        gripX = x;
        c.setColor (studio.day ? 0x66000000 : 0x55ffffff);
        for (int r = 0; r < 5; ++r)
            for (int col = 0; col < 2; ++col)
                c.circle (x + 9 + col * 5, y + h * 0.5f - 11 + r * 5.2f, 2.2f);

        auto key = [&] (float kx, float kw, unsigned top, unsigned bottom)
        {
            c.setColor (0x88000000);
            c.roundedRectangle (kx, y + 6, kw, h - 10, 6);
            c.setColor (Brush::linear (Color (top), Color (bottom), Point (kx, y + 5), Point (kx, y + h - 5)));
            c.roundedRectangle (kx, y + 5, kw, h - 11, 6);
            c.setColor (0x33ffffff);
            c.fill (kx + 5, y + 6, kw - 10, 1);
        };
        const float ky = y + 5, kh = h - 11;
        rewindX = x + 28;
        key (rewindX, 44, 0xff3a3d44, 0xff23252a);
        c.setColor (0xffd9dce1);
        c.fill (rewindX + 15, ky + kh * 0.5f - 7, 2.2f, 14);
        {
            visage::Path p;
            p.moveTo (rewindX + 30, ky + kh * 0.5f - 7);
            p.lineTo (rewindX + 30, ky + kh * 0.5f + 7);
            p.lineTo (rewindX + 18.5f, ky + kh * 0.5f);
            p.close();
            c.fill (p);
        }

        playX = rewindX + 52;
        if (studio.playing)
        {
            c.setColor (draw::withAlpha (neon::lime, 0.5f));
            c.roundedRectangleShadow (playX, ky, 64, kh, 6, 20);
        }
        key (playX, 64, studio.playing ? 0xffa6ff8f : 0xff6fe85a, studio.playing ? 0xff4fd23a : 0xff36a828);
        c.setColor (0xff0d2a0a);
        if (studio.playing)
        {
            c.fill (playX + 25, ky + kh * 0.5f - 7, 5, 14);
            c.fill (playX + 34, ky + kh * 0.5f - 7, 5, 14);
        }
        else
        {
            visage::Path p;
            p.moveTo (playX + 26, ky + kh * 0.5f - 8);
            p.lineTo (playX + 26, ky + kh * 0.5f + 8);
            p.lineTo (playX + 40, ky + kh * 0.5f);
            p.close();
            c.fill (p);
        }

        recX = playX + 72;
        key (recX, 44, 0xff3a3d44, 0xff23252a);
        c.setColor (0xffd9dce1);
        c.circle (recX + 16, ky + kh * 0.5f - 6, 12);

        // LED clock
        const float cx = recX + 54, cw = 170;
        draw::glass (c, cx, ky - 1, cw, kh + 2);
        const unsigned ledColour = studio.playing ? neon::lime : neon::amber;
        const auto clockFont = monoBold (27);
        c.setColor (draw::withAlpha (ledColour, 0.08f));
        draw::text (c, "8:88.888", clockFont, Font::kRight, cx, ky, cw - 12, kh);
        c.setColor (draw::withAlpha (ledColour, 0.35f));
        draw::text (c, draw::clock (studio.playhead), clockFont, Font::kRight, cx + 0.8f, ky, cw - 12, kh);
        c.setColor (ledColour);
        draw::text (c, draw::clock (studio.playhead), clockFont, Font::kRight, cx, ky, cw - 12, kh);

        // tempo
        const float bx = cx + cw + 8, bw = 72;
        draw::glass (c, bx, ky - 1, bw, kh + 2);
        c.setColor (0xffcfe9ff);
        draw::text (c, draw::fmt ("%.2f", studio.data.bpm), monoFont (14), Font::kCenter, bx, ky + 2, bw, kh * 0.55f);
        c.setColor (0xff6f8aa6);
        draw::text (c, "BPM", monoFont (10), Font::kCenter, bx, ky + kh * 0.5f, bw, kh * 0.45f);
    }

    void drawRef (Canvas& c, float x, float w)
    {
        const float h = 34, y = (height() - h) * 0.5f;
        c.setColor (studio.day ? 0xffd5d8dd : 0xff121317);
        c.roundedRectangle (x, y, w, h, 7);
        c.setColor (studio.day ? 0x26000000 : 0x14ffffff);
        c.roundedRectangleBorder (x, y, w, h, 7, 1);
        const auto ch = chrome (studio.day);
        c.setColor (ch.inkQuiet);
        draw::spaced (c, "REF", boldFont (10.5f), x + 12, y, h, 1.5f);
        c.setColor (ch.inkDim);
        draw::text (c, "Off", uiFont (13), Font::kLeft, x + 52, y, 80, h);
        c.setColor (ch.inkDim);
        {
            visage::Path p;
            p.moveTo (x + w - 64, y + h * 0.5f - 5);
            p.lineTo (x + w - 64, y + h * 0.5f + 5);
            p.lineTo (x + w - 56, y + h * 0.5f);
            p.close();
            c.fill (p);
        }
        draw::text (c, "A/B", uiFont (12.5f), Font::kLeft, x + w - 44, y, 40, h);
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        const float px = e.position.x, py = e.position.y;
        for (auto& g : groups)
            if (g.id == 2 && px >= g.x && px < g.x + 24)
            {
                draggingTransport = true;
                dragOffset = px - g.x;
                return;
            }
        for (int i = 0; i < (int) stepX.size(); ++i)
            if (px >= stepX[(size_t) i].first && px < stepX[(size_t) i].first + stepX[(size_t) i].second && py > 10 && py < height() - 10)
            {
                if (i == 0 && studio.onImport) studio.onImport();
                if (studio.data.hasSession || i == 0) studio.setStep ((Step) i);
                return;
            }
        if (px >= rewindX && px < rewindX + 44 && studio.onRewind) return studio.onRewind();
        if (px >= playX && px < playX + 64 && studio.onPlay) return studio.onPlay();
        if (px >= recX && px < recX + 44 && studio.onRecord) return studio.onRecord();
    }

    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (! draggingTransport) return;
        transportX = std::clamp (e.position.x - dragOffset, 16.0f, width() - 16 - 470);
        redraw();
    }

    void mouseUp (const visage::MouseEvent&) override { draggingTransport = false; }
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
            case Step::importStep: y = session (c, y); break;
            case Step::align:      y = align (c, y, avail - 10); break;
            case Step::levels:     y = align (c, y, 186); y = velocity (c, y + 10); break;
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
    header->setBounds (m, m, W - 2 * m, 62);
    const float top = m + 62 + 14;
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

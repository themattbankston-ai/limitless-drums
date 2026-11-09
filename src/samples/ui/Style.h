#pragma once
// The sample build's look: the approved palette and the small drawing helpers every step shares.
#include "app/ui/Draw.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ld::samples::style
{
using namespace ld::ui;
using visage::Canvas;

// the approved palette
constexpr unsigned ground = 0xff232325, bar1 = 0xff38383b, bar2 = 0xff2b2b2e, bar3 = 0xff2f2f32, ruler = 0xff29292b, lane = 0xff18181a,
                   head = 0xff303033, headSel = 0xff323a3c, line = 0xff141415, well = 0xff19191a, field = 0xff1c1c1e,
                   ink = 0xfff0f0f1, ink2 = 0xffc2c2c6, ink3 = 0xffa6a6aa, btn = 0xff4c4c50, btnOn = 0xff57575c,
                   accent = 0xff2fbfae, accentDark = 0xff173d39, accentInk = 0xff9ff0e5, gold = 0xffe0a43c;
constexpr unsigned velColour[] = { 0xff3d5a80, 0xff3f6f8f, 0xff3f8a8a, 0xff5b8f5a, 0xff9a7a35, 0xffa4513d, 0xff8a4f7a, 0xff6a5aa8, 0xff4f7ab0, 0xff4f9a9a, 0xff7a9a4f, 0xffb08a3f };
constexpr int nVelColours = 12;
inline const char* stepNames[8] = { "Setup", "Hits", "Align", "Tune", "Tone", "Clean", "Level", "Export" };
inline const char* modeNames[6] = { "Hit", "Layer", "Sweep", "Groove", "MIDI", "Mono" };
inline const char* heightNames[4] = { "Small", "Medium", "Large", "Jumbo" };

/** Text in a box; text too wide for the box is cut short with an ellipsis rather than spilling into its neighbours. */
inline void txt (Canvas& c, const std::string& s, const visage::Font& f, unsigned col, float x, float y, float w, float h, visage::Font::Justification j = visage::Font::kLeft)
{
    c.setColor (col);
    if (w > 8 && textWidth (f, s) > w)
    {
        const std::string ell = "\xe2\x80\xa6";
        std::string t = s;
        while (! t.empty() && textWidth (f, t + ell) > w)
        {
            t.pop_back();
            while (! t.empty() && (static_cast<unsigned char> (t.back()) & 0xc0) == 0x80) t.pop_back();   // (whole UTF-8 characters)
            if (! t.empty() && (static_cast<unsigned char> (t.back()) & 0xc0) == 0xc0) t.pop_back();
        }
        draw::text (c, t + ell, f, j, x, y, w, h);
        return;
    }
    draw::text (c, s, f, j, x, y, w, h);
}

inline void button (Canvas& c, float x, float y, float w, float h, const std::string& label, bool on, unsigned onCol = btnOn)
{
    if (on) { c.setColor (onCol); c.roundedRectangle (x, y, w, h, 3); }
    txt (c, label, on ? boldFont (12) : bodyFont (12), on ? ink : ink2, x, y, w, h, visage::Font::kCenter);
}

inline std::string fmtTime (double s)
{
    s = std::max (0.0, s);
    char b[32];
    std::snprintf (b, sizeof b, "%d:%06.3f", (int) (s / 60), std::fmod (s, 60.0));
    return b;
}

inline std::string fmtDb (float db)
{
    if (db <= -99) return "-inf";
    char b[16];
    std::snprintf (b, sizeof b, "%+.1f", db);
    return std::abs (db) < 0.05f ? std::string (" 0.0") : std::string (b);
}

// the monitor fader's law: +6 dB at the top, 0 dB at 78 %, silence at the bottom
inline float faderPos (float db) { return db <= -90 ? 0.0f : std::clamp (0.78f * std::pow (10.0f, db / 40.0f), 0.0f, 1.0f); }
inline float faderDb (float p) { return p <= 0.001f ? -100.0f : std::clamp (40.0f * std::log10 (p / 0.78f), -90.0f, 6.0f); }

} // namespace ld::samples::style

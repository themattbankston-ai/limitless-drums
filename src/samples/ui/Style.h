#pragma once
// The sample build's look: the approved palette and the small drawing helpers every step shares.
#include "app/ui/Draw.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ld::samples::style
{
using namespace ld::ui;
using visage::Canvas;

// ---- design tokens: graphite surfaces, coral for selection and the primary action, mint for "on" and play.
// Contrast measured against `ground` (WCAG ratio): ink 14.9, ink2 9.9, ink3 6.5, ink4 4.9 (and 4.5 on panel),
// coral as text/lines 5.5, mint 9.5; white on coralFill 4.6, mintText on mint 8.6. ink4 is for large text and
// non-text only on raised surfaces (3.9 there). Checked by hand with the WCAG formula, not a certification.
namespace tok
{
    // surfaces, deepest first
    constexpr unsigned lane = 0xff161618, ground = 0xff1c1c1e, panel = 0xff242427, raised = 0xff2d2d31, control = 0xff3a3a3f,
                       controlHi = 0xff45454b, hairline = 0xff111113, divider = 0xff38383d;
    // text
    constexpr unsigned ink = 0xfff2efec, ink2 = 0xffc8c5c2, ink3 = 0xffa3a09d, ink4 = 0xff8c8986;
    // coral: what's selected, the one primary action, emphasis worth looking at
    constexpr unsigned coral = 0xffef6a4c, coralFill = 0xffc84d34, coralFillTop = 0xffd2583e, coralDeep = 0xff4a2620, coralInk = 0xffffb7a5;
    // mint: switched on, playing, done
    constexpr unsigned mint = 0xff4fd8a8, mintFill = 0xff3fbf92, mintDeep = 0xff17382e, mintInk = 0xff9ff0d4, mintText = 0xff0b2a20;
    // states
    constexpr unsigned warn = 0xffffc78a, error = 0xffff7a6b, focus = 0xff8ab4ff;
    // proportion: the golden ratio. Layouts split their width major : minor = phi : 1 (61.8 % / 38.2 %); spacing
    // and sizes step along the Fibonacci numbers (each about phi times the last); type steps by sqrt(phi).
    constexpr float phi = 1.6180340f, major = 0.6180340f, minor = 0.3819660f;
    // spacing and shape (Fibonacci)
    constexpr float s1 = 3, s2 = 5, s3 = 8, s4 = 13, s5 = 21, s6 = 34, radius = 5, radiusLg = 8, pill = 13;
    // type sizes: 10 x sqrt(phi)^n = 10, 12.7, 16.2, 20.6, 26.2 (tSmall sits between the first two for dense labels)
    constexpr float tCaption = 10, tSmall = 11, tBody = 12, tLabel = 13, tTitle = 16, tValue = 17, tHead = 21, tDisplay = 26;
    // control sizes (Fibonacci where a size is free: 21, 34, 55; knobs 40 and 65 are phi apart)
    constexpr float rowH = 26, buttonH = 34, knobLg = 65, knobSm = 40, switchW = 34, switchH = 21;
    /** The narrower part of a golden split of `total`, kept between lo and hi (so small windows still fit their controls). */
    constexpr float goldenMinor (float total, float lo, float hi) { return total * minor < lo ? lo : total * minor > hi ? hi : total * minor; }
}

// the names the steps already use, now on the tokens (so one palette drives every screen)
constexpr unsigned ground = tok::ground, bar1 = tok::raised, bar2 = tok::panel, bar3 = 0xff28282b, ruler = 0xff222225, lane = tok::lane,
                   head = 0xff2b2b2f, headSel = 0xff3a2d2a, line = tok::hairline, well = 0xff19191b, field = 0xff1e1e21,
                   ink = tok::ink, ink2 = tok::ink2, ink3 = tok::ink3, btn = tok::control, btnOn = tok::controlHi,
                   accent = tok::coral, accentDark = tok::mintDeep, accentInk = tok::mintInk, gold = tok::coralFill;
constexpr unsigned velColour[] = { 0xff3d5a80, 0xff3f6f8f, 0xff3f8a8a, 0xff5b8f5a, 0xff9a7a35, 0xffa4513d, 0xff8a4f7a, 0xff6a5aa8, 0xff4f7ab0, 0xff4f9a9a, 0xff7a9a4f, 0xffb08a3f };
constexpr int nVelColours = 12;
inline const char* stepNames[8] = { "Setup", "Hits", "Align", "Tune", "Tone", "Clean", "Level", "Export" };
inline const char* modeNames[3] = { "Hit", "Velocity layer", "All layers" };   ///< the three views (and what Play auditions)
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

/** The one primary action on a screen (Auto Prep, Export, Analyze): coral, white text. */
inline void primaryButton (Canvas& c, float x, float y, float w, float h, const std::string& label, bool enabled = true)
{
    c.setColor (enabled ? visage::Brush::linear (visage::Color (tok::coralFillTop), visage::Color (tok::coralFill), visage::Point (0, y), visage::Point (0, y + h))
                        : visage::Brush::solid (visage::Color (tok::control)));
    c.roundedRectangle (x, y, w, h, tok::radius);
    txt (c, label, boldFont (tok::tLabel), enabled ? 0xffffffff : tok::ink4, x, y, w, h, visage::Font::kCenter);
}

/** A segmented choice: one of `labels`; the chosen one is coral. Returns nothing: callers add the hit targets. */
inline void segmented (Canvas& c, float x, float y, float w, float h, const std::vector<std::string>& labels, int chosen)
{
    if (labels.empty()) return;
    c.setColor (tok::lane); c.roundedRectangle (x, y, w, h, tok::radius);
    const float sw = w / (float) labels.size();
    for (int i = 0; i < (int) labels.size(); ++i)
    {
        const float bx = x + sw * (float) i;
        if (i == chosen) { c.setColor (tok::coralFill); c.roundedRectangle (bx + 2, y + 2, sw - 4, h - 4, tok::radius - 2); }
        txt (c, labels[(size_t) i], i == chosen ? boldFont (tok::tBody) : bodyFont (tok::tBody), i == chosen ? 0xffffffff : tok::ink2, bx, y, sw, h, visage::Font::kCenter);
    }
}

/** An on/off switch: mint when on. The label, if any, sits to its right. */
inline void toggleSwitch (Canvas& c, float x, float y, bool on, const std::string& label = {}, float labelW = 0)
{
    const float w = tok::switchW, h = tok::switchH;
    c.setColor (on ? tok::mintFill : tok::control); c.roundedRectangle (x, y, w, h, h / 2);
    c.setColor (on ? 0xffffffff : tok::ink3); c.circle (on ? x + w - h + 3 : x + 3, y + 3, h - 6);
    if (! label.empty()) txt (c, label, bodyFont (tok::tBody), on ? tok::ink : tok::ink2, x + w + 8, y - 2, labelW > 0 ? labelW : 120, h + 4);
}

/** A knob: v in 0..1 over a 270-degree sweep from 7:30 to 4:30; bipolar knobs fill from 12:00. Name above, value below.
    (Angles here: 0 = up, clockwise. Visage's arcs measure from 3 o'clock, clockwise, and take half the span: arcUp()
    converts. Measured on a calibration render, 10 Nov.) */
inline void arcUp (Canvas& c, float x, float y, float d, float thickness, float centreFromUp, float span)
{
    c.roundedArc (x, y, d, thickness, centreFromUp - 1.5707963f, span / 2);
}

inline void knob (Canvas& c, float cx, float cy, float d, float v, const std::string& name, const std::string& value, bool bipolar = false, bool focused = false, unsigned bg = tok::panel)
{
    constexpr float pi = 3.14159265f, sweep = 1.5f * pi, start = 1.25f * pi;
    v = std::clamp (v, 0.0f, 1.0f);
    const float x = cx - d / 2, y = cy - d / 2, t = std::max (3.0f, d * 0.07f);
    c.setColor (tok::control); arcUp (c, x, y, d, t, 0.0f, sweep);
    const float from = bipolar ? 0.5f : 0.0f, a0 = start + sweep * std::min (from, v), a1 = start + sweep * std::max (from, v);
    if (a1 - a0 > 0.01f) { c.setColor (tok::coral); arcUp (c, x, y, d, t, (a0 + a1) / 2, a1 - a0); }
    const float cap = d - 2 * t - 8;
    const float a = start + sweep * v;
    // the scalloped black cap: a dark disc with nine finger notches cut out of its edge, turning with the value
    c.setColor (visage::Brush::linear (visage::Color (0xff34343a), visage::Color (0xff121214), visage::Point (0, cy - cap / 2), visage::Point (0, cy + cap / 2)));
    c.circle (cx - cap / 2, cy - cap / 2, cap);
    c.setColor (bg);
    for (int i = 0; i < 9; ++i)
    {
        const float sa = a + (i + 0.5f) * 2 * pi / 9, sr = cap * 0.56f, sd = cap * 0.2f;
        c.circle (cx + sr * std::sin (sa) - sd / 2, cy - sr * std::cos (sa) - sd / 2, sd);
    }
    c.setColor (visage::Brush::linear (visage::Color (0xff2a2a2f), visage::Color (0xff19191c), visage::Point (0, cy - cap * 0.33f), visage::Point (0, cy + cap * 0.33f)));
    c.circle (cx - cap * 0.33f, cy - cap * 0.33f, cap * 0.66f);   // the raised centre
    if (focused) { c.setColor (tok::focus); c.ring (cx - d / 2 - 3, cy - d / 2 - 3, d + 6, 2.0f); }
    const float r0 = cap * 0.12f, r1 = cap * 0.4f;
    c.setColor (tok::ink);
    c.segment (cx + r0 * std::sin (a), cy - r0 * std::cos (a), cx + r1 * std::sin (a), cy - r1 * std::cos (a), 2.5f, true);
    if (! name.empty()) txt (c, name, boldFont (tok::tSmall), tok::ink2, cx - 60, y - 20, 120, 16, visage::Font::kCenter);
    if (! value.empty()) txt (c, value, monoFont (tok::tLabel), tok::ink, cx - 60, y + d + 4, 120, 18, visage::Font::kCenter);
}

/** A raised panel with an optional small-caps title. */
inline void panelBox (Canvas& c, float x, float y, float w, float h, const std::string& title = {})
{
    c.setColor (tok::panel); c.roundedRectangle (x, y, w, h, tok::radiusLg);
    c.setColor (tok::divider); c.roundedRectangleBorder (x, y, w, h, tok::radiusLg, 1);
    if (! title.empty()) txt (c, title, boldFont (tok::tSmall), tok::ink3, x + tok::s4, y + tok::s3, w - 2 * tok::s4, 14);
}

/** Keyboard focus: drawn the same everywhere. */
inline void focusRing (Canvas& c, float x, float y, float w, float h)
{
    c.setColor (tok::focus); c.roundedRectangleBorder (x - 2, y - 2, w + 4, h + 4, tok::radius + 2, 2);
}

/** Every token and control on one sheet, for design review (LD_GALLERY=1 draws it over the app). */
inline void gallery (Canvas& c, float x, float y, float w, float h)
{
    c.setColor (tok::ground); c.fill (x, y, w, h);
    float cx = x + 24, cy = y + 20;
    txt (c, "Surfaces", boldFont (tok::tSmall), tok::ink3, cx, cy, 200, 16);
    const unsigned surf[] = { tok::lane, tok::ground, tok::panel, tok::raised, tok::control, tok::controlHi };
    const char* sn[] = { "lane", "ground", "panel", "raised", "control", "controlHi" };
    for (int i = 0; i < 6; ++i) { c.setColor (surf[i]); c.roundedRectangle (cx + i * 92.f, cy + 20, 84, 40, tok::radius); c.setColor (tok::divider); c.roundedRectangleBorder (cx + i * 92.f, cy + 20, 84, 40, tok::radius, 1); txt (c, sn[i], bodyFont (tok::tSmall), tok::ink2, cx + i * 92.f, cy + 62, 84, 16, visage::Font::kCenter); }
    cy += 90;
    txt (c, "Text and accents", boldFont (tok::tSmall), tok::ink3, cx, cy, 200, 16);
    const unsigned tc[] = { tok::ink, tok::ink2, tok::ink3, tok::ink4, tok::coral, tok::mint, tok::warn, tok::error };
    const char* tn[] = { "ink", "ink2", "ink3", "ink4", "coral", "mint", "warn", "error" };
    for (int i = 0; i < 8; ++i) txt (c, tn[i], boldFont (tok::tLabel), tc[i], cx + i * 70.f, cy + 20, 70, 20);
    cy += 56;
    txt (c, "Actions", boldFont (tok::tSmall), tok::ink3, cx, cy, 200, 16);
    primaryButton (c, cx, cy + 20, 120, tok::buttonH, "Auto Prep");
    primaryButton (c, cx + 132, cy + 20, 120, tok::buttonH, "Export", false);
    segmented (c, cx + 270, cy + 20, 240, tok::buttonH, { "Hit", "Layer", "Sweep" }, 1);
    segmented (c, cx + 524, cy + 20, 120, tok::buttonH, { "A", "B" }, 1);
    toggleSwitch (c, cx + 660, cy + 25, true, "Preserve room arrival", 170);
    toggleSwitch (c, cx + 860, cy + 25, false, "Room reduction", 130);
    cy += 70;
    txt (c, "Knobs", boldFont (tok::tSmall), tok::ink3, cx, cy, 200, 16);
    knob (c, cx + 50, cy + 70, tok::knobLg, 0.0f, "Min", "0 %");
    knob (c, cx + 160, cy + 70, tok::knobLg, 0.62f, "Tone", "+0.25", true);
    knob (c, cx + 270, cy + 70, tok::knobLg, 0.3f, "Crack", "-0.40", true, true);
    knob (c, cx + 380, cy + 70, tok::knobLg, 1.0f, "Max", "100 %");
    knob (c, cx + 480, cy + 70, tok::knobSm, 0.4f, "Q", "0.70");
    focusRing (c, cx + 560, cy + 50, 120, tok::buttonH);
    txt (c, "focus ring", bodyFont (tok::tBody), tok::ink2, cx + 560, cy + 50, 120, tok::buttonH, visage::Font::kCenter);
    cy += 150;
    panelBox (c, cx, cy, 420, 90, "PANEL TITLE");
    txt (c, "Body text in a panel, ink2 on panel.", bodyFont (tok::tBody), tok::ink2, cx + tok::s4, cy + 40, 380, 18);
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

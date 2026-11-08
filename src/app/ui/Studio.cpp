#include <chrono>
#include "Studio.h"
#include <complex>
#include "Draw.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ld::ui
{
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
            const auto st = i == 0 && ! studio.data.hasSession ? std::string ("CLICK TO IMPORT") : H.status[i];
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
        // previous: bar + triangle pointing left, centred on the cap (52, 232); next mirrored on (184, 232)
        c.fill (X (45), X (226), X (2.2f), X (12));
        c.triangle (X (58), X (226), X (58), X (238), X (48), X (232));
        c.fill (X (190.8f), X (226), X (2.2f), X (12));
        c.triangle (X (178), X (226), X (178), X (238), X (188), X (232));
        ringLight (118, 232, 78, 0xff3dffd2, H.playing ? 1.0f : 0.7f);
        c.setColor (Color (0xffc9fff2));
        if (H.playing) { c.roundedRectangle (X (110), X (223), X (6.5f), X (18), X (1.5f)); c.roundedRectangle (X (121.5f), X (223), X (6.5f), X (18), X (1.5f)); }
        else c.triangle (X (112), X (220), X (112), X (244), X (132), X (232));   // optical centre a little right of the geometric one

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
            const std::string nm = i == 3 ? (H.rudiments ? "\xe2\x80\xb9 RUDIMENTS \xe2\x80\xba" : "\xe2\x80\xb9 GROOVE \xe2\x80\xba") : modeName[i];
            draw::text (c, nm, boldFont (X (i == 3 && H.rudiments ? 8.2f : 9.5f)), Font::kCenter, X (x), X (y + 40), X (w), X (12));
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
            if (in (272 + 138 * i, 18, 128, 142))
            {
                if (i == 0 && ! studio.data.hasSession) { if (studio.onImport) studio.onImport(); return; }
                if (studio.onStepClicked) studio.onStepClicked ((Step) i);
                return;
            }
        if (in (1244, 18, 214, 142)) { if (studio.onAutoPrep) studio.onAutoPrep(); return; }
        if (near (1506, 62, 32)) { if (studio.onUndo) studio.onUndo(); return; }
        if (near (1578, 62, 32)) { if (studio.onRedo) studio.onRedo(); return; }
        if (in (1496, 128, 110, 28)) { studio.setHistoryVisible (! studio.isHistoryVisible()); return; }
        if (near (52, 232, 22)) { if (studio.onPrevHit) studio.onPrevHit(); return; }
        if (near (118, 232, 38)) { if (studio.onPlay) studio.onPlay(); return; }
        if (near (184, 232, 22)) { if (studio.onNextHit) studio.onNextHit(); return; }
        for (int i = 0; i < 3; ++i) if (in (528 + 50 * i, 210, 50, 44)) { if (studio.onAB) studio.onAB (i); return; }
        if (in (694 + 84 * 3, 201 + 38, 76, 24)) { if (studio.onToggle) studio.onToggle ("rudiments"); return; }
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

    static constexpr float rulerH = 8, stripH = 30;   // the ruler is a thin scrub band; seconds don't matter here
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
        if (studio.step() == Step::align && d.align.valid) return drawAlignRows (c, lh, t0, n);

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

    // ------------------------------------------------------------------ the Align step's rows
    // Each track over what it lines up to, around the selected hit: the reference filled grey, the track as
    // recorded as a ghost, the track where it sits now in its colour. On the right, its lamp (Lead, In phase,
    // Corrected, Manual), what it lines up to, its delay and polarity, and how it sums before and after.
    int alignIndexOf (int uiTrack) const
    {
        const auto& d = studio.data;
        if (uiTrack < 0 || uiTrack >= (int) d.tracks.size()) return -1;
        for (int k = 0; k < (int) d.align.mics.size(); ++k) if (d.align.mics[(size_t) k].name == d.tracks[(size_t) uiTrack].name) return k;
        return uiTrack < (int) d.align.mics.size() ? uiTrack : -1;
    }

    void drawAlignRows (Canvas& c, float lh, float t0, int n)
    {
        const auto& d = studio.data; const auto& A = d.align;
        const unsigned vio = neon::violet;
        // the caption where the ruler sits
        c.setColor (0xff0b0c0f); c.fill (0, 0, width(), t0);
        c.setColor (0x40000000); c.fill (0, t0 - 1, width(), 1);
        const float waveW = std::round (width() * 0.6f);
        c.setColor (0xff8b9099);
        draw::text (c, "HIT " + std::to_string (A.hit + 1) + "  \xc2\xb7  EACH TRACK OVER WHAT IT LINES UP TO", boldFont (9), Font::kLeft, 14, 0, waveW, t0);
        draw::text (c, "LINES UP TO", boldFont (9), Font::kLeft, waveW + 160, 0, 120, t0);
        draw::text (c, "SUM", boldFont (9), Font::kRight, width() - 130, 0, 116, t0);
        c.setClampBounds (0, t0, width(), height() - t0 - 2);
        for (int i = 0; i < n; ++i)
        {
            const float y = t0 + i * lh - studio.view.scrollY;
            if (y + lh < t0 || y > height()) continue;
            const auto& tr = d.tracks[(size_t) i];
            const int k = alignIndexOf (i);
            c.setColor (0xd9000000); c.fill (0, y + lh - 1, width(), 1);
            c.setColor (0x14ffffff); c.fill (0, y + lh, width(), 1);
            if (k < 0) continue;
            const auto& m = A.mics[(size_t) k];
            const bool selected = k == A.mic;
            if (selected) { c.setColor (draw::withAlpha (vio, 0.10f)); c.fill (0, y, width(), lh - 1); c.setColor (draw::withAlpha (vio, 0.8f)); c.fill (0, y, 3, lh - 1); }

            // the wave
            const float wx = 12, wy = y + 8, wh = lh - 16, ww = waveW - 24;
            c.setColor (0x0dffffff); c.fill (wx, wy + wh / 2, ww, 1);
            if (! m.ref.empty())
            {
                const size_t N = m.ref.size();
                // each wave at its own height: the shapes are what line up, not the levels
                float pr = 1e-6f, pc = 1e-6f;
                for (size_t q = 0; q < N; ++q) { pr = std::max (pr, std::abs (m.ref[q])); pc = std::max ({ pc, std::abs (m.cur[q]), std::abs (m.rec[q]) }); }
                auto px = [&] (size_t q) { return wx + (float) q * ww / (float) (N - 1); };
                float pk = pr;
                auto py = [&] (float v) { return wy + wh / 2 - v / pk * wh * 0.47f; };
                if (m.status != "lead")
                {
                    visage::Path ref; ref.moveTo (px (0), py (0));
                    for (size_t q = 0; q < N; ++q) ref.lineTo (px (q), py (m.ref[q]));
                    ref.lineTo (px (N - 1), py (0)); ref.close();
                    c.setColor (0x33b9bec6); c.fill (ref);
                    pk = pc;
                    visage::Path rec; rec.moveTo (px (0), py (m.rec[0]));
                    for (size_t q = 1; q < N; ++q) rec.lineTo (px (q), py (m.rec[q]));
                    c.setColor (0x2effffff); c.stroke (rec, 0, 0, width(), height(), 0.8f);
                }
                visage::Path cur; cur.moveTo (px (0), py (m.cur[0]));
                for (size_t q = 1; q < N; ++q) cur.lineTo (px (q), py (m.cur[q]));
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (draw::withAlpha (tr.colourA, 0.22f)); c.stroke (cur, 0, 0, width(), height(), 4.5f);
                c.setBlendMode (visage::BlendMode::Alpha);
                c.setColor (tr.colourA); c.stroke (cur, 0, 0, width(), height(), 1.4f);
                // the hit's onset
                const float ox0 = wx + 1.5f / m.spanMs * ww;
                c.setColor (0x80ff5a3d);
                for (float yy = wy; yy < wy + wh; yy += 4) c.fill (ox0, yy, 1, 2);
                c.setColor (0xff5f6670);
                draw::text (c, draw::fmt ("%.0f ms", (double) m.spanMs), monoFont (8.5f), Font::kRight, wx, wy - 2, ww, 11);
            }

            // the lamp
            const float ix = waveW + 8;
            const unsigned lampCol = m.status == "lead" ? neon::amber : m.status == "corrected" ? 0xff7cff9b : m.status == "manual" ? neon::magenta : 0xff4c8dff;
            const char* lampTxt = m.status == "lead" ? "LEAD" : m.status == "corrected" ? "CORRECTED" : m.status == "manual" ? "MANUAL" : "IN PHASE";
            const float ly = y + 16;
            c.setColor (draw::withAlpha (lampCol, 0.4f)); c.circle (ix - 5, ly - 5, 10);
            c.setColor (lampCol); c.circle (ix - 2.5f, ly - 2.5f, 5);
            c.setColor (lampCol);
            draw::text (c, lampTxt, boldFont (9.5f), Font::kLeft, ix + 9, ly - 7, 140, 14);
            if (m.status == "lead")
            {
                c.setColor (0xff8b9099);
                draw::text (c, "Everything lines up to this track", bodyFont (10.5f), Font::kLeft, ix, ly + 10, width() - ix - 12, 16);
                continue;
            }
            // delay, polarity, phase EQ, L=R
            c.setColor (0xffe8ecf2);
            const std::string dl = A.samples ? draw::fmt ("%+.2f smp", (double) m.delaySmp) : draw::fmt ("%+.2f ms", (double) m.delayMs);
            draw::text (c, dl, monoBold (10.5f), Font::kLeft, ix, ly + 10, 90, 16);
            float bx = ix + 88;
            auto badge = [&] (const char* t, bool on, unsigned col)
            {
                const float w = textWidth (boldFont (8.5f), t) + 10;
                c.setColor (on ? draw::withAlpha (col, 0.22f) : Color (0x0dffffff)); c.roundedRectangle (bx, ly + 11, w, 14, 3);
                c.setColor (on ? Color (col) : Color (0xff3e434b)); draw::text (c, t, boldFont (8.5f), Font::kCenter, bx, ly + 11, w, 14);
                bx += w + 4;
            };
            badge ("\xc3\x98", m.flipped, neon::magenta);
            badge ("EQ", m.eq, vio);
            if (m.lrAvail) badge ("L=R", m.lr, neon::cyan);
            // what it lines up to
            c.setColor (0xffb9bec6);
            draw::text (c, m.to, bodyFont (10.5f), Font::kLeft, waveW + 160, ly - 7, width() - waveW - 300, 14);
            // the sum, before and after
            if (std::isfinite (m.before) && std::isfinite (m.after))
            {
                const float sx = width() - 130, sw = 116, sy = ly + 13;
                c.setColor (0x14ffffff); c.roundedRectangle (sx, sy, sw, 6, 3);
                auto pos = [&] (float v) { return sx + std::clamp ((v + 1) / 2, 0.0f, 1.0f) * sw; };
                const unsigned col = m.after < 0 ? 0xffff5a3d : 0xff7cff9b;
                c.setColor (draw::withAlpha (col, 0.85f)); c.roundedRectangle (sx, sy, pos (m.after) - sx, 6, 3);
                c.setColor (0xccffffff); c.fill (pos (m.before) - 0.75f, sy - 3, 1.5f, 12);
                c.setColor (0xff8b9099);
                draw::text (c, draw::fmt ("%+.2f", (double) m.before) + " \xe2\x86\x92 " + draw::fmt ("%+.2f", (double) m.after), monoFont (9.5f), Font::kRight, sx, ly - 7, sw, 14);
            }
        }
        c.setClampBounds (0, 0, width(), height());
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
        c.setColor (0x0dffffff);
        c.fill (0, 0, width(), rulerH);
        c.setColor (0x1affffff);
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
            const std::string label = "VEL " + std::to_string (k + 1) + "   " + std::to_string (L.numHits) + " RR";
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

    /** Catmull-Rom through the points, as cubic segments (the liquid edge). */
    static void smoothTo (visage::Path& p, const std::vector<Point>& pts, bool move)
    {
        if (pts.empty()) return;
        if (move) p.moveTo (pts[0].x, pts[0].y); else p.lineTo (pts[0].x, pts[0].y);
        for (size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const auto& p0 = pts[i ? i - 1 : 0]; const auto& p1 = pts[i]; const auto& p2 = pts[i + 1]; const auto& p3 = pts[std::min (pts.size() - 1, i + 2)];
            p.bezierTo (p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6, p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6, p2.x, p2.y);
        }
    }

    /** Style A: the real waveform as a liquid. Zoomed out, the min/max envelope with smoothed edges, a gradient
        across the lane in the mic's colours, a glossy top, a bright rim and a glow (the bloom adds the rest).
        Zoomed in to single samples, the actual wave as a glowing line filled to the centre. Stereo mics show
        left above right. A morphs to B (processed) with the A/B switch. */
    void drawWave (Canvas& c, const TrackInfo& tr, float y, float lh)
    {
        const auto& w = *tr.wave;
        const float mix = w.pyrB.empty() ? 0.0f : studio.abMix;
        const float pk = w.peak (mix);
        const float vz = pk > 0 ? std::clamp (0.92f / pk, 1.0f, 12.0f) : 1.0f;
        const int nch = w.channels();
        const double sr = w.sampleRate, pps = studio.view.pixelsPerSecond, spp = sr / pps;
        const float W = width();
        const Color ca = tr.mute ? Color (0xff59606b) : Color (tr.colourA), cb = tr.mute ? Color (0xff3d434c) : Color (tr.colourB);
        const float x0 = std::max (0.0f, timeToX (0.0)), x1 = std::min (W, timeToX (w.length() / sr));
        if (x1 - x0 < 1) return;
        const Brush across = Brush::linear (visage::Gradient (ca, cb, ca), Point (x0, 0), Point (x1, 0));

        for (int ch = 0; ch < nch; ++ch)
        {
            const float band = lh / (float) nch, cy = y + band * (ch + 0.5f), amp = band * 0.44f;
            if (nch > 1 && ch > 0) { c.setColor (0x0dffffff); c.fill (0, y + band * ch, W, 1); }
            c.setColor (ca.withAlpha (0.16f));
            c.fill (0, cy, W, 1);
            if (nch > 1)
            {
                c.setColor (0x47ffffff);
                draw::text (c, ch ? "R" : "L", boldFont (8.5f), Font::kLeft, 4, y + band * ch + 2, 12, 10);
            }

            if (spp >= 1.5)
            {
                // the envelope, one column per pixel, smoothed a little for the liquid edge
                const int n = (int) std::ceil (x1 - x0);
                std::vector<float> hi ((size_t) n), lo ((size_t) n);
                for (int i = 0; i < n; ++i)
                {
                    const double t = studio.view.start + (x0 + i) / pps;
                    const auto r = w.range (ch, (long) std::floor (t * sr), (long) std::floor ((t + 1.0 / pps) * sr), mix);
                    hi[(size_t) i] = std::min (1.0f, r.second * vz); lo[(size_t) i] = std::max (-1.0f, r.first * vz);
                }
                auto blur = [] (std::vector<float>& v)
                {
                    static const float k[5] = { 0.06f, 0.24f, 0.4f, 0.24f, 0.06f };
                    std::vector<float> o (v.size());
                    for (size_t i = 0; i < v.size(); ++i)
                    {
                        float a = 0;
                        for (int j = -2; j <= 2; ++j) a += v[std::clamp<size_t> ((size_t) std::max<long> (0, (long) i + j), 0, v.size() - 1)] * k[j + 2];
                        o[i] = a;
                    }
                    v.swap (o);
                };
                blur (hi); blur (lo);
                std::vector<Point> up, dn;
                for (int i = 0; i < n; i += 2) { up.push_back ({ x0 + i, cy - hi[(size_t) i] * amp }); dn.push_back ({ x0 + i, cy - lo[(size_t) i] * amp }); }
                if (up.size() < 2) continue;
                visage::Path body;
                smoothTo (body, up, true);
                std::vector<Point> back (dn.rbegin(), dn.rend());
                smoothTo (body, back, false);
                body.close();
                // the rims and glow follow the sound only (silence stays a hairline)
                visage::Path top, bottom;
                auto addRuns = [&] (visage::Path& p, const std::vector<Point>& pts, const std::vector<float>& v)
                {
                    std::vector<Point> run;
                    for (size_t k = 0; k < pts.size(); ++k)
                    {
                        const bool loud = std::abs (v[std::min (v.size() - 1, k * 2)]) > 0.02f;
                        if (loud) run.push_back (pts[k]);
                        if ((! loud || k + 1 == pts.size()) && ! run.empty())
                        {
                            if (k > 0 && ! loud) run.push_back (pts[k]);
                            if (run.size() > 1) smoothTo (p, run, true);
                            run.clear();
                            if (! loud && k + 1 < pts.size()) {}
                        }
                        else if (loud && run.size() == 1 && k > 0) run.insert (run.begin(), pts[k - 1]);
                    }
                };
                addRuns (top, up, hi); addRuns (bottom, dn, lo);
                c.setColor (ca.withAlpha (0.10f)); c.stroke (top, 0, 0, W, height(), 7.0f); c.stroke (bottom, 0, 0, W, height(), 7.0f);
                c.setColor (ca.withAlpha (0.18f)); c.stroke (top, 0, 0, W, height(), 3.0f); c.stroke (bottom, 0, 0, W, height(), 3.0f);
                c.setColor (across); c.fill (body);
                c.setColor (Brush::linear (visage::Gradient (Color (0x66ffffff), Color (0x0affffff), Color (0x0d000000), Color (0x5900001e)),
                                           Point (0, cy - amp), Point (0, cy + amp)));
                c.fill (body);
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (Color (0xb3ffffff)); c.stroke (top, 0, 0, W, height(), 1.1f);
                c.setColor (Color (0x47ffffff)); c.stroke (bottom, 0, 0, W, height(), 1.0f);
                c.setBlendMode (visage::BlendMode::Alpha);
            }
            else
            {
                // single samples: the actual wave
                const long s0 = std::max (0L, (long) std::floor ((studio.view.start + x0 / pps) * sr) - 2);
                const long s1 = std::min (w.length(), (long) std::ceil ((studio.view.start + x1 / pps) * sr) + 2);
                std::vector<Point> pts;
                for (long nS = s0; nS < s1; ++nS)
                {
                    const auto r = w.range (ch, nS, nS + 1, mix);
                    const float v = std::clamp ((std::abs (r.second) > std::abs (r.first) ? r.second : r.first) * vz, -1.0f, 1.0f);
                    pts.push_back ({ timeToX (nS / sr), cy - v * amp });
                }
                if (pts.size() < 2) continue;
                visage::Path fillP;
                fillP.moveTo (pts.front().x, cy);
                smoothTo (fillP, pts, false);
                fillP.lineTo (pts.back().x, cy);
                fillP.close();
                visage::Path line;
                smoothTo (line, pts, true);
                c.setColor (Brush::linear (visage::Gradient (ca.withAlpha (0.7f), cb.withAlpha (0.1f), ca.withAlpha (0.7f)), Point (0, cy - amp), Point (0, cy + amp)));
                c.fill (fillP);
                c.setColor (ca.withAlpha (0.22f)); c.stroke (line, 0, 0, W, height(), 7.0f);
                c.setColor (across); c.stroke (line, 0, 0, W, height(), 2.6f);
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (Color (0xe6eafcff)); c.stroke (line, 0, 0, W, height(), 1.0f);
                c.setBlendMode (visage::BlendMode::Alpha);
            }
        }
    }

    /** Without the audio (a file the engine hasn't read yet): the old block peaks. */
    void drawBlocks (Canvas& c, const TrackInfo& tr, float mid, float lh)
    {
        const float y = mid - lh * 0.5f;
        const float mixB = tr.peaksB.size() == tr.peaks.size() ? studio.abMix : 0.0f;
        auto peakAt = [&] (size_t b) { return mixB <= 0 ? tr.peaks[b] : tr.peaks[b] + (tr.peaksB[b] - tr.peaks[b]) * mixB; };
        float lanePeak = 0.0f;
        for (size_t b = 0; b < tr.peaks.size(); ++b) lanePeak = std::max (lanePeak, peakAt (b));
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
            for (int b = b0; b < b1; ++b) p = std::max (p, peakAt ((size_t) b));
            const float a = std::min (1.0f, p * vz) * amp;
            if (a < 0.6f) continue;
            c.setColor (glow.withAlpha (0.22f * dim));
            c.fill ((float) x - 1, mid - a * 1.06f, 3, a * 2.12f);
            c.setColor (core);
            if (tr.mute) c.setColor (Color (0xff5a606a));
            c.fill ((float) x, mid - a, 1, a * 2);
        }

    }

    void drawLane (Canvas& c, int i, float y, float lh)
    {
        const auto& d = studio.data;
        const auto& tr = d.tracks[(size_t) i];
        const float mid = y + lh * 0.5f;

        // an engraved groove between tracks
        c.setColor (0xd9000000);
        c.fill (0, y + lh - 1, width(), 1);
        c.setColor (0x14ffffff);
        c.fill (0, y + lh, width(), 1);
        c.setColor (draw::withAlpha (tr.colourA, 0.28f));
        c.fill (0, mid, width(), 1);

        if (tr.wave && tr.wave->channels() > 0) drawWave (c, tr, y, lh);
        else drawBlocks (c, tr, mid, lh);

        // level chips at the end of each velocity zone: small and quiet
        if (i < (int) (d.layers.empty() ? 0 : d.layers[0].micDb.size()))
        {
            const auto f = monoFont (9.5f);
            for (auto& L : d.layers)
            {
                const float x1 = timeToX (L.end);
                if (x1 < 40 || x1 > width() + 40 || ! std::isfinite (L.micDb[(size_t) i])) continue;
                const auto txt = draw::fmt ("%.1f", (double) L.micDb[(size_t) i]);
                const float w = textWidth (f, txt) + 8;
                c.setColor (0x73000000);
                c.roundedRectangle (x1 - w - 6, y + lh - 17, w, 13, 3);
                c.setColor (i == d.referenceTrack ? draw::withAlpha (neon::amber, 0.75f) : Color (0x8cc9ced6));
                draw::text (c, txt, f, Font::kCenter, x1 - w - 6, y + lh - 17, w, 13);
            }
        }
        // the selected hit: this mic's level on it
        const int sh = studio.selectedHit;
        if (sh >= 0 && sh < (int) d.hits.size() && i < (int) d.hits[(size_t) sh].micDb.size() && std::isfinite (d.hits[(size_t) sh].micDb[(size_t) i]))
        {
            const float x = timeToX (d.hits[(size_t) sh].time);
            if (x > -60 && x < width() + 10)
            {
                const auto f = monoBold (10.5f);
                const auto txt = draw::fmt ("%.1f", (double) d.hits[(size_t) sh].micDb[(size_t) i]);
                const float w = textWidth (f, txt) + 10;
                c.setColor (0xd9000000);
                c.roundedRectangle (x + 8, y + 4, w, 16, 4);
                c.setColor (draw::withAlpha (tr.colourA, 0.7f));
                c.roundedRectangleBorder (x + 8, y + 4, w, 16, 4, 1);
                c.setColor (i == d.referenceTrack ? Color (neon::amber) : Color (0xfff2efe8));
                draw::text (c, txt, f, Font::kCenter, x + 8, y + 4, w, 16);
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
        if (studio.step() == Step::align && studio.data.align.valid)
        {
            const int i = (int) std::floor ((e.position.y - top() + studio.view.scrollY) / studio.laneHeight());
            if (i >= 0 && i < (int) studio.data.tracks.size()) { studio.selectedTrack = i; if (studio.onSelectTrack) studio.onSelectTrack (i); }
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
// The track headers: a machined module per mic (rendered parts), Pro Tools style: the name, S and M, the pan
// (two knobs on a stereo mic, one per channel), the fader's level, a vertical mini fader and the meters.
// Holding the mini fader brings up the big fader beside it.
namespace parts
{
    #define LD_PART(n) reinterpret_cast<const unsigned char*> (BinaryData::n##_png), BinaryData::n##_pngSize
    static void image (Canvas& c, const unsigned char* d, int n, float x, float y, float w, float h) { c.setColor (0xffffffff); c.image (d, n, x, y, w, h); }
}

/** The fader law: Pro Tools-like, +6 dB at the top, 0 dB about three quarters up, -inf at the bottom. */
static float faderPos (float db) { return db <= -100 ? 0.0f : std::cbrt (std::pow (10.0f, db / 20.0f) / 2.0f); }
static float faderDb (float p) { return p <= 0.001f ? -100.0f : 20.0f * std::log10 (2.0f * p * p * p); }

struct Studio::Strips : visage::Frame
{
    explicit Strips (Studio& s) : studio (s) {}
    Studio& studio;

    static constexpr float W = 236, titleH = 40;
    struct Box { float x, y, w, h; bool contains (Point p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; } };
    /** The module of track i (screen coords of this frame). */
    Box module (int i) const
    {
        const float lh = studio.laneHeight();
        const float y = studio.screenContentTop() + i * lh - studio.view.scrollY;
        return { 4, y + 2.5f, W - 8, lh - 5 };
    }
    // parts of a module, in module coordinates (the render's layout)
    static Box nameBox() { return { 8, 6.5f, 134, 22 }; }
    static Box soloBox() { return { 8, 35.5f, 20, 22 }; }
    static Box muteBox() { return { 32, 35.5f, 20, 22 }; }
    static Point knobAt (bool stereo, int k) { return stereo ? Point (k ? 95.0f : 69.0f, 46.5f) : Point (74.0f, 46.5f); }
    static float knobR (bool stereo) { return stereo ? 8.5f : 9.5f; }
    Box slot (const Box& m) const { return { m.x + 167, m.y + 10, 4, m.h - 20 }; }
    float capY (const Box& m, float db) const { const auto s = slot (m); return s.y + 4 + (1.0f - faderPos (db) / faderPos (6.0f)) * (s.h - 8); }

    int dragFrom = -1, dropAt = -1; float dragY = 0; bool dragging = false;
    int faderTrack = -1, panTrack = -1, panSide = 0; float grabY = 0, grabValue = 0;

    void draw (Canvas& c) override
    {
        const auto& d = studio.data;
        // the title plate
        parts::image (c, LD_PART (title), 0, 0, W, titleH);
        c.setColor (0xffb9bec6);
        draw::spaced (c, "TRACKS", boldFont (10), (W - 74) / 2, 13, 14, 3.2f);

        c.setClampBounds (0, studio.screenContentTop(), width(), height() - studio.screenContentTop());
        for (int i = 0; i < (int) d.tracks.size(); ++i)
        {
            const auto m = module (i);
            if (m.y + m.h < 0 || m.y > height()) continue;
            drawModule (c, i, m);
        }
        c.setClampBounds (0, 0, width(), height());
        if (dragging && dropAt >= 0)
        {
            const float lh = studio.laneHeight(), y = studio.screenContentTop() - studio.view.scrollY + dropAt * lh;
            c.setColor (Color (neon::cyan).withAlpha (0.35f)); c.roundedRectangle (2, y - 4, width() - 4, 8, 4);
            c.setColor (Color (neon::cyan)); c.roundedRectangle (4, y - 1.5f, width() - 8, 3, 1.5f);
        }
    }

    void drawModule (Canvas& c, int i, const Box& m)
    {
        const auto& tr = studio.data.tracks[(size_t) i];
        const Color ca (tr.colourA), cb (tr.colourB);
        const bool sel = i == studio.selectedTrack;
        // the metal: top, a stretched middle, bottom
        const float topH = std::min (66.0f, m.h - 10), botH = 10;
        parts::image (c, LD_PART (module_top), m.x, m.y, m.w, topH);
        if (m.h - topH - botH > 0) parts::image (c, LD_PART (module_mid), m.x, m.y + topH, m.w, m.h - topH - botH);
        parts::image (c, LD_PART (module_bot), m.x, m.y + m.h - botH, m.w, botH);
        if (sel)
        {
            c.setColor (ca.withAlpha (0.16f)); c.roundedRectangleShadow (m.x, m.y, m.w, m.h, 5, 12);
            c.setColor (ca.withAlpha (0.55f)); c.roundedRectangleBorder (m.x, m.y, m.w, m.h, 5, 1.2f);
        }
        // the colour strip down the left edge
        c.setColor (ca.withAlpha (0.35f)); c.roundedRectangle (m.x + 1.5f, m.y + 6, 6, m.h - 12, 3);
        c.setColor (Brush::linear (ca, cb, Point (0, m.y + 6), Point (0, m.y + m.h - 6)));
        c.roundedRectangle (m.x + 3, m.y + 7, 2.5f, m.h - 14, 1.25f);

        // name in the glass, LEAD in amber at its right; the activity LED flashes with the mic's sound
        const auto nb = nameBox();
        const bool lead = i == studio.data.referenceTrack;
        c.setColor (0xfff2efe8);
        draw::text (c, tr.name, uiFont (13), Font::kCenter, m.x + nb.x + (lead ? 0 : 0), m.y + nb.y - 0.5f, nb.w - (lead ? 30 : 0), nb.h);
        if (lead)
        {
            c.setColor (neon::amber);
            draw::text (c, "LEAD", boldFont (8), Font::kRight, m.x + nb.x, m.y + nb.y - 0.5f, nb.w - 8, nb.h);
        }
        const float act = i < (int) meters.size() ? meters[(size_t) i] : 0.0f;
        {
            const float lx = m.x + 150, ly = m.y + 17.5f;
            c.setColor (0xff1a1c20); c.circle (lx - 3, ly - 3, 6);
            if (act > 0.02f)
            {
                const float a = std::min (1.0f, act * 1.6f);
                c.setColor (ca.withAlpha (0.45f * a)); c.circle (lx - 6, ly - 6, 12);
                c.setColor (ca.withAlpha (a)); c.circle (lx - 2.5f, ly - 2.5f, 5);
            }
        }

        // S and M: the buttons are in the render; the lit state and the letters are drawn
        auto button = [&] (Box b, const char* label, bool on, unsigned col)
        {
            const float bx = m.x + b.x, by = m.y + b.y - 2;
            if (on)
            {
                c.setColor (Color (col).withAlpha (0.5f)); c.roundedRectangleShadow (bx, by, b.w, b.h, 3, 10);
                c.setColor (col); c.roundedRectangle (bx + 1, by + 1, b.w - 2, b.h - 2, 3);
            }
            c.setColor (on ? 0xff1a1200 : 0xff8b9099);
            draw::text (c, label, boldFont (10), Font::kCenter, bx, by, b.w, b.h);
        };
        button (soloBox(), "S", tr.solo, neon::gold);
        button (muteBox(), "M", tr.mute, neon::red);

        // pan: one knob, or two on a stereo mic (left channel, right channel)
        const int nk = tr.stereo ? 2 : 1;
        for (int k = 0; k < nk; ++k)
        {
            const auto kp = knobAt (tr.stereo, k);
            const float r = knobR (tr.stereo), cx = m.x + kp.x, cy = m.y + kp.y;
            const float p = tr.stereo ? (k ? tr.panR : tr.panL) : tr.pan;
            const float side = r * 3.2f;
            if (tr.stereo) parts::image (c, LD_PART (knob_stereo), cx - side / 2, cy - side / 2 + 1.5f, side, side);
            else parts::image (c, LD_PART (knob_mono), cx - side / 2, cy - side / 2 + 1.5f, side, side);
            // the lit arc on the acrylic ring, from centre to the value
            const float R = r * 1.26f, a1 = p * 0.75f * 3.14159265f;
            c.setBlendMode (visage::BlendMode::Add);
            if (std::abs (p) > 0.01f)
            {
                const float lo = std::min (0.0f, a1), hi = std::max (0.0f, a1);
                c.setColor (ca.withAlpha (0.35f)); c.arc (cx - R - 2.5f, cy - R - 2.5f, 2 * R + 5, 5.0f, (lo + hi) / 2 - 1.5707963f, (hi - lo) / 2, true);
                c.setColor (ca); c.arc (cx - R - 1, cy - R - 1, 2 * R + 2, 2.0f, (lo + hi) / 2 - 1.5707963f, (hi - lo) / 2, true);
            }
            else
            {
                c.setColor (ca.withAlpha (0.5f)); c.circle (cx - 3.5f, cy - R - 3.5f, 7);
                c.setColor (ca); c.circle (cx - 1.5f, cy - R - 1.5f, 3);
            }
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (0xff1b1d21);
            c.segment (cx + std::sin (a1) * 2.5f, cy - std::cos (a1) * 2.5f, cx + std::sin (a1) * r * 0.72f, cy - std::cos (a1) * r * 0.72f, 1.6f, true);
            if (tr.stereo)
            {
                c.setColor (0xff6a707a);
                draw::text (c, k ? "R" : "L", boldFont (7.5f), Font::kCenter, cx - 6, m.y + 57, 12, 9);
            }
        }
        c.setColor (0xffd6dbe2);
        draw::text (c, tr.volumeDb <= -99 ? "-inf" : draw::fmt ("%.1f", (double) tr.volumeDb), monoBold (10), Font::kRight, m.x + 106, m.y + 40, 38, 14);

        // the mini fader: a milled slot lit below the cap, the cap from the render
        const auto sl = slot (m);
        c.setColor (0xff020203); c.roundedRectangle (sl.x - 0.5f, sl.y - 0.5f, sl.w + 1, sl.h + 1, 2.5f);
        c.setColor (0x10ffffff); c.fill (sl.x, sl.y + sl.h, sl.w, 1);
        const float cyF = capY (m, tr.volumeDb);
        const float fillTop = cyF + 7;
        if (sl.y + sl.h - fillTop > 1)
        {
            c.setColor (ca.withAlpha (0.35f)); c.roundedRectangle (sl.x - 2, fillTop, sl.w + 4, sl.y + sl.h - fillTop, 3);
            c.setColor (Brush::linear (ca, cb, Point (0, fillTop), Point (0, sl.y + sl.h)));
            c.roundedRectangle (sl.x + 0.5f, fillTop, sl.w - 1, sl.y + sl.h - fillTop, 1.5f);
        }
        for (float db : { 0.0f, -12.0f, -40.0f })
        {
            const float ty = capY (m, db);
            c.setColor (db == 0 ? 0x73ffffff : 0x33ffffff);
            c.fill (sl.x + 8, ty, db == 0 ? 7 : 4, 1);
        }
        parts::image (c, LD_PART (cap_small), sl.x + 2 - 16, cyF - 10, 32, 20);
        c.setColor (ca); c.roundedRectangle (sl.x + 2 - 9, cyF - 1.6f, 18, 2, 1);
        c.setColor (ca.withAlpha (0.45f)); c.roundedRectangle (sl.x + 2 - 11, cyF - 3, 22, 5, 2.5f);
        if (faderTrack == i) { c.setColor (ca.withAlpha (0.8f)); c.roundedRectangleBorder (sl.x + 2 - 13, cyF - 7.5f, 26, 14, 3, 1.5f); }

        // meters: one window, or two on a stereo mic
        const int nm = tr.stereo ? 2 : 1;
        for (int k = 0; k < nm; ++k)
        {
            const float mx = m.x + 192 + k * 8, my = sl.y, mh = sl.h;
            c.setColor (0xff020203); c.roundedRectangle (mx, my, 6, mh, 2);
            c.setColor (0x0dffffff); c.fill (mx, my + mh, 6, 1);
            const float lv = std::min (1.0f, i < (int) chanMeters.size() && k < (int) chanMeters[(size_t) i].size() ? chanMeters[(size_t) i][(size_t) k] : 0.0f);
            if (lv > 0.002f)
            {
                const float bh = (mh - 2) * lv;
                c.setColor (Brush::linear (visage::Gradient (Color (0xff3dffd2), Color (neon::lime), Color (neon::amber), Color (neon::magenta)),
                                           Point (0, my + mh), Point (0, my)));
                c.roundedRectangle (mx + 1, my + mh - 1 - bh, 4, bh, 1.5f);
            }
        }
    }

    std::vector<float> meters;                       ///< per track, for the activity LED
    std::vector<std::vector<float>> chanMeters;      ///< per track, per channel

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        auto& d = studio.data;
        for (int i = 0; i < (int) d.tracks.size(); ++i)
        {
            const auto m = module (i);
            if (! m.contains (e.position)) continue;
            const Point p (e.position.x - m.x, e.position.y - m.y);
            const auto& tr = d.tracks[(size_t) i];
            if (soloBox().contains (p)) { if (studio.onSolo) studio.onSolo (i, ! tr.solo); return; }
            if (muteBox().contains (p)) { if (studio.onMute) studio.onMute (i, ! tr.mute); return; }
            for (int k = 0; k < (tr.stereo ? 2 : 1); ++k)
            {
                const auto kp = knobAt (tr.stereo, k);
                if (std::hypot (p.x - kp.x, p.y - kp.y) > knobR (tr.stereo) + 5) continue;
                if (e.repeatClickCount() >= 2)
                {
                    if (tr.stereo) { if (studio.onPanLR) studio.onPanLR (i, k, k ? 1.0f : -1.0f); }
                    else if (studio.onPan) studio.onPan (i, 0.0f);
                    return;
                }
                panTrack = i; panSide = k; grabY = e.position.y; grabValue = tr.stereo ? (k ? tr.panR : tr.panL) : tr.pan;
                return;
            }
            const auto sl = slot (m);
            if (e.position.x > sl.x - 14 && e.position.x < sl.x + 18 && e.position.y > sl.y - 8 && e.position.y < sl.y + sl.h + 8)
            {
                if (e.repeatClickCount() >= 2) { if (studio.onVolume) studio.onVolume (i, 0.0f); return; }
                faderTrack = i; grabY = e.position.y; grabValue = faderPos (tr.volumeDb);
                studio.showBigFader (i);
                redraw();
                return;
            }
            if (nameBox().contains (p) && e.repeatClickCount() >= 2 && studio.onRename) { studio.onRename (i); return; }
            studio.selectedTrack = i; dragFrom = i; dragY = e.position.y; dragging = false;
            if (studio.onSelectTrack) studio.onSelectTrack (i);
            studio.redrawAll();
            return;
        }
    }

    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (faderTrack >= 0)
        {
            // the travel of the big fader: fine control whatever the lane height
            const float travel = 220;
            const float p = std::clamp (grabValue - (e.position.y - grabY) / travel * faderPos (6.0f), 0.0f, faderPos (6.0f));
            if (studio.onVolume) studio.onVolume (faderTrack, faderDb (p));
            return;
        }
        if (panTrack >= 0)
        {
            const float v = std::clamp (grabValue - (e.position.y - grabY) / 120.0f, -1.0f, 1.0f);
            const auto& tr = studio.data.tracks[(size_t) panTrack];
            if (tr.stereo) { if (studio.onPanLR) studio.onPanLR (panTrack, panSide, v); }
            else if (studio.onPan) studio.onPan (panTrack, v);
            return;
        }
        if (dragFrom < 0) return;
        if (! dragging && std::abs (e.position.y - dragY) < 6) return;
        dragging = true;
        const float lh = studio.laneHeight(), top = studio.screenContentTop() - studio.view.scrollY;
        dropAt = std::clamp ((int) std::floor ((e.position.y - top) / lh + 0.5f), 0, (int) studio.data.tracks.size());
        redraw();
    }

    void mouseUp (const visage::MouseEvent&) override
    {
        if (faderTrack >= 0) { faderTrack = -1; studio.showBigFader (-1); redraw(); return; }
        panTrack = -1;
        if (dragging && dropAt >= 0 && dropAt != dragFrom && dropAt != dragFrom + 1 && studio.onMoveTrack)
            studio.onMoveTrack (dragFrom, dropAt > dragFrom ? dropAt - 1 : dropAt);
        dragFrom = dropAt = -1; dragging = false;
        redraw();
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
// The big fader: the same plate, slot and cap as a mini fader, larger, beside the mic while its fader is held.
struct Studio::BigFader : visage::Frame
{
    explicit BigFader (Studio& s) : studio (s) { setIgnoresMouseEvents (true, false); setVisible (false); }
    Studio& studio;
    int track = -1;
    static constexpr float W = 96, H = 316, slotY = 66, slotH = 228;

    void draw (Canvas& c) override
    {
        if (track < 0 || track >= (int) studio.data.tracks.size()) return;
        const auto& tr = studio.data.tracks[(size_t) track];
        const Color ca (tr.colourA), cb (tr.colourB);
        c.setColor (0xd9000000); c.roundedRectangleShadow (8, 26, W, H, 12, 34);
        c.setColor (ca.withAlpha (0.2f)); c.roundedRectangleShadow (8, 12, W, H, 12, 30);
        const float ox = 8, oy = 12;
        parts::image (c, LD_PART (bigfader), ox, oy, W, H);
        c.setColor (ca);
        draw::text (c, tr.name, boldFont (8.5f), Font::kCenter, ox + 16, oy + 6, W - 32, 14);
        // the readout
        draw::glass (c, ox + 10, oy + 26, 76, 24, 5);
        c.setColor (neon::amber);
        draw::text (c, tr.volumeDb <= -99 ? "-inf" : draw::fmt ("%+.1f dB", (double) tr.volumeDb), monoBold (13), Font::kCenter, ox + 10, oy + 26, 76, 24);
        // the slot, lit below the cap, and the scale
        const float sx = ox + 44, sy = oy + slotY;
        c.setColor (0xff020203); c.roundedRectangle (sx, sy, 8, slotH, 4);
        c.setColor (0x14ffffff); c.fill (sx, sy + slotH, 8, 1);
        auto yOf = [&] (float db) { return sy + 6 + (1.0f - faderPos (db) / faderPos (6.0f)) * (slotH - 12); };
        const float cy = yOf (tr.volumeDb);
        if (sy + slotH - cy - 14 > 1)
        {
            c.setColor (ca.withAlpha (0.4f)); c.roundedRectangle (sx - 3, cy + 14, 14, sy + slotH - cy - 14, 7);
            c.setColor (Brush::linear (ca, cb, Point (0, cy + 14), Point (0, sy + slotH)));
            c.roundedRectangle (sx + 1.5f, cy + 14, 5, sy + slotH - cy - 16, 2.5f);
        }
        for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
        {
            const float ty = yOf (db);
            c.setColor (db == 0 ? 0x8cffffff : 0x40ffffff);
            c.fill (sx + 14, ty, db == 0 ? 14 : 9, 1);
            if (std::abs (ty - cy) < 16) continue;
            c.setColor (0xff9aa0a8);
            draw::text (c, db > 0 ? "+6" : draw::fmt ("%.0f", (double) db), monoFont (8.5f), Font::kRight, ox + 4, ty - 6, 34, 12);
        }
        parts::image (c, LD_PART (cap_big), sx + 4 - 30, cy - 20, 60, 40);
        c.setColor (ca.withAlpha (0.55f)); c.roundedRectangle (sx + 4 - 22, cy - 4, 44, 8, 4);
        c.setColor (ca); c.roundedRectangle (sx + 4 - 20, cy - 1.2f, 40, 2.5f, 1.2f);
    }
};

//==============================================================================
// Mix: the mixer window (approved design mix_v5). One rendered strip per mic (mono or stereo), then a divider and
// the wider Drum Bus. Each strip: name LCD, five insert slots, R1 / R2 send knobs, pan, M / S, fader with its scale,
// the liquid-tube meters (one per channel), fader and peak readouts. Click a strip to edit its Tone and Shape in the
// rack; click an insert slot for its menu, Cmd-click to bypass it; drag knobs and faders, double-click to reset.
struct Studio::Console : visage::Frame
{
    explicit Console (Studio& s) : studio (s) {}
    Studio& studio;
    std::vector<float> meters, peaks;
    float busMeter = 0, busPeak = 0;

    // the strip renders' geometry, in render pixels (112 or 144 wide, 690 tall)
    struct Geo { float W, insX, insW, r1, r2, pan, mX, mW, sX, sW, slot, readX, readW; std::vector<std::pair<float, float>> tubes; };
    static const Geo& geo (bool bus, bool stereo)
    {
        static const Geo mono { 112, 8, 96, 32, 80, 56, 12, 42, 58, 42, 34, 14, 50, { { 80, 20 } } };
        static const Geo st   { 112, 8, 96, 32, 80, 56, 12, 42, 58, 42, 34, 14, 50, { { 71, 15 }, { 90, 15 } } };
        static const Geo bs   { 144, 8, 128, 40, 104, 72, 14, 56, 74, 56, 48, 14, 66, { { 92, 19 }, { 116, 19 } } };
        return bus ? bs : stereo ? st : mono;
    }
    static constexpr float RH = 690, gapR = 5, divW = 24;
    float K = 1, Y0 = 0;
    std::vector<float> xs;   // left edge of each strip; the bus is last
    float divX = 0;

    void layout()
    {
        const int n = (int) studio.data.tracks.size();
        const float avail = width() - 16;
        K = std::min ((height() - 8) / RH, avail / (n * (112 + gapR) + divW + 12 + 144));
        Y0 = 4;
        xs.clear();
        for (int i = 0; i < n; ++i) xs.push_back (8 + i * (112 + gapR) * K);
        const float bx = width() - 8 - 144 * K;
        divX = bx - (divW + 6) * K;
        xs.push_back (bx);
    }
    // a point on a strip's render (the camera tilt: the same projection as the rack)
    Point P (int si, float px, float py) const
    {
        const auto& G = geo (si == (int) xs.size() - 1, false);
        return { xs[(size_t) si] + (G.W / 2 + (px - G.W / 2) / 1.01f) * K, Y0 + (345 + (py - 345) * 0.98163f / 1.01f) * K };
    }
    float fH() const { return 340 * K * 0.98163f / 1.01f; }
    /** The fader scale: +6 at the top, 0 dB at 12 %, -60 near the bottom (the mockup's law). */
    static float dbF (float db)
    {
        static const float m[7][2] = { { 6, 0 }, { 0, 0.12f }, { -6, 0.27f }, { -12, 0.42f }, { -24, 0.62f }, { -40, 0.8f }, { -60, 0.97f } };
        for (int i = 0; i < 6; ++i)
            if (db <= m[i][0] && db >= m[i + 1][0]) { const float u = (m[i][0] - db) / (m[i][0] - m[i + 1][0]); return m[i][1] + u * (m[i + 1][1] - m[i][1]); }
        return db > 6 ? 0.0f : 1.0f;
    }
    static float dbOfF (float f)
    {
        float lo = -100, hi = 6;
        for (int it = 0; it < 40; ++it) { const float mid = (lo + hi) / 2; if (dbF (mid) > f) lo = mid; else hi = mid; }
        return f >= 0.985f ? -100.0f : (lo + hi) / 2;
    }
    static float sendPos (float db) { return db <= -59.5f ? 0.0f : std::clamp ((db + 60) / 66, 0.0f, 1.0f); }   // -60..+6
    static float sendDb (float p) { return p <= 0.005f ? -60.0f : -60 + p * 66; }

    void arcRing (Canvas& c, Point p, float r, float v, unsigned col, bool bip, float w)
    {
        const float pi = 3.14159265f, a0 = -0.75f * pi, full = 1.5f * pi;
        c.setColor (0x14ffffff); c.arc (p.x - r - w / 2, p.y - r - w / 2, 2 * r + w, w, 0, full / 2, true);
        float s0 = bip ? 0.0f : a0, s1 = bip ? (v - 0.5f) * full : a0 + v * full;
        if (s1 < s0) std::swap (s0, s1);
        if (s1 - s0 < 0.01f) return;
        c.setBlendMode (visage::BlendMode::Add);
        c.setColor (Color (col).withAlpha (0.3f)); c.arc (p.x - r - w * 1.5f, p.y - r - w * 1.5f, 2 * r + 3 * w, 3 * w, (s0 + s1) / 2, (s1 - s0) / 2, true);
        c.setColor (Color (col)); c.arc (p.x - r - w / 2, p.y - r - w / 2, 2 * r + w, w, (s0 + s1) / 2, (s1 - s0) / 2, true);
        c.setBlendMode (visage::BlendMode::Alpha);
    }

    /** The liquid tube: tinted glass, glowing liquid with a bright meniscus, a curved highlight, a peak bead. */
    void tube (Canvas& c, float x, float y, float w, float h, float lev, float peak, unsigned colour)
    {
        const Color col (colour), r = w / 2 > 0 ? Color (colour) : Color (colour);
        const float rad = w / 2;
        c.setColor (Brush::linear (Color (0xf20a0c10), Color (0xe6161a22), Point (x, 0), Point (x + w / 2, 0)));
        c.roundedRectangle (x, y, w, h, rad);
        for (float d : { 0.0f, -6.0f, -12.0f, -24.0f, -40.0f }) { c.setColor (0x1affffff); c.fill (x + 2, y + dbF (d) * h, w * 0.3f, 1); }
        lev = std::clamp (lev, 0.0f, 1.0f);
        if (lev > 0.004f)
        {
            const float top = y + h * (1 - lev);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (col.withAlpha (0.18f)); c.roundedRectangleShadow (x - w, top - w * 0.5f, w * 3, y + h - top + w, rad * 2, w * 1.6f);
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (Brush::linear (col.withAlpha (1.0f).interpolateWith (Color (0xff000000), 0.55f), col.interpolateWith (Color (0xffffffff), 0.45f), Point (0, y + h), Point (0, top)));
            c.roundedRectangle (x + 0.5f, top, w - 1, y + h - top - 0.5f, std::min (rad, (y + h - top) / 2));
            c.setColor (Brush::linear (Color (0x73000000), Color (0x00000000), Point (x, 0), Point (x + w * 0.3f, 0)));
            c.fill (x, top + 2, w * 0.3f, y + h - top - 3);
            c.setColor (0x38ffffff); c.fill (x + w * 0.36f, top + 2, w * 0.14f, y + h - top - 4);
            c.setColor (col.interpolateWith (Color (0xffffffff), 0.7f)); c.roundedRectangle (x + 1, top - 0.7f, w - 2, 1.6f, 0.8f);
        }
        // the glass: a specular streak, a rim
        c.setColor (0x4dffffff); c.roundedRectangle (x + w * 0.18f, y + rad * 0.7f, w * 0.1f, h - rad * 1.4f, w * 0.05f);
        c.setColor (0x8cd2dceb); c.roundedRectangleBorder (x, y, w, h, rad, 1);
        if (peak > 0.01f)
        {
            const float py = y + h * (1 - std::clamp (peak, 0.0f, 1.0f));
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (col.withAlpha (0.5f)); c.circle (x + w / 2 - w * 0.45f, py - w * 0.45f, w * 0.9f);
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (col.interpolateWith (Color (0xffffffff), 0.6f)); c.circle (x + w / 2 - w * 0.25f, py - w * 0.18f, w * 0.5f);
        }
    }

    void draw (Canvas& c) override
    {
        layout();
        const auto& d = studio.data;
        const auto& mv = d.mix;
        const int n = (int) d.tracks.size();
        meters.resize ((size_t) n, 0.0f); peaks.resize ((size_t) n, 0.0f);
        c.setColor (0xff08090b); c.roundedRectangle (0, 0, width(), height(), 8);
        parts::image (c, LD_PART (divider), divX, Y0, divW * K, RH * K);
        for (int si = 0; si <= n; ++si)
        {
            const bool bus = si == n;
            const TrackInfo* tr = bus ? nullptr : &d.tracks[(size_t) si];
            const bool stereo = bus || tr->stereo;
            const auto& G = geo (bus, stereo);
            const float x0 = xs[(size_t) si], W = G.W * K;
            const unsigned col = bus ? neon::orange : tr->colourA;
            const Color cc (col);
            if (bus) parts::image (c, LD_PART (strip_bus), x0, Y0, W, RH * K);
            else if (stereo) parts::image (c, LD_PART (strip_stereo), x0, Y0, W, RH * K);
            else parts::image (c, LD_PART (strip_mono), x0, Y0, W, RH * K);
            // a wash of the track colour over the metal
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (cc.withAlpha (0.07f)); c.roundedRectangle (x0 + 4 * K, Y0 + 4 * K, W - 8 * K, 682 * K, 6);
            c.setBlendMode (visage::BlendMode::Alpha);
            const bool sel = bus ? mv.sel < 0 : mv.sel == si;
            if (sel)
            {
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (cc.withAlpha (0.25f)); c.roundedRectangleBorder (x0 - 3, Y0 - 3, W + 6, RH * K + 6, 9, 6);
                c.setBlendMode (visage::BlendMode::Alpha);
                c.setColor (cc.withAlpha (0.95f)); c.roundedRectangleBorder (x0 + 1, Y0 + 1, W - 2, RH * K - 2, 7, 2);
            }
            // name LCD
            {
                const auto p = P (si, 8, 8); const float nw = (G.W - 16) * K, nh = 24 * K;
                c.setColor (Brush::linear (cc.withAlpha (0.18f), cc.withAlpha (0.06f), Point (0, p.y), Point (0, p.y + nh)));
                c.roundedRectangle (p.x + 2, p.y + 2, nw - 4, nh - 4, 3);
                c.setColor (cc.interpolateWith (Color (0xffffffff), 0.55f));
                draw::text (c, bus ? std::string ("Drum Bus") : tr->name, uiFont (std::min (10.0f, 11 * K)), Font::kCenter, p.x, p.y, nw, nh);
            }
            // inserts
            const auto* ch = bus ? &mv.bus : (si < (int) mv.chans.size() ? &mv.chans[(size_t) si] : nullptr);
            for (int k = 0; k < 5; ++k)
            {
                const auto p = P (si, G.insX, 40 + k * 26); const float iw = G.insW * K, ih = 22 * K;
                const auto* in = ch ? &ch->ins[(size_t) k] : nullptr;
                const bool has = in && ! in->name.empty();
                if (has)
                {
                    c.setBlendMode (visage::BlendMode::Add);
                    c.setColor (Brush::linear (cc.withAlpha (in->on ? 0.10f : 0.03f), cc.withAlpha (in->on ? 0.30f : 0.08f), Point (0, p.y), Point (0, p.y + ih)));
                    c.roundedRectangle (p.x + 4, p.y + 3, iw - 8, ih - 8, 4);
                    if (in->on) { c.setColor (cc); c.fill (p.x + 12, p.y + ih - 6, iw - 24, 1.4f); }
                    c.setBlendMode (visage::BlendMode::Alpha);
                }
                c.setColor (has ? (in->on ? 0xfff4f6fa : 0xff6c717a) : 0xff4a4f57);
                draw::text (c, has ? in->name : std::string(), boldFont (std::min (7.6f, 8.6f * K)), Font::kCenter, p.x, p.y, iw, ih - 3);
            }
            // R1 / R2 sends
            const float r1 = ch ? ch->r1 : -60, r2 = ch ? ch->r2 : -60;
            const float kr = (bus ? 16.5f : 15) * K;
            arcRing (c, P (si, G.r1, 198), kr, sendPos (r1), 0xffa78bff, false, 2);
            arcRing (c, P (si, G.r2, 198), kr, sendPos (r2), 0xff29e3ff, false, 2);
            c.setColor (0xffb9bec6);
            for (auto [k, lbl] : { std::pair<float, const char*> { G.r1, "R1" }, { G.r2, "R2" } })
            {
                const auto p = P (si, k, 198);
                draw::text (c, lbl, boldFont (6.8f), Font::kCenter, p.x - 14, p.y + (bus ? 22 : 21) * K - 6, 28, 12);
            }
            // pan
            {
                const auto p = P (si, G.pan, 250);
                const float pan = bus ? 0.0f : tr->pan;
                arcRing (c, p, (bus ? 19 : 17.5f) * K, pan * 0.5f + 0.5f, bus ? neon::orange : 0xffffffffu, true, 2);
                c.setColor (0xff8b9099);
                const std::string pl = bus ? std::string ("BAL") : std::abs (pan) < 0.01f ? std::string ("C") : draw::fmt (pan < 0 ? "L%.0f" : "R%.0f", std::abs (pan) * 100);
                draw::text (c, pl, boldFont (6.5f), Font::kCenter, p.x - 16, p.y + (bus ? 23 : 21.5f) * K - 6, 32, 12);
            }
            // M / S
            {
                const bool mute = bus ? mv.busMute : tr->mute, solo = bus ? false : tr->solo;
                auto key = [&] (float kx, float kw, const char* lbl, bool on, unsigned kc)
                {
                    const auto p = P (si, kx, 276); const float w = kw * K, h = 20 * K;
                    if (on)
                    {
                        c.setBlendMode (visage::BlendMode::Add);
                        c.setColor (Color (kc).withAlpha (0.22f)); c.roundedRectangle (p.x + 3, p.y + 3, w - 6, h - 6, 3);
                        c.setBlendMode (visage::BlendMode::Alpha);
                    }
                    c.setColor (on ? kc : 0xff9aa0a9u);
                    draw::text (c, lbl, boldFont (8), Font::kCenter, p.x, p.y, w, h);
                };
                key (G.mX, G.mW, "M", mute, neon::red);
                if (! bus) key (G.sX, G.sW, "S", solo, neon::gold);
            }
            // fader: scale, lit groove, cap
            const float fdb = bus ? mv.busFader : tr->volumeDb;
            const auto ft = P (si, G.slot, 306); const float fh = fH(), cy = ft.y + dbF (fdb) * fh;
            c.setColor (cc.withAlpha (0.85f)); c.fill (ft.x - 1.2f, cy, 2.4f, ft.y + fh - cy);
            for (float dd : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -40.0f })
            {
                const float yy = ft.y + dbF (dd) * fh;
                c.setColor (0x47ffffff); c.fill (ft.x - 16 * K, yy, 7 * K, 1);
                c.setColor (0xff6f767f);
                draw::text (c, dd > 0 ? std::string ("+6") : draw::fmt ("%.0f", dd), monoFont (6.5f), Font::kRight, ft.x - 17 * K - 30, yy - 5, 28, 10);
            }
            parts::image (c, LD_PART (cap_small), ft.x - 16 * K, cy - 11 * K, 32 * K, 22 * K);
            c.setColor (cc); c.fill (ft.x - 13 * K, cy - 0.75f, 26 * K, 1.5f);
            // meters
            const float lv = bus ? busMeter : meters[(size_t) si], pk = bus ? busPeak : peaks[(size_t) si];
            for (auto [tx, tw] : G.tubes)
            {
                const auto p = P (si, tx, 306);
                tube (c, p.x, p.y, tw * K, fh, lv, pk, col);
            }
            // readouts
            const auto rp = P (si, G.readX, 652);
            c.setColor (bus ? neon::orange : 0xffe8ecf2u);
            draw::text (c, fdb <= -99 ? std::string ("-inf") : draw::fmt ("%+.1f", fdb), monoBold (8.5f), Font::kCenter, rp.x, rp.y, G.readW * K, 22 * K);
            const float tc = G.tubes.size() == 2 ? (G.tubes[0].first + G.tubes[1].first + G.tubes[1].second) / 2 : G.tubes[0].first + G.tubes[0].second / 2;
            const float pdb = pk > 0.001f ? 20 * std::log10 (pk) : -100;
            c.setColor (pdb > -2 ? 0xffff5a5fu : cc.interpolateWith (Color (0xffffffff), 0.4f).toARGB());
            draw::text (c, pdb <= -99 ? std::string ("-inf") : draw::fmt ("%.1f", pdb), monoBold (8), Font::kCenter, P (si, tc, 652).x - 24, rp.y, 48, 22 * K);
        }
    }

    // ---- hit testing
    enum Part { none, strip, ins, r1, r2, pan, mute, solo, fader };
    struct Hit { int si = -1; Part part = none; int slot = -1; };
    Hit hitAt (Point q) const
    {
        const int n = (int) studio.data.tracks.size();
        for (int si = 0; si <= n && si < (int) xs.size(); ++si)
        {
            const bool bus = si == n;
            const auto& G = geo (bus, bus || studio.data.tracks[(size_t) si].stereo);
            const float x0 = xs[(size_t) si];
            if (q.x < x0 || q.x > x0 + G.W * K || q.y < Y0 || q.y > Y0 + RH * K) continue;
            auto in = [&] (float px, float py, float w, float h) { const auto p = P (si, px, py); return q.x >= p.x && q.x < p.x + w * K && q.y >= p.y && q.y < p.y + h * K; };
            auto near = [&] (float px, float py, float r) { const auto p = P (si, px, py); return std::hypot (q.x - p.x, q.y - p.y) < r * K; };
            for (int k = 0; k < 5; ++k) if (in (G.insX, 40 + k * 26, G.insW, 22)) return { si, ins, k };
            if (near (G.r1, 198, 20)) return { si, r1 };
            if (near (G.r2, 198, 20)) return { si, r2 };
            if (near (G.pan, 250, 22)) return { si, pan };
            if (in (G.mX, 276, G.mW, 20)) return { si, mute };
            if (! bus && in (G.sX, 276, G.sW, 20)) return { si, solo };
            if (in (G.slot - 18, 296, 36, 360)) return { si, fader };
            return { si, strip };
        }
        return {};
    }

    Hit drag; float grabY = 0, grabV = 0;

    float valueOf (const Hit& h) const
    {
        const auto& d = studio.data; const auto& mv = d.mix; const int n = (int) d.tracks.size();
        const bool bus = h.si == n;
        const auto* ch = bus ? &mv.bus : (h.si < (int) mv.chans.size() ? &mv.chans[(size_t) h.si] : nullptr);
        switch (h.part)
        {
            case r1: return sendPos (ch ? ch->r1 : -60);
            case r2: return sendPos (ch ? ch->r2 : -60);
            case pan: return bus ? 0.0f : d.tracks[(size_t) h.si].pan;
            case fader: return dbF (bus ? mv.busFader : d.tracks[(size_t) h.si].volumeDb);
            default: return 0;
        }
    }
    void send (const Hit& h, float v)
    {
        const int n = (int) studio.data.tracks.size();
        const bool bus = h.si == n;
        const std::string who = bus ? std::string ("bus") : std::to_string (h.si);
        if (! studio.onMix) return;
        switch (h.part)
        {
            case r1: studio.onMix ("send:" + who + ":0", sendDb (v)); break;
            case r2: studio.onMix ("send:" + who + ":1", sendDb (v)); break;
            case pan: if (! bus && studio.onPan) studio.onPan (h.si, v); break;
            case fader: if (bus) studio.onMix ("busfader", dbOfF (v)); else if (studio.onVolume) studio.onVolume (h.si, dbOfF (v)); break;
            default: break;
        }
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        const auto h = hitAt (e.position);
        if (h.si < 0) return;
        const int n = (int) studio.data.tracks.size();
        const bool bus = h.si == n;
        const std::string who = bus ? std::string ("bus") : std::to_string (h.si);
        // any click on a strip selects it for the rack
        if (studio.onMix && (bus ? studio.data.mix.sel >= 0 : studio.data.mix.sel != h.si)) studio.onMix ("sel:" + who, 0);
        switch (h.part)
        {
            case ins:
                if (e.isCmdDown() || e.isAltDown()) { if (studio.onMix) studio.onMix ("insbyp:" + who + ":" + std::to_string (h.slot), 0); }
                else if (! e.isRightButton() && studio.onMix && ! (bus ? studio.data.mix.bus : studio.data.mix.chans[(size_t) h.si]).ins[(size_t) h.slot].name.empty())
                    studio.onMix ("plug:open:" + who + ":" + std::to_string (h.slot), 0);   // a filled slot opens its window; right-click for the menu
                else if (studio.onMixMenu)
                {
                    const auto p = P (h.si, geo (bus, true).insX, 40 + h.slot * 26);
                    studio.onMixMenu ("ins:" + who + ":" + std::to_string (h.slot), p.x + positionInWindow().x, p.y + positionInWindow().y, geo (bus, true).insW * K, 22 * K);
                }
                return;
            case mute: if (bus) { if (studio.onMix) studio.onMix ("busmute", studio.data.mix.busMute ? 0 : 1); } else if (studio.onMute) studio.onMute (h.si, ! studio.data.tracks[(size_t) h.si].mute); return;
            case solo: if (studio.onSolo) studio.onSolo (h.si, ! studio.data.tracks[(size_t) h.si].solo); return;
            case r1: case r2: case pan: case fader:
                if (e.repeatClickCount() >= 2)
                {
                    send (h, h.part == fader ? dbF (0) : h.part == pan ? 0.0f : 0.0f);
                    return;
                }
                drag = h; grabY = e.position.y; grabV = valueOf (h);
                return;
            default: return;
        }
    }
    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (drag.si < 0) return;
        const float dy = e.position.y - grabY;
        if (drag.part == fader) send (drag, std::clamp (grabV + dy / fH(), 0.0f, 1.0f));
        else if (drag.part == pan) send (drag, std::clamp (grabV - dy / 120.0f, -1.0f, 1.0f));
        else send (drag, std::clamp (grabV - dy / 150.0f, 0.0f, 1.0f));
    }
    void mouseUp (const visage::MouseEvent&) override { drag = {}; }
};

//==============================================================================
// The rack: units for the current step, room and master meters always at the bottom.
struct Studio::Rack : visage::Frame
{
    explicit Rack (Studio& s) : studio (s) {}
    Studio& studio;
    float roomMeter = 0, masterMeter = 0;
    double padFlash[8] {};
    static constexpr float DW = 318, DH = 664;
    float k = 1, ox = 0;
    // The renders look up 11 degrees at the panel and frame it 1% wide, so the panel lands slightly smaller in the
    // picture, more so up and down. Everything drawn on top goes through the same projection, so it sits on the
    // part it belongs to anywhere in the rack.
    static constexpr float projX = 1.0f / 1.01f, projY = 0.98163f / 1.01f;
    float X (float v) const { return ox + (DW / 2 + (v - DW / 2) * projX) * k; }
    float Y (float v) const { return (DH / 2 + (v - DH / 2) * projY) * k; }
    float panelX (float px) const { return DW / 2 + ((px - ox) / k - DW / 2) / projX; }
    float panelY (float py) const { return DH / 2 + (py / k - DH / 2) / projY; }
    float S (float v) const { return v * k; }
    static constexpr float padX[3] = { 30, 123, 216 }, padY[2] = { 384, 456 };

    void resized() override
    {
        k = std::min (width() / DW, height() / DH);
        ox = std::round ((width() - DW * k) / 2);
    }

    void arcKnob (Canvas& c, float cx, float cy, float v, unsigned col, float r = 21)
    {
        // the lit value on the frosted ring, and the pointer on the silver cap
        const float R = S (r * 1.24f), a0 = -0.75f * 3.14159265f, a1 = a0 + 1.5f * 3.14159265f * std::clamp (v, 0.0f, 1.0f);
        const float mid = (a0 + a1) / 2 - 1.5707963f, half = (a1 - a0) / 2;
        c.setBlendMode (visage::BlendMode::Add);
        c.setColor (Color (col).withAlpha (0.28f)); c.arc (X (cx) - R - S (5), Y (cy) - R - S (5), 2 * R + S (10), S (10), mid, half, true);
        c.setColor (Color (col).withAlpha (0.95f)); c.arc (X (cx) - R - S (1.6f), Y (cy) - R - S (1.6f), 2 * R + S (3.2f), S (3.2f), mid, half, true);
        c.setColor (Color (0xb3ffffff)); c.arc (X (cx) - R - S (0.5f), Y (cy) - R - S (0.5f), 2 * R + S (1), S (1), mid, half, true);
        c.setBlendMode (visage::BlendMode::Alpha);
        c.setColor (0xff1b1d21);
        c.segment (X (cx) + std::sin (a1) * S (5), Y (cy - 3) - std::cos (a1) * S (5), X (cx) + std::sin (a1) * S (r * 0.67f), Y (cy - 3) - std::cos (a1) * S (r * 0.67f), S (2.2f), true);
    }

    void silk (Canvas& c, const std::string& t, float x, float y, unsigned col, float size = 9, float sp = 1.8f)
    {
        c.setColor (col);
        draw::spaced (c, t, boldFont (S (size)), X (x), Y (y), S (12), S (sp));
    }

    void draw (Canvas& c) override
    {
        c.setColor (0xff050506);
        c.fill (0, 0, width(), height());
        c.setColor (0xffffffff);
        if (studio.step() == Step::align) { drawAlign (c); return; }
        if (studio.step() == Step::tune) { drawTune (c); return; }
        if (studio.step() == Step::mix) { drawMix (c); return; }
        c.image (reinterpret_cast<const unsigned char*> (BinaryData::rack_png), BinaryData::rack_pngSize, ox, 0, DW * k, DH * k);
        const auto& d = studio.data;
        const auto& lv = d.level;
        const unsigned green = 0xff7cff9b;

        // LEVEL module
        silk (c, "LEVEL", 36, 18, green, 9.5f, 2.2f);
        c.setColor (Color (green).withAlpha (lv.valid ? 0.5f : 0.15f)); c.circle (X (30) - S (4), Y (23) - S (4), S (8));
        c.setColor (lv.valid ? Color (green) : Color (0xff2a2d33)); c.circle (X (30) - S (2), Y (23) - S (2), S (4));
        c.setColor (0xff8b9099);
        draw::text (c, lv.valid ? std::to_string (lv.onStandard) + " / " + std::to_string (lv.total) + " ON STANDARD" : std::string ("NOT MEASURED YET"), boldFont (S (8.5f)),
                    Font::kRight, X (120), Y (16), S (168), S (12));

        // the screen: the lead mic's level per velocity
        const float sx = 26, sy = 44, sw = 266, sh = 132;
        c.setColor (0x14789aff);
        for (int g = 1; g < 4; ++g) c.fill (X (sx + 6), Y (sy + g * sh / 4), S (sw - 12), 1);
        for (int g = 1; g < 6; ++g) c.fill (X (sx + g * sw / 6), Y (sy + 6), 1, S (sh - 12));
        silk (c, "LEAD MIC  dB PER VELOCITY", sx + 10, sy + 8, 0xff6a707a, 8, 1.2f);
        std::vector<float> curve;
        for (float v : lv.curveDb) if (std::isfinite (v)) curve.push_back (v);
        if (curve.size() >= 2)
        {
            const float lo = *std::min_element (curve.begin(), curve.end()), hi = *std::max_element (curve.begin(), curve.end());
            const float span = std::max (1.0f, hi - lo);
            std::vector<Point> pts;
            for (size_t i = 0; i < curve.size(); ++i)
                pts.push_back ({ X (sx + 22 + i * (sw - 44) / (curve.size() - 1)), Y (sy + sh - 22 - (curve[i] - lo) / span * (sh - 52)) });
            visage::Path fillP; fillP.moveTo (pts.front().x, Y (sy + sh - 6));
            for (auto& p : pts) fillP.lineTo (p.x, p.y);
            fillP.lineTo (pts.back().x, Y (sy + sh - 6)); fillP.close();
            c.setColor (Brush::linear (Color (0x4d2ee6ff), Color (0x002ee6ff), Point (0, Y (sy + 20)), Point (0, Y (sy + sh))));
            c.fill (fillP);
            visage::Path line; line.moveTo (pts[0].x, pts[0].y); for (size_t i = 1; i < pts.size(); ++i) line.lineTo (pts[i].x, pts[i].y);
            const Brush grad = Brush::linear (visage::Gradient (Color (neon::blue), Color (neon::cyan), Color (green), Color (neon::amber)),
                                              Point (pts.front().x, 0), Point (pts.back().x, 0));
            c.setColor (Color (neon::cyan).withAlpha (0.25f)); c.stroke (line, 0, 0, width(), height(), S (7));
            c.setColor (grad); c.stroke (line, 0, 0, width(), height(), S (2.4f));
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const unsigned col = draw::layerColour ((int) i, (int) pts.size());
                c.setColor (Color (col).withAlpha (0.35f)); c.circle (pts[i].x - S (8), pts[i].y - S (8), S (16));
                c.setColor (col); c.circle (pts[i].x - S (4), pts[i].y - S (4), S (8));
                c.setColor (0xccffffff); c.circle (pts[i].x - S (2.4f), pts[i].y - S (2.4f), S (2.8f));
            }
            c.setColor (0xff6a707a);
            draw::text (c, draw::fmt ("%.1f dB LOUDEST", (double) hi), monoFont (S (8.5f)), Font::kRight, X (sx + 100), Y (sy + sh - 18), S (sw - 112), S (12));
        }
        else
        {
            c.setColor (0xff5f6670);
            draw::text (c, "Run Auto Prep to measure", bodyFont (S (11)), Font::kCenter, X (sx), Y (sy), S (sw), S (sh));
        }

        // the buttons
        const struct { const char* t; float x; bool on; } btn[4] = { { "PER MIC", 26, ! lv.linked }, { "LINKED", 90, lv.linked }, { "EVEN", 166, ! lv.pro }, { "STANDARD", 230, lv.pro } };
        for (auto& b : btn) { const Key kk { b.x, 190, 60, 30, "" }; keyLabel (c, kk, b.t, b.on, green); }

        // the knobs: the Level settings
        const struct { float x; unsigned col; float v; const char* label; std::string val; } kn[4] = {
            { 59, neon::amber, (lv.shape - 0.3f) / 1.7f, "SHAPE", draw::fmt ("%.2f", (double) lv.shape) },
            { 126, neon::cyan, (lv.offsetDb + 12) / 24, "OFFSET", draw::fmt ("%+.1f dB", (double) lv.offsetDb) },
            { 193, neon::magenta, (lv.rangeDb - 6) / 42, "RANGE", draw::fmt ("%.0f dB", (double) lv.rangeDb) },
            { 260, green, (lv.winMs - 10) / 190, "WINDOW", draw::fmt ("%.0f ms", (double) lv.winMs) } };
        for (auto& kk : kn)
        {
            arcKnob (c, kk.x, 276, kk.v, kk.col);
            c.setColor (0xffb9bec6);
            draw::text (c, kk.label, boldFont (S (8.5f)), Font::kCenter, X (kk.x - 32), Y (309), S (64), S (12));
            c.setColor (kk.col);
            draw::text (c, kk.val, monoBold (S (10.5f)), Font::kCenter, X (kk.x - 36), Y (322), S (72), S (13));
        }

        // PADS
        silk (c, "PADS", 30, 362, 0xffb9bec6);
        c.setColor (0xff8b9099);
        draw::text (c, "KEYS 1-6 \xc2\xb7 RANDOM RR", boldFont (S (8)), Font::kRight, X (120), Y (360), S (168), S (12));
        const int nPads = std::min (6, std::max (1, (int) d.layers.size()));
        for (int v = 0; v < 6; ++v)
        {
            const float px = padX[v % 3], py = padY[v / 3];
            const bool live = v < nPads && ! d.layers.empty();
            const unsigned col = draw::layerColour (v, std::max (6, (int) d.layers.size()));
            const float flash = (float) std::max (0.0, 1.0 - (studio.now - padFlash[v]) / 0.35);
            const float glow = live ? 0.07f + 0.7f * flash : 0.03f;
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (col).withAlpha (glow)); c.roundedRectangleShadow (X (px + 2), Y (py + 2), S (76), S (56), S (7), S (14));
            c.setColor (Color (col).withAlpha (glow * 1.2f)); c.roundedRectangle (X (px + 4), Y (py + 38), S (72), S (18), S (6));
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (Color (col).withAlpha (live ? 0.45f + 0.5f * flash : 0.15f));
            c.roundedRectangleBorder (X (px), Y (py), S (80), S (60), S (7), S (1));
            silk (c, "VEL " + std::to_string (v + 1), px + 9, py + 8, live ? col : 0xff4a4f57, 8.5f, 1.2f);
            c.setColor (0xff9aa0a8);
            draw::text (c, std::to_string (v + 1), monoFont (S (9)), Font::kRight, X (px), Y (py + 42), S (72), S (12));
        }
        if (studio.now - *std::max_element (std::begin (padFlash), std::end (padFlash)) < 0.4) redraw();

        // ROOM and MASTER
        silk (c, "ROOM", 30, 566, 0xffb9bec6); silk (c, "MASTER", 170, 566, 0xffb9bec6);
        auto meter = [&] (float x, float lvl)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const float w = (126 - 12) * std::min (1.0f, lvl * (ch ? 0.96f : 1.0f));
                if (w < 0.5f) continue;
                const float yy = 591 + ch * 9;
                c.setColor (Brush::linear (visage::Gradient (Color (0xff3dffd2), Color (green), Color (neon::amber), Color (neon::magenta)), Point (X (x + 6), 0), Point (X (x + 120), 0)));
                c.roundedRectangle (X (x + 6), Y (yy), S (w), S (5), S (2.5f));
            }
        };
        meter (26, roomMeter); meter (166, masterMeter);
        auto dbOf = [] (float l) { return l > 1e-5f ? 20 * std::log10 (l) : -100.0f; };
        c.setColor (0xff9aa0a8);
        draw::text (c, roomMeter > 1e-4f ? draw::fmt ("%.1f dB", (double) dbOf (roomMeter)) : std::string ("-inf"), monoFont (S (10)), Font::kLeft, X (30), Y (618), S (110), S (12));
        draw::text (c, masterMeter > 1e-4f ? draw::fmt ("%.1f dB PEAK", (double) dbOf (masterMeter)) : std::string ("-inf"), monoFont (S (10)), Font::kLeft, X (170), Y (618), S (120), S (12));
    }

    // ------------------------------------------------------------------ the Align instrument (Align step)
    // Three modules: ALIGN (align again, the readout, the timing keys), SCOPE (the two selectors, the CRT, the
    // jog wheel and its keys), LISTEN (sum by frequency, the pair keys, hear what you gain, back to measured).
    struct Key { float x, y, w, h; const char* id; };
    static constexpr Key keys[] = {
        { 26, 40, 170, 30, "again" }, { 26, 82, 86, 26, "t:tight" }, { 116, 82, 86, 26, "t:pro" }, { 206, 82, 86, 26, "t:natural" },
        { 26, 150, 130, 28, "menu:mic" }, { 162, 150, 130, 28, "menu:to" },
        { 26, 350, 40, 26, "n:-4" }, { 70, 350, 40, 26, "n:-1" }, { 208, 350, 40, 26, "n:1" }, { 252, 350, 40, 26, "n:4" },
        { 26, 386, 84, 26, "spread" }, { 208, 386, 84, 26, "snap" },
        { 26, 424, 84, 26, "flip" }, { 117, 424, 84, 26, "eq" }, { 208, 424, 84, 26, "lr" },
        { 26, 586, 130, 28, "pair:0" }, { 162, 586, 130, 28, "pair:1" }, { 26, 620, 130, 28, "gain" }, { 162, 620, 130, 28, "reset" } };
    static constexpr float scX = 26, scY = 186, scW = 266, scH = 150, jogX = 159, jogY = 380, jogR = 36;
    std::string pressed;              // the key held down
    bool jogging = false, dragging = false;
    float dragFrom = 0, jogA0 = 0, liveDelta = 0;   // samples, while the jog or a drag on the scope moves the mic

    const Key* keyAt (float x, float y) const
    {
        for (auto& kk : keys) if (x >= kk.x && x < kk.x + kk.w && y >= kk.y && y < kk.y + kk.h) return &kk;
        return nullptr;
    }

    void led (Canvas& c, float x, float y, bool on, unsigned col)
    {
        if (on) { c.setColor (Color (col).withAlpha (0.45f)); c.circle (X (x) - S (6), Y (y) - S (6), S (12)); c.setColor (col); }
        else c.setColor (0xff2a2d33);
        c.circle (X (x) - S (2.2f), Y (y) - S (2.2f), S (4.4f));
    }

    void keyLabel (Canvas& c, const Key& kk, const std::string& t, bool on, unsigned col, bool enabled = true, bool = true)
    {
        // a key that is on glows from inside, with a lit strip along its lower edge; the label is centred on the
        // key's top face (raised about 1.3 px above its footprint in the render)
        const float x = kk.x + 3, y = kk.y + 2.5f - 1.3f, w = kk.w - 6, h = kk.h - 6;
        const bool down = ! pressed.empty() && pressed == kk.id;
        if ((on && enabled) || down)
        {
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (col).withAlpha (down ? 0.22f : 0.13f)); c.roundedRectangle (X (x), Y (y), S (w), S (h), S (3.5f));
            c.setColor (Color (col).withAlpha (0.35f)); c.roundedRectangleShadow (X (x + 8), Y (y + h - 3), S (w - 16), S (2), S (1), S (6));
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (Color (col).withAlpha (0.95f)); c.roundedRectangle (X (x + 8), Y (y + h - 3), S (w - 16), S (1.6f), S (0.8f));
        }
        c.setColor (! enabled ? 0xff3e434b : on ? 0xfff2f4f8 : 0xff9aa0a9);
        draw::text (c, t, boldFont (S (8)), Font::kCenter, X (x), Y (y), S (w), S (h - 2));
    }

    static unsigned heatColour (float h)
    {
        // cold (fighting) to hot (locked in), Hitline's heat colours
        const unsigned stops[4] = { 0xff4d7cff, 0xff9b8cff, 0xffffb347, 0xffff5a3d };
        const float x = std::clamp (h, 0.0f, 1.0f) * 3; const int i = std::min (2, (int) x); const float f = x - (float) i;
        return Color (stops[i]).interpolateWith (Color (stops[i + 1]), f).toARGB();
    }

    void drawAlign (Canvas& c)
    {
        c.image (reinterpret_cast<const unsigned char*> (BinaryData::rack_align_png), BinaryData::rack_align_pngSize, ox, 0, DW * k, DH * k);
        const auto& A = studio.data.align;
        const unsigned vio = neon::violet, hot = 0xffff5a3d, cool = 0xff7cff9b;
        const bool live = A.valid;
        auto unit = [&] (float smp) { return A.samples ? draw::fmt ("%+.2f smp", (double) smp) : draw::fmt ("%+.2f ms", (double) (smp / A.sr * 1000)); };

        // ALIGN
        silk (c, "ALIGN", 40, 18, 0xffcbbcff, 9.5f, 2.2f);
        led (c, 30, 23, live, vio);
        c.setColor (0xff8b9099);
        const int nm = (int) A.mics.size() - (live ? 1 : 0);
        draw::text (c, live ? std::to_string (std::max (0, nm)) + " TRACKS \xc2\xb7 " + std::to_string (A.fightingAfter) + " FIGHTING" : std::string ("NOT LINED UP YET"),
                    boldFont (S (8.5f)), Font::kRight, X (120), Y (16), S (168), S (12));
        keyLabel (c, keys[0], "ALIGN AGAIN", true, vio, true);
        c.setColor (0xff5f6670); draw::text (c, "FIGHTING", boldFont (S (6.5f)), Font::kCenter, X (204), Y (44.5f), S (88), S (9));
        c.setColor (live ? (A.fightingAfter ? hot : cool) : Color (0xff3e434b));
        draw::text (c, live ? std::to_string (A.fightingBefore) + " \xe2\x86\x92 " + std::to_string (A.fightingAfter) : std::string ("\xe2\x80\x94"), monoBold (S (11)),
                    Font::kCenter, X (204), Y (52), S (88), S (15));
        keyLabel (c, keys[1], "TIGHT PHASE", A.timing == "tight", vio, live);
        keyLabel (c, keys[2], "STANDARD PHASE", A.timing == "pro", vio, live);
        keyLabel (c, keys[3], "AS RECORDED", A.timing == "natural", vio, live);

        // SCOPE: the two selectors
        silk (c, "TRACK", 30, 139, 0xffb9bec6, 7.5f, 1.4f); silk (c, "LINES UP TO", 166, 139, 0xffb9bec6, 7.5f, 1.4f);
        const auto* M = live && A.mic >= 0 && A.mic < (int) A.mics.size() ? &A.mics[(size_t) A.mic] : nullptr;
        const std::string micName = M ? M->name : std::string ("\xe2\x80\x94");
        auto selector = [&] (const Key& kk, const std::string& t)
        {
            c.setColor (live ? 0xffe8ecf2 : 0xff4a4f57);
            draw::text (c, t, bodyFont (S (10.5f)), Font::kLeft, X (kk.x + 9), Y (kk.y), S (kk.w - 28), S (kk.h));
            c.setColor (Color (vio).withAlpha (live ? 1.0f : 0.3f));
            const float ax = X (kk.x + kk.w - 14), ay = Y (kk.y + kk.h / 2 - 1);
            visage::Path tri; tri.moveTo (ax - S (4), ay - S (2)); tri.lineTo (ax + S (4), ay - S (2)); tri.lineTo (ax, ay + S (3)); tri.close();
            c.fill (tri);
        };
        selector (keys[4], micName);
        selector (keys[5], M ? M->to : std::string ("\xe2\x80\x94"));

        // the CRT: a graticule, the reference filled grey, the mic as recorded as a ghost, the mic where it sits
        // now in its heat colour; the heat strip along the bottom with the hottest delay (red) and where it is (green)
        const float gx = scX + 6, gy = scY + 6, gw = scW - 12, gh = scH - 30;
        c.setColor (0x0f8b5cff); c.fill (X (scX), Y (scY), S (scW), S (scH));
        c.setColor (0x168b5cff);
        for (int g = 1; g < 10; ++g) c.fill (X (gx + g * gw / 10), Y (gy), 1, S (gh));
        for (int g = 1; g < 6; ++g) c.fill (X (gx), Y (gy + g * gh / 6), S (gw), 1);
        c.setColor (0x308b5cff); c.fill (X (gx), Y (gy + gh / 2), S (gw), 1);
        if (M && ! A.ref.empty())
        {
            const size_t N = A.ref.size();
            float pk = 1e-6f; for (size_t i = 0; i < N; ++i) pk = std::max ({ pk, std::abs (A.ref[i]), std::abs (A.cur[i]), std::abs (A.rec[i]) });
            const float shift = liveDelta;    // samples, while dragging or jogging
            const float spp = A.spanMs / 1000 * A.sr / (float) (N - 1);   // samples per point
            auto px = [&] (size_t i) { return X (gx + (float) i * gw / (float) (N - 1)); };
            auto py = [&] (float v) { return Y (gy + gh / 2 - v / pk * gh * 0.46f); };
            auto shifted = [&] (float pt) -> float
            {
                const float q = pt + shift / spp; const long i0 = (long) std::floor (q); const float f = q - (float) i0;
                auto g = [&] (long j) { return j >= 0 && j < (long) N ? A.cur[(size_t) j] : 0.0f; };
                return g (i0) * (1 - f) + g (i0 + 1) * f;
            };
            visage::Path ref; ref.moveTo (px (0), py (0));
            for (size_t i = 0; i < N; ++i) ref.lineTo (px (i), py (A.ref[i]));
            ref.lineTo (px (N - 1), py (0)); ref.close();
            c.setColor (0x40b9bec6); c.fill (ref);
            visage::Path rec; rec.moveTo (px (0), py (A.rec[0]));
            for (size_t i = 1; i < N; ++i) rec.lineTo (px (i), py (A.rec[i]));
            c.setColor (0x40ffffff); c.stroke (rec, 0, 0, width(), height(), S (0.8f));
            visage::Path cur; cur.moveTo (px (0), py (shifted (0)));
            for (size_t i = 1; i < N; ++i) cur.lineTo (px (i), py (shifted ((float) i)));
            // the heat where it sits (live while moving)
            float h = A.curHeat;
            if (shift != 0 && ! A.heat.empty())
            {
                const float q = (A.lag + shift - A.heatFrom) / A.heatStep;
                const int j = std::clamp ((int) std::lround (q), 0, (int) A.heat.size() - 1); h = A.heat[(size_t) j];
            }
            const unsigned hc = heatColour (std::isfinite (h) ? h : 0);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (hc).withAlpha (0.25f)); c.stroke (cur, 0, 0, width(), height(), S (5));
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (hc); c.stroke (cur, 0, 0, width(), height(), S (1.6f));
            // the hit's onset
            const float ox0 = gx + A.preMs / A.spanMs * gw;
            c.setColor (0x99ff5a3d);
            for (float yy = gy; yy < gy + gh; yy += 4) c.fill (X (ox0), Y (yy), S (1), S (2));
            // readouts
            c.setColor (0xffe8ecf2);
            draw::text (c, unit (A.lag + shift) + (M->flipped ? "  flipped" : ""), monoBold (S (9)), Font::kLeft, X (gx + 4), Y (gy + 2), S (150), S (11));
            c.setColor (hc); c.roundedRectangleBorder (X (gx + gw - 62), Y (gy + 2), S (60), S (13), S (6.5f), S (1));
            draw::text (c, std::string (h > 0.6f ? "HOT " : h > 0.3f ? "WARM " : "COLD ") + draw::fmt ("%.2f", (double) (std::isfinite (h) ? h : 0)), monoBold (S (8)),
                        Font::kCenter, X (gx + gw - 62), Y (gy + 2), S (60), S (13));
            // the heat strip
            if (A.heat.size() > 1)
            {
                const float hy = scY + scH - 20, hh = 14;
                float lo = *std::min_element (A.heat.begin(), A.heat.end()), hi2 = *std::max_element (A.heat.begin(), A.heat.end());
                const float spanH = std::max (1e-3f, hi2 - lo);
                for (size_t j = 0; j < A.heat.size(); ++j)
                {
                    const float xx = gx + (float) j * gw / (float) A.heat.size();
                    const float v = (A.heat[j] - lo) / spanH;
                    c.setColor (Color (heatColour (A.heat[j])).withAlpha (0.25f + 0.6f * v));
                    c.fill (X (xx), Y (hy + hh * (1 - v)), S (gw / (float) A.heat.size() + 0.5f), S (hh * v + 1));
                }
                auto mark = [&] (float lag, unsigned col)
                {
                    const float q = (lag - A.heatFrom) / A.heatStep / (float) A.heat.size();
                    if (q < 0 || q > 1) return;
                    c.setColor (col); c.fill (X (gx + q * gw) - S (0.75f), Y (hy - 2), S (1.5f), S (hh + 4));
                };
                mark (A.best, 0xffff4d5e); mark (A.lag + shift, cool);
            }
        }
        else
        {
            c.setColor (0xff5f6670);
            draw::text (c, live ? "Pick a track" : "Run Auto Prep to line the tracks up", bodyFont (S (11)), Font::kCenter, X (scX), Y (scY), S (scW), S (scH));
        }

        // the jog wheel: its ring lights with the heat, the dimple turns with the delay
        {
            const float R = S (jogR * 1.1f);
            const float h = M ? (std::isfinite (A.curHeat) ? A.curHeat : 0) : 0;
            const float a0 = -0.75f * 3.14159265f, a1 = a0 + 1.5f * 3.14159265f * std::clamp (h, 0.0f, 1.0f);
            const float mid = (a0 + a1) / 2 - 1.5707963f, half = (a1 - a0) / 2;
            const unsigned col = heatColour (h);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (col).withAlpha (M ? 0.3f : 0.06f)); c.arc (X (jogX) - R - S (5), Y (jogY) - R - S (5), 2 * R + S (10), S (10), mid, half, true);
            c.setColor (Color (col).withAlpha (M ? 0.95f : 0.15f)); c.arc (X (jogX) - R - S (1.6f), Y (jogY) - R - S (1.6f), 2 * R + S (3.2f), S (3.2f), mid, half, true);
            c.setBlendMode (visage::BlendMode::Alpha);
            const float turn = (A.lag + liveDelta) * 0.25f;   // a quarter turn per sample
            c.setColor (Color (vio).withAlpha (M ? 0.9f : 0.2f));
            c.circle (X (jogX) + std::sin (turn) * S (jogR * 0.62f) - S (2.5f), Y (jogY) - std::cos (turn) * S (jogR * 0.62f) - S (2.5f), S (5));
        }
        keyLabel (c, keys[6], "\xe2\x88\x92" "1", false, vio, M != nullptr, false);
        keyLabel (c, keys[7], "\xe2\x88\x92\xc2\xbc", false, vio, M != nullptr, false);
        keyLabel (c, keys[8], "+\xc2\xbc", false, vio, M != nullptr, false);
        keyLabel (c, keys[9], "+1", false, vio, M != nullptr, false);
        c.setColor (0xff6a707a);
        draw::text (c, "SAMPLES", boldFont (S (6.5f)), Font::kCenter, X (26), Y (338), S (84), S (10));
        draw::text (c, "SAMPLES", boldFont (S (6.5f)), Font::kCenter, X (208), Y (338), S (84), S (10));
        keyLabel (c, keys[10], A.spread ? "SPREAD OF HITS" : "THIS HIT", true, vio, M != nullptr);
        keyLabel (c, keys[11], "SNAP TO HOT", false, hot, M != nullptr);
        keyLabel (c, keys[12], "\xc3\x98 FLIP", M && M->flipped, neon::magenta, M != nullptr);
        keyLabel (c, keys[13], "PHASE EQ", M && M->eq, vio, M && M->eqAvail);
        keyLabel (c, keys[14], "L = R", M && M->lr, neon::cyan, M && M->lrAvail);

        // LISTEN: sum by frequency, before (outline) and after (lit)
        silk (c, "LISTEN", 36, 478, 0xffb9bec6, 9.5f, 2.2f);
        c.setColor (0xff8b9099);
        draw::text (c, "SUM BY FREQUENCY", boldFont (S (8)), Font::kRight, X (120), Y (476), S (168), S (12));
        {
            const float bx = 26, by = 500, bw = 266, bh = 74;
            static const char* lab[8] = { "60", "120", "250", "500", "1k", "2k", "4k", "8k" };
            for (int b = 0; b < 8; ++b)
            {
                const float cx = bx + 12 + b * (bw - 24) / 8, w = (bw - 24) / 8 - 8;
                c.setColor (0xff4a4f57); draw::text (c, lab[b], monoFont (S (7)), Font::kCenter, X (cx), Y (by + bh - 12), S (w), S (10));
                if (! M) continue;
                auto hgt = [&] (float v) { return std::clamp ((v + 1) / 2, 0.0f, 1.0f) * (bh - 34); };
                const float hb = hgt (M->bandsBefore[(size_t) b]), ha = hgt (M->bandsAfter[(size_t) b]);
                const unsigned col = M->bandsAfter[(size_t) b] < 0 ? hot : cool;
                c.setColor (Color (col).withAlpha (0.18f)); c.roundedRectangleShadow (X (cx), Y (by + bh - 14 - ha), S (w), S (ha), S (2), S (6));
                c.setColor (Color (col).withAlpha (0.85f)); c.roundedRectangle (X (cx), Y (by + bh - 14 - ha), S (w), S (ha), S (2));
                c.setColor (0xccffffff); c.roundedRectangleBorder (X (cx), Y (by + bh - 14 - hb), S (w), S (std::max (1.0f, hb)), S (2), S (1));
            }
            if (M && std::isfinite (M->before) && std::isfinite (M->after))
            {
                c.setColor (0xffb9bec6);
                draw::text (c, draw::fmt ("%+.2f", (double) M->before) + " \xe2\x86\x92 " + draw::fmt ("%+.2f", (double) M->after), monoBold (S (8.5f)), Font::kRight, X (bx), Y (by + 5), S (bw - 8), S (11));
            }
            if (M && ! M->eqText.empty()) { c.setColor (0xff8b9099); draw::text (c, M->eqText, bodyFont (S (8)), Font::kLeft, X (bx + 8), Y (by + 5), S (170), S (11)); }
        }
        keyLabel (c, keys[15], "\xe2\x96\xb6 PAIR AS RECORDED", false, vio, M != nullptr, false);
        keyLabel (c, keys[16], "\xe2\x96\xb6 PAIR ALIGNED", false, vio, M != nullptr, false);
        keyLabel (c, keys[17], "HOLD: HEAR THE GAIN", pressed == "gain", hot, M != nullptr);
        keyLabel (c, keys[18], "BACK TO MEASURED", M && M->status == "manual", vio, M && M->status == "manual");
    }

    // ------------------------------------------------------------------ the Tune instrument (Tune step)
    // TUNE: read-from, the strobe, the readout, the cents meter, Tune it / Read again, one ratio / even out RRs,
    // the target, fine and A4 knobs and Nearest. PITCH MAP: every hit. LISTEN: original, tuned, A/B, the notes.
    static constexpr Key tkeys[] = {
        { 162, 34, 130, 26, "menu:from" },
        { 26, 240, 130, 28, "on" }, { 162, 240, 130, 28, "read" }, { 26, 276, 130, 26, "mode:drum" }, { 162, 276, 130, 26, "mode:rr" },
        { 230, 318, 62, 26, "nearest" },
        { 26, 572, 84, 26, "play:0" }, { 117, 572, 84, 26, "play:1" }, { 208, 572, 84, 26, "play:ab" }, { 26, 606, 130, 26, "tone:rec" }, { 162, 606, 130, 26, "tone:tgt" },
        { 26, 520, 266, 16, "menu:range" } };
    static constexpr float tkX[3] = { 59, 126, 193 }, tkY = 330, tkR = 18;
    int tuneKnob = -1; float tuneY0 = 0, tuneV0 = 0, tuneLive = NAN;
    std::chrono::steady_clock::time_point strobeT0 = std::chrono::steady_clock::now();

    const Key* tkeyAt (float x, float y) const
    {
        for (auto& kk : tkeys) if (x >= kk.x && x < kk.x + kk.w && y >= kk.y && y < kk.y + kk.h) return &kk;
        return nullptr;
    }

    float tuneKnobValue (int i) const
    {
        const auto& T = studio.data.tune;
        if (i == tuneKnob && std::isfinite (tuneLive)) return tuneLive;
        return i == 0 ? T.targetPos : i == 1 ? (T.cents + 50) / 100 : (T.a4 - 415) / 51;
    }

    void drawTune (Canvas& c)
    {
        c.image (reinterpret_cast<const unsigned char*> (BinaryData::rack_tune_png), BinaryData::rack_tune_pngSize, ox, 0, DW * k, DH * k);
        const auto& T = studio.data.tune;
        const unsigned lime = 0xffc8ff3d, amber = neon::amber, hot = 0xffff5a3d;
        const bool live = T.read;
        silk (c, "TUNE", 40, 18, 0xffdfff9a, 9.5f, 2.2f);
        c.setColor (live ? Color (lime).withAlpha (0.45f) : Color (0)); c.circle (X (30) - S (6), Y (23) - S (6), S (12));
        c.setColor (live ? lime : 0xff2a2d33); c.circle (X (30) - S (2.2f), Y (23) - S (2.2f), S (4.4f));
        // read from
        {
            const auto& kk = tkeys[0];
            c.setColor (0xff8b9099); draw::text (c, "READ FROM", boldFont (S (7)), Font::kRight, X (80), Y (40), S (76), S (14));
            c.setColor (0xffe8ecf2); draw::text (c, T.from, bodyFont (S (10)), Font::kLeft, X (kk.x + 8), Y (kk.y), S (kk.w - 26), S (kk.h));
            const float ax = X (kk.x + kk.w - 13), ay = Y (kk.y + kk.h / 2 - 1);
            visage::Path tri; tri.moveTo (ax - S (4), ay - S (2)); tri.lineTo (ax + S (4), ay - S (2)); tri.lineTo (ax, ay + S (3)); tri.close();
            c.setColor (lime); c.fill (tri);
        }
        if (! T.tunable)
        {
            c.setColor (0xff8b9099);
            draw::text (c, "This drum doesn't get tuned", bodyFont (S (11)), Font::kCenter, X (26), Y (198), S (266), S (30));
            return;
        }
        // the strobe: three rings of bars that drift with how far the selected hit is from its note (as it now
        // plays), and stand still when it's in tune; the note in the middle
        {
            const float cx = X (88), cy = Y (129), r = S (56);
            const bool advised = ! T.advice.empty() && ! (T.anyway && T.on);
            const float cents = live && std::isfinite (T.hitCents) ? T.hitCents : 0;
            const unsigned col = advised ? 0xff3a3f45 : ! live ? 0xff2d3a1f : std::abs (cents) < 3 ? lime : std::abs (cents) < 12 ? amber : hot;
            const double t = std::chrono::duration<double> (std::chrono::steady_clock::now() - strobeT0).count();
            // each ring a band of lit segments, like a strobe disc behind glass; inner rings turn faster
            for (int ring = 0; ring < 3; ++ring)
            {
                const int n = 12 + ring * 6;
                const float rr = r * (0.9f - ring * 0.2f), th = r * 0.13f, step = 6.2831853f / (float) n;
                const float spin = (float) (t * cents * 0.03 * (ring + 1)) * (ring % 2 ? -1.0f : 1.0f);
                for (int b = 0; b < n; ++b)
                {
                    const float a = spin + b * step;
                    c.setBlendMode (visage::BlendMode::Add);
                    c.setColor (Color (col).withAlpha (live ? 0.18f : 0.06f)); c.arc (cx - rr - th, cy - rr - th, 2 * (rr + th), th * 2, a, step * 0.3f, true);
                    c.setBlendMode (visage::BlendMode::Alpha);
                    c.setColor (Color (col).withAlpha (live ? 0.95f : 0.28f)); c.arc (cx - rr, cy - rr, 2 * rr, th, a, step * 0.25f, false);
                }
            }
            c.setColor (live && ! advised ? 0xfff2f4f8 : 0xff6f767f);
            draw::text (c, advised ? std::string ("OFF") : live && ! T.hitNote.empty() ? T.hitNote : std::string ("\xe2\x80\x94"), monoBold (S (17)), Font::kCenter, cx - S (30), cy - S (12), S (60), S (20));
            c.setColor (col);
            draw::text (c, live ? draw::fmt ("%+.0f\xc2\xa2", (double) cents) : std::string(), monoBold (S (8.5f)), Font::kCenter, cx - S (30), cy + S (8), S (60), S (11));
            if (live && ! advised) redraw();   // the strobe keeps turning
        }
        // the readout
        {
            const float rx = 160, ry = 70;
            auto row = [&] (int i, const char* l, const std::string& val, unsigned col = 0xffe8ecf2)
            {
                c.setColor (0xff6f767f); draw::text (c, l, boldFont (S (6.8f)), Font::kLeft, X (rx + 8), Y (ry + 8 + i * 18), S (64), S (14));
                c.setColor (col); draw::text (c, val, monoBold (S (8.5f)), Font::kRight, X (rx + 52), Y (ry + 8 + i * 18), S (72), S (14));
            };
            if (live)
            {
                row (0, "SITS AT", T.sitsAt + draw::fmt (" %+.0f\xc2\xa2", (double) T.medCents));
                row (1, "TARGET", T.on ? T.tunedTo : std::string ("off"), T.on ? lime : 0xff6f767f);
                row (2, "PITCH", draw::fmt ("%.1f Hz", (double) T.medF));
                row (3, "SHIFT", T.on ? draw::fmt ("%+.2f st", (double) T.shiftSemis) : std::string ("\xe2\x80\x94"));
                row (4, "RR SPREAD", std::isfinite (T.spreadBefore) ? draw::fmt ("%.0f", (double) T.spreadBefore) + "\xe2\x86\x92" + draw::fmt ("%.0f\xc2\xa2", (double) T.spreadAfter) : std::string ("\xe2\x80\x94"));
                row (5, "WORST", T.worstLayer >= 0 ? "V" + std::to_string (T.worstLayer + 1) + draw::fmt (" %+.0f\xc2\xa2", (double) T.worstCents) : std::string ("\xe2\x80\x94"));
            }
            else if (T.advice.empty() || T.on) { c.setColor (0xff6f767f); draw::text (c, "Read the pitch", bodyFont (S (10)), Font::kCenter, X (rx), Y (ry), S (132), S (118)); }
            if (! T.advice.empty() && ! T.on)
            {
                // the advice, over the readout
                c.setColor (0xff0a0c09); c.fill (X (rx + 3), Y (ry + 3), S (126), S (112));
                c.setColor (amber);
                draw::text (c, T.advice == "cymbal" ? "CYMBALS DON'T" : "NO STEADY PITCH", boldFont (S (8.5f)), Font::kCenter, X (rx), Y (ry + 22), S (132), S (14));
                draw::text (c, T.advice == "cymbal" ? "GET TUNED" : "HERE", boldFont (S (8.5f)), Font::kCenter, X (rx), Y (ry + 36), S (132), S (14));
                c.setColor (0xffb9bec6);
                draw::text (c, "Tuning is off.", bodyFont (S (9)), Font::kCenter, X (rx), Y (ry + 60), S (132), S (13));
                draw::text (c, "Leave it off: it", bodyFont (S (9)), Font::kCenter, X (rx), Y (ry + 74), S (132), S (13));
                draw::text (c, "isn't recommended.", bodyFont (S (9)), Font::kCenter, X (rx), Y (ry + 88), S (132), S (13));
            }
        }
        // the cents meter: \xc2\xb150 cents, the selected hit as recorded (grey) and as it plays now
        {
            const float mx = 34, mw = 250, my = 198, mh = 30;
            for (int k2 = -50; k2 <= 50; k2 += 10)
            {
                const float x = mx + (k2 + 50) / 100.0f * mw;
                c.setColor (k2 == 0 ? Color (lime).withAlpha (0.8f) : Color (0x40ffffff));
                c.fill (X (x), Y (my + (k2 == 0 ? 5 : 9)), S (1), S (k2 == 0 ? 20 : 12));
            }
            if (live && std::isfinite (T.hitCents))
            {
                auto mark = [&] (float cv, unsigned col, float w)
                {
                    const float x = mx + (std::clamp (cv, -50.0f, 50.0f) + 50) / 100.0f * mw;
                    c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (col).withAlpha (0.3f)); c.fill (X (x) - S (w * 2), Y (my + 4), S (w * 4), S (22)); c.setBlendMode (visage::BlendMode::Alpha);
                    c.setColor (col); c.fill (X (x) - S (w / 2), Y (my + 4), S (w), S (22));
                };
                if (std::isfinite (T.hitCentsBefore)) mark (T.hitCentsBefore, 0x80ffffff, 1.2f);
                mark (T.hitCents, std::abs (T.hitCents) < 3 ? lime : std::abs (T.hitCents) < 12 ? amber : hot, 2);
            }
        }
        // keys
        if (! T.advice.empty() && ! T.on) keyLabel (c, tkeys[1], "TUNE ANYWAY", false, amber, true);
        else keyLabel (c, tkeys[1], T.on ? "TUNED" : "TUNE IT", T.on, T.anyway && ! T.advice.empty() ? amber : lime, live);
        keyLabel (c, tkeys[2], live ? "READ AGAIN" : "READ PITCH", false, lime, true);
        if (! T.advice.empty() && T.on)
        {
            c.setColor (amber);
            draw::text (c, "NOT RECOMMENDED", boldFont (S (6.5f)), Font::kCenter, X (26), Y (230), S (130), S (9));
        }
        keyLabel (c, tkeys[3], "ONE RATIO", live && ! T.rr, lime, live && T.on);
        keyLabel (c, tkeys[4], "EVEN OUT RRs", live && T.rr, lime, live && T.on);
        keyLabel (c, tkeys[5], "NEAREST", T.target.rfind ("Nearest", 0) == 0, lime, live && T.on);
        // knobs: target note, fine cents, A4
        {
            const unsigned kc[3] = { lime, 0xffb39cff, amber };
            const char* lab[3] = { "TARGET", "FINE", "A4" };
            for (int i = 0; i < 3; ++i)
            {
                arcKnob (c, tkX[i], tkY, tuneKnobValue (i), Color (kc[i]).withAlpha (live && T.on ? 1.0f : 0.35f).toARGB(), tkR);
                c.setColor (0xffb9bec6); draw::text (c, lab[i], boldFont (S (7)), Font::kCenter, X (tkX[i] - 30), Y (354), S (60), S (10));
                std::string val;
                if (i == 0) val = T.target.rfind ("Nearest", 0) == 0 && live ? T.sitsAt : T.target;
                else if (i == 1) { const float cv = std::isfinite (tuneLive) && tuneKnob == 1 ? tuneLive * 100 - 50 : T.cents; val = draw::fmt ("%+.0f\xc2\xa2", (double) std::round (cv)); }
                else { const float av = std::isfinite (tuneLive) && tuneKnob == 2 ? 415 + tuneLive * 51 : T.a4; val = draw::fmt ("%.0f", (double) std::round (av)); }
                c.setColor (kc[i]); draw::text (c, val, monoBold (S (8.5f)), Font::kCenter, X (tkX[i] - 30), Y (365), S (60), S (11));
            }
        }

        // PITCH MAP
        silk (c, "PITCH MAP", 40, 404, 0xffb9bec6, 8.5f, 2.0f);
        c.setColor (0xff8b9099); draw::text (c, "EVERY HIT \xc2\xb7 GREY = AS RECORDED", boldFont (S (7)), Font::kRight, X (120), Y (402), S (168), S (12));
        {
            const float px0 = 34, pw = 250, py0 = 420, ph = 96;
            c.setColor (0x0dffffff); c.fill (X (px0), Y (py0 + ph / 2), S (pw), 1);
            const size_t N = T.before.size();
            if (live && N)
            {
                float span = 30;
                for (size_t i = 0; i < N; ++i) { if (std::isfinite (T.before[i])) span = std::max (span, std::abs (T.before[i])); if (std::isfinite (T.after[i])) span = std::max (span, std::abs (T.after[i])); }
                span *= 1.15f;
                auto xOf = [&] (size_t i) { return px0 + ((float) i + 0.5f) * pw / (float) N; };
                auto yOf = [&] (float v) { return py0 + ph / 2 - v / span * ph / 2; };
                for (size_t i = 1; i < N; ++i)
                    if (T.layer[i] != T.layer[i - 1]) { c.setColor (0x14ffffff); c.fill (X ((xOf (i) + xOf (i - 1)) / 2), Y (py0), 1, S (ph)); }
                for (size_t i = 0; i < N; ++i)
                {
                    if (! std::isfinite (T.after[i])) continue;
                    const float x = xOf (i), yb = yOf (T.before[i]), ya = yOf (T.after[i]);
                    const unsigned col = T.layers ? draw::layerColour (T.layer[i], T.layers) : lime;
                    if (std::isfinite (T.before[i]) && std::abs (ya - yb) > 1.5f) { c.setColor (0x33ffffff); c.fill (X (x) - 0.5f, Y (std::min (ya, yb)), 1, S (std::abs (ya - yb))); }
                    if (std::isfinite (T.before[i])) { c.setColor (0x66b9bec6); c.circle (X (x) - S (1.6f), Y (yb) - S (1.6f), S (3.2f)); }
                    const float rr = (int) i == T.hit ? 2.8f : 2.0f;
                    c.setColor (col); c.circle (X (x) - S (rr), Y (ya) - S (rr), S (2 * rr));
                    if ((int) i == T.hit) { c.setColor (0xffffffff); c.roundedRectangleBorder (X (x) - S (5), Y (ya) - S (5), S (10), S (10), S (5), S (1.2f)); }
                }
                c.setColor (0xff5f6670);
                draw::text (c, draw::fmt ("+%.0f\xc2\xa2", (double) span), monoFont (S (7)), Font::kLeft, X (px0), Y (py0 - 1), S (40), S (9));
                draw::text (c, draw::fmt ("-%.0f\xc2\xa2", (double) span), monoFont (S (7)), Font::kLeft, X (px0), Y (py0 + ph - 9), S (40), S (9));
            }
            else { c.setColor (0xff5f6670); draw::text (c, "Read the pitch to see every hit here", bodyFont (S (10)), Font::kCenter, X (26), Y (py0), S (266), S (ph)); }
            // the reading range, a menu
            c.setColor (0xff8b9099);
            draw::text (c, draw::fmt ("%.0f", (double) T.fmin) + "\xe2\x80\x93" + draw::fmt ("%.0f Hz", (double) T.fmax) + "  \xc2\xb7  skip " + draw::fmt ("%.0f ms \xe2\x96\xbe", (double) T.skipMs),
                        monoFont (S (7.5f)), Font::kRight, X (26), Y (521), S (258), S (12));
        }

        // LISTEN
        silk (c, "LISTEN", 40, 560, 0xffb9bec6, 8.5f, 2.0f);
        keyLabel (c, tkeys[6], "\xe2\x96\xb6 ORIGINAL", false, lime, live);
        keyLabel (c, tkeys[7], "\xe2\x96\xb6 TUNED", false, lime, live && T.on);
        keyLabel (c, tkeys[8], "A / B", false, lime, live && T.on);
        keyLabel (c, tkeys[9], "RECORDED NOTE", false, lime, live);
        keyLabel (c, tkeys[10], "TARGET NOTE", false, lime, live && T.on);
    }

    void tuneDown (float x, float y, const visage::MouseEvent& e)
    {
        const auto& T = studio.data.tune;
        for (int i = 0; i < 3; ++i)
            if (std::hypot (x - tkX[i], y - tkY) <= tkR * 1.35f)
            {
                if (! T.read || ! T.on) return;
                if (e.repeat_click_count >= 2 && i > 0) { if (studio.onTune) studio.onTune (i == 1 ? "fine" : "a4", i == 1 ? 0.0 : 440.0); return; }
                tuneKnob = i; tuneY0 = y; tuneV0 = tuneKnobValue (i); tuneLive = tuneV0; return;
            }
        const Key* kk = tkeyAt (x, y);
        if (! kk || ! studio.onTune) return;
        const std::string id = kk->id;
        pressed = id; redraw();
        if (id.rfind ("menu:", 0) == 0) { if (studio.onTuneMenu) studio.onTuneMenu (id.substr (5), X (kk->x), Y (kk->y), S (kk->w), S (kk->h)); return; }
        if (id == "read") { studio.onTune ("read", 0); return; }
        if (id == "on" && ! T.advice.empty() && ! T.on) { studio.onTune ("anyway", 1); return; }
        if (! T.read) return;
        if (id == "on") { studio.onTune ("on", T.on ? 0 : 1); return; }
        if (id == "mode:drum" || id == "mode:rr") { if (T.on) studio.onTune ("mode", id == "mode:rr"); return; }
        if (id == "nearest") { if (T.on) studio.onTune ("nearest", 0); return; }
        if (id.rfind ("play:", 0) == 0) { studio.onTune ("play", id == "play:0" ? 0 : id == "play:1" ? 1 : 2); return; }
        if (id.rfind ("tone:", 0) == 0) { studio.onTune ("tone", id == "tone:tgt"); return; }
    }

    void tuneDrag (float, float y)
    {
        if (tuneKnob < 0) return;
        tuneLive = std::clamp (tuneV0 + (tuneY0 - y) / 160.0f, 0.0f, 1.0f);
        if (tuneKnob == 1) tuneLive = (std::round (tuneLive * 100 - 50) + 50) / 100;
        if (tuneKnob == 2) tuneLive = (std::round (415 + tuneLive * 51) - 415) / 51;
        redraw();
    }

    void tuneUp()
    {
        if (tuneKnob >= 0 && std::isfinite (tuneLive) && studio.onTune && std::abs (tuneLive - tuneV0) > 1e-4f)
        {
            if (tuneKnob == 0) studio.onTune ("target", tuneLive);
            else if (tuneKnob == 1) studio.onTune ("fine", std::round (tuneLive * 100 - 50));
            else studio.onTune ("a4", std::round (415 + tuneLive * 51));
        }
        tuneKnob = -1; tuneLive = NAN; pressed.clear(); redraw();
    }

    // ------------------------------------------------------------------ the Mix rack (approved design mix_v5)
    // TONE (dark copper): Standard / All flat, the channel, the tone curve, Tone / Crack / Rings / Tame.
    // SHAPE (deep teal): the transient designer, Attack and Sustain big, Decay and Release small.
    // REFERENCE (satin aluminium): the deck, Match curve and Match envelope.
    // REVERB (plum): R1 and R2 side by side, type, Decay, Pre, Size, Tone, Width; 100 % wet.
    static constexpr Key mkeys[] = {
        { 26, 30, 84, 24, "start" }, { 117, 30, 84, 24, "flat" }, { 208, 30, 84, 24, "menu:chan" },
        { 26, 438, 104, 20, "ref:play" }, { 134, 438, 76, 20, "ref:load" }, { 214, 438, 78, 20, "ref:remove" },
        { 30, 494, 122, 22, "menu:verb0" }, { 166, 494, 122, 22, "menu:verb1" } };
    struct MKnob { float x, y, r; };
    // 0 unused (pro tone), 1 tone, 2 crack, 3 rings, 4 tame, 5 attack, 6 decay, 7 sustain, 8 release, 9 unused, 10 match curve,
    // 11 match envelope, 12..16 R1 decay pre size tone width, 17..21 R2
    static MKnob mknob (int i)
    {
        static const MKnob t[22] = { { -99, -99, 0 }, { 59, 160, 15 }, { 126, 160, 15 }, { 193, 160, 15 }, { 260, 160, 15 },
                                     { 70, 312, 22 }, { 136, 312, 13 }, { 248, 312, 22 }, { 182, 312, 13 }, { -99, -99, 0 },
                                     { 206, 404, 14 }, { 262, 404, 14 },
                                     { 91, 545, 20 }, { 44, 607, 9.3f }, { 76, 607, 9.3f }, { 108, 607, 9.3f }, { 138, 607, 9.3f },
                                     { 227, 545, 20 }, { 180, 607, 9.3f }, { 212, 607, 9.3f }, { 244, 607, 9.3f }, { 274, 607, 9.3f } };
        return t[i];
    }
    static constexpr int nMKnobs = 22;
    int mixKnob = -1; float mixY0 = 0, mixV0 = 0, mixLive = NAN;

    static bool mixBipolar (int i) { return i == 1 || i == 2 || (i >= 5 && i < 9) || i == 15 || i == 20; }
    // the knob's value as 0..1 on its ring
    float mixKnobNorm (int i) const
    {
        const auto& V = studio.data.mix;
        if (i == mixKnob && std::isfinite (mixLive)) return mixLive;
        if (i < 5) { const float v = V.tone[(size_t) i]; return i == 1 || i == 2 ? (v + 1) / 2 : v; }
        if (i < 10) { const float v = V.shape[(size_t) (i - 5)]; return i < 9 ? (v + 1) / 2 : v; }
        if (i == 10) return V.refAmt;
        if (i == 11) return V.refEnvAmt;
        const auto& R = V.verbs[(size_t) (i >= 17)];
        switch ((i - 12) % 5)
        {
            case 0: return (R.decay - 0.25f) / 2.75f;
            case 1: return std::sqrt (R.pre / 250);
            case 2: return R.size / 2;
            case 3: return (R.tone + 1) / 2;
            default: return R.width / 2;
        }
    }
    /** What the action carries for knob i at ring position n. */
    static float mixValueOf (int i, float n)
    {
        if (i < 12) return mixBipolar (i) ? n * 2 - 1 : n;
        switch ((i - 12) % 5)
        {
            case 0: return 0.25f + n * 2.75f;
            case 1: return n * n * 250;
            case 2: return n * 2;
            case 3: return n * 2 - 1;
            default: return n * 2;
        }
    }
    static std::string mixValueText (int i, float v)
    {
        if (i < 12) return mixBipolar (i) ? draw::fmt ("%+.0f", (double) std::round (v * 100)) : draw::fmt ("%.0f%%", (double) std::round (v * 100));
        switch ((i - 12) % 5)
        {
            case 0: return draw::fmt ("x%.2f", v);
            case 1: return draw::fmt ("%.0f ms", v);
            case 2: return draw::fmt ("%.0f%%", (double) std::round (v * 100));
            case 3: return draw::fmt ("%+.1f", v);
            default: return draw::fmt ("%.0f%%", (double) std::round (v * 100));
        }
    }

    void mixArc (Canvas& c, float cx, float cy, float r, float v, unsigned col, bool bip, float w)
    {
        // the value lit on the frosted ring: bipolar knobs light from the top centre
        const float pi = 3.14159265f, a0 = -0.75f * pi, full = 1.5f * pi, R = S (r * 1.24f);
        float s0 = bip ? 0.0f : a0, s1 = bip ? (v - 0.5f) * full : a0 + v * full;
        if (s1 < s0) std::swap (s0, s1);
        if (s1 - s0 < 0.01f) return;
        c.setBlendMode (visage::BlendMode::Add);
        c.setColor (Color (col).withAlpha (0.28f)); c.arc (X (cx) - R - S (w * 1.6f), Y (cy - 3) - R - S (w * 1.6f), 2 * R + S (w * 3.2f), S (w * 3.2f), (s0 + s1) / 2, (s1 - s0) / 2, true);
        c.setColor (Color (col).withAlpha (0.95f)); c.arc (X (cx) - R - S (w / 2), Y (cy - 3) - R - S (w / 2), 2 * R + S (w), S (w), (s0 + s1) / 2, (s1 - s0) / 2, true);
        c.setBlendMode (visage::BlendMode::Alpha);
    }

    void drawMix (Canvas& c)
    {
        c.image (reinterpret_cast<const unsigned char*> (BinaryData::rack_mix2_png), BinaryData::rack_mix2_pngSize, ox, 0, DW * k, DH * k);
        const auto& V = studio.data.mix;
        const unsigned amber = 0xffffa040, vio = 0xffa78bff, cyan = 0xff29e3ff;
        const bool live = V.valid;
        // ---- TONE
        silk (c, "TONE", 36, 13, 0xffffd2a6, 8.5f, 2.0f);
        keyLabel (c, mkeys[0], "STANDARD", false, amber, live);
        keyLabel (c, mkeys[1], "ALL FLAT", false, amber, live && V.fxOn);
        {
            const auto& kk = mkeys[2];
            c.setColor (live ? 0xffe8ecf2 : 0xff4a4f57);
            draw::text (c, V.selName, bodyFont (S (10)), Font::kLeft, X (kk.x + 8), Y (kk.y + 1.2f), S (kk.w - 24), S (kk.h - 6));
            const float ax = X (kk.x + kk.w - 12), ay = Y (kk.y + kk.h / 2 - 1.5f);
            visage::Path tri; tri.moveTo (ax - S (3.5f), ay - S (2)); tri.lineTo (ax + S (3.5f), ay - S (2)); tri.lineTo (ax, ay + S (2.5f)); tri.close();
            c.setColor (amber); c.fill (tri);
        }
        {
            const float gx = 32, gy = 64, gw = 254, gh = 64;
            c.setColor (0x14ffffff);
            for (float f : { 100.0f, 1000.0f, 10000.0f }) { const float x = gx + std::log2 (f / 60) / std::log2 (16000.0f / 60) * gw; c.fill (X (x), Y (gy), 1, S (gh)); }
            if (live && V.specBefore.size() == 30)
            {
                auto xOf = [&] (int j) { const float f = 1000.0f * std::pow (2.0f, (j - 16) / 3.0f); return gx + std::log2 (f / 60) / std::log2 (16000.0f / 60) * gw; };
                float hi = -1e9f;
                for (int j = 2; j < 30; ++j) hi = std::max ({ hi, V.specBefore[(size_t) j], V.specAfter[(size_t) j] });
                const float lo = hi - 42;
                auto yOf = [&] (float db) { return gy + 4 + (1 - std::clamp ((db - lo) / (hi - lo), 0.0f, 1.0f)) * (gh - 12); };
                auto line = [&] (const std::vector<float>& v, const std::vector<float>* add, float amt, unsigned col, float w, bool glow)
                {
                    visage::Path p; bool first = true;
                    for (int j = 2; j < 30; ++j)
                    {
                        const float f = 1000.0f * std::pow (2.0f, (j - 16) / 3.0f);
                        if (f < 55 || f > 17000) continue;
                        const float db = v[(size_t) j] + (add ? (*add)[(size_t) j] * amt : 0.0f);
                        if (first) { p.moveTo (X (xOf (j)), Y (yOf (db))); first = false; } else p.lineTo (X (xOf (j)), Y (yOf (db)));
                    }
                    if (glow) { c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (col).withAlpha (0.25f)); c.stroke (p, 0, 0, width(), height(), S (w * 3)); c.setBlendMode (visage::BlendMode::Alpha); }
                    c.setColor (col); c.stroke (p, 0, 0, width(), height(), S (w));
                };
                if (V.hasRef && V.refDelta.size() == 30) line (V.specBefore, &V.refDelta, 1.0f, 0xccb39cff, 1.0f, false);
                line (V.specAfter, nullptr, 0, amber, 1.8f, true);
                line (V.specBefore, nullptr, 0, 0xa6e8ecf2, 1.0f, false);
                c.setColor (0xff8b9099);
                draw::text (c, V.hasRef ? "WHITE: RECORDED  \xc2\xb7  AMBER: NOW  \xc2\xb7  VIOLET: REF" : "WHITE: RECORDED  \xc2\xb7  AMBER: NOW", boldFont (S (6)), Font::kRight, X (gx), Y (gy + 1), S (gw - 4), S (9));
            }
            else { c.setColor (0xff5f6670); draw::text (c, "Run Auto Prep to mix", bodyFont (S (10)), Font::kCenter, X (gx), Y (gy), S (gw), S (gh)); }
        }
        const char* tl[5] = { "", "TONE", V.cymbal ? "STICK" : "CRACK", V.cymbal ? "HARSH" : "RINGS", "TAME" };
        const unsigned tc[5] = { 0, cyan, 0xffff4fb0, vio, 0xffc8ff3d };
        // ---- SHAPE
        silk (c, "SHAPE", 36, 215, 0xffa8f0f0, 8.5f, 2.0f);
        c.setColor (0xff7fb8bb); draw::text (c, "TRANSIENT DESIGNER", boldFont (S (6.5f)), Font::kRight, X (150), Y (215), S (136), S (10));
        {
            const float gx = 32, gy = 231, gw = 254, gh = 34;
            if (live && ! V.envBefore.empty())
            {
                auto path = [&] (const std::vector<float>& e)
                {
                    visage::Path p;
                    for (size_t q = 0; q < e.size(); ++q)
                    {
                        const float x = gx + (float) q / (float) (e.size() - 1) * gw, y = gy + std::clamp (-e[q] / 60.0f, 0.0f, 1.0f) * gh;
                        if (q == 0) p.moveTo (X (x), Y (y)); else p.lineTo (X (x), Y (y));
                    }
                    return p;
                };
                const auto pa = path (V.envAfter);
                c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (0xff3ff0e0).withAlpha (0.25f)); c.stroke (pa, 0, 0, width(), height(), S (4)); c.setBlendMode (visage::BlendMode::Alpha);
                c.setColor (0xff3ff0e0); c.stroke (pa, 0, 0, width(), height(), S (1.6f));
                c.setColor (0xa6e8ecf2); c.stroke (path (V.envBefore), 0, 0, width(), height(), S (1));
            }
        }
        const char* sl[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
        const unsigned sc[4] = { 0xffff9a3d, cyan, 0xffc8ff3d, vio };
        // ---- REFERENCE (light deck: dark labels)
        silk (c, "REFERENCE", 36, 370, 0xff24272d, 7.5f, 1.8f);
        c.setColor (0xff1d2026); draw::text (c, "MATCH", boldFont (S (6.6f)), Font::kCenter, X (210), Y (372), S (48), S (10));
        {
            const float wx = 26, wy = 382, ww = 150, wh = 48;
            c.setColor (0xe6060710); c.roundedRectangle (X (wx + 1), Y (wy + 1), S (ww - 2), S (wh - 3), S (4));
            if (V.hasRef)
            {
                if (! V.refWave.empty())
                {
                    const float mid = wy + 20; float pk = 1e-6f; for (float x : V.refWave) pk = std::max (pk, x);
                    for (size_t q = 0; q < V.refWave.size(); ++q)
                    {
                        const float x = wx + 6 + (float) q * (ww - 12) / (float) V.refWave.size(), h = V.refWave[q] / pk * 15;
                        c.setColor (Color (vio).withAlpha (0.85f)); c.fill (X (x), Y (mid - h), S (1.1f), S (2 * h + 0.5f));
                    }
                }
                c.setColor (0xffe8ecf2);
                draw::text (c, V.refName.empty() ? std::string ("Reference") : V.refName, bodyFont (S (8.5f)), Font::kLeft, X (wx + 6), Y (wy + wh - 14), S (ww - 12), S (11));
            }
            else
            {
                c.setColor (0xff8b9099);
                draw::text (c, "Load any drum sound", bodyFont (S (8.5f)), Font::kCenter, X (wx), Y (wy + 9), S (ww), S (12));
                draw::text (c, "you'd like to match", bodyFont (S (8.5f)), Font::kCenter, X (wx), Y (wy + 22), S (ww), S (12));
            }
        }
        keyLabel (c, mkeys[3], "\xe2\x96\xb6 REFERENCE VS MIX", false, vio, V.hasRef && V.refAudio);
        keyLabel (c, mkeys[4], V.hasRef ? "CHANGE" : "LOAD", false, vio, live);
        keyLabel (c, mkeys[5], "REMOVE", false, vio, V.hasRef);
        // ---- REVERB
        silk (c, "REVERB", 36, 476, 0xfff2b8e6, 7.5f, 1.8f);
        for (int u = 0; u < 2; ++u)
        {
            const float ox2 = u ? 162.0f : 26.0f;
            const auto& R = V.verbs[(size_t) u];
            const unsigned col = u ? cyan : vio;
            c.setColor (col); draw::text (c, u ? "R2" : "R1", boldFont (S (9)), Font::kLeft, X (ox2 + 10), Y (497), S (24), S (12));
            std::string ty = R.type; for (auto& ch : ty) ch = (char) std::toupper ((unsigned char) ch);
            c.setColor (live ? 0xffe8ecf2 : 0xff4a4f57); draw::text (c, ty, boldFont (S (9)), Font::kCenter, X (ox2 + 30), Y (497), S (70), S (12));
            visage::Path tri; const float ax = X (ox2 + 116), ay = Y (502);
            tri.moveTo (ax - S (4), ay - S (2)); tri.lineTo (ax + S (4), ay - S (2)); tri.lineTo (ax, ay + S (3)); tri.close();
            c.setColor (col); c.fill (tri);
        }
        c.setColor (0xff8a7f8c);
        draw::text (c, "100% WET  \xc2\xb7  FED BY EACH CHANNEL'S R1 / R2 SEND", boldFont (S (6)), Font::kCenter, X (26), Y (643), S (266), S (10));

        // ---- every knob: lit ring, label, value
        static const char* vl[5] = { "DECAY", "PRE", "SIZE", "TONE", "WIDTH" };
        for (int i = 1; i < nMKnobs; ++i)
        {
            if (i == 9) continue;
            const auto kn = mknob (i);
            const float n = mixKnobNorm (i);
            unsigned col; const char* lab; unsigned labCol = 0xffd8c6b4, valCol;
            if (i < 5) { col = tc[i]; lab = tl[i]; }
            else if (i < 9) { col = sc[i - 5]; lab = sl[i - 5]; labCol = 0xffbfe6e6; }
            else if (i < 12) { col = i == 10 ? 0xff8b5cff : 0xff0aa5c4; lab = i == 10 ? "CURVE" : "ENVELOPE"; labCol = 0xff1d2026; }
            else { col = i < 17 ? vio : cyan; lab = vl[(i - 12) % 5]; labCol = 0xffe6b8da; }
            valCol = i == 10 || i == 11 ? Color (col).interpolateWith (Color (0xff000000), 0.35f).toARGB() : col;
            const bool en = live && ! (i == 10 && ! V.hasRef) && ! (i == 11 && ! V.hasRefEnv);
            mixArc (c, kn.x, kn.y, kn.r, n, Color (col).withAlpha (en ? 1.0f : 0.3f).toARGB(), mixBipolar (i), kn.r < 10 ? 1.8f : 3);
            const float ly = kn.y + kn.r * 1.24f + (kn.r < 10 ? 3 : 5);
            c.setColor (labCol); draw::text (c, lab, boldFont (S (kn.r < 10 ? 6 : 6.8f)), Font::kCenter, X (kn.x - 30), Y (ly), S (60), S (9));
            c.setColor (valCol); draw::text (c, mixValueText (i, mixValueOf (i, n)), monoBold (S (kn.r < 10 ? 6.5f : 7.8f)), Font::kCenter, X (kn.x - 30), Y (ly + (kn.r < 10 ? 8 : 9)), S (60), S (9));
        }
    }

    void mixDown (float x, float y, const visage::MouseEvent& e)
    {
        const auto& V = studio.data.mix;
        if (! V.valid || ! studio.onMix) return;
        for (int i = 1; i < nMKnobs; ++i)
        {
            if (i == 9) continue;
            const auto kn = mknob (i);
            if (std::hypot (x - kn.x, y - (kn.y - 3)) > std::max (12.0f, kn.r * 1.4f)) continue;
            if ((i == 10 && ! V.hasRef) || (i == 11 && ! V.hasRefEnv)) return;
            if (e.repeat_click_count >= 2)
            {
                // double-click: back to the knob's default
                static const float defs[22] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.6f, 0.5f, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 };
                studio.onMix ("knob:" + std::to_string (i), defs[i]);
                return;
            }
            mixKnob = i; mixY0 = y; mixV0 = mixKnobNorm (i); mixLive = mixV0; return;
        }
        for (auto& kk : mkeys)
        {
            if (x < kk.x || x >= kk.x + kk.w || y < kk.y || y >= kk.y + kk.h) continue;
            const std::string id = kk.id;
            pressed = id; redraw();
            if (id.rfind ("menu:", 0) == 0) { if (studio.onMixMenu) studio.onMixMenu (id.substr (5), X (kk.x), Y (kk.y), S (kk.w), S (kk.h)); return; }
            studio.onMix (id, 0);
            return;
        }
    }

    void mixDrag (float, float y)
    {
        if (mixKnob < 0) return;
        mixLive = std::clamp (mixV0 + (mixY0 - y) / 160.0f, 0.0f, 1.0f);
        if (mixBipolar (mixKnob) && std::abs (mixLive - 0.5f) < 0.015f) mixLive = 0.5f;   // a detent at the centre
        if (mixKnob >= 12 && (mixKnob - 12) % 5 == 0 && std::abs (mixValueOf (mixKnob, mixLive) - 1) < 0.04f) mixLive = 0.75f / 2.75f;   // decay detent at x1
        redraw();
    }

    void mixUp()
    {
        if (mixKnob >= 0 && std::isfinite (mixLive) && studio.onMix && std::abs (mixLive - mixV0) > 1e-4f)
            studio.onMix ("knob:" + std::to_string (mixKnob), std::round (mixValueOf (mixKnob, mixLive) * 100) / 100);
        mixKnob = -1; mixLive = NAN; pressed.clear(); redraw();
    }

    void alignDown (float x, float y)
    {
        const auto& A = studio.data.align;
        auto fire = [&] (const std::string& a, double v = 0) { if (studio.onAlign) studio.onAlign (a, A.mic, v); };
        const float dj = std::hypot (x - jogX, y - jogY);
        if (A.mic >= 0 && dj <= jogR * 1.15f) { jogging = true; jogA0 = std::atan2 (y - jogY, x - jogX); liveDelta = 0; return; }
        if (A.mic >= 0 && x >= scX && x < scX + scW && y >= scY && y < scY + scH - 22) { dragging = true; dragFrom = x; liveDelta = 0; return; }
        const Key* kk = keyAt (x, y);
        if (! kk) return;
        const std::string id = kk->id;
        if (! A.valid && id != "again") return;
        pressed = id; redraw();
        if (id.rfind ("menu:", 0) == 0) { if (studio.onAlignMenu) studio.onAlignMenu (id.substr (5), X (kk->x), Y (kk->y), S (kk->w), S (kk->h)); return; }
        if (id.rfind ("t:", 0) == 0) { if (studio.onAlign) studio.onAlign ("timing:" + id.substr (2), A.mic, 0); return; }
        if (id.rfind ("n:", 0) == 0) { fire ("nudge", std::stoi (id.substr (2))); return; }
        if (id == "pair:0" || id == "pair:1") { fire ("pair", id.back() == '1'); return; }
        if (id == "gain") { fire ("gain", 1); return; }
        if (id == "spread") { fire ("spread", A.spread ? 0 : 1); return; }
        fire (id);
    }

    void mouseDrag (const visage::MouseEvent& e) override
    {
        const float x = panelX (e.position.x), y = panelY (e.position.y);
        if (studio.step() == Step::tune) { tuneDrag (x, y); return; }
        if (studio.step() == Step::mix) { mixDrag (x, y); return; }
        if (studio.step() != Step::align) return;
        const auto& A = studio.data.align;
        if (jogging)
        {
            // a quarter sample per 15 degrees, as far round as you like
            float a = std::atan2 (y - jogY, x - jogX), d = a - jogA0;
            if (d > 3.14159265f) d -= 6.2831853f; if (d < -3.14159265f) d += 6.2831853f;
            jogA0 = a;
            liveDelta += d / (3.14159265f / 12) * 0.25f;
            redraw();
        }
        else if (dragging && ! A.ref.empty())
        {
            // drag the wave: it follows the hand, sample-accurate
            const float spp = A.spanMs / 1000 * A.sr / (float) (A.ref.size() - 1);
            const float ptsPerPx = (float) (A.ref.size() - 1) / (scW - 12);
            liveDelta = -(x - dragFrom) * ptsPerPx * spp;
            redraw();
        }
    }

    void mouseUp (const visage::MouseEvent&) override
    {
        if (studio.step() == Step::tune) { tuneUp(); return; }
        if (studio.step() == Step::mix) { mixUp(); return; }
        if (studio.step() != Step::align) return;
        const auto& A = studio.data.align;
        if ((jogging || dragging) && std::abs (liveDelta) >= 0.125f && studio.onAlign)
            studio.onAlign ("lag", A.mic, std::round ((A.lag + liveDelta) * 4) / 4);
        if (pressed == "gain" && studio.onAlign) studio.onAlign ("gain", A.mic, 0);
        jogging = dragging = false; liveDelta = 0; pressed.clear();
        redraw();
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        studio.requestKeyboardFocus();
        const float dx = panelX (e.position.x), dy = panelY (e.position.y);
        if (studio.step() == Step::align) { alignDown (dx, dy); return; }
        if (studio.step() == Step::tune) { tuneDown (dx, dy, e); return; }
        if (studio.step() == Step::mix) { mixDown (dx, dy, e); return; }
        for (int v = 0; v < 6; ++v)
        {
            const float px = padX[v % 3], py = padY[v / 3];
            if (dx >= px && dx < px + 80 && dy >= py && dy < py + 60) { if (studio.onPad) studio.onPad (v); return; }
        }
    }
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
        const float limit = width() - 300;   // the zoom, theme and history controls own the right side
        auto chip = [&] (const std::string& txt, unsigned col, bool ledOn, unsigned ledCol)
        {
            const auto f = bodyFont (12);
            const float w = textWidth (f, txt) + (ledOn ? 34 : 22);
            if (x + w > limit) { x = limit + 1; return; }
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
        c.setClampBounds (x + 4, 0, std::max (0.0f, limit - x), h);
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
//==============================================================================
// A built-in plugin's window: floats over the studio, drag it by its title bar. The faceplate is a Blender render;
// everything that moves or shows a value is drawn here on the render's camera (panel px, tilted 11 degrees), and
// every label is centred on its control at the height of the surface it sits on.
namespace pw
{
    #define LD_IMG(n) reinterpret_cast<const unsigned char*> (BinaryData::n), BinaryData::n##Size
    static Font stencil (float s) { return fontOf (BinaryData::BlackOpsOneRegular_ttf, BinaryData::BlackOpsOneRegular_ttfSize, s); }
    struct Img { const unsigned char* d; int n; };
    static const Img sawSharp[8] = { { LD_IMG (skull_saw_0_jpg) }, { LD_IMG (skull_saw_1_jpg) }, { LD_IMG (skull_saw_2_jpg) }, { LD_IMG (skull_saw_3_jpg) },
                                     { LD_IMG (skull_saw_4_jpg) }, { LD_IMG (skull_saw_5_jpg) }, { LD_IMG (skull_saw_6_jpg) }, { LD_IMG (skull_saw_7_jpg) } };
    static const Img viseRam { LD_IMG (vise_ram_png) };
    static const float viseRamBox[4] = { 729, 54, 1231, 313 };   // in the 1960 x 1088 render
    static Font cinzel (float s) { return fontOf (BinaryData::CinzelBold_ttf, BinaryData::CinzelBold_ttfSize, s); }
    static const Img portGate { LD_IMG (port_gate_png) }, portArch { LD_IMG (port_arch_png) };
    static const float portGateBox[4] = { 690, 197, 1270, 675 }, portArchBox[4] = { 663, 131, 1297, 688 };
    static Font oswald (float s) { return fontOf (BinaryData::OswaldSemiBold_ttf, BinaryData::OswaldSemiBold_ttfSize, s); }
    /** Magnitudes (dB) of a real signal's spectrum, n a power of two; out has n/2 bins. */
    static void fftDb (std::vector<float> x, std::vector<float>& out)
    {
        const size_t n = x.size();
        std::vector<std::complex<float>> a (n);
        for (size_t i = 0; i < n; ++i) a[i] = { x[i] * (0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) n)), 0.0f };
        for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const float ang = -6.2831853f / (float) len; const std::complex<float> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len) { std::complex<float> w (1, 0); for (size_t j = 0; j < len / 2; ++j) { const auto u = a[i + j], t = a[i + j + len / 2] * w; a[i + j] = u + t; a[i + j + len / 2] = u - t; w *= wl; } }
        }
        out.resize (n / 2);
        for (size_t i = 0; i < n / 2; ++i) out[i] = 20 * std::log10 (std::abs (a[i]) * 4.0f / (float) n + 1e-9f);
    }
    static const Img sawBlur[8] = { { LD_IMG (skull_sawb_0_jpg) }, { LD_IMG (skull_sawb_1_jpg) }, { LD_IMG (skull_sawb_2_jpg) }, { LD_IMG (skull_sawb_3_jpg) },
                                    { LD_IMG (skull_sawb_4_jpg) }, { LD_IMG (skull_sawb_5_jpg) }, { LD_IMG (skull_sawb_6_jpg) }, { LD_IMG (skull_sawb_7_jpg) } };
}

struct Studio::PluginWindow : visage::Frame
{
    explicit PluginWindow (Studio& s) : studio (s) { setVisible (false); }
    Studio& studio;
    PluginView v;
    float FW = 980;   ///< the faceplate's width in panel px (Ember is wider)
    static constexpr float FH = 544, TH = 46, WH = 600;
    float WW() const { return FW + 20; }
    float k = 1;                  // window scale
    float sawAngle = 0;           // in teeth
    // drag state
    bool moving = false; Point grab;
    std::string dragKey; float dragY0 = 0, dragV0 = 0, liveV = NAN;

    // the face's camera: panel px (+ height in Blender units) to frame px
    float BX() const { return 10 * k; }
    float BY() const { return TH * k; }
    Point P (float px, float py, float z = 0) const
    {
        return { BX() + (FW / 2 + (px - FW / 2) / 1.01f) * k, BY() + (FH / 2 + (py - FH / 2) * 0.98163f / 1.01f - z * 100 * 0.190809f / 1.01f) * k };
    }
    float S (float x) const { return x * k; }
    void label (Canvas& c, const std::string& t, float px, float py, float z, const Font& f, unsigned col, float boxW = 160, float boxH = 24)
    {
        const auto p = P (px, py, z);
        c.setColor (col);
        draw::text (c, t, f, Font::kCenter, p.x - S (boxW) / 2, p.y - S (boxH) / 2, S (boxW), S (boxH));
    }
    void glowLabel (Canvas& c, const std::string& t, float px, float py, float z, const Font& f, unsigned col, float boxW = 160)
    {
        // lit lettering: the colour, then the same again added on top so it reads as light rather than paint
        label (c, t, px, py, z, f, col, boxW);
        c.setBlendMode (visage::BlendMode::Add);
        label (c, t, px, py, z, f, Color (col).withAlpha (0.45f).toARGB(), boxW);
        c.setBlendMode (visage::BlendMode::Alpha);
    }
    void readout (Canvas& c, float px, float py, const std::string& t, float w = 70)
    {
        const auto p = P (px, py);
        draw::glass (c, p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3));
        glowLabel (c, t, px, py, 0, monoBold (S (10.5f)), 0xffff7a2a, w);
    }

    // ---------------------------------------------------------------- Skull Cutter's controls
    struct Ctl { const char* key; float x, y, r; int kind; };   // kind 0 knob, 1 key, 2 dome, 3 tag, 4 stepped
    static const std::vector<Ctl>& skullCtls()
    {
        static const std::vector<Ctl> c = {
            { "drive", 812, 222, 64, 0 }, { "tone", 82, 456, 25, 0 }, { "lowcut", 194, 456, 25, 0 }, { "highcut", 306, 456, 25, 0 },
            { "mix", 676, 456, 25, 0 }, { "out", 788, 456, 25, 0 }, { "style", 904, 456, 25, 4 }, { "destroy", 490, 452, 24, 2 },
            { "boom", 95, 360, 0, 1 }, { "sploosh", 173, 360, 0, 1 }, { "auto", 251, 360, 0, 1 },
            { "m:decap", 74, 39, 0, 3 }, { "m:devil", 166, 39, 0, 3 }, { "m:saturn", 258, 39, 0, 3 }, { "m:fatso", 350, 39, 0, 3 } };
        return c;
    }
    std::string model() const { return v.gets ("model", "decap"); }
    /** The knob's position 0..1 for this model, and the value it stands for. */
    struct Range { std::string path; double lo, hi; bool log; bool on; };
    Range rangeOf (const std::string& key) const
    {
        if (v.type == "comp") return viseRange (key);
        if (v.type == "gate") return portRange (key);
        if (v.type == "pultec") return emberRange (key);
        if (v.type == "eq") return prismRange (key);
        const auto m = model();
        if (key == "mix") return { "mix", 0, 1, false, true };
        if (key == "out") return { "out", -24, 24, false, true };
        if (key == "drive")
        {
            if (m == "devil") return { "devil.crush", 0, 10, false, true };
            if (m == "saturn") return { "saturn.drive", -12, 30, false, true };
            if (m == "fatso") return { "fatso.input", -12, 24, false, true };
            return { "decap.drive", 0, 10, false, true };
        }
        if (key == "tone")
        {
            if (m == "devil") return { "devil.darkness", 0, 10, false, true };
            if (m == "saturn") return { "saturn.tone", -5, 5, false, true };
            if (m == "fatso") return { "fatso.warmth", 0, 7, false, true };
            return { "decap.tone", -5, 5, false, true };
        }
        if (key == "lowcut") return { "decap.lowcut", 20, 500, true, m == "decap" };
        if (key == "highcut") return { "decap.highcut", 1500, 20000, true, m == "decap" };
        return { "", 0, 1, false, false };
    }
    double defOf (const std::string& path) const
    {
        static const std::map<std::string, double> d { { "decap.drive", 3 }, { "devil.crush", 4 }, { "devil.darkness", 2 }, { "decap.lowcut", 20 }, { "decap.highcut", 20000 }, { "mix", 1 },
            { "dist.input", 5 }, { "dist.attack", 5 }, { "dist.release", 5 }, { "76.input", 6.2 }, { "76.attack", 3 }, { "76.release", 3 },
            { "fair.thresh", 5 }, { "fair.tc", 2 }, { "thr", -40 }, { "range", -40 }, { "hyst", 4 }, { "atk", 0.3 }, { "hold", 40 }, { "rel", 120 }, { "look", 1.5 },
            { "hpf", 20 }, { "lpf", 20000 }, { "lfBoost", 0 }, { "lfAtten", 0 }, { "bw", 5 }, { "hfBoost", 0 }, { "hfAtten", 0 }, { "drive", 1 }, { "160.thresh", -27 }, { "chan.thresh", -24 }, { "chan.attack", 10 }, { "chan.release", 120 }, { "scHpf", 20 } };
        auto i = d.find (path); if (i != d.end()) return i->second;
        // Prism's bands: "bands.<n>.<what>"
        auto ends = [&] (const char* e) { const std::string se (e); return path.size() > se.size() && path.compare (path.size() - se.size(), se.size(), se) == 0; };
        if (ends (".f")) return 1000;
        if (ends (".q")) return 1;
        if (ends (".dyn.thr")) return -30;
        if (ends (".dyn.range")) return -6;
        return 0.0;
    }
    float posOf (const std::string& key) const
    {
        if (key == dragKey && std::isfinite (liveV)) return liveV;
        const auto R = rangeOf (key);
        double x = v.get (R.path, defOf (R.path));
        if (R.path == "decap.lowcut" && x < 20) x = 20;
        if (R.path == "decap.highcut" && x <= 0) x = 20000;
        return (float) std::clamp (R.log ? std::log (x / R.lo) / std::log (R.hi / R.lo) : (x - R.lo) / (R.hi - R.lo), 0.0, 1.0);
    }
    double valueAt (const std::string& key, float pos) const
    {
        const auto R = rangeOf (key);
        return R.log ? R.lo * std::pow (R.hi / R.lo, (double) pos) : R.lo + (R.hi - R.lo) * pos;
    }
    std::string text (const std::string& key, float pos) const
    {
        const auto R = rangeOf (key); const double x = valueAt (key, pos);
        if (! R.on) return "off";
        if (v.type == "eq")
        {
            auto ends = [&] (const char* e) { const std::string se (e); return R.path.size() > se.size() && R.path.compare (R.path.size() - se.size(), se.size(), se) == 0; };
            if (ends (".f")) return x < 1000 ? draw::fmt ("%.0f Hz", x) : draw::fmt ("%.2f kHz", x / 1000);
            if (ends (".q")) return draw::fmt ("%.2f", x);
            if (ends (".dyn.thr")) return v.get (bandPath ("dyn.on"), 0) > 0.5 ? draw::fmt ("%.0f dB", x) : std::string ("off");
            return draw::fmt ("%+.1f dB", x);
        }
        if (v.type == "pultec")
        {
            if (R.path == "out") return draw::fmt ("%+.1f dB", x);
            return draw::fmt ("%.1f", x);
        }
        if (v.type == "gate")
        {
            if (R.path == "thr" || R.path == "range" || R.path == "hyst") return draw::fmt ("%.0f dB", x);
            if (R.path == "atk" || R.path == "look") return draw::fmt (x < 10 ? "%.1f ms" : "%.0f ms", x);
            if (R.path == "hold" || R.path == "rel") return draw::fmt ("%.0f ms", x);
            if (R.path == "hpf") return x <= 20.5 ? std::string ("off") : x < 1000 ? draw::fmt ("%.0f Hz", x) : draw::fmt ("%.1f kHz", x / 1000);
            if (R.path == "lpf") return x >= 19900 ? std::string ("off") : draw::fmt (x < 10000 ? "%.1f kHz" : "%.0f kHz", x / 1000);
        }
        if (v.type == "comp")
        {
            if (R.path == "scHpf") return x <= 20.5 ? std::string ("off") : draw::fmt ("%.0f Hz", x);
            if (R.path == "76.input" || R.path == "fair.input") return draw::fmt ("%+.1f dB", x);
            if (R.path == "160.thresh" || R.path == "chan.thresh") return draw::fmt ("%.1f dB", x);
            if (R.path == "chan.attack") return draw::fmt (x < 10 ? "%.1f ms" : "%.0f ms", x);
            if (R.path == "chan.release") return draw::fmt ("%.0f ms", x);
            if (R.path == "fair.tc") return draw::fmt ("%.0f", std::round (x));
        }
        if (R.path == "mix") return draw::fmt ("%.0f %%", x * 100);
        if (R.path == "out" || R.path == "saturn.drive" || R.path == "fatso.input") return draw::fmt ("%+.1f dB", x);
        if (R.path == "decap.lowcut") return x <= 20.5 ? std::string ("off") : draw::fmt ("%.0f Hz", x);
        if (R.path == "decap.highcut") return x >= 19900 ? std::string ("off") : draw::fmt ("%.1f kHz", x / 1000);
        if (R.path == "decap.tone" || R.path == "saturn.tone") return draw::fmt ("%+.1f", x);
        return draw::fmt ("%.1f", x);
    }
    std::vector<std::string> styles() const
    {
        const auto m = model();
        if (m == "decap") return { "A", "E", "N", "T", "P" };
        if (m == "saturn") return { "gentle", "tape", "tube", "transformer", "heavy" };
        if (m == "fatso") return { "off", "buss", "gp", "spank" };
        return {};
    }
    std::string stylePath() const { const auto m = model(); return m == "fatso" ? "fatso.comp" : m + ".style"; }
    int styleIndex() const
    {
        const auto st = styles(); if (st.empty()) return -1;
        const auto cur = v.gets (stylePath(), st[0]);
        for (size_t i = 0; i < st.size(); ++i) if (st[i] == cur) return (int) i;
        return 0;
    }

    void drawSkull (Canvas& c)
    {
        const float fw = S (FW), fh = S (FH), bx = BX(), by = BY();
        c.setColor (0xffffffff);
        c.image (LD_IMG (skull_plate_jpg), bx, by, fw, fh);
        const bool destroy = model() == "decap" && v.get ("decap.punish", 0) > 0.5;
        const float drive = posOf ("drive");
        if (destroy) { c.setBlendMode (visage::BlendMode::Add); c.setColor (0xffffffff); c.image (LD_IMG (skull_cracks_jpg), bx, by, fw, fh); c.setBlendMode (visage::BlendMode::Alpha); }
        // the saw: spins faster with drive; motion-blurred when fast
        {
            static const float box[4] = { 640, 124, 1320, 785 };   // in the 1960 x 1088 render
            const float sx = fw / 1960, sy = fh / 1088;
            const int f = ((int) std::floor (sawAngle * 8) % 8 + 8) % 8;
            const auto& im = (v.on && drive > 0.5f) ? pw::sawBlur[f] : pw::sawSharp[f];
            c.setColor (0xffffffff);
            c.image (im.d, im.n, bx + box[0] * sx, by + box[1] * sy, (box[2] - box[0]) * sx, (box[3] - box[1]) * sy);
        }
        c.setColor (0xffffffff);
        c.image (LD_IMG (skull_skull_png), bx, by, fw, fh);
        const float eyes = v.on ? std::clamp ((drive - 0.15f) / 0.75f, 0.0f, 1.0f) : 0.0f;
        if (eyes > 0.01f) { c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (0xffffffff).withAlpha (eyes)); c.image (LD_IMG (skull_eyes_jpg), bx, by, fw, fh); c.setBlendMode (visage::BlendMode::Alpha); }

        const unsigned cream = 0xffe9dcc0, hot = 0xffff7a2a, ink = 0xd90a0908;
        // model tags (stamped steel, tops 0.035 up)
        const char* tags[4] = { "VALVE", "CRUSH", "MULTI", "TAPE" };
        const char* models[4] = { "decap", "devil", "saturn", "fatso" };
        for (int i = 0; i < 4; ++i)
        {
            const float px = 32 + i * 92 + 42;
            if (model() == models[i])
            {
                const auto a = P (32 + i * 92, 24, 0.035f);
                c.setColor (Color (hot).withAlpha (0.9f)); c.roundedRectangleBorder (a.x + 1, a.y + 1, S (84 / 1.01f) - 2, S (30 * 0.972f) - 2, S (5), S (1.4f));
                glowLabel (c, tags[i], px, 39, 0.035f, pw::stencil (S (12)), 0xffff8a3a);
            }
            else { label (c, tags[i], px, 39.8f, 0.035f, pw::stencil (S (12)), 0x29ffffff); label (c, tags[i], px, 39, 0.035f, pw::stencil (S (12)), ink); }
        }
        // VU needle in the porthole (dial printed in the render)
        {
            const float lv = studio.pluginLevel (v.who);
            const float fr = 106 * 0.99f;
            const auto pc = P (170, 204 + 0.6f * fr, -0.02f);
            const float vR = 0.42f * 2 * fr / 1.01f * k;
            const float a = (-140 + 100 * std::clamp (lv, 0.0f, 1.0f)) * 3.14159265f / 180;
            c.setColor (0x66000000); c.segment (pc.x + std::cos (a) * S (16) + S (3), pc.y + std::sin (a) * S (16) + S (2), pc.x + std::cos (a) * (vR + S (12)) + S (3), pc.y + std::sin (a) * (vR + S (12)) + S (2), S (1.6f), true);
            c.setColor (0xff120c08); c.segment (pc.x + std::cos (a) * S (16), pc.y + std::sin (a) * S (16), pc.x + std::cos (a) * (vR + S (12)), pc.y + std::sin (a) * (vR + S (12)), S (1.6f), true);
            c.setColor (0xffb0261c); c.segment (pc.x + std::cos (a) * (vR - S (14)), pc.y + std::sin (a) * (vR - S (14)), pc.x + std::cos (a) * (vR + S (12)), pc.y + std::sin (a) * (vR + S (12)), S (1.6f), true);
            c.setColor (0xff2a1d14); c.circle (pc.x - S (11), pc.y - S (11), S (22));
        }
        // DRIVE: painted scale, lit arc, pointer
        {
            const auto d0 = P (812, 222); const float pos = drive;
            const float pi = 3.14159265f, A0 = 0.75f * pi, ASP = 1.5f * pi;
            for (int i = 0; i <= 40; ++i)
            {
                const float a = A0 + ASP * i / 40; const bool big = i % 4 == 0, hotZ = i >= 30, lit = hotZ && pos > 0.75f && v.on;
                const float r0 = 98, r1 = big ? 110 : 104;
                c.setColor (hotZ ? (lit ? 0xffff5a2au : 0xffc03a22u) : 0xbfe8d6b4u);
                c.segment (d0.x + std::cos (a) * S (r0) / 1.01f, d0.y + std::sin (a) * S (r0) * 0.972f, d0.x + std::cos (a) * S (r1) / 1.01f, d0.y + std::sin (a) * S (r1) * 0.972f, S (big ? 2.2f : 1.2f), false);
            }
            for (int i = 0; i <= 10; ++i) { if (i == 5) continue; const float a = A0 + ASP * i / 10; label (c, std::to_string (i), 812 + std::cos (a) * 124, 222 + std::sin (a) * 124, 0, pw::stencil (S (15)), i >= 8 ? 0xffd8452au : cream, 40); }
            c.setBlendMode (visage::BlendMode::Add);
            const float R = S (92) / 1.01f, half = ASP * pos / 2;
            const float centre = -0.75f * pi + half;   // visage arcs: 0 = up, clockwise
            c.setColor (Color (0xffff5a1e).withAlpha (v.on ? 0.3f : 0.12f)); c.arc (d0.x - R - S (4), d0.y - R - S (4), 2 * R + S (8), S (8), centre, half, true);
            c.setColor (Color (0xffff5a1e).withAlpha (v.on ? 0.95f : 0.4f)); c.arc (d0.x - R - S (1.5f), d0.y - R - S (1.5f), 2 * R + S (3), S (3), centre, half, true);
            c.setBlendMode (visage::BlendMode::Alpha);
            const auto cc = P (812, 222, 0.39f); const float cr = S (0.8f * 64), pa = A0 + ASP * pos;
            c.setColor (0xe61c120a); c.segment (cc.x + std::cos (pa) * cr * 0.3f, cc.y + std::sin (pa) * cr * 0.3f, cc.x + std::cos (pa) * cr * 0.88f, cc.y + std::sin (pa) * cr * 0.88f, S (4.2f), true);
            c.setColor (0xffe0301a); c.circle (cc.x + std::cos (pa) * cr * 0.9f - S (3), cc.y + std::sin (pa) * cr * 0.9f - S (3), S (6));
            label (c, "DRIVE", 812, 96, 0, pw::stencil (S (20)), cream, 200, 30);
            readout (c, 812, 370, text ("drive", pos), 86);
        }
        // the small knobs
        const char* names[5] = { "TONE", "LOW CUT", "HIGH CUT", "MIX", "OUTPUT" };
        const char* keys[5] = { "tone", "lowcut", "highcut", "mix", "out" };
        const float xs[5] = { 82, 194, 306, 676, 788 };
        for (int i = 0; i < 5; ++i)
        {
            const auto R = rangeOf (keys[i]);
            const float pos = posOf (keys[i]);
            const float pi = 3.14159265f, A0 = 0.75f * pi, ASP = 1.5f * pi;
            const auto kc = P (xs[i], 456); 
            for (int j = 0; j <= 10; ++j) { const float a = A0 + ASP * j / 10; c.setColor (0x80e8d6b4); c.segment (kc.x + std::cos (a) * S (38) / 1.01f, kc.y + std::sin (a) * S (38) * 0.972f, kc.x + std::cos (a) * S (42) / 1.01f, kc.y + std::sin (a) * S (42) * 0.972f, S (1.2f), false); }
            const auto cc = P (xs[i], 456, 0.34f); const float cr = S (20), a = A0 + ASP * pos;
            c.setColor (R.on ? 0xe61c120au : 0x661c120au); c.segment (cc.x + std::cos (a) * cr * 0.25f, cc.y + std::sin (a) * cr * 0.25f, cc.x + std::cos (a) * cr * 0.9f, cc.y + std::sin (a) * cr * 0.9f, S (3), true);
            std::string nm = names[i];
            if (i == 0 && model() == "devil") nm = "DARKNESS"; else if (i == 0 && model() == "fatso") nm = "WARMTH";
            if (i == 2 - 2 + 0 && false) {}
            label (c, nm, xs[i], 406, 0, pw::stencil (S (12.5f)), R.on ? cream : 0x66e9dcc0u, 120);
            readout (c, xs[i], 506, text (keys[i], pos), 68);
        }
        // STYLE: Roman numerals around the dark knob
        {
            const auto st = styles(); const int sel = styleIndex();
            const char* rn[5] = { "I", "II", "III", "IV", "V" };
            for (int i = 0; i < 5; ++i)
            {
                const float a = -3.14159265f / 2 + (i - 2) * 0.66f;
                const float px = 904 + std::cos (a) * 46, py = 456 + std::sin (a) * 46;
                const bool en = i < (int) st.size();
                if (en && i == sel) glowLabel (c, rn[i], px, py, 0, pw::stencil (S (13)), hot, 40);
                else label (c, rn[i], px, py, 0, pw::stencil (S (13)), en ? cream : 0x40e9dcc0u, 40);
            }
            if (sel >= 0)
            {
                const auto cc = P (904, 456, 0.34f); const float a = -3.14159265f / 2 + (sel - 2) * 0.66f, cr = S (20);
                c.setColor (0xe61c120a); c.segment (cc.x + std::cos (a) * cr * 0.2f, cc.y + std::sin (a) * cr * 0.2f, cc.x + std::cos (a) * cr * 0.9f, cc.y + std::sin (a) * cr * 0.9f, S (3), true);
            }
            label (c, model() == "fatso" ? "COMP" : "STYLE", 904, 508, 0, pw::stencil (S (12.5f)), st.empty() ? 0x66e9dcc0u : cream, 120);
        }
        // DESTROY
        if (destroy) glowLabel (c, "DESTROY", 490, 506, 0, pw::stencil (S (14)), 0xffff3a1a, 200);
        else label (c, "DESTROY", 490, 506, 0, pw::stencil (S (14)), model() == "decap" ? 0xffb8321eu : 0x55b8321eu, 200);
        if (destroy)
        {
            const auto d = P (490, 452, 0.05f);
            c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (0xffff2a10).withAlpha (0.35f)); c.circle (d.x - S (40), d.y - S (40), S (80)); c.setBlendMode (visage::BlendMode::Alpha);
        }
        // BOOM / SPLOOSH / AUTO keys (caps 0.06 up)
        const char* kn[3] = { "BOOM", "SPLOOSH", "AUTO" };
        const char* kp[3] = { "decap.thump", "decap.steep", "auto" };
        for (int i = 0; i < 3; ++i)
        {
            const float px = 60 + i * 78 + 35, py = 348 + 12;
            const bool en = i == 2 || model() == "decap";
            const bool on = en && v.get (kp[i], i == 2 ? 1 : 0) > 0.5;
            if (on)
            {
                const auto a = P (px - 16, 348 + 20, 0.06f);
                c.setColor (0xffff6a2a); c.roundedRectangle (a.x, a.y, S (32), S (1.8f), S (0.9f));
                glowLabel (c, kn[i], px, py - 1, 0.06f, pw::stencil (S (10.5f)), 0xffff8a3a, 70);
            }
            else label (c, kn[i], px, py - 1, 0.06f, pw::stencil (S (10.5f)), en ? 0x99e9dcc0u : 0x40e9dcc0u, 70);
        }
        if (! v.on) { c.setColor (0x8c000000); c.fill (bx, by, fw, fh); c.setColor (0xffe9dcc0); draw::text (c, "BYPASSED", pw::stencil (S (28)), Font::kCenter, bx, by, fw, fh); }
    }


    // ---------------------------------------------------------------- The Vise
    // models: PANIC (dist), FET (76), VARI-MU (fair), VCA (160), CLEAN (chan)
    std::string cmodel() const { const auto m = v.gets ("model", "dist"); return m.empty() ? std::string ("dist") : m; }
    Range viseRange (const std::string& key) const
    {
        const auto m = cmodel();
        if (key == "mix") return { "mix", 0, 1, false, true };
        if (key == "out") return { "out", -24, 24, false, true };
        if (key == "schpf") return { "scHpf", 20, 500, true, true };
        if (key == "input")
        {
            if (m == "dist") return { "dist.input", 0, 10, false, true };
            if (m == "76") return { "76.input", -6, 30, false, true };
            if (m == "fair") return { "fair.input", -6, 18, false, true };
            return { m + ".thresh", -60, 0, false, true };
        }
        if (key == "attack")
        {
            if (m == "dist") return { "dist.attack", 0, 10, false, true };
            if (m == "76") return { "76.attack", 1, 7, false, true };
            if (m == "fair") return { "fair.thresh", 0, 10, false, true };
            if (m == "chan") return { "chan.attack", 0.1, 100, true, true };
            return { "", 0, 1, false, false };
        }
        if (key == "release")
        {
            if (m == "dist") return { "dist.release", 0, 10, false, true };
            if (m == "76") return { "76.release", 1, 7, false, true };
            if (m == "fair") return { "fair.tc", 1, 6, false, true };
            if (m == "chan") return { "chan.release", 10, 2000, true, true };
            return { "", 0, 1, false, false };
        }
        return { "", 0, 1, false, false };
    }
    std::string viseName (const std::string& key) const
    {
        const auto m = cmodel();
        if (key == "input") return m == "160" || m == "chan" ? "THRESH" : "INPUT";
        if (key == "attack") return m == "fair" ? "THRESH" : "ATTACK";
        if (key == "release") return m == "fair" ? "TIME" : "RELEASE";
        return key;
    }
    struct Ratio { std::vector<std::string> vals, labels; bool str; };
    Ratio ratios() const
    {
        const auto m = cmodel();
        if (m == "dist") return { { "1", "2", "3", "4", "6", "10", "20", "nuke" }, { "1", "2", "3", "4", "6", "10", "20", "NUK" }, true };
        if (m == "76") return { { "4", "8", "12", "20", "all" }, { "4", "8", "12", "20", "ALL" }, true };
        if (m == "160") return { { "1.5", "2", "3", "4", "6", "10", "20", "60" }, { "1.5", "2", "3", "4", "6", "10", "20", "INF" }, false };
        if (m == "chan") return { { "1.5", "2", "3", "4", "6", "8", "10", "20" }, { "1.5", "2", "3", "4", "6", "8", "10", "20" }, false };
        return { {}, {}, false };
    }
    int ratioIndex() const
    {
        const auto R = ratios(); if (R.vals.empty()) return -1;
        const auto path = cmodel() + ".ratio";
        if (R.str) { const auto cur = v.gets (path, "4"); for (size_t i = 0; i < R.vals.size(); ++i) if (R.vals[i] == cur) return (int) i; return 3 < (int) R.vals.size() ? 3 : 0; }
        const double cur = v.get (path, cmodel() == "160" ? 4 : 3);
        int best = 0; for (size_t i = 0; i < R.vals.size(); ++i) if (std::abs (std::stod (R.vals[i]) - cur) < std::abs (std::stod (R.vals[(size_t) best]) - cur)) best = (int) i;
        return best;
    }
    double ratioValue() const
    {
        const int i = ratioIndex(); if (i < 0) return 3.2;
        const auto s = ratios().vals[(size_t) i];   // a copy: ratios() is a temporary
        return s == "nuke" ? 60 : s == "all" ? 24 : std::stod (s);
    }
    /** The gain reduction under the playhead (dB); stopped, the most in the 300 ms after it, so a hit under the playhead shows. */
    float grTarget() const
    {
        if (! v.gr || v.gr->empty() || ! v.on) return 0;
        const auto& g = *v.gr;
        const long a = (long) (studio.playhead * 1000) - (studio.playing ? 15 : 0), b = a + (studio.playing ? 16 : 300);
        float m = 0;
        for (long q = std::max (0L, a); q < std::min ((long) g.size(), b); ++q) m = std::max (m, g[(size_t) q]);
        return m;
    }
    float grNow = 0, beaconA = 0, sweep = 0;
    /** Where the ratio dial's positions sit: the top stays clear for its name, the bottom for its readout. */
    static float ratioAngle (int i, int n)
    {
        static const float five[5] = { 135, 180, 225, 315, 360 };
        static const float eight[8] = { 135, 165, 195, 225, 315, 345, 375, 405 };
        return (n == 5 ? five[i] : eight[std::min (i, 7)]) * 3.14159265f / 180;
    }
    void labelLS (Canvas& c, const std::string& t, float px, float py, float z, const Font& f, unsigned col, float ls)
    {
        const auto u = visage::String::convertToUtf32 (t);
        float tw = 0; std::vector<float> w;
        for (char32_t ch : u) { w.push_back (f.stringWidth (std::u32string (1, ch))); tw += w.back(); }
        tw += S (ls) * (float) (u.size() - 1);
        const auto p = P (px, py, z);
        float x0 = p.x - tw / 2;
        c.setColor (col);
        for (size_t i = 0; i < u.size(); ++i)
        {
            c.text (visage::String (std::u32string (1, u[i])), f, Font::kLeft, x0, p.y - S (12), w[i] + S (4), S (24));
            x0 += w[i] + S (ls);
        }
    }
    void nixie (Canvas& c, float px, float py, const std::string& t, float w, bool dim = false)
    {
        const auto p = P (px, py);
        c.setColor (0xff0b0803); c.roundedRectangle (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3));
        c.setColor (0x40ffb21a); c.roundedRectangleBorder (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3), S (1));
        if (dim) label (c, t, px, py, 0, monoBold (S (10.5f)), 0x59ffb21a, w);
        else glowLabel (c, t, px, py, 0, monoBold (S (10.5f)), 0xffffb21a, w);
    }
    void amberPtr (Canvas& c, float px, float py, float z, float r, float a, bool en = true)
    {
        const auto cc = P (px, py, z);
        const float x0 = cc.x + std::cos (a) * S (r) * 0.15f, y0 = cc.y + std::sin (a) * S (r) * 0.15f, x1 = cc.x + std::cos (a) * S (r) * 0.95f, y1 = cc.y + std::sin (a) * S (r) * 0.95f;
        if (en) { c.setBlendMode (visage::BlendMode::Add); c.setColor (0x59ff8a00); c.segment (x0, y0, x1, y1, S (7), true); c.setBlendMode (visage::BlendMode::Alpha); }
        c.setColor (en ? 0xffffb21a : 0x80b8a070); c.segment (x0, y0, x1, y1, S (3), true);
    }
    void ticks (Canvas& c, float px, float py, float r0, float r1, int n, unsigned col, float w)
    {
        const float pi = 3.14159265f; const auto o = P (px, py);
        c.setColor (col);
        for (int i = 0; i < n; ++i)
        {
            const float a = 0.75f * pi + 1.5f * pi * i / (n - 1);
            c.segment (o.x + std::cos (a) * S (r0) / 1.01f, o.y + std::sin (a) * S (r0) * 0.972f, o.x + std::cos (a) * S (r1) / 1.01f, o.y + std::sin (a) * S (r1) * 0.972f, S (w), false);
        }
    }
    static const std::vector<Ctl>& viseCtls()
    {
        static const std::vector<Ctl> c = {
            { "input", 845, 296, 44, 0 }, { "model", 845, 112, 30, 4 }, { "ratio", 82, 452, 23, 4 }, { "attack", 194, 452, 23, 0 }, { "release", 306, 452, 23, 0 },
            { "schpf", 676, 452, 23, 0 }, { "mix", 788, 452, 23, 0 }, { "out", 904, 452, 23, 0 } };
        return c;
    }
    static Point modelPos (int i) { const float a = (-180 + 45 * i) * 3.14159265f / 180; return { 845 + std::cos (a) * 84, 112 + std::sin (a) * 66 }; }

    void drawVise (Canvas& c)
    {
        const float fw = S (FW), fh = S (FH), bx = BX(), by = BY(), pi = 3.14159265f;
        const unsigned AMB = 0xffffb21a, INK = 0xff16130e;
        const auto m = cmodel();
        c.setColor (0xffffffff);
        c.image (LD_IMG (vise_plate_jpg), bx, by, fw, fh);
        // the beacon: lit and sweeping when it's working hard
        if (beaconA > 0.01f)
        {
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (0xffffffff).withAlpha (beaconA)); c.image (LD_IMG (vise_beacon_jpg), bx, by, fw, fh);
            const auto b = P (348, 40, 0.34f);
            // two soft beams: nested wedges, faint, so the light falls off at the edges and with distance
            c.saveState(); c.trimClampBounds (bx, by, fw, fh);
            for (int k = 0; k < 2; ++k)
                for (int j = 1; j <= 4; ++j)
                {
                    const float a = sweep + k * pi, hw = 0.05f * j, L = S (520 - 70 * j);
                    c.setColor (Color (0xffff9614).withAlpha (0.022f * beaconA));
                    c.triangle (b.x, b.y, b.x + std::cos (a - hw) * L, b.y + std::sin (a - hw) * L, b.x + std::cos (a + hw) * L, b.y + std::sin (a + hw) * L);
                }
            c.restoreState();
            c.setBlendMode (visage::BlendMode::Alpha);
        }
        // ---- the screen: the hits under the press; whatever rises above the ram face is pressed flat against it
        const float gr = grNow, travel = std::min (gr, 20.0f) / 20 * 58, faceY = 132 + 22 + travel;
        {
            const float x0 = 340, y0 = 160, w0 = 300, h0 = 196, zs = -0.07f;
            const auto a = P (x0, y0, zs);
            c.saveState(); c.trimClampBounds (a.x, a.y, S (w0 / 1.01f), S (h0 * 0.972f));
            c.setColor (0x0dffb21a);
            for (int i = 1; i < 6; ++i) { const auto q = P (x0, y0 + h0 * i / 6, zs); c.fill (q.x, q.y, S (w0), 1); }
            for (int i = 1; i < 10; ++i) { const auto q = P (x0 + w0 * i / 10, y0, zs); c.fill (q.x, q.y, 1, S (h0)); }
            const float base = y0 + h0 - 14, limit = faceY - 4, room = base - limit, full = (base - (132 + 22 + 4)) * 1.12f;
            // the envelope: the input's peaks across 1.2 s (scrolling with the playhead), on a 48 dB scale
            std::vector<float> col ((size_t) w0 + 1, 0.0f);
            const double span = 1.2, t0 = studio.playhead - (studio.playing ? 1.0 : 0.05);
            auto peakAt = [&] (double t0s, double t1s)
            {
                float pk = 0;
                for (size_t i = 0; i < studio.data.tracks.size(); ++i)
                {
                    if (v.who >= 0 && (int) i != v.who) continue;
                    const auto& tr = studio.data.tracks[i];
                    const long b0 = (long) (t0s * tr.blocksPerSecond), b1 = std::max (b0 + 1, (long) (t1s * tr.blocksPerSecond));
                    for (long q = std::max (0L, b0); q < std::min ((long) tr.peaks.size(), b1); ++q) pk = std::max (pk, tr.peaks[(size_t) q]);
                }
                return pk;
            };
            for (size_t i = 0; i <= (size_t) w0; ++i)
            {
                const double ta = t0 + span * (double) i / w0, tb = t0 + span * (double) (i + 1) / w0;
                const float pk = peakAt (ta, tb);
                const float db = pk > 1e-6f ? 20 * std::log10 (pk) : -120;
                col[i] = std::clamp ((db + 48) / 48, 0.0f, 1.0f) * full;
            }
            auto pathTo = [&] (bool clampTop)
            {
                visage::Path pth; const auto s0 = P (x0, base, zs); pth.moveTo (s0.x, s0.y);
                for (size_t i = 0; i < col.size(); ++i) { const auto q = P (x0 + (float) i, base - (clampTop ? std::min (col[i], room) : col[i]), zs); pth.lineTo (q.x, q.y); }
                const auto e = P (x0 + w0, base, zs); pth.lineTo (e.x, e.y); pth.close();
                return pth;
            };
            const auto ly = P (x0, limit, zs).y, byy = P (x0, base, zs).y;
            c.setColor (Brush::linear (Color (0xe6bed7f0), Color (0x5978a0be), Point (0, ly), Point (0, byy)));
            c.fill (pathTo (true));
            // contact: amber where the hits meet the ram
            c.saveState(); c.trimClampBounds (a.x, ly, S (w0), S (40));
            c.setColor (Brush::linear (Color (0xffffaa1e), Color (0x00ffaa1e), Point (0, ly), Point (0, ly + S (40))));
            c.fill (pathTo (true));
            c.restoreState();
            c.setBlendMode (visage::BlendMode::Add);
            for (size_t i = 0; i < col.size(); ++i)
                if (col[i] >= room && gr > 0.3f) { const auto q = P (x0 + (float) i, limit, zs); c.setColor (0xffffd27a); c.fill (q.x, q.y - S (1.5f), S (1.2f), S (3.5f)); c.setColor (0x40ff8a00); c.fill (q.x - S (2), q.y - S (5), S (5), S (10)); }
            c.setBlendMode (visage::BlendMode::Alpha);
            // the ram's shadow on the glass
            c.setColor (Brush::linear (Color (0x73000000), Color (0x00000000), Point (0, ly), Point (0, ly + S (14))));
            c.fill (a.x, ly, S (w0), S (14));
            c.setColor (0x1fffffff); { const auto q = P (x0, base, zs); c.fill (q.x, q.y, S (w0), 1); }
            glowLabel (c, draw::fmt (gr < 0.05f ? "%.1f dB" : "-%.1f dB", gr), x0 + w0 - 38, y0 + h0 - 28, zs, monoBold (S (11)), AMB, 90);
            c.restoreState();
        }
        // ---- the ram, slid down by the gain reduction (above the crossbar it's hidden in the cylinder)
        {
            const auto cl = P (0, 128, 0.38f);
            c.saveState(); c.trimClampBounds (bx, cl.y + 1, fw, fh);
            const float sx = fw / 1960, sy = fh / 1088, dy = S (travel * 0.98163f / 1.01f);
            const auto* bb = pw::viseRamBox;
            c.setColor (0xffffffff);
            c.image (pw::viseRam.d, pw::viseRam.n, bx + bb[0] * sx, by + bb[1] * sy + dy, (bb[2] - bb[0]) * sx, (bb[3] - bb[1]) * sy);
            c.restoreState();
        }
        // ---- gauge needle (the dial is printed in the render)
        {
            const float fr = 108 * 0.99f;
            const auto gc = P (160, 228, -0.03f);
            const float ga = (135 + 270 * std::min (gr, 20.0f) / 20) * pi / 180, len = S (0.8f * fr * 0.98f), tail = S (16);
            const float nx = std::cos (ga), ny = std::sin (ga), px2 = -ny, py2 = nx;
            auto tri = [&] (float ox, float oy, unsigned colr)
            {
                c.setColor (colr);
                c.triangle (gc.x - nx * tail + px2 * S (4) + ox, gc.y - ny * tail + py2 * S (4) + oy, gc.x + nx * len + ox, gc.y + ny * len + oy, gc.x - nx * tail - px2 * S (4) + ox, gc.y - ny * tail - py2 * S (4) + oy);
            };
            tri (S (3), S (3), 0x59000000);
            tri (0, 0, 0xff151515);
            c.setColor (0xffc8201a); c.segment (gc.x + nx * len * 0.72f, gc.y + ny * len * 0.72f, gc.x + nx * len, gc.y + ny * len, S (2.4f), true);
            c.setColor (0xff2a2a2e); c.circle (gc.x - S (10), gc.y - S (10), S (20));
            c.setColor (0xff8a8a90); c.circle (gc.x - S (7.5f), gc.y - S (7.5f), S (15));
            c.setColor (0xffe8e8ec); c.circle (gc.x - S (5), gc.y - S (6), S (6));
        }
        // ---- model selector
        static const char* MN[5] = { "PANIC", "FET", "VARI-MU", "VCA", "CLEAN" };
        static const char* MK[5] = { "dist", "76", "fair", "160", "chan" };
        int mi = 0; for (int i = 0; i < 5; ++i) if (m == MK[i]) mi = i;
        for (int i = 0; i < 5; ++i)
        {
            const auto q = modelPos (i);
            if (i == mi) glowLabel (c, MN[i], q.x, q.y, 0, pw::stencil (S (11)), AMB, 90);
            else labelLS (c, MN[i], q.x, q.y, 0, pw::stencil (S (11)), INK, 0.5f);
        }
        amberPtr (c, 845, 112, 0.315f, 30 * 0.62f / 0.95f * 0.95f, (-180 + 45 * mi) * pi / 180);
        // ---- INPUT / THRESH
        {
            const float pos = posOf ("input"), A0 = 0.75f * pi, ASP = 1.5f * pi;
            ticks (c, 845, 296, 76, 84, 21, INK, 1.6f);
            for (int i = 0; i <= 10; ++i) { const float a = A0 + ASP * i / 10; label (c, std::to_string (i), 845 + std::cos (a) * 96, 296 + std::sin (a) * 96, 0, pw::stencil (S (13)), INK, 30); }
            const auto o = P (845, 296); const float R = S (70);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (0x4dff8a00); c.arc (o.x - R - S (4), o.y - R - S (4), 2 * R + S (8), S (8), -0.75f * pi + ASP * pos / 2, ASP * pos / 2, true);
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (AMB); c.arc (o.x - R - S (1.5f), o.y - R - S (1.5f), 2 * R + S (3), S (3), -0.75f * pi + ASP * pos / 2, ASP * pos / 2, true);
            amberPtr (c, 845, 296, 0.315f, 44 * 0.62f, A0 + ASP * pos);
            labelLS (c, viseName ("input"), 845, 176, 0, pw::stencil (S (15)), INK, 3);
            nixie (c, 845, 379, text ("input", pos), 72);
        }
        // ---- bottom row
        const float KY = 452;
        {
            const auto R = ratios(); const int ri = ratioIndex(); const bool en = ri >= 0;
            const size_t n = R.vals.size();
            for (size_t i = 0; i < n; ++i)
            {
                const float a = ratioAngle ((int) i, (int) n);
                const float px = 82 + std::cos (a) * 50, py = KY + std::sin (a) * 40;
                if ((int) i == ri) glowLabel (c, R.labels[i], px, py, 0, pw::stencil (S (n > 5 ? 9 : 10)), AMB, 40);
                else label (c, R.labels[i], px, py, 0, pw::stencil (S (n > 5 ? 8.5f : 9.5f)), INK, 40);
            }
            if (en) amberPtr (c, 82, KY, 0.315f, 23 * 0.62f, ratioAngle (ri, (int) n));
            labelLS (c, "RATIO", 82, KY - 48, 0, pw::stencil (S (12)), en ? INK : 0x6616130eu, 1.6f);
            nixie (c, 82, KY + 50, en ? R.labels[(size_t) ri] + (R.labels[(size_t) ri] == "ALL" || R.labels[(size_t) ri] == "NUK" || R.labels[(size_t) ri] == "INF" ? "" : " : 1") : "3 : 1", 68, ! en);
        }
        const char* keys[5] = { "attack", "release", "schpf", "mix", "out" };
        const char* fixed[5] = { "ATTACK", "RELEASE", "SC HPF", "MIX", "OUTPUT" };
        const float xs[5] = { 194, 306, 676, 788, 904 };
        for (int i = 0; i < 5; ++i)
        {
            const auto R = rangeOf (keys[i]); const float pos = posOf (keys[i]);
            ticks (c, xs[i], KY, 36, 40, 11, R.on ? 0xa616130eu : 0x4016130eu, 1.2f);
            amberPtr (c, xs[i], KY, 0.315f, 23 * 0.62f, 0.75f * pi + 1.5f * pi * pos, R.on);
            labelLS (c, i < 2 ? viseName (keys[i]) : fixed[i], xs[i], KY - 48, 0, pw::stencil (S (12)), R.on ? INK : 0x6616130eu, 1.6f);
            nixie (c, xs[i], KY + 50, R.on ? text (keys[i], pos) : std::string ("--"), 68, ! R.on);
        }
        // ---- the transfer curve: input level across, output up, with the hit under the playhead on it
        {
            const float tx = 490 - 59, ty = KY - 33, tw = 118, th = 66, z = -0.06f;
            const auto a = P (tx, ty, z);
            c.saveState(); c.trimClampBounds (a.x, a.y, S (tw), S (th));
            double thr = -24, inDb = 0;
            if (m == "dist") { thr = -14.5; inDb = (v.get ("dist.input", 5) - 5) * 4 + 6.3; }
            else if (m == "76") { thr = -14; inDb = v.get ("76.input", 6.2); }
            else if (m == "fair") { thr = -4 - 3 * v.get ("fair.thresh", 5); inDb = v.get ("fair.input", 0); }
            else thr = v.get (m + ".thresh", m == "160" ? -27 : -24);
            const double eff = thr - inDb, ratio = ratioValue();
            auto outOf = [&] (double L) { return L < eff ? L : eff + (L - eff) / ratio; };
            auto pt = [&] (double L, double O) { return P (tx + 4 + (float) ((L + 60) / 60) * (tw - 8), ty + th - 4 - (float) ((O + 60) / 60) * (th - 8), z); };
            c.setColor (0x1fffb21a); { const auto p0 = pt (-60, -60), p1 = pt (0, 0); c.segment (p0.x, p0.y, p1.x, p1.y, 1, false); }
            visage::Path cv; { const auto p0 = pt (-60, outOf (-60)); cv.moveTo (p0.x, p0.y); }
            for (int i = 1; i <= 60; ++i) { const double L = -60 + i; const auto q = pt (L, outOf (L)); cv.lineTo (q.x, q.y); }
            c.setBlendMode (visage::BlendMode::Add); c.setColor (0x4dff8a00); c.stroke (cv, 0, 0, width(), height(), S (5)); c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (AMB); c.stroke (cv, 0, 0, width(), height(), S (2));
            const float lv = studio.pluginLevel (v.who);
            const double L = std::clamp ((lv > 1e-5f ? 20 * std::log10 ((double) lv) : -60.0) + inDb, -60.0, 0.0);
            const auto d = pt (L, outOf (L));
            c.setColor (0x80ffb21a); c.circle (d.x - S (5), d.y - S (5), S (10));
            c.setColor (0xffffffff); c.circle (d.x - S (3), d.y - S (3), S (6));
            c.restoreState();
            labelLS (c, "CURVE", 490, KY - 48, 0, pw::stencil (S (12)), INK, 1.6f);
        }
        if (! v.on) { c.setColor (0x8c000000); c.fill (bx, by, fw, fh); c.setColor (0xfff0e2b0); draw::text (c, "BYPASSED", pw::stencil (S (28)), Font::kCenter, bx, by, fw, fh); }
    }

    // ---------------------------------------------------------------- Portcullis
    Range portRange (const std::string& key) const
    {
        if (key == "thr") return { "thr", -60, 0, false, true };
        if (key == "range") return { "range", -80, 0, false, true };
        if (key == "hyst") return { "hyst", 0, 12, false, true };
        if (key == "atk") return { "atk", 0.02, 50, true, true };
        if (key == "hold") return { "hold", 0, 500, false, true };
        if (key == "rel") return { "rel", 1, 2000, true, true };
        if (key == "look") return { "look", 0, 10, false, true };
        if (key == "hpf") return { "hpf", 20, 2000, true, true };
        if (key == "lpf") return { "lpf", 1000, 20000, true, true };
        return { "", 0, 1, false, false };
    }
    /** RATIO: "gate" (shut) or an expander, 1.5:1 to 8:1. */
    static const std::vector<double>& portRatios() { static const std::vector<double> r { 0, 1.5, 2, 3, 4, 6, 8 }; return r; }
    int portRatioIndex() const { const double r = v.get ("ratio", 0); int b = 0; for (size_t i = 0; i < portRatios().size(); ++i) if (std::abs (portRatios()[i] - r) < std::abs (portRatios()[(size_t) b] - r)) b = (int) i; return b; }
    float openNow = 0;
    float openTarget() const
    {
        // how far open the gate is under the playhead: its attenuation against its range
        if (! v.on) return 1;
        if (! v.gr || v.gr->empty()) return 0;
        const double rg = std::max (1.0, -v.get ("range", -40));
        // playing: under the playhead; stopped: the most open in the 300 ms after it, so the hit there shows
        const long a = (long) (studio.playhead * 1000), b = a + (studio.playing ? 1 : 300);
        float att = 1e9f;
        for (long q = std::max (0L, a); q < std::min ((long) v.gr->size(), b); ++q) att = std::min (att, (*v.gr)[(size_t) q]);
        if (att > 1e8f) return 0;
        return (float) std::clamp (1.0 - att / rg, 0.0, 1.0);
    }
    void carved (Canvas& c, const std::string& t, float px, float py, float size, float ls, float z = 0.045f)
    {
        labelLS (c, t, px, py + 1.1f, z, pw::cinzel (S (size)), 0x38ffecd2, ls);
        labelLS (c, t, px, py, z, pw::cinzel (S (size)), 0xeb060504, ls);
    }
    void litC (Canvas& c, const std::string& t, float px, float py, float size, unsigned col, float ls, float z = 0.045f)
    {
        labelLS (c, t, px, py, z, pw::cinzel (S (size)), col, ls);
        c.setBlendMode (visage::BlendMode::Add);
        labelLS (c, t, px, py, z, pw::cinzel (S (size)), Color (col).withAlpha (0.5f).toARGB(), ls);
        c.setBlendMode (visage::BlendMode::Alpha);
    }
    void gnix (Canvas& c, float px, float py, const std::string& t, float w)
    {
        const auto p = P (px, py);
        c.setColor (0xff030806); c.roundedRectangle (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3));
        c.setColor (0x406dffb0); c.roundedRectangleBorder (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3), S (1));
        glowLabel (c, t, px, py, 0, monoBold (S (10.5f)), 0xff6dffb0, w);
    }
    void gptr (Canvas& c, float px, float py, float z, float r, float a)
    {
        const auto cc = P (px, py, z);
        const float x0 = cc.x + std::cos (a) * S (r) * 0.2f, y0 = cc.y + std::sin (a) * S (r) * 0.2f, x1 = cc.x + std::cos (a) * S (r) * 0.95f, y1 = cc.y + std::sin (a) * S (r) * 0.95f;
        c.setBlendMode (visage::BlendMode::Add); c.setColor (0x591aff7a); c.segment (x0, y0, x1, y1, S (7), true); c.setBlendMode (visage::BlendMode::Alpha);
        c.setColor (0xff6dffb0); c.segment (x0, y0, x1, y1, S (2.6f), true);
    }
    static const std::vector<Ctl>& portCtls()
    {
        static const std::vector<Ctl> c = {
            { "thr", 120, 212, 44, 0 }, { "range", 100, 452, 21, 0 }, { "ratio", 230, 452, 21, 4 }, { "hyst", 360, 452, 21, 0 }, { "atk", 490, 452, 21, 0 },
            { "hold", 620, 452, 21, 0 }, { "rel", 750, 452, 21, 0 }, { "look", 880, 452, 21, 0 }, { "hpf", 820, 252, 22, 0 }, { "lpf", 912, 252, 22, 0 } };
        return c;
    }
    void drawPort (Canvas& c)
    {
        const float fw = S (FW), fh = S (FH), bx = BX(), by = BY(), pi = 3.14159265f, sx = fw / 1960, sy = fh / 1088;
        const unsigned GRN = 0xff6dffb0;
        c.setColor (0xffffffff);
        c.image (LD_IMG (port_plate_jpg), bx, by, fw, fh);
        const float open = openNow;
        // ---- behind the gate: the hits, bright where the gate let them through
        const float x0 = 340, x1 = 640, ys = 182, yb = 338, ry = 76, zs = -0.12f;
        {
            const auto a = P (x0, ys - ry, zs), b = P (x1, yb, zs);
            c.saveState(); c.trimClampBounds (a.x, a.y, b.x - a.x, b.y - a.y);
            c.setColor (0xf2070b09); c.fill (a.x, a.y, b.x - a.x, b.y - a.y);
            c.setColor (0x0d6dffb0); for (int i = 1; i < 8; ++i) { const auto q = P (x0 + 300 * i / 8.0f, 80, zs); c.fill (q.x, a.y, 1, b.y - a.y); }
            const float mid = (ys + yb) / 2 + 8;
            const double span = 1.2, t0 = studio.playhead - (studio.playing ? 1.0 : 0.05);
            const double rg = std::max (1.0, -v.get ("range", -40));
            for (int i = 0; i < 300; ++i)
            {
                const double ta = t0 + span * i / 300, tb = t0 + span * (i + 1) / 300;
                float pk = 0;
                for (size_t ti = 0; ti < studio.data.tracks.size(); ++ti)
                {
                    if (v.who >= 0 && (int) ti != v.who) continue;
                    const auto& tr = studio.data.tracks[ti];
                    const long b0 = (long) (ta * tr.blocksPerSecond), b1 = std::max (b0 + 1, (long) (tb * tr.blocksPerSecond));
                    for (long q = std::max (0L, b0); q < std::min ((long) tr.peaks.size(), b1); ++q) pk = std::max (pk, tr.peaks[(size_t) q]);
                }
                const float db = pk > 1e-6f ? 20 * std::log10 (pk) : -120;
                const float hh = std::clamp ((db + 60) / 60, 0.0f, 1.0f) * 56;
                bool on = false;
                if (v.gr && ! v.gr->empty()) { const long q = std::clamp ((long) (ta * 1000), 0L, (long) v.gr->size() - 1); on = (*v.gr)[(size_t) q] < rg * 0.5; }
                const auto q0 = P (x0 + i, mid - hh, zs), q1 = P (x0 + i, mid + hh, zs);
                if (on) { c.setColor (0x146dffb0); c.fill (q0.x, a.y, S (1.2f), b.y - a.y); }
                c.setColor (on ? 0xeb96ffc8 : 0x59788c82); c.fill (q0.x, q0.y, S (1.2f), q1.y - q0.y);
            }
            // the threshold, dashed, both sides
            const float th = std::clamp (((float) v.get ("thr", -40) + 60) / 60, 0.0f, 1.0f) * 56;
            for (float yy : { mid - th, mid + th })
                for (float xx = x0; xx < x1; xx += 12)
                {
                    const auto q = P (xx, yy, zs);
                    c.setColor (0x40ffe27a); c.fill (q.x, q.y - S (2), S (7), S (4));
                    c.setColor (0xffffe27a); c.fill (q.x, q.y - S (0.7f), S (7), S (1.4f));
                }
            c.restoreState();
        }
        // ---- the portcullis, raised by how open the gate is, then the arch's stones over it
        {
            const auto top = P (0, (ys - ry - 6) - 2);
            const float lift = S (open * 200 * 0.98163f / 1.01f);
            const auto* gb = pw::portGateBox;
            c.saveState(); c.trimClampBounds (bx, top.y, fw, fh);
            c.setColor (0x8c000000); c.image (pw::portGate.d, pw::portGate.n, bx + gb[0] * sx + S (6), by + gb[1] * sy - lift + S (8), (gb[2] - gb[0]) * sx, (gb[3] - gb[1]) * sy);
            c.setColor (0xffffffff); c.image (pw::portGate.d, pw::portGate.n, bx + gb[0] * sx, by + gb[1] * sy - lift, (gb[2] - gb[0]) * sx, (gb[3] - gb[1]) * sy);
            c.restoreState();
            const auto* ab = pw::portArchBox;
            c.image (pw::portArch.d, pw::portArch.n, bx + ab[0] * sx, by + ab[1] * sy, (ab[2] - ab[0]) * sx, (ab[3] - ab[1]) * sy);
        }
        // ---- the lanterns: green when it's open, red when it's shut
        c.setBlendMode (visage::BlendMode::Add);
        c.setColor (Color (0xffffffff).withAlpha (std::clamp ((open - 0.3f) / 0.4f, 0.0f, 1.0f)));
        c.image (LD_IMG (port_open_jpg), bx, by, fw, fh);
        c.setColor (Color (0xffffffff).withAlpha (std::clamp ((0.7f - open) / 0.4f, 0.0f, 1.0f) * (v.on ? 1.0f : 0.0f)));
        c.image (LD_IMG (port_closed_jpg), bx, by, fw, fh);
        c.setBlendMode (visage::BlendMode::Alpha);
        if (open > 0.5f) litC (c, "OPEN", 262, 212, 12, GRN, 1.5f); else carved (c, "OPEN", 262, 212, 12, 1.5f);
        if (open < 0.5f && v.on) litC (c, "CLOSED", 722, 212, 12, 0xffff6a5a, 1.5f); else carved (c, "CLOSED", 722, 212, 12, 1.5f);
        // ---- THRESH
        {
            const float pos = posOf ("thr"), A0 = 0.75f * pi, ASP = 1.5f * pi;
            ticks (c, 120, 212, 66, 70, 25, 0xcc040403, 1.4f);
            const char* tl[7] = { "-60", "-50", "-40", "-30", "-20", "-10", "0" };
            for (int i = 0; i < 7; ++i) { const float a = A0 + ASP * i / 6; carved (c, tl[i], 120 + std::cos (a) * 79, 212 + std::sin (a) * 79, 9.5f, 0.2f, 0.03f); }
            const auto o = P (120, 212); const float R = S (64);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (0x4d1aff7a); c.arc (o.x - R - S (4), o.y - R - S (4), 2 * R + S (8), S (8), -0.75f * pi + ASP * pos / 2, ASP * pos / 2, true);
            c.setBlendMode (visage::BlendMode::Alpha);
            c.setColor (GRN); c.arc (o.x - R - S (1.5f), o.y - R - S (1.5f), 2 * R + S (3), S (3), -0.75f * pi + ASP * pos / 2, ASP * pos / 2, true);
            gptr (c, 120, 212, 0.29f, 44 * 0.6f, A0 + ASP * pos);
            carved (c, "THRESH", 120, 100, 13, 3);
            gnix (c, 120, 320, text ("thr", pos), 80);
        }
        // ---- the row
        const char* keys[7] = { "range", "ratio", "hyst", "atk", "hold", "rel", "look" };
        const char* names[7] = { "RANGE", "RATIO", "HYST", "ATTACK", "HOLD", "RELEASE", "LOOK-AHEAD" };
        for (int i = 0; i < 7; ++i)
        {
            const float px = 100 + i * 130, KY = 452;
            float pos; std::string t;
            if (i == 1) { const int ri = portRatioIndex(); pos = ri / 6.0f; t = ri == 0 ? std::string ("gate") : draw::fmt ("%.1f : 1", portRatios()[(size_t) ri]); }
            else { pos = posOf (keys[i]); t = text (keys[i], pos); }
            ticks (c, px, KY, 34, 38, i == 1 ? 7 : 11, 0xb3040403, 1.2f);
            gptr (c, px, KY, 0.29f, 21 * 0.6f, 0.75f * pi + 1.5f * pi * pos);
            carved (c, names[i], px, KY - 48, 11, 1.2f);
            gnix (c, px, KY + 48, t, 74);
        }
        // ---- the key
        carved (c, "KEY SOURCE", 858, 50, 11, 2);
        {
            const int kt = (int) v.get ("keyT", -1);
            std::string ks = "Self";
            if (kt >= 0 && kt < (int) studio.data.tracks.size()) ks = studio.data.tracks[(size_t) kt].name;
            if (ks.size() > 14) ks = ks.substr (0, 13) + ".";
            glowLabel (c, ks, 850, 92, 0.06f, monoBold (S (12)), GRN, 130);
            label (c, "v", 918, 92, 0.06f, monoBold (S (9)), 0xb36dffb0, 20);
        }
        const char* kb[2] = { "LISTEN", "FLIP" };
        for (int i = 0; i < 2; ++i)
        {
            const bool on = v.get (i == 0 ? "listen" : "flip", 0) > 0.5;
            const float px = 790 + i * 82 + 33, py = 132 + 13;
            if (on) litC (c, kb[i], px, py, 11, GRN, 1.5f, 0.08f); else carved (c, kb[i], px, py, 11, 1.5f, 0.08f);
        }
        const char* kn[2] = { "KEY HPF", "KEY LPF" }; const char* kk[2] = { "hpf", "lpf" };
        for (int i = 0; i < 2; ++i)
        {
            const float px = i == 0 ? 820 : 912, pos = posOf (kk[i]);
            ticks (c, px, 252, 35, 39, 11, 0xb3040403, 1.2f);
            gptr (c, px, 252, 0.29f, 22 * 0.6f, 0.75f * pi + 1.5f * pi * pos);
            carved (c, kn[i], px, 204, 10.5f, 1.2f);
            gnix (c, px, 298, text (kk[i], pos), 70);
        }
        if (v.get ("listen", 0) > 0.5) glowLabel (c, "LISTENING TO THE KEY", 490, 360, 0.02f, monoBold (S (10)), 0xffffe27a, 300);
        if (! v.on) { c.setColor (0x8c000000); c.fill (bx, by, fw, fh); c.setColor (0xffe8e0d0); draw::text (c, "BYPASSED", pw::cinzel (S (28)), Font::kCenter, bx, by, fw, fh); }
    }

    // ---------------------------------------------------------------- the live spectrum (Ember's screen, Prism's graph)
    static constexpr int SPEC_N = 160;
    std::vector<float> spec = std::vector<float> (SPEC_N, -120.0f);
    void updateSpectrum (double dt)
    {
        // what's going into the plugin under the playhead: 2048 samples of its mic (or every mic, on the bus)
        std::vector<float> now (SPEC_N, -120.0f);
        if (studio.playing)
        {
            std::vector<float> x (2048, 0.0f); double sr = 44100; bool any = false;
            for (size_t i = 0; i < studio.data.tracks.size(); ++i)
            {
                if (v.who >= 0 && (int) i != v.who) continue;
                const auto& tr = studio.data.tracks[i];
                if (! tr.wave || ! tr.wave->a || tr.mute) continue;
                sr = tr.wave->sampleRate;
                const long s0 = (long) (studio.playhead * sr) - 1024;
                for (auto& ch : *tr.wave->a)
                    for (long q = 0; q < 2048; ++q) { const long m = s0 + q; if (m >= 0 && m < (long) ch.size()) x[(size_t) q] += ch[(size_t) m] / (float) tr.wave->a->size(); }
                any = true;
            }
            if (any)
            {
                std::vector<float> db; pw::fftDb (x, db);
                for (int i = 0; i < SPEC_N; ++i)
                {
                    const double f0 = 20 * std::pow (1000.0, i / (double) SPEC_N), f1 = 20 * std::pow (1000.0, (i + 1) / (double) SPEC_N);
                    const int b0 = std::max (1, (int) (f0 * 2048 / sr)), b1 = std::max (b0 + 1, (int) (f1 * 2048 / sr));
                    float m = -120; for (int b = b0; b < std::min (b1, (int) db.size()); ++b) m = std::max (m, db[(size_t) b]);
                    now[(size_t) i] = m + 3.0f * (float) std::log2 (f0 / 1000);   // tilted +3 dB/oct, so drums read flat-ish
                }
            }
        }
        // phosphor afterglow: up at once, down slowly
        for (int i = 0; i < SPEC_N; ++i) spec[(size_t) i] = std::max (now[(size_t) i], spec[(size_t) i] - (float) dt * 40.0f);
    }

    // ---------------------------------------------------------------- Ember (the tube EQ)
    Range emberRange (const std::string& key) const
    {
        if (key == "out") return { "out", -24, 24, false, true };
        if (key == "drive") return { "drive", 0, 10, false, v.get ("tube", 1) > 0.5 };
        for (const char* k : { "lfBoost", "lfAtten", "bw", "hfBoost", "hfAtten" }) if (key == k) return { key, 0, 10, false, v.get ("eqp", 1) > 0.5 };
        return { "", 0, 1, false, false };
    }
    struct Steps { const char* path; std::vector<double> vals; std::vector<std::string> labels; const char* unit; };
    static const Steps& emberSteps (const std::string& key)
    {
        static const Steps lf { "lf", { 20, 30, 60, 100 }, { "20", "30", "60", "100" }, "Hz" };
        static const Steps hf { "hf", { 3, 4, 5, 8, 10, 12, 16 }, { "3", "4", "5", "8", "10", "12", "16" }, "kHz" };
        static const Steps hfa { "hfa", { 5, 10, 20 }, { "5", "10", "20" }, "kHz" };
        return key == "lf" ? lf : key == "hf" ? hf : hfa;
    }
    int stepIndex (const Steps& st, double def) const
    {
        const double cur = v.get (st.path, def); int b = 0;
        for (size_t i = 0; i < st.vals.size(); ++i) if (std::abs (st.vals[i] - cur) < std::abs (st.vals[(size_t) b] - cur)) b = (int) i;
        return b;
    }
    float heatNow = 0, flick[3] = { 0, 0, 0 }, flickT = 0;
    static const std::vector<Ctl>& emberCtls()
    {
        static const std::vector<Ctl> c = {
            { "lf", 100, 310, 26, 4 }, { "lfBoost", 220, 310, 26, 0 }, { "lfAtten", 340, 310, 26, 0 }, { "hf", 480, 310, 26, 4 }, { "bw", 600, 310, 26, 0 },
            { "hfBoost", 720, 310, 26, 0 }, { "hfa", 860, 310, 26, 4 }, { "hfAtten", 980, 310, 26, 0 }, { "drive", 120, 462, 30, 0 }, { "out", 940, 462, 30, 0 },
            { "eqp", 270, 476, 18, 2 }, { "meq", 350, 476, 18, 2 }, { "tube", 430, 476, 18, 2 }, { "natural", 670, 476, 18, 2 }, { "linear", 750, 476, 18, 2 } };
        return c;
    }
    void eng (Canvas& c, const std::string& t, float px, float py, float size, float ls, unsigned col = 0xffefe2c4)
    {
        labelLS (c, t, px, py - 0.8f, 0, pw::oswald (S (size)), 0x8c000000, ls);
        labelLS (c, t, px, py, 0, pw::oswald (S (size)), col, ls);
    }
    void litE (Canvas& c, const std::string& t, float px, float py, float size, float ls)
    {
        labelLS (c, t, px, py, 0, pw::oswald (S (size)), 0xffffb35a, ls);
        c.setBlendMode (visage::BlendMode::Add); labelLS (c, t, px, py, 0, pw::oswald (S (size)), 0x80ff7a10, ls); c.setBlendMode (visage::BlendMode::Alpha);
    }
    void enix (Canvas& c, float px, float py, const std::string& t, float w)
    {
        const auto p = P (px, py);
        c.setColor (0xff0d0703); c.roundedRectangle (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3));
        c.setColor (0x40ffb35a); c.roundedRectangleBorder (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3), S (1));
        glowLabel (c, t, px, py, 0, monoBold (S (10.5f)), 0xffffb35a, w);
    }
    void creamPtr (Canvas& c, float px, float py, float z, float r, float a, bool en = true)
    {
        const auto cc = P (px, py, z);
        const float x0 = cc.x + std::cos (a) * S (r) * 0.25f, y0 = cc.y + std::sin (a) * S (r) * 0.25f, x1 = cc.x + std::cos (a) * S (r) * 0.92f, y1 = cc.y + std::sin (a) * S (r) * 0.92f;
        c.setColor (0x80000000); c.segment (x0 + S (1), y0 + S (1), x1 + S (1), y1 + S (1), S (2.6f), true);
        c.setColor (en ? 0xfff3ead6 : 0x80f3ead6); c.segment (x0, y0, x1, y1, S (2.6f), true);
    }
    void drawEmber (Canvas& c)
    {
        const float fw = S (FW), fh = S (FH), bx = BX(), by = BY(), pi = 3.14159265f, A0 = 0.75f * pi, ASP = 1.5f * pi;
        c.setColor (0xffffffff);
        c.image (LD_IMG (ember_plate_jpg), bx, by, fw, fh);
        // the valves: glow with Drive, flickering a touch while audio passes
        {
            const float heat = heatNow * (1 + 0.04f * (flick[0] + flick[1] + flick[2]) / 3);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (0xffffffff).withAlpha (std::clamp (heat, 0.0f, 1.0f)));
            c.image (LD_IMG (ember_hot_jpg), bx, by, fw, fh);
            c.setBlendMode (visage::BlendMode::Alpha);
        }
        // ---- VU needle
        {
            const float vx = 205, vy = 98, vw = 230, vh = 122, Rn = 0.92f * vh;
            const auto a0 = P (vx - vw / 2, vy - vh / 2, -0.05f);
            c.saveState(); c.trimClampBounds (a0.x, a0.y, S (vw / 1.01f), S (vh * 0.972f));
            const float lv = studio.pluginLevel (v.who) * (float) std::pow (10.0, (v.get ("out", 0)) / 20);
            const float vu = std::clamp (lv, 0.0f, 1.1f) / 1.1f;
            const float a = (-128 + 76 * vu) * pi / 180;
            const auto pc = P (vx, vy - vh / 2 + 1.18f * vh, -0.05f);
            c.setColor (0x66000000); c.segment (pc.x + std::cos (a) * S (Rn * 0.45f) + S (3), pc.y + std::sin (a) * S (Rn * 0.45f) + S (2), pc.x + std::cos (a) * S (Rn + 14) + S (3), pc.y + std::sin (a) * S (Rn + 14) + S (2), S (1.5f), true);
            c.setColor (0xff1a120a); c.segment (pc.x + std::cos (a) * S (Rn * 0.45f), pc.y + std::sin (a) * S (Rn * 0.45f), pc.x + std::cos (a) * S (Rn + 14), pc.y + std::sin (a) * S (Rn + 14), S (1.5f), true);
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (0xffffa046).withAlpha (0.06f + 0.1f * heatNow)); c.fill (a0.x, a0.y, S (vw), S (vh));
            c.setBlendMode (visage::BlendMode::Alpha);
            c.restoreState();
        }
        // ---- the transformer cans' decals
        for (int i = 0; i < 2; ++i)
        {
            const float x = i ? 922.0f : 808.0f, y = 96, r = 44;
            labelLS (c, i ? "OUTPUT" : "INPUT", x, y - r * 0.58f, 0.29f, pw::oswald (S (8)), 0xbfefe2c4, 1.4f);
            labelLS (c, "TRANSFORMER", x, y + r * 0.58f, 0.29f, pw::oswald (S (7)), 0x99efe2c4, 1.2f);
        }
        // ---- section headers with engraved rules
        struct Hd { const char* t; float a, b; };
        for (auto [t, a, b] : { Hd { "LOW FREQUENCY", 100, 340 }, Hd { "HIGH FREQUENCY", 480, 720 }, Hd { "ATTENUATION", 860, 980 } })
        {
            const float cx = (a + b) / 2, y = 238;
            eng (c, t, cx, y, 11, 2.4f);
            const float w = (textWidth (pw::oswald (S (11)), t) + S (2.4f) * (float) (std::strlen (t) - 1)) / S (1) / 2 + 10;
            c.setColor (0x66efe2c4);
            for (auto [x0, x1] : { std::pair<float, float> { a - 40, cx - w }, { cx + w, b + 40 } })
                if (x1 > x0) { const auto p0 = P (x0, y), p1 = P (x1, y); c.fill (p0.x, p0.y, p1.x - p0.x, S (1)); }
        }
        // ---- row A
        const float KY = 310;
        auto stepped = [&] (const std::string& key, float px, const char* name, double def)
        {
            const auto& st = emberSteps (key); const int sel = stepIndex (st, def); const int n = (int) st.vals.size();
            const bool en = v.get ("eqp", 1) > 0.5;
            for (int i = 0; i < n; ++i)
            {
                const float a = A0 + ASP * i / (n - 1);
                if (i == sel && en) litE (c, st.labels[(size_t) i], px + std::cos (a) * 50, KY + std::sin (a) * 49, 10, 0.3f);
                else eng (c, st.labels[(size_t) i], px + std::cos (a) * 50, KY + std::sin (a) * 49, 10, 0.3f);
            }
            creamPtr (c, px, KY, 0.29f, 26 * 0.78f, A0 + ASP * sel / (n - 1), en);
            eng (c, name, px, KY + 53, 11, 1.6f);
            enix (c, px, KY + 75, st.labels[(size_t) sel] + " " + st.unit, 64);
        };
        auto cont = [&] (const std::string& key, float px, const char* name)
        {
            const auto R = rangeOf (key); const float pos = posOf (key);
            ticks (c, px, KY, 37, 41, 11, 0x99efe2c4, 1.2f);
            const char* tl[3] = { "0", "5", "10" };
            for (int i = 0; i < 3; ++i) { const float a = A0 + ASP * i / 2; eng (c, tl[i], px + std::cos (a) * 50, KY + std::sin (a) * 49, 10, 0.3f); }
            creamPtr (c, px, KY, 0.29f, 26 * 0.78f, A0 + ASP * pos, R.on);
            eng (c, name, px, KY + 53, 11, 1.6f);
            enix (c, px, KY + 75, text (key, pos), 64);
        };
        stepped ("lf", 100, "CPS", 60); cont ("lfBoost", 220, "BOOST"); cont ("lfAtten", 340, "ATTEN");
        stepped ("hf", 480, "KCS", 10); cont ("bw", 600, "BANDWIDTH"); cont ("hfBoost", 720, "BOOST");
        stepped ("hfa", 860, "ATTEN SEL", 20); cont ("hfAtten", 980, "ATTEN");
        // ---- DRIVE / OUTPUT
        for (int i = 0; i < 2; ++i)
        {
            const float px = i ? 940.0f : 120.0f, py = 462; const std::string key = i ? "out" : "drive";
            const auto R = rangeOf (key); const float pos = posOf (key);
            eng (c, i ? "OUTPUT" : "DRIVE", px, 404.5f, 11, 2);
            const auto o = P (px, py); const float Rr = S (45);
            if (R.on)
            {
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (0x4dff7a10); c.arc (o.x - Rr - S (4), o.y - Rr - S (4), 2 * Rr + S (8), S (8), -0.75f * pi + ASP * pos / 2, ASP * pos / 2, true);
                c.setBlendMode (visage::BlendMode::Alpha);
                c.setColor (0xffffb35a); c.arc (o.x - Rr - S (1.3f), o.y - Rr - S (1.3f), 2 * Rr + S (2.6f), S (2.6f), -0.75f * pi + ASP * pos / 2, ASP * pos / 2, true);
            }
            creamPtr (c, px, py, 0.29f, 30 * 0.78f, A0 + ASP * pos, R.on);
            enix (c, px, py + 56, R.on ? text (key, pos) : std::string ("off"), 70);
        }
        // ---- push buttons with jewel lamps
        const char* bn[5] = { "EQP", "MID EQ", "TUBE", "NATURAL", "LINEAR" };
        const bool lin = v.gets ("phase", "min") == "lin";
        const bool on[5] = { v.get ("eqp", 1) > 0.5, v.get ("meq", 0) > 0.5, v.get ("tube", 1) > 0.5, ! lin, lin };
        const float bxs[5] = { 270, 350, 430, 670, 750 };
        for (int i = 0; i < 5; ++i)
        {
            if (on[i])
            {
                const auto j = P (bxs[i], 476 - 34, 0.06f);
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (0x40ff7814); c.circle (j.x - S (22), j.y - S (22), S (44));
                c.setColor (0x99ff9628); c.circle (j.x - S (9), j.y - S (9), S (18));
                c.setColor (0xf2ffe6a0); c.circle (j.x - S (4), j.y - S (4), S (8));
                c.setBlendMode (visage::BlendMode::Alpha);
                litE (c, bn[i], bxs[i], 476 + 34, 10.5f, 1.2f);
            }
            else eng (c, bn[i], bxs[i], 476 + 34, 10.5f, 1.2f);
        }
        eng (c, "PHASE", 710, 414, 11, 2.4f); eng (c, "SECTIONS", 350, 414, 11, 2.4f);
        // ---- the green screen: the curve in phosphor, the live spectrum glowing behind it
        {
            const float cx = 530, cy = 466, cr = 46, z = -0.04f;
            const auto o = P (cx, cy, z);
            auto inCircle = [&] (float dx, float y) { const float h = std::sqrt (std::max (0.0f, cr * cr - dx * dx)); return std::clamp (y, cy - h, cy + h); };
            c.setColor (0x1f7dffa0);
            for (int i = -2; i <= 2; ++i)
            {
                const float d = i * cr / 3, h = std::sqrt (cr * cr - d * d);
                { const auto p0 = P (cx - h, cy + d, z), p1 = P (cx + h, cy + d, z); c.fill (p0.x, p0.y, p1.x - p0.x, 1); }
                { const auto p0 = P (cx + d, cy - h, z), p1 = P (cx + d, cy + h, z); c.fill (p0.x, p0.y, 1, p1.y - p0.y); }
            }
            const int W = (int) (2 * cr);
            for (int i = 0; i < W; ++i)
            {
                const int si = std::clamp (i * SPEC_N / W, 0, SPEC_N - 1);
                const float lvl = std::clamp ((spec[(size_t) si] + 60) / 60, 0.0f, 1.0f);
                if (lvl <= 0.01f) continue;
                const float dx = -cr + i, yb = inCircle (dx, cy + cr), yt = inCircle (dx, cy + cr - lvl * cr * 1.6f);
                const auto p0 = P (cx + dx, yt, z), p1 = P (cx + dx, yb, z);
                c.setColor (Brush::linear (Color (0x477dffa0), Color (0x0a7dffa0), Point (0, P (0, cy - cr, z).y), Point (0, P (0, cy + cr, z).y)));
                c.fill (p0.x, p0.y, S (1.05f), p1.y - p0.y);
            }
            if (! v.curve.empty())
            {
                visage::Path cv;
                for (int i = 0; i <= W; ++i)
                {
                    const int ci = std::clamp (i * (PluginView::CURVE_N - 1) / W, 0, PluginView::CURVE_N - 1);
                    const float dx = -cr + i, y = inCircle (dx, cy - v.curve[(size_t) ci] * 2.2f);
                    const auto q = P (cx + dx, y, z);
                    if (i == 0) cv.moveTo (q.x, q.y); else cv.lineTo (q.x, q.y);
                }
                c.setBlendMode (visage::BlendMode::Add); c.setColor (0x597dffa0); c.stroke (cv, 0, 0, width(), height(), S (6)); c.setBlendMode (visage::BlendMode::Alpha);
                c.setColor (0xff7dffa0); c.stroke (cv, 0, 0, width(), height(), S (2));
            }
            (void) o;
        }
        if (! v.on) { c.setColor (0x8c000000); c.fill (bx, by, fw, fh); c.setColor (0xffefe2c4); draw::text (c, "BYPASSED", pw::oswald (S (28)), Font::kCenter, bx, by, fw, fh); }
    }

    // ---------------------------------------------------------------- Prism (the EQ)
    // each band is one colour of the spectrum, from its beam out of the prism to its glow on the curve, its key and its knob rings
    static constexpr unsigned BANDCOL[6] = { 0xffff5a5a, 0xffffa040, 0xffffe45a, 0xff57ff9e, 0xff57c8ff, 0xffb46bff };
    int selBand = 0;
    int nBands() const { return std::min (6, (int) v.get ("bands.count", 0)); }
    std::string bandPath (const std::string& what, int b = -1) const { return "bands." + std::to_string (b < 0 ? selBand : b) + "." + what; }
    std::string bandType (int b) const { return v.gets (bandPath ("type", b), "bell"); }
    Range prismRange (const std::string& key) const
    {
        const bool has = selBand < nBands();
        const bool cut = has && (bandType (selBand) == "lowcut" || bandType (selBand) == "highcut");
        if (key == "out") return { "out", -24, 24, false, true };
        if (key == "freq") return { bandPath ("f"), 20, 20000, true, has };
        if (key == "gain") return { bandPath ("g"), -24, 24, false, has && ! cut && bandType (selBand) != "notch" };
        if (key == "q") return { bandPath ("q"), 0.1, 10, true, has };
        if (key == "dthr") return { bandPath ("dyn.thr"), -60, 0, false, has && v.get (bandPath ("dyn.on"), 0) > 0.5 };
        if (key == "drange") return { bandPath ("dyn.range"), -24, 24, false, has && v.get (bandPath ("dyn.on"), 0) > 0.5 };
        return { "", 0, 1, false, false };
    }
    // the graph: 20 Hz - 20 kHz across, +-24 dB
    static constexpr float DX = 130, DY = 28, DW = 670, DH = 272, ZD = -0.1f;
    static float FX (double f) { return DX + (float) (std::log10 (f / 20) / 3) * DW; }
    static float GY (double d) { return DY + DH * 0.46f - (float) d / 24 * (DH * 0.42f); }
    float nodeY (int b) const
    {
        const auto t = bandType (b);
        if (t == "lowcut" || t == "highcut" || t == "notch") return GY (-3);
        return GY (v.get (bandPath ("g", b), 0));
    }
    struct Drag2 { int band = -1; float x0 = 0, y0 = 0; double f0 = 0, g0 = 0, f = 0, g = 0; } node;
    float meterL = 0, meterPk = 0;
    void keyBase (Canvas& c, float x, float y, float w, float h, bool on, unsigned col)
    {
        const auto a = P (x - 2, y - 2, 0.03f);
        if (on)
        {
            c.setBlendMode (visage::BlendMode::Add);
            c.setColor (Color (col).withAlpha (0.25f)); c.roundedRectangleBorder (a.x - S (2), a.y - S (2), S (w + 8), S (h + 8), S (8), S (5));
            c.setColor (col); c.roundedRectangleBorder (a.x, a.y, S (w + 4), S (h + 4), S (6), S (2.4f));
            c.setBlendMode (visage::BlendMode::Alpha);
        }
        else { c.setColor (Color (col).withAlpha (0.3f)); c.roundedRectangleBorder (a.x, a.y, S (w + 4), S (h + 4), S (6), S (1.4f)); }
    }
    void icon (Canvas& c, const std::string& kind, float cx, float cy, unsigned col, bool on)
    {
        const float z = 0.09f;
        auto Q = [&] (float dx, float dy) { return P (cx + dx, cy + dy, z); };
        const unsigned ink = on ? 0xffffffffu : 0xd914161au;
        auto curve = [&] (auto f)
        {
            visage::Path pth;
            for (int i = 0; i <= 24; ++i) { const float t = i / 24.0f; const auto q = Q (-14 + 28 * t, f (t)); if (i == 0) pth.moveTo (q.x, q.y); else pth.lineTo (q.x, q.y); }
            if (on) { c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (col).withAlpha (0.5f)); c.stroke (pth, 0, 0, width(), height(), S (5)); c.setBlendMode (visage::BlendMode::Alpha); }
            c.setColor (ink); c.stroke (pth, 0, 0, width(), height(), S (2));
        };
        auto seg = [&] (float x0, float y0, float x1, float y1) { const auto a = Q (x0, y0), b = Q (x1, y1); c.setColor (ink); c.segment (a.x, a.y, b.x, b.y, S (2), true); };
        if (kind == "bell") curve ([] (float t) { return 4 - 9 * std::exp (-std::pow ((t - 0.5f) * 4.2f, 2.0f)); });
        else if (kind == "lowshelf") curve ([] (float t) { return -4 + 8 / (1 + std::exp (-(t - 0.5f) * 14)); });
        else if (kind == "highshelf") curve ([] (float t) { return 4 - 8 / (1 + std::exp (-(t - 0.5f) * 14)); });
        else if (kind == "lowcut") curve ([] (float t) { return t < 0.45f ? 9 - (t / 0.45f) * 12 : -3.0f; });
        else if (kind == "highcut") curve ([] (float t) { return t > 0.55f ? -3 + ((t - 0.55f) / 0.45f) * 12 : -3.0f; });
        else if (kind == "notch") curve ([] (float t) { return -3 + 11 * std::exp (-std::pow ((t - 0.5f) * 14, 2.0f)); });
        else if (kind == "dyn") curve ([] (float t) { return std::sin (t * 3.14159265f * 3) * 5 * (1 - t * 0.6f); });
        else if (kind == "power")
        {
            const auto o = Q (0, 1); c.setColor (ink); c.arc (o.x - S (8), o.y - S (8), S (16), S (2), 0, 3.14159265f * 0.78f, true);
            seg (0, -9, 0, 0);
        }
        else if (kind == "solo")
        {
            const auto o = Q (0, 2); c.setColor (ink); c.arc (o.x - S (9), o.y - S (9), S (18), S (2), 0, 3.14159265f / 2, true);
            const auto l = Q (-11, 1), r = Q (6, 1); c.fill (l.x, l.y, S (5), S (8)); c.fill (r.x, r.y, S (5), S (8));
        }
        else if (kind == "delete") { seg (-8, -6, 8, -6); seg (-3, -9, 3, -9); seg (-6, -4, -5, 9); seg (-5, 9, 5, 9); seg (5, 9, 6, -4); }
    }
    void ringKnob (Canvas& c, float px, float py, float r, float pos, unsigned col, bool bip, bool en)
    {
        const float pi = 3.14159265f, rr = r * 1.24f;
        const auto o = P (px, py, 0.016f);
        c.setColor (0x0fffffff); c.arc (o.x - S (rr), o.y - S (rr), S (2 * rr), S (3), 0, 0.75f * pi, true);
        if (en)
        {
            float s0 = bip ? 0.0f : -0.75f * pi, s1 = -0.75f * pi + 1.5f * pi * pos;
            if (s1 < s0) std::swap (s0, s1);
            if (s1 - s0 > 0.01f)
            {
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (Color (col).withAlpha (0.3f)); c.arc (o.x - S (rr + 3), o.y - S (rr + 3), S (2 * rr + 6), S (8), (s0 + s1) / 2, (s1 - s0) / 2, true);
                c.setColor (col); c.arc (o.x - S (rr + 0.5f), o.y - S (rr + 0.5f), S (2 * rr + 1), S (3.2f), (s0 + s1) / 2, (s1 - s0) / 2, true);
                c.setBlendMode (visage::BlendMode::Alpha);
            }
        }
        const auto t = P (px, py, 0.195f); const float a = 0.75f * pi + 1.5f * pi * pos;
        c.setColor (en ? 0xe614161au : 0x7314161au);
        c.segment (t.x + std::cos (a) * S (r) * 0.2f, t.y + std::sin (a) * S (r) * 0.2f, t.x + std::cos (a) * S (r) * 0.72f, t.y + std::sin (a) * S (r) * 0.72f, S (2.4f), true);
    }
    void pnix (Canvas& c, float px, float py, const std::string& t, float w, unsigned col)
    {
        const auto p = P (px, py);
        c.setColor (0xff05070a); c.roundedRectangle (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3));
        c.setColor (Color (col).withAlpha (0.25f)); c.roundedRectangleBorder (p.x - S (w) / 2, p.y - S (9), S (w), S (18), S (3), S (1));
        glowLabel (c, t, px, py, 0, monoBold (S (10.5f)), col, w);
    }
    void drawPrism (Canvas& c)
    {
        const float fw = S (FW), fh = S (FH), bx = BX(), by = BY();
        const int nb = nBands();
        if (selBand > nb) selBand = nb;
        const unsigned sc = BANDCOL[std::min (selBand, 5)];
        const Font f9 = uiFont (S (9)), f10 = uiFont (S (10));
        c.setColor (0xffffffff);
        c.image (LD_IMG (prism_plate_jpg), bx, by, fw, fh);
        // ---- the graph
        {
            const auto a = P (DX, DY, ZD), b = P (DX + DW, DY + DH, ZD);
            c.saveState(); c.trimClampBounds (a.x, a.y, b.x - a.x, b.y - a.y);
            c.setColor (0xd1020306); c.fill (a.x, a.y, b.x - a.x, b.y - a.y);
            for (double f : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            {
                const auto q = P (FX (f), DY, ZD); c.setColor (0x12b4cdf0); c.fill (q.x, a.y, 1, b.y - a.y);
                label (c, f >= 1000 ? draw::fmt ("%.0fk", f / 1000) : draw::fmt ("%.0f", f), FX (f), DY + DH - 12, ZD, f9, 0x73aabedc, 40);
            }
            for (int d : { -18, -12, -6, 0, 6, 12, 18 })
            {
                const auto q = P (DX, GY (d), ZD); c.setColor (d == 0 ? 0x29c8dcffu : 0x0d96b4dcu); c.fill (a.x, q.y, b.x - a.x, 1);
                if (d) label (c, draw::fmt (d > 0 ? "+%.0f" : "%.0f", (double) d), DX + DW - 16, GY (d) - 7, ZD, f9, 0x66aabedc, 30);
            }
            // the beams from the prism's side to each band's node
            for (int k = 0; k < nb; ++k)
            {
                const auto p0 = P (DX, DY + 40 + k * 14, ZD), p1 = P (FX (v.get (bandPath ("f", k), 1000)), nodeY (k), ZD);
                c.setBlendMode (visage::BlendMode::Add);
                c.setColor (Brush::linear (Color (BANDCOL[k]).withAlpha (0.0f), Color (BANDCOL[k]).withAlpha (k == selBand ? 0.3f : 0.14f), Point (p0.x, 0), Point (p1.x, 0)));
                c.segment (p0.x, p0.y, p1.x, p1.y, S (k == selBand ? 6 : 3.5f), true);
                c.setBlendMode (visage::BlendMode::Alpha);
            }
            // the live spectrum, as faint white light
            {
                const float base = DY + DH - 14;
                visage::Path sp; { const auto q = P (DX, DY + DH, ZD); sp.moveTo (q.x, q.y); }
                for (int i = 0; i <= SPEC_N; ++i)
                {
                    const float d = spec[(size_t) std::min (i, SPEC_N - 1)];
                    const float y = std::clamp (base - (d + 72) / 72 * (DH - 24), DY, DY + DH);
                    const auto q = P (DX + DW * i / SPEC_N, y, ZD); sp.lineTo (q.x, q.y);
                }
                { const auto q = P (DX + DW, DY + DH, ZD); sp.lineTo (q.x, q.y); } sp.close();
                c.setColor (Brush::linear (Color (0x38dce6ff), Color (0x05dce6ff), Point (0, a.y), Point (0, b.y)));
                c.fill (sp);
            }
            // each band's own shape as coloured light
            for (int k = 0; k < nb && k < (int) v.bandCurves.size(); ++k)
            {
                const auto t = bandType (k);
                if (t == "lowcut" || t == "highcut") continue;
                visage::Path bp; { const auto q = P (DX, GY (0), ZD); bp.moveTo (q.x, q.y); }
                const auto& cv = v.bandCurves[(size_t) k];
                for (int i = 0; i < PluginView::CURVE_N; ++i) { const auto q = P (DX + DW * i / (PluginView::CURVE_N - 1), std::clamp (GY (cv[(size_t) i]), DY, DY + DH), ZD); bp.lineTo (q.x, q.y); }
                { const auto q = P (DX + DW, GY (0), ZD); bp.lineTo (q.x, q.y); } bp.close();
                c.setBlendMode (visage::BlendMode::Add); c.setColor (Color (BANDCOL[k]).withAlpha (k == selBand ? 0.3f : 0.15f)); c.fill (bp); c.setBlendMode (visage::BlendMode::Alpha);
            }
            // the curve: white light
            if (! v.curve.empty())
            {
                visage::Path cv;
                for (int i = 0; i < PluginView::CURVE_N; ++i)
                {
                    const auto q = P (DX + DW * i / (PluginView::CURVE_N - 1), std::min (DY + DH + 4, GY (v.curve[(size_t) i])), ZD);
                    if (i == 0) cv.moveTo (q.x, q.y); else cv.lineTo (q.x, q.y);
                }
                c.setBlendMode (visage::BlendMode::Add); c.setColor (0x4dffffff); c.stroke (cv, 0, 0, width(), height(), S (7)); c.setBlendMode (visage::BlendMode::Alpha);
                c.setColor (0xfff4f8ff); c.stroke (cv, 0, 0, width(), height(), S (2.4f));
            }
            // the nodes
            for (int k = 0; k < nb; ++k)
            {
                const auto q = P (FX (v.get (bandPath ("f", k), 1000)), nodeY (k), ZD);
                const bool on = v.get (bandPath ("on", k), 1) > 0.5;
                const float r = k == selBand ? 22 : 14;
                c.setBlendMode (visage::BlendMode::Add);
                for (int j = 3; j >= 1; --j) { c.setColor (Color (BANDCOL[k]).withAlpha ((on ? 0.22f : 0.08f) * (4 - j) / 3)); c.circle (q.x - S (r * j / 3), q.y - S (r * j / 3), S (2 * r * j / 3)); }
                c.setBlendMode (visage::BlendMode::Alpha);
                const float d = k == selBand ? 5 : 3.6f;
                c.setColor (on ? 0xffffffffu : 0x80ffffffu); c.circle (q.x - S (d), q.y - S (d), S (2 * d));
            }
            if (selBand < nb)
            {
                const double f = v.get (bandPath ("f"), 1000), g = v.get (bandPath ("g"), 0);
                const std::string t = (f >= 1000 ? draw::fmt ("%.2f kHz", f / 1000) : draw::fmt ("%.0f Hz", f)) + "  " + draw::fmt ("%+.1f dB", g);
                float nx = FX (f) + 18, ny = nodeY (selBand) - 34;
                if (nx + 120 > DX + DW) nx = FX (f) - 130;
                if (ny < DY + 4) ny = nodeY (selBand) + 14;
                const auto q = P (nx, ny, ZD);
                c.setColor (0xeb04060a); c.roundedRectangle (q.x, q.y, S (112), S (22), S (5));
                c.setColor (Color (sc).withAlpha (0.4f)); c.roundedRectangleBorder (q.x, q.y, S (112), S (22), S (5), S (1));
                glowLabel (c, t, nx + 56, ny + 11, ZD, uiFont (S (10)), sc, 112);
            }
            if (nb == 0) label (c, "Press a BAND key to add a band", DX + DW / 2, DY + DH * 0.46f - 24, ZD, uiFont (S (12)), 0x99c8d2e6, 400);
            c.restoreState();
        }
        // ---- the beam: white in from the left, through the prism, out as six colours
        {
            const float px = 66, py = 160, side = 82, h = side * 0.8660254f, inY = py - h / 6, lx = px - side / 4, rx = px + side / 4;
            c.setBlendMode (visage::BlendMode::Add);
            auto wb = [&] (float x0, float y0, float x1, float y1, unsigned col, float w, float al)
            { const auto a = P (x0, y0, 0.25f), b = P (x1, y1, 0.25f); c.setColor (Color (col).withAlpha (al)); c.segment (a.x, a.y, b.x, b.y, S (w), true); };
            wb (-10, inY + 6, lx, inY, 0xffffffff, 8, 0.25f); wb (-10, inY + 6, lx, inY, 0xffffffff, 2, 0.95f);
            const auto ig = P (px, inY, 0.25f);
            for (int j = 3; j >= 1; --j) { c.setColor (Color (0xffffffff).withAlpha (0.12f * (4 - j))); c.circle (ig.x - S (10 * j), ig.y - S (10 * j), S (20 * j)); }
            for (int k = 0; k < 6; ++k)
            {
                const bool live = k < nb;
                wb (rx, inY + k * 1.2f, DX + 2, DY + 40 + k * 14, BANDCOL[k], 6, live ? 0.25f : 0.07f);
                wb (rx, inY + k * 1.2f, DX + 2, DY + 40 + k * 14, BANDCOL[k], 1.6f, live ? 0.95f : 0.25f);
            }
            c.setBlendMode (visage::BlendMode::Alpha);
        }
        glowLabel (c, "P R I S M", 66, 22, 0, boldFont (S (15)), 0xffffffff, 120);
        // ---- the meter: two segmented bars and a peak line
        {
            const float mx = 840, my = 40, mw = 56, mh = 206, zz = -0.06f; const int segs = 34; const float bw = 16;
            const float lvDb = meterL > 1e-6f ? 20 * std::log10 (meterL) : -120, pkDb = meterPk > 1e-6f ? 20 * std::log10 (meterPk) : -120;
            auto tOf = [] (float db) { return std::clamp ((db + 60) / 60, 0.0f, 1.0f); };
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = mx + 10 + ch * (bw + 4);
                for (int k = 0; k < segs; ++k)
                {
                    const float t = (float) k / segs, yy = my + mh - 8 - (k + 1) * (mh - 16) / segs;
                    const bool lit = t < tOf (lvDb);
                    const unsigned col = t > 0.92f ? 0xffff5a5a : t > 0.82f ? 0xffffe45a : 0xffe8f4ff;
                    const auto q = P (x, yy + 1, zz);
                    c.setColor (lit ? col : 0x0dffffffu); c.fill (q.x, q.y, S (bw), S ((mh - 16) / segs - 2));
                }
                const auto q = P (x, my + mh - 8 - tOf (pkDb) * (mh - 16), zz);
                if (pkDb > -60) { c.setColor (0xffffffff); c.fill (q.x, q.y, S (bw), S (1.6f)); }
            }
            const float marks[5][2] = { { 0, 1 }, { -6, 0.9f }, { -12, 0.8f }, { -24, 0.6f }, { -48, 0.2f } };
            for (auto& m : marks) label (c, draw::fmt ("%.0f", m[0]), mx + mw + 16, my + mh - 8 - m[1] * (mh - 16), 0, f9, 0x99e1e6f0, 30);
            labelLS (c, "OUT", mx + mw / 2, my + mh + 13, 0, f10, 0xbfe1e6f0, 2);
            pnix (c, mx + mw / 2, my + mh + 32, pkDb > -100 ? draw::fmt ("%.1f dB", pkDb) : std::string ("-inf"), 62, 0xffffffff);
        }
        // ---- keys
        for (int k = 0; k < 6; ++k)
        {
            const float x = 130 + k * 52, y = 324; const bool on = k == selBand && k < nb, exists = k < nb, addable = k == nb;
            keyBase (c, x, y, 40, 36, on, exists || addable ? BANDCOL[k] : 0x40ffffffu);
            label (c, exists ? std::to_string (k + 1) : addable ? std::string ("+") : std::to_string (k + 1), x + 20, y + 18, 0.09f, uiFont (S (15)),
                   on ? 0xffffffffu : exists ? BANDCOL[k] : addable ? Color (BANDCOL[k]).withAlpha (0.6f).toARGB() : 0x33ffffffu, 40);
        }
        static const char* SH[6] = { "bell", "lowshelf", "highshelf", "lowcut", "highcut", "notch" };
        const bool has = selBand < nb;
        for (int i = 0; i < 6; ++i)
        {
            const float x = 470 + i * 52, y = 324; const bool on = has && bandType (selBand) == SH[i];
            keyBase (c, x, y, 40, 36, on, has ? sc : 0x40ffffffu);
            icon (c, SH[i], x + 20, y + 18, sc, on);
        }
        labelLS (c, "BAND", 130 + (5 * 52 + 40) / 2.0f, 312, 0, uiFont (S (9.5f)), 0x8ce1e6f0, 2.5f);
        labelLS (c, "SHAPE", 470 + (5 * 52 + 40) / 2.0f, 312, 0, uiFont (S (9.5f)), 0x8ce1e6f0, 2.5f);
        const char* LK[3] = { "power", "solo", "delete" };
        for (int i = 0; i < 3; ++i)
        {
            const float x = 46, y = 390 + i * 48.0f;
            const bool on = has && i == 0 && v.get (bandPath ("on"), 1) > 0.5;
            keyBase (c, x, y, 40, 36, on, has && i != 1 ? sc : 0x30ffffffu);
            icon (c, LK[i], x + 20, y + 18, sc, on);
        }
        {
            const bool dyn = has && v.get (bandPath ("dyn.on"), 0) > 0.5;
            keyBase (c, 480, 448, 40, 36, dyn, has ? sc : 0x40ffffffu); icon (c, "dyn", 500, 466, sc, dyn);
            labelLS (c, "DYNAMIC", 500, 420, 0, f10, 0xcce1e6f0, 1.6f);
        }
        {
            const bool lin = v.gets ("phase", "min") == "lin";
            keyBase (c, 822, 316, 92, 36, true, 0xffffffff);
            glowLabel (c, lin ? "LINEAR" : "NATURAL", 868, 334, 0.09f, uiFont (S (10.5f)), 0xffffffff, 92);
            labelLS (c, "PHASE", 868, 302, 0, f10, 0xbfe1e6f0, 2);
        }
        {
            const float x = 28, y = 326, w = 80;
            labelLS (c, "BAND", x + w / 2, y + 11, -0.05f, uiFont (S (8.5f)), 0x99dce1eb, 1.5f);
            glowLabel (c, has ? std::to_string (selBand + 1) : std::string ("-"), x + w / 2, y + 32, -0.05f, boldFont (S (24)), sc, 60);
        }
        // ---- knobs
        const char* kk[5] = { "freq", "gain", "q", "dthr", "drange" };
        const char* kl[5] = { "FREQ", "GAIN", "Q", "DYN THRESH", "DYN RANGE" };
        const float kx[5] = { 180, 290, 400, 600, 710 };
        for (int i = 0; i < 5; ++i)
        {
            const auto R = rangeOf (kk[i]); const float pos = posOf (kk[i]);
            ringKnob (c, kx[i], 466, 22, pos, sc, i == 1 || i == 4, R.on);
            labelLS (c, kl[i], kx[i], 420, 0, f10, 0xcce1e6f0, 1.6f);
            pnix (c, kx[i], 512, R.on ? text (kk[i], pos) : std::string (has ? "--" : ""), 82, sc);
        }
        {
            const float pos = posOf ("out");
            ringKnob (c, 868, 466, 24, pos, 0xffffffff, true, true);
            labelLS (c, "OUTPUT", 868, 418, 0, f10, 0xcce1e6f0, 1.6f);
            pnix (c, 868, 516, text ("out", pos), 72, 0xffffffff);
        }
        if (! v.on) { c.setColor (0x8c000000); c.fill (bx, by, fw, fh); c.setColor (0xffe8eef8); draw::text (c, "BYPASSED", boldFont (S (28)), Font::kCenter, bx, by, fw, fh); }
    }
    std::string prismAt (Point q)
    {
        auto in = [&] (float x, float y, float w, float h, float z = 0.05f) { const auto a = P (x, y, z); return q.x >= a.x && q.x < a.x + S (w) && q.y >= a.y && q.y < a.y + S (h); };
        for (int k = 0; k < 6; ++k) if (in (130 + k * 52, 324, 40, 36)) return "band:" + std::to_string (k);
        for (int i = 0; i < 6; ++i) if (in (470 + i * 52, 324, 40, 36)) return "shape:" + std::to_string (i);
        for (int i = 0; i < 3; ++i) if (in (46, 390 + i * 48.0f, 40, 36)) return std::string (i == 0 ? "bandon" : i == 1 ? "solo" : "delete");
        if (in (480, 448, 40, 36)) return "dyn";
        if (in (822, 316, 92, 36)) return "phase";
        const char* kk[5] = { "freq", "gain", "q", "dthr", "drange" }; const float kx[5] = { 180, 290, 400, 600, 710 };
        for (int i = 0; i < 5; ++i) { const auto p = P (kx[i], 466, 0.2f); if (std::hypot (q.x - p.x, q.y - p.y) < S (28)) return kk[i]; }
        { const auto p = P (868, 466, 0.2f); if (std::hypot (q.x - p.x, q.y - p.y) < S (30)) return "out"; }
        // a node on the graph
        for (int k = nBands() - 1; k >= 0; --k)
        {
            const auto p = P (FX (v.get (bandPath ("f", k), 1000)), nodeY (k), ZD);
            if (std::hypot (q.x - p.x, q.y - p.y) < S (14)) return "node:" + std::to_string (k);
        }
        return {};
    }

    void draw (Canvas& c) override
    {
        // shadow, chrome, title bar
        c.setColor (0xe6000000); c.roundedRectangleShadow (S (6), S (12), width() - S (12), height() - S (12), S (10), S (30));
        c.setColor (0xff131315); c.roundedRectangle (0, 0, width(), height(), S (10));
        c.setColor (Brush::linear (Color (0xff2b2b30), Color (0xff1a1a1e), Point (0, 0), Point (0, S (38))));
        c.roundedRectangle (0, 0, width(), S (38), S (10));
        c.setColor (0xff1a1a1e); c.fill (0, S (20), width(), S (18));
        c.setColor (0x99000000); c.fill (0, S (37), width(), 1);
        const unsigned col = v.type == "eq" ? BANDCOL[std::min (selBand, 5)] : v.type == "comp" ? 0xffffb21a : v.type == "gate" ? 0xff6dffb0 : v.type == "pultec" ? 0xffffb35a : 0xffff6a2a;
        // bypass power
        {
            const float px = S (22), py = S (19), r = S (6.5f);
            c.setColor (v.on ? col : 0xff5a5f68);
            c.arc (px - r, py - r, 2 * r, S (2), 3.14159265f, 3.14159265f * 0.82f, true);
            c.segment (px, py - S (8), px, py - S (1), S (2), true);
        }
        draw::glass (c, S (42), S (9), S (150), S (20), S (4));
        c.setColor (col); draw::text (c, v.track, monoBold (S (10.5f)), Font::kCenter, S (42), S (9), S (150), S (20));
        c.setColor (0x73dcdce6); draw::text (c, "INSERT " + std::to_string (v.slot + 1), uiFont (S (10)), Font::kLeft, S (212), S (9), S (120), S (20));
        c.setColor (0xebf0f0f5);
        const std::string title = v.type == "sat" ? "SKULL CUTTER" : v.type == "comp" ? "THE VISE" : v.type == "gate" ? "PORTCULLIS" : v.type == "pultec" ? "EMBER" : v.type == "eq" ? "PRISM" : v.type == "width" ? "HORIZON" : v.type;
        draw::text (c, title, boldFont (S (13)), Font::kCenter, 0, S (9), width(), S (20));
        c.setColor (0x99e6e6eb); draw::text (c, "\xe2\x9c\x95", uiFont (S (12)), Font::kCenter, width() - S (40), S (9), S (26), S (20));
        if (v.type == "sat") drawSkull (c);
        else if (v.type == "comp") drawVise (c);
        else if (v.type == "gate") drawPort (c);
        else if (v.type == "pultec") drawEmber (c);
        else if (v.type == "eq") drawPrism (c);
        else { c.setColor (0xff8b9099); draw::text (c, "This plugin's window is next.", uiFont (S (14)), Font::kCenter, BX(), BY(), S (FW), S (FH)); }
    }

    // ---- interaction
    std::string ctlAt (Point q) const
    {
        if (v.type == "comp")
        {
            for (int i = 0; i < 5; ++i) { const auto mp = modelPos (i); const auto p = P (mp.x, mp.y); if (std::abs (q.x - p.x) < S (34) && std::abs (q.y - p.y) < S (11)) return "cm:" + std::to_string (i); }
            for (auto& ct : viseCtls()) { const auto p = P (ct.x, ct.y, 0.2f); if (std::hypot (q.x - p.x, q.y - p.y) < S (std::max (ct.r * 1.25f, 22.0f))) return ct.key; }
            return {};
        }
        if (v.type == "eq") return const_cast<PluginWindow*> (this)->prismAt (q);
        if (v.type == "pultec")
        {
            for (auto& ct : emberCtls()) { const auto p = P (ct.x, ct.y, 0.2f); if (std::hypot (q.x - p.x, q.y - p.y) < S (std::max (ct.r * 1.25f, 22.0f))) return ct.key; }
            return {};
        }
        if (v.type == "gate")
        {
            for (auto& ct : portCtls()) { const auto p = P (ct.x, ct.y, 0.2f); if (std::hypot (q.x - p.x, q.y - p.y) < S (std::max (ct.r * 1.25f, 22.0f))) return ct.key; }
            auto in = [&] (float x, float y, float w, float h) { const auto a = P (x, y, 0.06f); return q.x >= a.x && q.x < a.x + S (w) && q.y >= a.y && q.y < a.y + S (h); };
            if (in (783, 77, 150, 30)) return "key";
            if (in (790, 132, 66, 26)) return "listen";
            if (in (872, 132, 66, 26)) return "flip";
            return {};
        }
        if (v.type != "sat") return {};
        for (auto& ct : skullCtls())
        {
            if (ct.kind == 0 || ct.kind == 4 || ct.kind == 2)
            {
                const auto p = P (ct.x, ct.y, 0.2f);
                if (std::hypot (q.x - p.x, q.y - p.y) < S (std::max (ct.r * 1.25f, 22.0f))) return ct.key;
            }
            else
            {
                const float w = ct.kind == 3 ? 84 : 70, h = ct.kind == 3 ? 30 : 24;
                const auto p = P (ct.x - w / 2, ct.y - h / 2, 0.05f);
                if (q.x >= p.x && q.x < p.x + S (w) && q.y >= p.y && q.y < p.y + S (h)) return ct.key;
            }
        }
        return {};
    }
    void send (const std::string& a, double val) { if (studio.onMix) studio.onMix ("plug:" + a, val); }

    void mouseDown (const visage::MouseEvent& e) override
    {
        const auto q = e.position;
        if (q.y < S (38))
        {
            if (q.x > width() - S (44)) { send ("close", 0); return; }
            if (q.x < S (36)) { send ("byp", v.on ? 0 : 1); return; }
            moving = true; grab = q; return;
        }
        const auto key = ctlAt (q);
        if (key.empty()) return;
        if (v.type == "comp")
        {
            static const char* MK[5] = { "dist", "76", "fair", "160", "chan" };
            auto setModel = [&] (int i) { if (studio.onMix) studio.onMix (std::string ("plugstr:model:") + MK[i], 0); };
            if (key.rfind ("cm:", 0) == 0) { setModel (std::stoi (key.substr (3))); return; }
            if (key == "model")
            {
                int mi = 0; for (int i = 0; i < 5; ++i) if (cmodel() == MK[i]) mi = i;
                setModel ((mi + (e.isRightButton() ? 4 : 1)) % 5); return;
            }
            if (key == "ratio")
            {
                const auto R = ratios(); if (R.vals.empty()) return;
                const int n = (int) R.vals.size(), nx = (ratioIndex() + (e.isRightButton() ? n - 1 : 1)) % n;
                if (R.str) { if (studio.onMix) studio.onMix ("plugstr:" + cmodel() + ".ratio:" + R.vals[(size_t) nx], 0); }
                else send ("set:" + cmodel() + ".ratio", std::stod (R.vals[(size_t) nx]));
                return;
            }
        }
        if (v.type == "eq")
        {
            const int nb = nBands();
            auto plug = [&] (const std::string& a) { if (studio.onMix) studio.onMix (a, 0); };
            if (key.rfind ("band:", 0) == 0)
            {
                const int k = std::stoi (key.substr (5));
                if (k < nb) { selBand = k; redraw(); return; }
                if (k == nb)
                {
                    // a new band where it's most likely wanted: the ends cut, the middle bells
                    static const double F[6] = { 30, 100, 400, 2000, 6000, 16000 };
                    static const char* T[6] = { "lowcut", "bell", "bell", "bell", "highshelf", "highcut" };
                    selBand = k;
                    plug ("plugband:add:" + std::string (T[k]) + ":" + draw::fmt ("%.0f", F[k]));
                }
                return;
            }
            if (key == "phase") { plug (std::string ("plugstr:phase:") + (v.gets ("phase", "min") == "lin" ? "min" : "lin")); return; }
            if (selBand >= nb && key != "out") return;
            if (key.rfind ("shape:", 0) == 0)
            {
                static const char* SH[6] = { "bell", "lowshelf", "highshelf", "lowcut", "highcut", "notch" };
                plug ("plugstr:" + bandPath ("type") + ":" + SH[std::stoi (key.substr (6))]); return;
            }
            if (key == "bandon") { send ("set:" + bandPath ("on"), v.get (bandPath ("on"), 1) > 0.5 ? 0 : 1); return; }
            if (key == "solo") return;
            if (key == "delete") { plug ("plugband:del:" + std::to_string (selBand)); selBand = std::max (0, selBand - 1); return; }
            if (key == "dyn") { send ("set:" + bandPath ("dyn.on"), v.get (bandPath ("dyn.on"), 0) > 0.5 ? 0 : 1); return; }
            if (key.rfind ("node:", 0) == 0)
            {
                const int k = std::stoi (key.substr (5)); selBand = k;
                node = {}; node.band = k; node.x0 = q.x; node.y0 = q.y; node.f0 = node.f = v.get (bandPath ("f", k), 1000); node.g0 = node.g = v.get (bandPath ("g", k), 0);
                redraw(); return;
            }
        }
        if (v.type == "pultec")
        {
            if (key == "eqp" || key == "meq" || key == "tube") { send ("set:" + key, v.get (key, key == "meq" ? 0 : 1) > 0.5 ? 0 : 1); return; }
            if (key == "natural" || key == "linear") { if (studio.onMix) studio.onMix (std::string ("plugstr:phase:") + (key == "linear" ? "lin" : "min"), 0); return; }
            if (key == "lf" || key == "hf" || key == "hfa")
            {
                const auto& st = emberSteps (key); const int n = (int) st.vals.size();
                const int nx = (stepIndex (st, key == "lf" ? 60 : key == "hf" ? 10 : 20) + (e.isRightButton() ? n - 1 : 1)) % n;
                send ("set:" + key, st.vals[(size_t) nx]); return;
            }
        }
        if (v.type == "gate")
        {
            if (key == "listen" || key == "flip") { send ("set:" + key, v.get (key, 0) > 0.5 ? 0 : 1); return; }
            if (key == "ratio")
            {
                const int n = (int) portRatios().size(), nx = (portRatioIndex() + (e.isRightButton() ? n - 1 : 1)) % n;
                send ("set:ratio", portRatios()[(size_t) nx]); return;
            }
            if (key == "key")
            {
                const auto a = P (783, 77, 0.06f);
                if (studio.onMixMenu) studio.onMixMenu ("plugkey", a.x + positionInWindow().x, a.y + S (30) + positionInWindow().y, S (150), S (22));
                return;
            }
        }
        if (key.rfind ("m:", 0) == 0) { if (studio.onMix) studio.onMix ("plugstr:model:" + key.substr (2), 0); return; }
        if (key == "destroy") { if (model() == "decap") send ("set:decap.punish", v.get ("decap.punish", 0) > 0.5 ? 0 : 1); return; }
        if (key == "boom" || key == "sploosh") { if (model() == "decap") { const std::string p2 = key == "boom" ? "decap.thump" : "decap.steep"; send ("set:" + p2, v.get (p2, 0) > 0.5 ? 0 : 1); } return; }
        if (key == "auto") { send ("set:auto", v.get ("auto", 1) > 0.5 ? 0 : 1); return; }
        if (key == "style")
        {
            const auto st = styles(); if (st.empty()) return;
            const int nx = (styleIndex() + (e.isRightButton() ? (int) st.size() - 1 : 1)) % (int) st.size();
            if (studio.onMix) studio.onMix ("plugstr:" + stylePath() + ":" + st[(size_t) nx], 0);
            return;
        }
        if (! rangeOf (key).on) return;
        if (e.repeatClickCount() >= 2)
        {
            const auto R = rangeOf (key); send ("set:" + R.path, defOf (R.path)); return;
        }
        dragKey = key; dragY0 = q.y; dragV0 = posOf (key); liveV = dragV0;
    }
    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (moving)
        {
            const float nx = std::clamp (x() + e.position.x - grab.x, -width() + 120, studio.width() - 120);
            const float ny = std::clamp (y() + e.position.y - grab.y, 0.0f, studio.height() - 40);
            setBounds (nx, ny, width(), height());
            studio.redrawAll();
            return;
        }
        if (node.band >= 0)
        {
            // drag a node: across is frequency, up and down is gain (cuts and notches only move across)
            const float ppx = S (DW / 1.01f), ppy = S (DH * 0.42f * 0.972f / 24);
            node.f = std::clamp (node.f0 * std::pow (1000.0, (e.position.x - node.x0) / ppx), 20.0, 20000.0);
            const auto t = bandType (node.band);
            if (t != "lowcut" && t != "highcut" && t != "notch") node.g = std::clamp (node.g0 - (e.position.y - node.y0) / ppy, -24.0, 24.0);
            v.num[bandPath ("f", node.band)] = node.f; v.num[bandPath ("g", node.band)] = node.g;
            redraw();
            return;
        }
        if (dragKey.empty()) return;
        liveV = std::clamp (dragV0 + (dragY0 - e.position.y) / S (220), 0.0f, 1.0f);
        redraw();
    }
    void mouseUp (const visage::MouseEvent&) override
    {
        if (! dragKey.empty() && std::isfinite (liveV) && std::abs (liveV - dragV0) > 1e-4f)
        {
            const auto R = rangeOf (dragKey);
            double val = valueAt (dragKey, liveV);
            val = R.path == "mix" ? std::round (val * 100) / 100 : std::round (val * 10) / 10;
            if (R.path == "fair.tc") val = std::round (val);
            if (R.path == "scHpf" || R.path == "chan.release" || (R.path == "chan.attack" && val >= 10)) val = std::round (val);
            send ("set:" + R.path, val);
        }
        if (node.band >= 0)
        {
            if (std::abs (node.f - node.f0) > 0.01) send ("set:" + bandPath ("f", node.band), std::round (node.f * (node.f < 1000 ? 1 : 0.1)) / (node.f < 1000 ? 1 : 0.1));
            if (std::abs (node.g - node.g0) > 0.01) send ("set:" + bandPath ("g", node.band), std::round (node.g * 10) / 10);
            node.band = -1;
        }
        moving = false; dragKey.clear(); liveV = NAN;
    }

    void advance (double dt)
    {
        if (! isVisible()) return;
        if (v.type == "pultec" || v.type == "eq")
        {
            updateSpectrum (dt);
            if (v.type == "eq")
            {
                const float lv = studio.pluginLevel (v.who) * (float) std::pow (10.0, v.get ("out", 0) / 20);
                meterL = std::max (lv, meterL * (float) std::pow (0.02, dt));
                meterPk = std::max (lv, meterPk - (float) dt * 0.15f * meterPk);
            }
            if (v.type == "pultec")
            {
                const float tgt = v.on && v.get ("tube", 1) > 0.5 ? std::min (1.0f, 0.25f + 0.75f * (float) v.get ("drive", 1) / 10) : 0.0f;
                heatNow += (tgt - heatNow) * (float) std::min (1.0, dt * 4);
                // flicker: smoothed noise per valve while audio passes, settling when it's quiet
                const float lv = studio.pluginLevel (v.who);
                flickT += (float) dt;
                for (int i = 0; i < 3; ++i)
                {
                    const float n = std::sin (flickT * (5.1f + i * 1.7f) + i * 2.1f) * std::sin (flickT * (2.3f + i * 0.9f));
                    flick[i] += ((lv > 0.02f ? n : 0.0f) - flick[i]) * (float) std::min (1.0, dt * 10);
                }
            }
            redraw();
            return;
        }
        if (v.type == "gate")
        {
            const float tgt = openTarget();
            openNow += (tgt - openNow) * (float) std::min (1.0, dt * (tgt > openNow ? 25.0 : 8.0));
            redraw();
            return;
        }
        if (v.type == "comp")
        {
            // the needle and the ram: quick down, slower back, like a real meter
            const float tgt = grTarget();
            grNow += (tgt - grNow) * (float) std::min (1.0, dt * (tgt > grNow ? 30.0 : 6.0));
            const float on = grNow > 10 ? 1.0f : grNow > 7 ? (grNow - 7) / 3 : 0.0f;
            beaconA += (on - beaconA) * (float) std::min (1.0, dt * 8);
            sweep += (float) dt * 5.0f * beaconA;
            redraw();
            return;
        }
        if (v.type != "sat") return;
        const float d = posOf ("drive");
        const float lv = studio.pluginLevel (v.who);
        // a slow idle turn, faster with drive, and kicked along by the signal
        sawAngle += (float) dt * (v.on ? (0.4f + 6.0f * d * d + 10.0f * lv * d) : 0.0f);
        redraw();
    }
};

Studio::Studio()
    : header (std::make_unique<Header> (*this)), strips (std::make_unique<Strips> (*this)), bigFader (std::make_unique<BigFader> (*this)),
      screen (std::make_unique<Screen> (*this)), console (std::make_unique<Console> (*this)),
      rack (std::make_unique<Rack> (*this)), footer (std::make_unique<Footer> (*this)),
      historyPanel (std::make_unique<HistoryPanel> (*this)), pluginWin (std::make_unique<PluginWindow> (*this))
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
    addChild (bigFader.get(), false);
    bigFader->setOnTop (true);
    addChild (pluginWin.get(), false);
    pluginWin->setOnTop (true);
}

Studio::~Studio()
{
    for (visage::Frame* f : { (visage::Frame*) pluginWin.get(), (visage::Frame*) bigFader.get(), (visage::Frame*) historyPanel.get(), (visage::Frame*) footer.get(), (visage::Frame*) rack.get(),
                              (visage::Frame*) console.get(), (visage::Frame*) screen.get(), (visage::Frame*) strips.get(), (visage::Frame*) header.get() })
        removeChild (f);
}

float Studio::laneHeight() const
{
    const int n = std::max (1, (int) data.tracks.size());
    const float avail = screen->height() - screen->top() - 4;
    return std::max (76.0f, avail / (float) n);
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
    const float stripW = Strips::W;
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

void Studio::setTrackState (int i, bool solo, bool mute, float volumeDb, float pan, float panL, float panR)
{
    if (i < 0 || i >= (int) data.tracks.size()) return;
    auto& t = data.tracks[(size_t) i];
    if (t.solo == solo && t.mute == mute && t.volumeDb == volumeDb && t.pan == pan && t.panL == panL && t.panR == panR) return;
    t.solo = solo; t.mute = mute; t.volumeDb = volumeDb; t.pan = pan; t.panL = panL; t.panR = panR;
    if (bigFader->track == i) bigFader->redraw();
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
    const float abTarget = hdr.processed ? 1.0f : 0.0f;
    if (std::abs (abMix - abTarget) > 0.001f) { abMix += (abTarget - abMix) * 0.22f; if (std::abs (abMix - abTarget) < 0.01f) abMix = abTarget; screen->redraw(); }
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
    strips->meters.resize (data.tracks.size(), 0.0f);
    strips->chanMeters.resize (data.tracks.size());
    bool stripsLive = false;
    for (size_t i = 0; i < data.tracks.size(); ++i)
    {
        const auto& tr = data.tracks[i];
        auto& cm = strips->chanMeters[i];
        const int nch = tr.wave ? tr.wave->channels() : 1;
        cm.resize ((size_t) nch, 0.0f);
        const float g = tr.mute ? 0.0f : std::pow (10.0f, tr.volumeDb / 20.0f);
        float any = 0;
        for (int k = 0; k < nch; ++k)
        {
            float l = 0;
            if (playing && tr.wave)
            {
                const double sr = tr.wave->sampleRate;
                const auto r = tr.wave->range (k, (long) ((playhead - 0.05) * sr), (long) ((playhead + 0.01) * sr), abMix);
                l = std::max (std::abs (r.first), std::abs (r.second)) * g;
            }
            cm[(size_t) k] = std::max (l, cm[(size_t) k] * 0.86f);
            any = std::max (any, cm[(size_t) k]);
        }
        strips->meters[i] = any;
        stripsLive = stripsLive || any > 0.002f;
    }
    if (stripsLive || playing) strips->redraw();
    console->peaks.resize (data.tracks.size(), 0.0f);
    // the tubes read on the fader's scale; the peak bead holds, then sinks slowly
    auto tubeLevel = [] (float amp) { return amp <= 0.00001f ? 0.0f : 1.0f - Console::dbF (20.0f * std::log10 (amp)); };
    float busAmp = 0.0f;
    for (size_t i = 0; i < data.tracks.size(); ++i)
    {
        const float l = levelAt (data.tracks[i]);
        console->meters[i] = std::max (tubeLevel (l), console->meters[i] * 0.9f);
        console->peaks[i] = std::max (console->meters[i], console->peaks[i] - 0.004f);
        if (data.tracks[i].isRoom) room = std::max (room, l);
        master = std::max (master, l);
        busAmp += l;
    }
    {
        const float bl = tubeLevel (std::min (2.0f, busAmp) * (data.mix.busMute ? 0.0f : std::pow (10.0f, data.mix.busFader / 20.0f)));
        console->busMeter = std::max (bl, console->busMeter * 0.9f);
        console->busPeak = std::max (console->busMeter, console->busPeak - 0.004f);
    }
    rack->roomMeter = std::max (room, rack->roomMeter * 0.9f);
    rack->masterMeter = std::max (master, rack->masterMeter * 0.9f);
    rack->redraw();
    if (currentStep == Step::mix) console->redraw();
    pluginWin->advance (lastTick > 0 ? std::min (0.1, t - lastTick) : 0.0);
    lastTick = t;
}

void Studio::setPlugin (PluginView p)
{
    const bool wasOpen = pluginWin->isVisible();
    const float oldW = pluginWin->FW;
    pluginWin->v = std::move (p);
    if (! pluginWin->v.open) { pluginWin->setVisible (false); redrawAll(); return; }
    pluginWin->FW = pluginWin->v.type == "pultec" ? 1060.0f : 980.0f;
    const float k = std::min (1.0f, (width() - 40) / pluginWin->WW());
    pluginWin->k = k;
    if (! wasOpen || oldW != pluginWin->FW)
        pluginWin->setBounds ((width() - pluginWin->WW() * k) / 2, std::max (20.0f, (height() - PluginWindow::WH * k) / 2), pluginWin->WW() * k, PluginWindow::WH * k);
    pluginWin->setVisible (true);
    pluginWin->redraw();
}

float Studio::pluginLevel (int who) const
{
    // the level the plugin window's meters show: its channel's (or the bus's) tube level, 0..1
    if (who < 0) return console->busMeter;
    return who < (int) console->meters.size() ? console->meters[(size_t) who] : 0.0f;
}

void Studio::showBigFader (int track)
{
    bigFader->track = track;
    if (track < 0) { bigFader->setVisible (false); return; }
    const float lh = laneHeight();
    const float rowMid = strips->y() + screenContentTop() + (track + 0.5f) * lh - view.scrollY;
    const float h = BigFader::H + 40, w = BigFader::W + 16;
    const float y = std::clamp (rowMid - h / 2, header->bottom(), height() - h - 4);
    bigFader->setBounds (strips->right() - 4, y, w, h);
    bigFader->setVisible (true);
    bigFader->redraw();
}

void Studio::flashPad (int v) { if (v >= 0 && v < 8) { rack->padFlash[v] = now; rack->redraw(); } }

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

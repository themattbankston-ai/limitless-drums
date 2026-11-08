#include "NewSession.h"
#include "Draw.h"

namespace ld::ui
{
namespace
{
struct DrumDef { const char* key; const char* name; unsigned colour; const char* path; int w; };
// Line icons on a 40 x 34 box (Kick: 34). Drawn twice, wide and faint then sharp, for the neon glow.
const DrumDef drums[9] = {
    { "kick",   "Kick",     0xffffb23e, "M17 4a13 13 0 1 0 0.01 0M17 11a6 6 0 1 0 0.01 0", 34 },
    { "snare",  "Snare",    0xffff3da8, "M4 10a16 5 0 1 0 32 0a16 5 0 1 0 -32 0M4 10v13c0 3 7 5 16 5s16-2 16-5V10", 40 },
    { "tom",    "Tom",      0xff9b6bff, "M5 9a13 4.5 0 1 0 26 0a13 4.5 0 1 0 -26 0M5 9v15c0 2.5 6 4.5 13 4.5s13-2 13-4.5V9", 36 },
    { "hat",    "Hi-hat",   0xff3dffd2, "M4 12h32M6 17h28M20 17v15", 40 },
    { "ride",   "Ride",     0xff2ee6ff, "M3 15c6-5 28-5 34 0M20 15v17M20 9.5a2.5 2.5 0 1 0 0.01 0", 40 },
    { "crash",  "Crash",    0xff7cff9b, "M5 16L35 10M20 13v19", 40 },
    { "china",  "China",    0xffff6a3d, "M4 11c4 6 28 6 32 0M8 13l-3-4M32 13l3-4M20 15v17", 40 },
    { "splash", "Splash",   0xffff7ad9, "M10 14c4-3 16-3 20 0M20 14v18", 40 },
    { "kit",    "Song", 0xffffd27a, "M20 14a8 8 0 1 0 0.01 0M3 14a6 2.5 0 1 0 12 0a6 2.5 0 1 0 -12 0M25 14a6 2.5 0 1 0 12 0a6 2.5 0 1 0 -12 0M3 7l10-2M27 5l10 2", 40 },
};

std::string hex (unsigned c)
{
    char b[8]; std::snprintf (b, sizeof (b), "#%06x", c & 0xffffff);
    return b;
}

std::string drumSvg (const DrumDef& d, bool on)
{
    const auto col = hex (d.colour);
    const std::string w = std::to_string (d.w);
    std::string body;
    if (on) body += "<path d='" + std::string (d.path) + "' fill='none' stroke='" + col + "' stroke-opacity='0.3' stroke-width='6' stroke-linecap='round' stroke-linejoin='round'/>";
    body += "<path d='" + std::string (d.path) + "' fill='none' stroke='" + col + "' stroke-opacity='" + (on ? "1" : "0.72") + "' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'/>";
    return "<svg xmlns='http://www.w3.org/2000/svg' width='" + w + "' height='34' viewBox='-2 -2 " + std::to_string (d.w + 4) + " 38'>" + body + "</svg>";
}

bool sameText (const std::string& a, const std::string& b)
{
    return a.size() == b.size() && std::equal (a.begin(), a.end(), b.begin(), [] (char x, char y) { return std::tolower ((unsigned char) x) == std::tolower ((unsigned char) y); });
}

const char* kTimes = "\xc3\x97";
const char* kDots = "\xe2\x80\xa6";
} // namespace

NewSession::NewSession()
{
    pal.setColor (visage::TextEditor::TextEditorBackground, Color (0));
    pal.setColor (visage::TextEditor::TextEditorText, Color (0xfff2efe8));
    pal.setColor (visage::TextEditor::TextEditorDefaultText, Color (0xff5f6670));
    pal.setColor (visage::TextEditor::TextEditorCaret, Color (neon::cyan));
    pal.setColor (visage::TextEditor::TextEditorSelection, Color (0x552ee6ff));
    setPalette (&pal);
    for (auto* ed : { &nameEd, &articEd })
    {
        ed->setTextFieldEntry();
        ed->setMargin (0, 0);
        ed->setMaxCharacters (80);
        addChild (ed);
        ed->onEnterKey() += [this] { if (canCreate() && onCreate) onCreate(); };
        ed->onEscapeKey() += [this] { if (onCancel) onCancel(); };
        ed->onFocus = [this] { redraw(); };
    }
    nameEd.setDefaultText ("Name this session");
    articEd.setDefaultText ("Articulation");
    nameEd.onTextChange() += [this] { st.name = nameEd.text().toUtf8(); redraw(); };
    articEd.onTextChange() += [this] { st.artic = articEd.text().toUtf8(); redraw(); };
    setVisible (false);
}

NewSession::~NewSession() = default;

void NewSession::open (NewSessionState s)
{
    st = std::move (s);
    hover.clear();
    dropHover = false;
    setVisible (true);
    refresh();
    nameEd.requestKeyboardFocus();
    nameEd.selectAll();
}

void NewSession::close()
{
    setVisible (false);
}

void NewSession::refresh()
{
    if (nameEd.text().toUtf8() != st.name) nameEd.setText (st.name);
    if (articEd.text().toUtf8() != st.artic) articEd.setText (st.artic);
    resized();
    redraw();
}

bool NewSession::canCreate() const { return ! st.folder.empty(); }

std::vector<std::string> NewSession::chipsFor() const
{
    const auto& d = st.drum;
    if (d == "kick") return { "Center", "Off-center", "Muted", "Soft beater", "Open" };
    if (d == "tom") return { "Center", "Rimshot", "Edge", "Muted", "Flam" };
    if (d == "hat") return { "Closed", "Tight", "Half open", "Open", "Pedal", "Splash", "Edge" };
    if (d == "ride") return { "Tip", "Bell", "Edge", "Crash", "Choke" };
    if (d == "crash" || d == "china" || d == "splash") return { "Edge", "Crash", "Bow", "Bell", "Choke" };
    if (d == "kit") return { "Full kit", "Grooves", "Fills" };
    return { "Center", "Rimshot", "Sidestick", "Edge", "Rim click", "Wires off", "Flam", "Roll" };
}

void NewSession::resized()
{
    k = std::min ({ 1.2f, (width() - 32) / DW, (height() - 32) / DH });
    k = std::max (k, 0.5f);
    ox = std::round ((width() - DW * k) * 0.5f);
    oy = std::round ((height() - DH * k) * 0.5f);
    nameEd.setFont (uiFont (X (17)));
    articEd.setFont (uiFont (X (15)));
    nameEd.setBounds (PX (56), PY (124), X (370), X (48));
    articEd.setBounds (PX (56), PY (454), X (166), X (44));
}

void NewSession::icon (Canvas& c, const std::string& key, const std::string& svgText, float x, float y, float w, float h)
{
    const std::string id = key + "@" + std::to_string ((int) (w * 10));
    auto it = svgs.find (id);
    if (it == svgs.end())
    {
        visage::Svg svg (reinterpret_cast<const unsigned char*> (svgText.data()), (int) svgText.size());
        svg.setDimensions ((int) std::round (w), (int) std::round (h), 1.0f);
        it = svgs.emplace (id, svg).first;
    }
    c.svg (it->second, x, y, w, h);
}

std::string NewSession::hitAt (float x, float y) const
{
    const float dx = (x - ox) / k, dy = (y - oy) / k;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (dx >= it->x && dx < it->x + it->w && dy >= it->y && dy < it->y + it->h) return it->id;
    if (dx >= 0 && dx < DW && dy >= 0 && dy < DH) return "panel";
    return {};
}

void NewSession::draw (Canvas& c)
{
    hits.clear();
    auto hit = [this] (const std::string& id, float x, float y, float w, float h) { hits.push_back ({ id, x, y, w, h }); };
    auto label = [&] (const std::string& s, float x, float y) { draw::spaced (c, s, boldFont (X (10.5f)), PX (x), PY (y), X (14), X (2.1f)); };

    // the studio, dimmed
    c.setColor (0xc4000000);
    c.fill (0, 0, width(), height());
    c.setColor (Brush::radial (Color (0x00000000), Color (0x88000000), Point (width() * 0.5f, height() * 0.52f), std::max (width(), height()) * 0.7f));
    c.fill (0, 0, width(), height());

    // the plate
    c.setColor (0xcc000000);
    c.roundedRectangleShadow (PX (0), PY (24), X (DW), X (DH), X (14), X (60));
    draw::plate (c, PX (0), PY (0), X (DW), X (DH), 0xff1b1c20, 0xff0a0a0c, 0xff000000, X (14), false);
    for (auto [sx, sy] : { std::pair { 15.5f, 15.5f }, { DW - 15.5f, 15.5f }, { 15.5f, DH - 15.5f }, { DW - 15.5f, DH - 15.5f } })
        draw::screw (c, PX (sx), PY (sy), X (5.5f));

    c.setColor (0xfff2efe8);
    draw::spaced (c, "NEW SESSION", boldFont (X (22)), PX (40), PY (28), X (28), X (6.2f));
    c.setColor (0xff9aa0a8);
    draw::text (c, "What you're working on. Then add the audio.", bodyFont (X (13)), Font::kLeft, PX (42), PY (62), X (500), X (18));

    // close
    {
        const bool h = hover == "cancel-x";
        c.setColor (0xff000000); c.circle (PX (835), PY (25), X (38));
        c.setColor (Brush::radial (Color (h ? 0xff3a3d45 : 0xff30333a), Color (0xff121316), Point (PX (848), PY (36)), X (22)));
        c.circle (PX (836), PY (26), X (36));
        c.setColor (h ? 0xfff2efe8 : 0xff8b9099);
        c.segment (PX (848), PY (38), PX (860), PY (50), X (1.8f), true);
        c.segment (PX (860), PY (38), PX (848), PY (50), X (1.8f), true);
        hit ("cancel-x", 836, 26, 36, 36);
    }

    c.setColor (0xff6a707a);
    label ("NAME", 42, 102);
    label ("LIBRARY", 462, 102);

    auto pocket = [&] (float x, float y, float w, float h, bool focus, bool hov)
    {
        draw::glass (c, PX (x), PY (y), X (w), X (h), X (10));
        if (focus)
        {
            c.setColor (Color (neon::cyan).withAlpha (0.16f));
            c.roundedRectangleShadow (PX (x), PY (y), X (w), X (h), X (10), X (16));
            c.setColor (Color (neon::cyan).withAlpha (0.75f));
            c.roundedRectangleBorder (PX (x), PY (y), X (w), X (h), X (10), X (1.5f));
        }
        else if (hov)
        {
            c.setColor (0x22ffffff);
            c.roundedRectangleBorder (PX (x), PY (y), X (w), X (h), X (10), X (1));
        }
    };
    auto chevron = [&] (float x, float y)
    {
        c.setColor (0xff6a707a);
        c.triangle (PX (x), PY (y), PX (x + 10), PY (y), PX (x + 5), PY (y + 6));
    };

    // name
    pocket (40, 124, 400, 48, nameEd.hasKeyboardFocus(), false);
    hit ("name", 40, 124, 400, 48);

    // library
    pocket (460, 124, 400, 48, false, hover == "library");
    draw::led (c, PX (484), PY (148), st.libraryColour, true, X (4));
    c.setColor (0xfff2efe8);
    draw::text (c, st.library, uiFont (X (15)), Font::kLeft, PX (500), PY (124), X (320), X (48));
    chevron (838, 145);
    hit ("library", 460, 124, 400, 48);

    // start from
    c.setColor (0xff6a707a);
    const float sfEnd = draw::spaced (c, "START FROM", boldFont (X (10.5f)), PX (42), PY (194), X (14), X (2.1f));
    c.setColor (0xff9aa0a8);
    draw::text (c, "the levels and tone of a session you already made, or the standard", bodyFont (X (12)), Font::kLeft, sfEnd + X (12), PY (194), X (520), X (14));
    pocket (40, 216, 400, 48, false, hover == "start");
    c.setColor (0xffd6d3cc);
    draw::text (c, st.startFrom, uiFont (X (15)), Font::kLeft, PX (56), PY (216), X (360), X (48));
    chevron (418, 237);
    hit ("start", 40, 216, 400, 48);

    // new library
    {
        const bool h = hover == "newlib";
        c.setColor (0x99000000); c.roundedRectangleShadow (PX (460), PY (222), X (200), X (44), X (10), X (10));
        c.setColor (0xff000000); c.roundedRectangle (PX (459), PY (217), X (202), X (46), X (11));
        c.setColor (Brush::linear (Color (h ? 0xff2c2f36 : 0xff24262c), Color (0xff141519), Point (0, PY (218)), Point (0, PY (262))));
        c.roundedRectangle (PX (460), PY (218), X (200), X (44), X (10));
        c.setColor (0x1affffff); c.fill (PX (470), PY (218.5f), X (180), X (1));
        c.setColor (h ? 0xfff2efe8 : 0xffc9ccd2);
        draw::text (c, "+", boldFont (X (15)), Font::kLeft, PX (506), PY (218), X (20), X (44));
        draw::spaced (c, "NEW LIBRARY", boldFont (X (12)), PX (526), PY (233), X (14), X (1.7f));
        hit ("newlib", 460, 218, 200, 44);
    }

    // drum
    c.setColor (0xff6a707a);
    label ("DRUM", 42, 292);
    for (int i = 0; i < 9; ++i)
    {
        const auto& d = drums[i];
        const float x = 40 + i * 92.0f, y = 316, w = 84, h = 92;
        const bool on = st.drum == d.key, hv = hover == std::string ("drum:") + d.key;
        draw::glass (c, PX (x), PY (y), X (w), X (h), X (10));
        if (on)
        {
            c.setColor (Brush::radial (Color (d.colour).withAlpha (0.42f), Color (d.colour).withAlpha (0.0f), Point (PX (x + w / 2), PY (y + h * 1.2f)), X (80)));
            c.roundedRectangle (PX (x), PY (y), X (w), X (h), X (10));
            c.setColor (Color (d.colour).withAlpha (0.22f));
            c.roundedRectangleShadow (PX (x), PY (y), X (w), X (h), X (10), X (18));
            c.setColor (Color (d.colour).withAlpha (0.8f));
            c.roundedRectangleBorder (PX (x), PY (y), X (w), X (h), X (10), X (1.5f));
            c.setColor (d.colour);
            c.roundedRectangle (PX (x + 22), PY (y), X (w - 44), X (2), X (1));
        }
        else if (hv)
        {
            c.setColor (Color (d.colour).withAlpha (0.35f));
            c.roundedRectangleBorder (PX (x), PY (y), X (w), X (h), X (10), X (1));
        }
        const float iw = (d.w + 4) * 1.0f, ih = 38;
        icon (c, std::string (d.key) + (on ? "+" : "-"), drumSvg (d, on), PX (x + (w - iw) / 2), PY (y + 14), X (iw), X (ih));
        c.setColor (on ? Color (d.colour) : Color (d.colour).interpolateWith (Color (0xff80858e), 0.35f).withAlpha (0.9f));
        draw::text (c, d.name, boldFont (X (11)), Font::kCenter, PX (x), PY (y + 60), X (w), X (16));
        hit (std::string ("drum:") + d.key, x, y, w, h);
    }

    // articulation
    c.setColor (0xff6a707a);
    label ("ARTICULATION", 42, 432);
    pocket (40, 454, 190, 44, articEd.hasKeyboardFocus(), false);
    hit ("artic", 40, 454, 190, 44);
    {
        float x = 244;
        auto chips = chipsFor();
        chips.push_back ("+");
        for (auto& ch : chips)
        {
            const auto f = boldFont (X (12));
            const float w = textWidth (f, ch) / k + 24;
            if (x + w > 862) break;
            const bool on = ch != "+" && sameText (ch, st.artic);
            const bool hv = hover == "chip:" + ch;
            const float y = 461, h = 30;
            if (on)
            {
                c.setColor (Brush::radial (Color (neon::magenta).withAlpha (0.5f), Color (0xff120a10), Point (PX (x + w / 2), PY (y + h * 1.4f)), X (w * 0.75f)));
                c.roundedRectangle (PX (x), PY (y), X (w), X (h), X (15));
                c.setColor (Color (neon::magenta).withAlpha (0.2f));
                c.roundedRectangleShadow (PX (x), PY (y), X (w), X (h), X (15), X (12));
                c.setColor (Color (neon::magenta).withAlpha (0.8f));
                c.roundedRectangleBorder (PX (x), PY (y), X (w), X (h), X (15), X (1.5f));
            }
            else
            {
                c.setColor (0xff0b0b0e);
                c.roundedRectangle (PX (x), PY (y), X (w), X (h), X (15));
                c.setColor (hv ? 0x30ffffff : 0x10ffffff);
                c.roundedRectangleBorder (PX (x), PY (y), X (w), X (h), X (15), X (1));
            }
            c.setColor (on ? 0xffffffff : hv ? 0xffc9ccd2 : 0xff7d838c);
            draw::text (c, ch, f, Font::kCenter, PX (x), PY (y), X (w), X (h));
            hit ("chip:" + ch, x, y, w, h);
            x += w + 7;
        }
    }

    // counts
    c.setColor (0xff6a707a);
    {
        const float e = draw::spaced (c, "VELOCITIES ", boldFont (X (10.5f)), PX (42), PY (548), X (14), X (2.1f));
        const float e2 = draw::spaced (c, kTimes, boldFont (X (10.5f)), e, PY (548), X (14), X (2.1f));
        draw::spaced (c, " ROUND ROBINS", boldFont (X (10.5f)), e2, PY (548), X (14), X (2.1f));
    }
    auto stepper = [&] (const char* id, float x, int v)
    {
        draw::glass (c, PX (x), PY (534), X (136), X (44), X (10));
        for (int s = 0; s < 2; ++s)
        {
            const std::string bid = std::string (id) + (s ? "+" : "-");
            c.setColor (hover == bid ? 0xfff2efe8 : 0xff8b9099);
            const float bx = s ? x + 96 : x;
            c.fill (PX (bx + 14), PY (555), X (12), X (2));
            if (s) c.fill (PX (bx + 19), PY (550), X (2), X (12));
            hit (bid, bx, 534, 40, 44);
        }
        c.setColor (Color (neon::amber).withAlpha (0.25f));
        c.roundedRectangleShadow (PX (x + 54), PY (546), X (28), X (20), X (6), X (12));
        c.setColor (neon::amber);
        draw::text (c, std::to_string (v), monoBold (X (24)), Font::kCenter, PX (x + 40), PY (534), X (56), X (44));
    };
    stepper ("vel", 300, st.vel);
    c.setColor (0xff6a707a);
    draw::text (c, kTimes, uiFont (X (18)), Font::kCenter, PX (436), PY (534), X (34), X (44));
    stepper ("rr", 470, st.rr);
    {
        const bool names = st.countsFromNames;
        const unsigned col = names ? 0xff3dffd2 : st.folder.empty() ? 0xff5f6670 : neon::amber;
        draw::led (c, PX (630), PY (556), col, names || ! st.folder.empty(), X (3));
        c.setColor (col);
        draw::spaced (c, names ? "READ FROM THE FILE NAMES" : st.folder.empty() ? "READ FROM THE FILE NAMES WHEN ADDED" : "SET BY HAND",
                      bodyFont (X (11)), PX (642), PY (549), X (14), X (0.8f));
    }

    // the audio
    {
        const float x = 40, y = 600, w = 820, h = 110;
        draw::glass (c, PX (x), PY (y), X (w), X (h), X (12));
        const bool live = dropHover || hover == "drop";
        c.setColor (Color (neon::cyan).withAlpha (dropHover ? 0.75f : live ? 0.42f : 0.26f));
        // dashed edge
        const float ix = x + 8, iy = y + 8, iw = w - 16, ih = h - 16;
        for (float t = ix + 10; t < ix + iw - 10; t += 9) { c.fill (PX (t), PY (iy), X (5), X (1.4f)); c.fill (PX (t), PY (iy + ih - 1.4f), X (5), X (1.4f)); }
        for (float t = iy + 10; t < iy + ih - 10; t += 9) { c.fill (PX (ix), PY (t), X (1.4f), X (5)); c.fill (PX (ix + iw - 1.4f), PY (t), X (1.4f), X (5)); }
        if (dropHover)
        {
            c.setColor (Color (neon::cyan).withAlpha (0.12f));
            c.roundedRectangle (PX (ix), PY (iy), X (iw), X (ih), X (9));
        }
        // a flowing wave along the bottom, cyan to violet to magenta
        {
            const int n = 120;
            float px0 = 0, py0 = 0;
            for (int i = 0; i <= n; ++i)
            {
                const float u = (float) i / n;
                const float wx = ix + u * iw;
                const float wy = iy + ih - 18 + std::sin (u * 9.4f + 0.6f) * 5.0f * std::sin (u * 3.14159f) + std::sin (u * 23.0f) * 1.2f;
                if (i > 0)
                {
                    const float fade = std::min (1.0f, std::min (u, 1 - u) * 5.0f);
                    const auto col = u < 0.5f ? Color (neon::cyan).interpolateWith (Color (0xff9b6bff), u * 2) : Color (0xff9b6bff).interpolateWith (Color (neon::magenta), u * 2 - 1);
                    c.setColor (col.withAlpha (0.28f * fade));
                    c.segment (PX (px0), PY (py0), PX (wx), PY (wy), X (5), true);
                    c.setColor (col.withAlpha (fade));
                    c.segment (PX (px0), PY (py0), PX (wx), PY (wy), X (1.8f), true);
                }
                px0 = wx; py0 = wy;
            }
        }
        if (st.folder.empty())
        {
            c.setColor (0xfff2efe8);
            draw::text (c, dropHover ? "Let go to add it" : "Drop a folder of mics here", uiFont (X (17)), Font::kCenter, PX (x), PY (y + 22), X (w), X (26));
            const auto f = bodyFont (X (12.5f));
            const std::string a = "or ", b = std::string ("Browse") + kDots, rest = "  \xc2\xb7  one folder per articulation, or a parent folder to pick from";
            const float tw = (textWidth (f, a) + textWidth (boldFont (X (12.5f)), b) + textWidth (f, rest));
            float tx = PX (x) + (X (w) - tw) / 2;
            c.setColor (0xff9aa0a8); draw::text (c, a, f, Font::kLeft, tx, PY (y + 50), X (40), X (18)); tx += textWidth (f, a);
            c.setColor (neon::cyan); draw::text (c, b, boldFont (X (12.5f)), Font::kLeft, tx, PY (y + 50), X (100), X (18)); tx += textWidth (boldFont (X (12.5f)), b);
            c.setColor (0xff9aa0a8); draw::text (c, rest, f, Font::kLeft, tx, PY (y + 50), X (600), X (18));
        }
        else
        {
            draw::led (c, PX (x + 34), PY (y + 40), 0xff3dffd2, true, X (4));
            c.setColor (0xfff2efe8);
            draw::text (c, st.folder, uiFont (X (17)), Font::kLeft, PX (x + 50), PY (y + 26), X (520), X (28));
            c.setColor (0xff9aa0a8);
            draw::text (c, st.folderDetail, bodyFont (X (12.5f)), Font::kLeft, PX (x + 50), PY (y + 52), X (520), X (18));
            c.setColor (neon::cyan);
            draw::text (c, std::string ("Change") + kDots, boldFont (X (12.5f)), Font::kRight, PX (x + w - 230), PY (y + 26), X (200), X (28));
        }
        hit ("drop", x, y, w, h);
    }

    // footer
    c.setColor (0xff6a707a);
    draw::text (c, "Velocities, round robins and mic names are read", bodyFont (X (12)), Font::kLeft, PX (42), PY (727), X (360), X (16));
    draw::text (c, "from the files; you can change them after.", bodyFont (X (12)), Font::kLeft, PX (42), PY (744), X (360), X (16));
    {
        const bool h = hover == "cancel";
        c.setColor (0x99000000); c.roundedRectangleShadow (PX (484), PY (722), X (130), X (52), X (12), X (10));
        c.setColor (0xff000000); c.roundedRectangle (PX (483), PY (717), X (132), X (54), X (13));
        c.setColor (Brush::linear (Color (h ? 0xff2c2f36 : 0xff24262c), Color (0xff141519), Point (0, PY (718)), Point (0, PY (770))));
        c.roundedRectangle (PX (484), PY (718), X (130), X (52), X (12));
        c.setColor (0x1affffff); c.fill (PX (494), PY (718.5f), X (110), X (1));
        c.setColor (h ? 0xfff2efe8 : 0xffc9ccd2);
        draw::spaced (c, "CANCEL", boldFont (X (12)), PX (515), PY (737), X (14), X (2));
        hit ("cancel", 484, 718, 130, 52);
    }
    {
        const bool ok = canCreate(), h = ok && hover == "create";
        const unsigned teal = 0xff3dffd2;
        if (ok)
        {
            c.setColor (Color (teal).withAlpha (h ? 0.45f : 0.3f));
            c.roundedRectangleShadow (PX (630), PY (718), X (230), X (52), X (12), X (26));
        }
        c.setColor (0xff000000); c.roundedRectangle (PX (629), PY (717), X (232), X (54), X (13));
        c.setColor (ok ? Brush::radial (Color (teal).withAlpha (h ? 0.7f : 0.55f), Color (0xff0a1412), Point (PX (745), PY (788)), X (150))
                       : Brush::solid (Color (0xff15171a)));
        c.roundedRectangle (PX (630), PY (718), X (230), X (52), X (12));
        c.setColor (ok ? Color (teal).withAlpha (0.85f) : Color (0x22ffffff));
        c.roundedRectangleBorder (PX (630), PY (718), X (230), X (52), X (12), X (1.5f));
        c.setColor (ok ? 0xffeafff9 : 0xff4d535c);
        draw::spaced (c, "CREATE SESSION", boldFont (X (13)), PX (676), PY (736), X (16), X (2.6f));
        hit ("create", 630, 718, 230, 52);
    }
}

void NewSession::mouseMove (const visage::MouseEvent& e)
{
    const auto h = hitAt (e.position.x, e.position.y);
    if (h != hover) { hover = h; redraw(); }
}

void NewSession::mouseExit (const visage::MouseEvent&) { if (! hover.empty()) { hover.clear(); redraw(); } }

void NewSession::mouseDown (const visage::MouseEvent& e)
{
    const auto h = hitAt (e.position.x, e.position.y);
    auto box = [this] (float x, float y, float w, float h, auto& cb) { if (cb) cb (PX (x), PY (y), X (w), X (h)); };
    if (h.empty()) return;   // outside the panel: nothing (a stray click shouldn't lose what you typed)
    if (h == "name") { nameEd.requestKeyboardFocus(); return; }
    if (h == "artic") { articEd.requestKeyboardFocus(); return; }
    requestKeyboardFocus();
    if (h == "cancel" || h == "cancel-x") { if (onCancel) onCancel(); return; }
    if (h == "create") { if (canCreate() && onCreate) onCreate(); return; }
    if (h == "library") { box (460, 124, 400, 48, onPickLibrary); return; }
    if (h == "start") { box (40, 216, 400, 48, onPickStart); return; }
    if (h == "newlib") { if (onNewLibrary) onNewLibrary(); return; }
    if (h == "drop") { if (onBrowse) onBrowse(); return; }
    if (h.rfind ("drum:", 0) == 0)
    {
        const auto d = h.substr (5);
        if (d != st.drum)
        {
            const auto old = chipsFor();
            const bool stock = std::find (old.begin(), old.end(), st.artic) != old.end() || st.artic.empty();
            st.drum = d;
            if (stock) st.artic = chipsFor().front();   // a typed articulation stays
            refresh();
        }
        return;
    }
    if (h.rfind ("chip:", 0) == 0)
    {
        const auto ch = h.substr (5);
        if (ch == "+") { articEd.setText (""); st.artic.clear(); articEd.requestKeyboardFocus(); redraw(); return; }
        st.artic = ch;
        refresh();
        return;
    }
    auto bump = [this] (int& v, int d) { v = std::clamp (v + d, 1, 16); st.countsFromNames = false; redraw(); };
    if (h == "vel-") bump (st.vel, -1);
    else if (h == "vel+") bump (st.vel, 1);
    else if (h == "rr-") bump (st.rr, -1);
    else if (h == "rr+") bump (st.rr, 1);
    redraw();
}

bool NewSession::keyPress (const visage::KeyEvent& e)
{
    if (e.keyCode() == visage::KeyCode::Escape) { if (onCancel) onCancel(); return true; }
    if (e.keyCode() == visage::KeyCode::Return) { if (canCreate() && onCreate) onCreate(); return true; }
    if (e.keyCode() == visage::KeyCode::Tab)
    {
        if (nameEd.hasKeyboardFocus()) articEd.requestKeyboardFocus(); else nameEd.requestKeyboardFocus();
        redraw();
        return true;
    }
    return true;   // while the window is up, nothing reaches the studio behind it
}

void NewSession::dropFiles (const std::vector<std::string>& paths)
{
    dropHover = false;
    redraw();
    if (onFilesDropped) onFilesDropped (paths);
}
} // namespace ld::ui

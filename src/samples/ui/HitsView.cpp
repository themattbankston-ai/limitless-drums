#include "HitsView.h"
#include "HitsLogic.h"
#include <tuple>
#include "Style.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace ld::samples
{
using namespace ld::ui;
using visage::Canvas;

using namespace style;

HitsView::HitsView (SampleModel& m) : model (m)
{
    setAcceptsKeystrokes (true);
}

void HitsView::takeChanged (bool refit)
{
    if (refit) { selHit = -1; needsFit = true; }
    redraw();
}

void HitsView::tick()
{
    if (model.isPlaying() && followOn)
    {
        const double p = model.position();
        if (p < scroll || p > timeAt (laneX() + laneW()) - 0.2) scroll = std::max (0.0, p - 0.1 * laneW() / pps);
    }
    // A then B: each pass of the loop switches
    if (ab == 2 && model.isPlaying())
    {
        const double p = model.position();
        if (p + 0.05 < lastPos) model.setProcessed (! model.processed());
        lastPos = p;
    }
    // redraw only while something moves (and once more after, so the meters fall to rest): idle costs nothing
    const bool active = model.isPlaying() || model.isRendering() || model.isPrepping() || drag != Drag::none;
    if (active || lastActive) redraw();
    lastActive = active;
}

float HitsView::trackH (int t) const
{
    static const float mono[4] = { 40, 58, 84, 120 }, st[4] = { 58, 84, 120, 170 };
    const bool stereo = t < (int) model.tracks().size() && model.tracks()[(size_t) t].channels > 1;
    return (stereo ? st : mono)[heightIdx];
}

float HitsView::trackY (int t) const
{
    float y = laneTop();
    for (int i = 0; i < t; ++i) y += trackH (i);
    return y;
}

void HitsView::fitAll()
{
    const double len = std::max (1.0, model.length());
    pps = laneW() / len;
    scroll = 0;
}

void HitsView::zoomBy (double f)
{
    const double centre = selB > selA ? (selA + selB) / 2 : timeAt (laneX() + laneW() / 2);
    pps = std::clamp (pps * f, 2.0, 200000.0);
    scroll = std::max (0.0, centre - (laneW() / 2) / pps);
}

void HitsView::selectHit (int i, bool play, bool andRedraw)
{
    const auto& H = model.hits();
    if (H.empty()) return;
    selHit = std::clamp (i, 0, (int) H.size() - 1);
    std::tie (selA, selB) = auditionRange (H, selHit, playMode);   // Layer: every round robin of this velocity; Sweep: every hit
    model.setLoop (selA, selB, loopOn);
    if (selA < scroll || selB > timeAt (laneX() + laneW())) scroll = std::max (0.0, selA - 0.25 * laneW() / pps);
    if (play) playSelection();
    if (andRedraw) redraw();
}

void HitsView::playSelection()
{
    if (selB > selA) model.setLoop (selA, selB, loopOn);
    model.setPosition (selA);
    model.play();
}

// ------------------------------------------------------------------------------------------------ drawing
void HitsView::draw (Canvas& c)
{
    static int frames = 0; static double t0 = juce::Time::getMillisecondCounterHiRes();
    const double d0 = juce::Time::getMillisecondCounterHiRes();
    struct Log { double d0; int& n; double t0; int st; ~Log() { if (std::getenv ("LD_FRAMELOG")) fprintf (stderr, "[draw] frame %d at %.1f s step %d took %.1f ms\n", ++n, (juce::Time::getMillisecondCounterHiRes() - t0) / 1000, st, juce::Time::getMillisecondCounterHiRes() - d0); } } log { d0, frames, t0, step };
    hits.clear();
    if (needsFit && width() > headW + inspW + 100 && model.isOpen())
    {
        needsFit = false;
        fitAll();
        if (selHit < 0 && ! model.hits().empty()) selectHit (0, false, false);   // (never ask for a redraw while drawing)
    }
    c.setColor (ground);
    c.fill (0, 0, width(), height());
    if (step != 1)
    {
        drawStep (c);
        drawAudition (c);
        drawToolbar (c);
        if (std::getenv ("LD_GALLERY")) gallery (c, 0, tbH + auH, width(), height() - tbH - auH);   // design review only
        return;
    }
    drawTracks (c);
    drawRulers (c);
    drawInspector (c);
    drawCloseUp (c);
    drawStepBar (c);
    drawAudition (c);
    drawToolbar (c);
    drawPopup (c);
    if (std::getenv ("LD_GALLERY")) gallery (c, 0, tbH + auH, width(), height() - tbH - auH);   // design review only
}

void HitsView::drawToolbar (Canvas& c)
{
    c.setColor (visage::Brush::linear (visage::Color (0xff27272a), visage::Color (tok::ground), visage::Point (0, 0), visage::Point (0, tbH)));
    c.fill (0, 0, width(), tbH);
    c.setColor (line); c.fill (0, tbH - 1, width(), 1);
    // the mark
    c.setColor (tok::lane); c.circle (14, 11, 30);
    c.setColor (tok::coral); c.ring (14, 11, 30, 1.5f);
    txt (c, "\xe2\x88\x9e", boldFont (18), tok::coral, 14, 11, 30, 30, visage::Font::kCenter);
    // narrower windows (down to the 1100 px minimum) drop the take line and the counter's Start/End/Len, and the
    // steps share whatever room is left, so all eight and the transport always fit
    const bool compact = width() < 1400;
    const float counterW = compact ? 132.0f : 250.0f, transportW = 36 * 3 + 46 + 10 + counterW;
    const float tx0 = width() - 104 - 12 - transportW;
    txt (c, "Limitless Drums", boldFont (13), ink, 54, compact ? 17 : 9, 120, 18);
    if (! compact)
        txt (c, model.isOpen() ? (model.takeName() + " \xc2\xb7 " + juce::String ((int) model.tracks().size()) + " tracks").toStdString() : std::string ("No take open"), bodyFont (11), ink3, 54, 27, 180, 16);
    // steps
    float x = compact ? 176.0f : 240.0f;
    const float sw = std::clamp ((tx0 - 12 - x - 4) / 8.0f, 50.0f, 84.0f);
    c.setColor (tok::lane); c.roundedRectangle (x, 12, 8 * sw + 4, 32, tok::radius);
    for (int i = 0; i < 8; ++i)
    {
        const float bx = x + 2 + i * sw;
        const bool on = i == step;
        const std::string label = sw >= 80 ? std::to_string (i + 1) + ". " + stepNames[i] : std::string (stepNames[i]);
        if (on)   // the step you're on: the one coral tab
        {
            c.setColor (visage::Brush::linear (visage::Color (tok::coralFillTop), visage::Color (tok::coralFill), visage::Point (0, 14), visage::Point (0, 42)));
            c.roundedRectangle (bx, 14, sw - 2, 28, tok::radius - 2);
        }
        txt (c, label, on ? boldFont (sw >= 66 ? 12 : 11) : bodyFont (sw >= 66 ? 12 : 11), on ? 0xffffffff : ink2, bx, 14, sw - 2, 28, visage::Font::kCenter);
        add (bx, 14, sw - 2, 28, "step:" + std::to_string (i));
    }
    // transport
    x = tx0;
    const char* tid[4] = { "prev", "stop", "play", "next" };
    for (int i = 0; i < 4; ++i)
    {
        const float bw = i == 2 ? 44.f : 34.f, bx = x + (i < 3 ? i * 36.f : 36 * 2 + 46);
        const bool playBtn = i == 2;
        c.setColor (playBtn ? visage::Brush::linear (visage::Color (tok::mint), visage::Color (tok::mintFill), visage::Point (0, 11), visage::Point (0, 41))
                            : visage::Brush::linear (visage::Color (tok::controlHi), visage::Color (tok::control), visage::Point (0, 11), visage::Point (0, 41)));
        c.roundedRectangle (bx, 11, bw, 30, tok::radius);
        const float cx = bx + bw / 2, cy = 26;
        c.setColor (playBtn ? tok::mintText : tok::ink);
        if (i == 1) c.fill (cx - 5, cy - 5, 10, 10);
        else if (i == 2) c.triangle (cx - 4, cy - 6, cx - 4, cy + 6, cx + 6, cy);
        else if (i == 0) { c.fill (cx - 6, cy - 6, 2, 12); c.triangle (cx + 5, cy - 6, cx + 5, cy + 6, cx - 3, cy); }
        else { c.fill (cx + 4, cy - 6, 2, 12); c.triangle (cx - 5, cy - 6, cx - 5, cy + 6, cx + 3, cy); }
        add (bx, 11, bw, 30, tid[i]);
    }
    // counter
    x += 36 * 3 + 46 + 10;
    c.setColor (0xff111113); c.roundedRectangle (x, 7, counterW, 38, tok::radius);
    txt (c, fmtTime (model.position()), monoFont (17), tok::mint, x + 10, 7, 120, 38);
    if (! compact)
    {
        txt (c, "Start " + fmtTime (selA), monoFont (9), ink3, x + 128, 9, 120, 12);
        txt (c, "End   " + fmtTime (selB), monoFont (9), ink3, x + 128, 20, 120, 12);
        txt (c, "Len   " + fmtTime (std::max (0.0, selB - selA)), monoFont (9), ink3, x + 128, 31, 120, 12);
    }
    // Auto Prep
    x = width() - 104;
    c.setColor (visage::Brush::linear (visage::Color (tok::controlHi), visage::Color (tok::control), visage::Point (0, 11), visage::Point (0, 41)));
    c.roundedRectangle (x, 11, 92, 30, tok::radius);
    txt (c, model.isPrepping() ? "Working\xe2\x80\xa6" : "Auto Prep", boldFont (12), ink, x, 11, 92, 30, visage::Font::kCenter);
    add (x, 11, 92, 30, "autoprep");
}

void HitsView::drawAudition (Canvas& c)
{
    const float y = tbH;
    c.setColor (bar2); c.fill (0, y, width(), auH);
    c.setColor (line); c.fill (0, y + auH - 1, width(), 1);
    txt (c, "AUDITION", boldFont (10), tok::ink3, 14, y, 80, auH);
    float x = 84;
    c.setColor (tok::lane); c.roundedRectangle (x, y + 9, 6 * 66 + 4, 34, tok::radius);
    for (int i = 0; i < 6; ++i)
    {
        const float bx = x + 2 + i * 66;
        button (c, bx, y + 11, 64, 30, modeNames[i], playMode == i, tok::coralFill);
        if (i >= 3) { c.setColor (0xa0161618); c.fill (bx, y + 11, 64, 30); }   // not in this build yet
        add (bx, y + 11, 64, 30, "mode:" + std::to_string (i));
    }
    x += 6 * 66 + 16;
    c.setColor (tok::lane); c.roundedRectangle (x, y + 9, 128, 34, tok::radius);
    const char* abn[3] = { "A", "B", "A\xe2\x86\x92" "B" };
    for (int i = 0; i < 3; ++i)
    {
        const float bx = x + 2 + i * 41.f, bw = i == 2 ? 42.f : 40.f;
        if (ab == i) { c.setColor (i == 0 ? tok::controlHi : tok::coralFill); c.roundedRectangle (bx, y + 11, bw, 30, tok::radius - 2); }
        txt (c, abn[i], boldFont (12), ab == i ? 0xffffffff : ink2, bx, y + 11, bw, 30, visage::Font::kCenter);
        add (bx, y + 11, bw, 30, "ab:" + std::to_string (i));
    }
    x += 128 + 16;
    const char* tn[4] = { "Loop", "Follow", "Level match", "Pads" };
    const bool tv[4] = { loopOn, followOn, lmOn, padsOn };
    for (int i = 0; i < 4; ++i)
    {
        const float w = textWidth (boldFont (12), tn[i]) + 22;
        if (tv[i]) { c.setColor (tok::mintDeep); c.roundedRectangle (x, y + 12, w, 26, tok::pill); c.setColor (0xff1f5443); c.roundedRectangleBorder (x, y + 12, w, 26, tok::pill, 1); }
        else { c.setColor (tok::divider); c.roundedRectangleBorder (x, y + 12, w, 26, tok::pill, 1); }
        txt (c, tn[i], tv[i] ? boldFont (12) : bodyFont (12), tv[i] ? tok::mintInk : tok::ink2, x, y + 12, w, 26, visage::Font::kCenter);
        add (x, y + 12, w, 26, std::string ("toggle:") + tn[i]);
        x += w + 4;
    }
    // master meter (sum of the tracks' meters, roughly): whatever room is left after the toggles, up to 180 px
    const float mw = std::min (180.0f, width() - 16 - (x + 14));
    x = width() - 16 - mw - 14;
    float l = -100, r = -100;
    for (int t = 0; t < (int) model.tracks().size(); ++t) { auto m = model.meter (t); l = std::max (l, m.first); r = std::max (r, m.second); }
    for (int ch = 0; mw > 30 && ch < 2; ++ch)
    {
        const float yy = y + 17 + ch * 10, lv = std::clamp ((ch ? r : l) + 60, 0.0f, 60.0f) / 60.0f;
        txt (c, ch ? "R" : "L", bodyFont (10), 0xff9c9ca1, x, yy - 4, 10, 14);
        c.setColor (0xff161617); c.fill (x + 14, yy, mw, 6);
        c.setColor (visage::Brush::horizontal (visage::Gradient (visage::Color (0xff2fbf6a), visage::Color (0xff2fbf6a), visage::Color (0xffe0c23c), visage::Color (0xffe0663c))));
        c.fill (x + 14, yy, mw * lv, 6);
    }
}

void HitsView::drawStepBar (Canvas& c)
{
    const float y = tbH + auH;
    c.setColor (bar3); c.fill (0, y, width(), sbH);
    c.setColor (line); c.fill (0, y + sbH - 1, width(), 1);
    float x = 14;
    const char* zl[4] = { "\xe2\x88\x92", "+", "\xe2\x86\x95\xe2\x88\x92", "\xe2\x86\x95+" };
    const char* zid[4] = { "zoomout", "zoomin", "wavedown", "waveup" };
    for (int i = 0; i < 4; ++i)
    {
        c.setColor (btn); c.roundedRectangle (x, y + 7, 30, 24, 3);
        txt (c, zl[i], boldFont (12), ink, x, y + 7, 30, 24, visage::Font::kCenter);
        add (x, y + 7, 30, 24, zid[i]);
        x += 33;
    }
    x += 8;
    c.setColor (field); c.roundedRectangle (x, y + 7, 92, 24, 3);
    txt (c, std::string ("Height: ") + heightNames[heightIdx], bodyFont (11), ink, x, y + 7, 92, 24, visage::Font::kCenter);
    add (x, y + 7, 92, 24, "height");
    x += 106;
    txt (c, "Fit", bodyFont (12), ink2, x, y + 7, 30, 24);
    add (x, y + 7, 30, 24, "fit");
    x += 44;
    txt (c, model.isOpen() ? ("Found " + std::to_string (model.hits().size()) + " of " + std::to_string (model.velocities() * model.roundRobins()) + " hits")
                           : std::string ("Drop recordings anywhere, or press Cmd-O"), bodyFont (12), ink2, x, y, 340, sbH);
    txt (c, status, bodyFont (12), 0xffffc78a, x + 280, y, width() - x - 700, sbH);
    txt (c, "Tab: next hit \xc2\xb7 Space: play \xc2\xb7 R/T: zoom", bodyFont (11), ink3, width() - 300, y, 290, sbH, visage::Font::kRight);
}

void HitsView::drawRulers (Canvas& c)
{
    const float y0 = tbH + auH + sbH, lx = laneX(), lw = laneW();
    const char* names[3] = { "Min:Sec", "Velocity layers", "Hits" };
    for (int r = 0; r < 3; ++r)
    {
        const float y = y0 + r * rulH;
        c.setColor (ruler); c.fill (0, y, lx + lw, rulH);
        c.setColor (line); c.fill (0, y + rulH - 1, lx + lw, 1);
        txt (c, names[r], bodyFont (11), ink3, 12, y, lx - 12, rulH);
    }
    c.setClampBounds (lx, y0, lw, 3 * rulH);
    // selection in the timeline
    if (selB > selA)
    {
        c.setColor (draw::withAlpha (accent, 0.28f)); c.fill (xOf (selA), y0, xOf (selB) - xOf (selA), rulH);
        c.setColor (accent); c.fill (xOf (selA), y0, 2, rulH); c.fill (xOf (selB) - 2, y0, 2, rulH);
    }
    // seconds
    double step = 0.01;
    for (double s : { 0.01, 0.05, 0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 }) { step = s; if (s * pps >= 70) break; }
    for (double t = std::floor (scroll / step) * step; xOf (t) < lx + lw; t += step)
    {
        c.setColor (0xff55555a); c.fill (xOf (t), y0, 1, rulH);
        char b[24]; std::snprintf (b, sizeof b, step < 0.1 ? "%d:%06.3f" : step < 1 ? "%d:%04.1f" : "%d:%02.0f", (int) (t / 60), std::fmod (t, 60.0));
        txt (c, b, monoFont (10), 0xffb4b4b8, xOf (t) + 4, y0, 60, rulH);
    }
    // velocity layers and hits
    const auto& H = model.hits();
    for (int v = 0; v < model.velocities(); ++v)
    {
        double a = 1e9, b = -1;
        for (auto& h : H) if (h.vel == v) { a = std::min (a, h.time); b = std::max (b, h.end); }
        if (b < 0) continue;
        const float xa = xOf (a), xb = xOf (b - 0.05);
        c.setColor (velColour[v % nVelColours]); c.roundedRectangle (xa, y0 + rulH + 3, std::max (2.0f, xb - xa), rulH - 6, 2);
        std::string label = "Vel " + std::to_string (v + 1) + (v == 0 ? " \xc2\xb7 softest" : v == model.velocities() - 1 ? " \xc2\xb7 hardest" : "");
        txt (c, label, boldFont (10), 0xfff4f4f5, xa + 5, y0 + rulH + 3, std::max (0.0f, xb - xa - 5), rulH - 6);
    }
    for (size_t i = 0; i < H.size(); ++i)
    {
        const float x = xOf (H[i].time);
        const bool sel = (int) i == selHit;
        const std::string n = std::to_string (i + 1);
        const float w = std::max (14.0f, textWidth (monoFont (9), n) + 4);
        c.setColor (sel ? accent : 0xff46464b); c.roundedRectangle (x, y0 + 2 * rulH + 3, w, 16, 2);
        txt (c, n, monoFont (9), sel ? 0xffffffff : 0xffe2e2e4, x, y0 + 2 * rulH + 3, w, 16, visage::Font::kCenter);
        add (x, y0 + 2 * rulH + 3, w, 16, "hit:" + std::to_string (i));
    }
    c.setClampBounds (0, 0, width(), height());
}

void HitsView::drawTracks (Canvas& c)
{
    const float lx = laneX(), lw = laneW(), bottom = height() - closeH;
    const auto& T = model.tracks();
    if (T.empty())
    {
        txt (c, "Drop the recordings of one take here (a folder or the files), or press Cmd-O.", bodyFont (14), ink2, lx, laneTop() + 60, lw, 30, visage::Font::kCenter);
        add (lx, laneTop(), lw, bottom - laneTop(), "open");
        return;
    }
    for (int t = 0; t < (int) T.size(); ++t)
    {
        const float y = trackY (t), h = trackH (t);
        if (y > bottom) break;
        const auto& tr = T[(size_t) t];
        // header
        c.setColor (t == popTrack ? headSel : head); c.fill (0, y, lx, h);
        c.setColor (tr.colour); c.fill (0, y, 5, h);
        c.setColor (line); c.fill (0, y + h - 1, lx + lw, 1);
        const float hy = y + std::max (4.0f, h / 2 - 22);
        const float nameW = lx - 98 - 15 - (tr.channels > 1 ? 52 : 6);
        txt (c, tr.name, boldFont (12), ink, 15, hy, nameW, 18);
        if (tr.channels > 1)
        {
            const float sx = 15 + std::min (nameW, textWidth (boldFont (12), tr.name)) + 6;
            c.setColor (0xff55555a); c.roundedRectangleBorder (sx, hy + 3, 42, 13, 2, 1);
            txt (c, "STEREO", boldFont (8), 0xffb8b8bc, sx, hy + 3, 42, 13, visage::Font::kCenter);
        }
        const char* sm[3] = { "S", "M", "\xc3\x98" };
        const bool smOn[3] = { model.solo (t), model.mute (t), model.flip (t) };
        const unsigned smCol[3] = { 0xffe0c23c, 0xff5c9cff, gold };
        for (int i = 0; i < 3; ++i)
        {
            const float bx = lx - 98 + i * 26;
            c.setColor (smOn[i] ? smCol[i] : 0xff48484c); c.roundedRectangle (bx, hy, 22, 18, 2);
            txt (c, sm[i], boldFont (10), smOn[i] ? 0xff1a1608 : 0xffe8e8ea, bx, hy, 22, 18, visage::Font::kCenter);
            add (bx, hy, 22, 18, std::string (i == 0 ? "solo:" : i == 1 ? "mute:" : "flip:") + std::to_string (t));
        }
        if (h >= 52)
        {
            const float ry = hy + 26;
            txt (c, "Mon", bodyFont (10), 0xff9c9ca1, 15, ry, 26, 18);
            c.setColor (field); c.roundedRectangle (42, ry, 56, 18, 2);
            c.setColor (popTrack == t && ! popPan ? accent : 0xff3c3c40); c.roundedRectangleBorder (42, ry, 56, 18, 2, 1);
            txt (c, fmtDb (model.monDb (t)), monoFont (10), ink, 42, ry, 52, 18, visage::Font::kRight);
            add (42, ry, 56, 18, "mon:" + std::to_string (t));
            txt (c, "Pan", bodyFont (10), 0xff9c9ca1, 108, ry, 24, 18);
            c.setColor (field); c.roundedRectangle (134, ry, 44, 18, 2);
            c.setColor (popTrack == t && popPan ? accent : 0xff3c3c40); c.roundedRectangleBorder (134, ry, 44, 18, 2, 1);
            const float p = model.pan (t);
            const std::string ps = std::abs (p) < 0.01f ? "C" : (p < 0 ? "<" : "") + std::to_string ((int) std::lround (std::abs (p) * 100)) + (p > 0 ? ">" : "");
            txt (c, ps, monoFont (10), ink2, 134, ry, 44, 18, visage::Font::kCenter);
            add (134, ry, 44, 18, "pan:" + std::to_string (t));
        }
        // meter
        c.setColor (well); c.fill (lx - 14, y, 14, h - 1);
        const auto m = model.meter (t);
        for (int ch = 0; ch < std::min (2, tr.channels); ++ch)
        {
            const float mw = tr.channels > 1 ? 4.0f : 8.0f, mx = lx - 11 + ch * 5, mh = h - 11;
            const float lv = std::clamp ((ch ? m.second : m.first) + 60, 0.0f, 60.0f) / 60.0f;
            c.setColor (0xff0f0f10); c.fill (mx, y + 5, mw, mh);
            c.setColor (visage::Brush::vertical (visage::Gradient (visage::Color (0xffe0663c), visage::Color (0xffe0c23c), visage::Color (0xff2fbf6a), visage::Color (0xff2fbf6a))));
            c.fill (mx, y + 5 + mh * (1 - lv), mw, mh * lv);
        }
        // lane: a stereo track draws left above right
        c.setClampBounds (lx, y, lw, h - 1);
        c.setColor (lane); c.fill (lx, y, lw, h - 1);
        c.setColor (draw::withAlpha (tr.colour, 0.13f)); c.fill (lx + 1, y + 1, lw - 2, h - 3);
        c.setColor (draw::withAlpha (tr.colour, 0.55f)); c.fill (lx + 1, y + 1, lw - 2, 11);
        txt (c, tr.name + ".wav", boldFont (9), 0xff0d0d0e, lx + 6, y + 1, 300, 11);
        const auto& w = *tr.wave;
        const int nch = std::max (1, w.channels());
        const float body = h - 14, chH = body / nch;
        const long len = w.length();
        const double sr = w.sampleRate;
        c.setColor (draw::withAlpha (tr.colour, 0.92f));
        for (int ch = 0; ch < nch; ++ch)
        {
            const float mid = y + 13 + chH * (ch + 0.5f), half = chH * 0.46f * waveScale;
            for (float x = 0; x < lw; x += 1)
            {
                const long s0 = (long) ((scroll + x / pps) * sr), s1 = std::max (s0 + 1, (long) ((scroll + (x + 1) / pps) * sr));
                if (s0 >= len) break;
                if (s1 <= 0) continue;
                auto [mn, mx] = w.range (ch, std::max (0L, s0), std::min (len, s1), 0.0f);
                mn = std::max (-1.0f, mn * waveScale) / std::max (1.0f, waveScale);
                mx = std::min (1.0f, mx * waveScale) / std::max (1.0f, waveScale);
                const float top = mid - std::min (1.0f, mx * waveScale) * chH * 0.46f, bot = mid - std::max (-1.0f, mn * waveScale) * chH * 0.46f;
                c.fill (lx + x, top, 1, std::max (1.0f, bot - top));
                (void) half;
            }
            if (nch > 1 && ch == 0) { c.setColor (0x8c000000); c.fill (lx, y + 13 + chH, lw, 1); c.setColor (draw::withAlpha (tr.colour, 0.92f)); }
        }
        // selection over the lane
        if (selB > selA) { c.setColor (0x29ffffff); c.fill (xOf (selA), y, xOf (selB) - xOf (selA), h - 1); }
        c.setClampBounds (0, 0, width(), height());
        add (lx, y, lw, h, "lane:" + std::to_string (t));
    }
    // playhead
    const float px = xOf (model.position());
    if (px >= lx && px <= lx + lw)
    {
        const float top = tbH + auH + sbH;
        c.setColor (draw::withAlpha (accent, 0.35f)); c.fill (px - 2, top, 5, bottom - top);
        c.setColor (accent); c.fill (px, top, 1, bottom - top);
    }
}

void HitsView::drawInspector (Canvas& c)
{
    const float x0 = width() - inspW, y0 = tbH + auH + sbH, w = inspW;
    c.setColor (bar2); c.fill (x0, y0, w, height() - y0);
    c.setColor (0xff0e0e0f); c.fill (x0, y0, 1, height() - y0);
    if (! model.isOpen()) return;
    const int V = model.velocities(), RR = model.roundRobins();
    const auto& H = model.hits();
    const bool all = (int) H.size() == V * RR;
    txt (c, "Found " + std::to_string (H.size()) + " of " + std::to_string (V * RR), boldFont (13), ink, x0 + 14, y0 + 12, 160, 20);
    txt (c, all ? "Every slot filled" : (model.isPrepared() ? "Check the count" : "Run Auto Prep"), bodyFont (12), all ? tok::mintInk : 0xffffc78a, x0 + 14, y0 + 12, w - 28, 20, visage::Font::kRight);
    txt (c, std::to_string (V) + " velocities \xc3\x97 " + std::to_string (RR) + " round robins, softest to hardest.", bodyFont (11), 0xffb4b4b8, x0 + 14, y0 + 36, w - 28, 16);
    // the grid: hardest on top
    const float gx = x0 + 44, gw = w - 58, cw = gw / RR, ch = 30;
    float gy = y0 + 62;
    for (int r = 0; r < RR; ++r) txt (c, std::to_string (r + 1), bodyFont (10), ink3, gx + r * cw, gy, cw, 14, visage::Font::kCenter);
    gy += 16;
    float peakMax = -100;
    for (auto& h : H) peakMax = std::max (peakMax, h.peakDb);
    for (int v = V - 1; v >= 0; --v)
    {
        const float y = gy + (V - 1 - v) * (ch + 3);
        txt (c, "V" + std::to_string (v + 1), boldFont (10), 0xffc8c8cc, x0 + 14, y, 28, ch);
        for (int r = 0; r < RR; ++r)
        {
            int idx = -1;
            for (size_t i = 0; i < H.size(); ++i) if (H[i].vel == v && H[i].rr == r) { idx = (int) i; break; }
            const float cx = gx + r * cw + 1.5f;
            const bool sel = idx >= 0 && idx == selHit;
            c.setColor (well); c.roundedRectangle (cx, y, cw - 3, ch, 3);
            c.setColor (sel ? accent : 0xff38383c); c.roundedRectangleBorder (cx, y, cw - 3, ch, 3, 1);
            if (idx >= 0)
            {
                const float hh = std::clamp ((H[(size_t) idx].peakDb + 48) / 48, 0.05f, 1.0f) * (ch - 7);
                c.setColor (sel ? accent : velColour[v % nVelColours]); c.fill (cx + 3, y + ch - 3 - hh, cw - 9, hh);
                add (cx, y, cw - 3, ch, "slot:" + std::to_string (idx));
            }
        }
    }
    float y = gy + V * (ch + 3) + 10;
    if (padsOn)
    {
        txt (c, "Pads", boldFont (12), ink, x0 + 14, y, 100, 18);
        y += 22;
        const float pw = (w - 28 - 5 * 4) / 6;
        for (int v = 0; v < std::min (6, V); ++v)
        {
            c.setColor (velColour[v]); c.roundedRectangle (x0 + 14 + v * (pw + 4), y, pw, 38, 4);
            txt (c, std::to_string (v + 1), boldFont (12), 0xff101011, x0 + 14 + v * (pw + 4), y, pw, 38, visage::Font::kCenter);
            add (x0 + 14 + v * (pw + 4), y, pw, 38, "pad:" + std::to_string (v));
        }
        y += 48;
    }
    txt (c, "Click a slot to hear it.", bodyFont (11), ink3, x0 + 14, y, w - 28, 16);
    txt (c, "Tab and Shift-Tab step through the hits.", bodyFont (11), ink3, x0 + 14, y + 16, w - 28, 16);
}

void HitsView::drawCloseUp (Canvas& c)
{
    const float y0 = height() - closeH, lx = laneX(), lw = laneW();
    c.setColor (0xff0b0b0c); c.fill (0, y0 - 1, lx + lw, 1);
    c.setColor (bar2); c.fill (0, y0, lx, closeH);
    c.setColor (0xff1f1f21); c.fill (lx, y0, lw, closeH);
    const auto& H = model.hits();
    if (selHit < 0 || selHit >= (int) H.size() || model.tracks().empty()) return;
    const auto& h = H[(size_t) selHit];
    txt (c, "Hit " + std::to_string (selHit + 1) + " \xc2\xb7 Vel " + std::to_string (h.vel + 1) + " \xc2\xb7 RR " + std::to_string (h.rr + 1), boldFont (12), ink, 12, y0 + 12, lx - 24, 18);
    char b[96];
    std::snprintf (b, sizeof b, "Starts %s \xc2\xb7 %.2f s", fmtTime (h.time).c_str(), h.end - h.time);
    txt (c, b, bodyFont (11), 0xffb4b4b8, 12, y0 + 34, lx - 24, 16);
    const int lead = std::clamp (model.leadTrack(), 0, (int) model.tracks().size() - 1);
    std::snprintf (b, sizeof b, "Peak %.1f dBFS on %s", h.peakDb, model.tracks()[(size_t) lead].name.c_str());
    txt (c, b, bodyFont (11), 0xffb4b4b8, 12, y0 + 52, lx - 24, 16);
    // the hit on the first track, start to end
    const auto& w = *model.tracks()[(size_t) lead].wave;
    const double sr = w.sampleRate, a = h.time - 0.005, e = h.end;
    float pk = 1e-6f;
    for (long n = (long) (a * sr); n < std::min (w.length(), (long) (e * sr)); n += 64) { auto [lo, hi] = w.range (0, std::max (0L, n), std::min (w.length(), n + 64), 0.0f); pk = std::max ({ pk, hi, -lo }); }
    const float mid = y0 + closeH / 2, half = closeH * 0.42f / pk;   // drawn to fit the hit (its level is in the text)
    c.setColor (draw::withAlpha (gold, 0.55f));
    for (float x = 0; x < lw; x += 1)
    {
        const long s0 = (long) ((a + (e - a) * x / lw) * sr), s1 = std::max (s0 + 1, (long) ((a + (e - a) * (x + 1) / lw) * sr));
        if (s0 < 0 || s0 >= w.length()) continue;
        auto [mn, mx] = w.range (0, s0, std::min (w.length(), s1), 0.0f);
        c.fill (lx + x, mid - mx * half, 1, std::max (1.0f, (mx - mn) * half));
    }
    const float sx = lx + (float) ((h.time - a) / (e - a) * lw);
    c.setColor (accent); c.fill (sx, y0, 2, closeH);
    txt (c, "Start", bodyFont (11), tok::mintInk, sx + 6, y0 + 6, 60, 14);
    c.setColor (0xffff8a7a); c.fill (lx + lw - 2, y0, 2, closeH);
}

void HitsView::drawPopup (Canvas& c)
{
    if (popTrack < 0 || popTrack >= (int) model.tracks().size()) return;
    const float x = popPan ? 120 : 34, y = trackY (popTrack) + 48, w = popPan ? 120 : 74, h = popPan ? 70 : 246;
    c.setColor (0x8c000000); c.roundedRectangleShadow (x, y + 4, w, h, 4, 16);
    c.setColor (0xff2a2a2d); c.roundedRectangle (x, y, w, h, 4);
    c.setColor (0xff0b0b0c); c.roundedRectangleBorder (x, y, w, h, 4, 1);
    add (x, y, w, h, "pop");
    if (popPan)
    {
        const float p = model.pan (popTrack);
        txt (c, "Pan", bodyFont (10), ink3, x, y + 6, w, 14, visage::Font::kCenter);
        c.setColor (0xff121213); c.fill (x + 12, y + 34, w - 24, 4);
        c.setColor (0xff4f9be8); c.fill (x + w / 2, y + 34, (w - 24) / 2 * p, 4);
        const float kx = x + w / 2 + (w - 24) / 2 * p;
        c.setColor (0xffd8d8da); c.roundedRectangle (kx - 6, y + 27, 12, 18, 2);
        txt (c, "\xe2\x8c\xa5-click: centre", bodyFont (9), ink3, x, y + 50, w, 14, visage::Font::kCenter);
        return;
    }
    const float db = model.monDb (popTrack);
    txt (c, fmtDb (db), monoFont (11), ink, x, y + 6, w, 16, visage::Font::kCenter);
    const float ty = y + 30, th = h - 66;
    const std::pair<const char*, float> marks[] = { { "+6", 6 }, { "0", 0 }, { "-5", -5 }, { "-10", -10 }, { "-20", -20 }, { "-40", -40 }, { "-inf", -100 } };
    for (auto& [lab, v] : marks)
    {
        const float yy = ty + th * (1 - faderPos (v));
        txt (c, lab, monoFont (9), ink3, x + 4, yy - 7, 22, 14);
        c.setColor (0xff5a5a5e); c.fill (x + 26, yy, 6, 1);
    }
    c.setColor (0xff121213); c.roundedRectangle (x + 46, ty, 4, th, 2);
    const float ky = ty + th * (1 - faderPos (db));
    c.setColor (0xff4f9be8); c.roundedRectangle (x + 46, ky, 4, ty + th - ky, 2);
    c.setColor (visage::Brush::linear (visage::Color (0xfff2f2f4), visage::Color (0xffb4b4b8), visage::Point (0, ky - 15), visage::Point (0, ky + 15)));
    c.roundedRectangle (x + 37, ky - 15, 22, 30, 3);
    c.setColor (0xff8a8a8f); c.fill (x + 39, ky, 18, 1);
    txt (c, "Monitor only", bodyFont (9), ink3, x, y + h - 32, w, 13, visage::Font::kCenter);
    txt (c, "\xe2\x8c\xa5-click: 0 dB", bodyFont (9), ink3, x, y + h - 18, w, 13, visage::Font::kCenter);
}

// ------------------------------------------------------------------------------------------------ input
const HitsView::Hit* HitsView::hitAt (float x, float y) const
{
    const int i = pickTarget (hits, x, y);   // the smallest target under the pointer wins
    return i < 0 ? nullptr : &hits[(size_t) i];
}

void HitsView::mouseDown (const visage::MouseEvent& e)
{
    requestKeyboardFocus();
    const auto* h = hitAt (e.position.x, e.position.y);
    if (popTrack >= 0 && (h == nullptr || h->id != "pop"))
    {
        // a click outside the pop-up closes it (and does nothing else)
        if (! (h && (h->id.rfind ("mon:", 0) == 0 || h->id.rfind ("pan:", 0) == 0))) { popTrack = -1; redraw(); return; }
    }
    if (h) act (h->id, e);
}

void HitsView::act (const std::string& id, const visage::MouseEvent& e)
{
    auto arg = [&] { return std::stoi (id.substr (id.find (':') + 1)); };
    if (id.rfind ("step:", 0) == 0) { setStep (arg()); popTrack = -1; return; }
    if (step != 1 && actStep (id, e)) { redraw(); return; }
    if (id == "play") { model.isPlaying() ? model.stop() : playSelection(); }
    else if (id == "stop") { model.stop(); }
    else if (id == "prev") { selectHit (selHit - 1, model.isPlaying()); }
    else if (id == "next") { selectHit (selHit + 1, model.isPlaying()); }
    else if (id == "autoprep") { if (onAutoPrep) onAutoPrep(); }
    else if (id == "open") { if (onOpen) onOpen(); }
    else if (id.rfind ("mode:", 0) == 0)
    {
        const int m = arg();
        if (m >= 3) { setStatus (std::string (modeNames[m]) + " audition isn't in this build yet: Hit, Layer and Sweep are."); return; }
        playMode = m;
        selectHit (selHit < 0 ? 0 : selHit, model.isPlaying(), false);
    }
    else if (id.rfind ("ab:", 0) == 0) { ab = arg(); model.setProcessed (ab != 0); }
    else if (id.rfind ("toggle:", 0) == 0)
    {
        const auto n = id.substr (7);
        if (n == "Loop") { loopOn = ! loopOn; model.setLoop (selA, selB, loopOn); }
        else if (n == "Follow") followOn = ! followOn;
        else if (n == "Level match") { lmOn = ! lmOn; model.setLevelMatch (lmOn); }
        else if (n == "Pads") padsOn = ! padsOn;
    }
    else if (id == "zoomout") zoomBy (0.5);
    else if (id == "zoomin") zoomBy (2.0);
    else if (id == "wavedown") waveScale = std::max (1.0f, waveScale / 1.5f);
    else if (id == "waveup") waveScale = std::min (16.0f, waveScale * 1.5f);
    else if (id == "height") heightIdx = (heightIdx + 1) % 4;
    else if (id == "fit") fitAll();
    else if (id.rfind ("hit:", 0) == 0 || id.rfind ("slot:", 0) == 0) selectHit (arg(), true);
    else if (id.rfind ("pad:", 0) == 0)
    {
        std::vector<int> c;
        for (size_t i = 0; i < model.hits().size(); ++i) if (model.hits()[i].vel == arg()) c.push_back ((int) i);
        if (! c.empty()) selectHit (c[(size_t) juce::Random::getSystemRandom().nextInt ((int) c.size())], true);
    }
    else if (id.rfind ("solo:", 0) == 0) model.setSolo (arg(), ! model.solo (arg()));
    else if (id.rfind ("mute:", 0) == 0) model.setMute (arg(), ! model.mute (arg()));
    else if (id.rfind ("flip:", 0) == 0) model.setFlip (arg(), ! model.flip (arg()));
    else if (id.rfind ("mon:", 0) == 0 || id.rfind ("pan:", 0) == 0)
    {
        const bool pan = id[0] == 'p';
        if (e.isAltDown()) { pan ? model.setPan (arg(), 0) : model.setMonDb (arg(), 0); }
        else if (popTrack == arg() && popPan == pan) popTrack = -1;
        else { popTrack = arg(); popPan = pan; }
    }
    else if (id == "pop")
    {
        if (e.isAltDown()) { popPan ? model.setPan (popTrack, 0) : model.setMonDb (popTrack, 0); }
        else
        {
            drag = popPan ? Drag::pan : Drag::fader;
            dragY0 = popPan ? e.position.x : e.position.y;
            dragV0 = popPan ? model.pan (popTrack) : faderPos (model.monDb (popTrack));
        }
    }
    else if (id.rfind ("lane:", 0) == 0)
    {
        drag = Drag::select;
        dragFrom = std::max (0.0, timeAt (e.position.x));
        dragTrack = arg();
        if (e.isShiftDown() && selB > selA) { if (dragFrom < selA) selA = dragFrom; else selB = dragFrom; }
        else { selA = selB = dragFrom; selHit = -1; }
        model.setPosition (selA);
    }
    redraw();
}

void HitsView::mouseDrag (const visage::MouseEvent& e)
{
    if (drag == Drag::slider)
    {
        // a slider: 220 px for its whole range; Shift for fine
        setSliderValue (sliderKey, dragV0 + (e.position.x - dragY0) / (e.isShiftDown() ? 1100.0 : 220.0) * sliderSpan (sliderKey));
    }
    else if (drag == Drag::select)
    {
        const double t = std::max (0.0, timeAt (e.position.x));
        selA = std::min (dragFrom, t);
        selB = std::max (dragFrom, t);
    }
    else if (drag == Drag::fader && popTrack >= 0)
    {
        const float p = std::clamp (dragV0 + (dragY0 - e.position.y) / 180.0f, 0.0f, 1.0f);
        model.setMonDb (popTrack, faderDb (p));
    }
    else if (drag == Drag::pan && popTrack >= 0)
        model.setPan (popTrack, std::clamp (dragV0 + (e.position.x - dragY0) / 48.0f, -1.0f, 1.0f));
    redraw();
}

void HitsView::mouseUp (const visage::MouseEvent&)
{
    if (drag == Drag::select && selB > selA) model.setLoop (selA, selB, loopOn);
    drag = Drag::none;
}

bool HitsView::mouseWheel (const visage::MouseEvent& e)
{
    if (e.isMainModifier()) zoomBy (e.wheel_delta_y > 0 ? 1.25 : 0.8);
    else scroll = std::max (0.0, scroll - (e.wheel_delta_x != 0 ? e.wheel_delta_x : e.wheel_delta_y) * 60 / pps);
    redraw();
    return true;
}

bool HitsView::keyPress (const visage::KeyEvent& e)
{
    using K = visage::KeyCode;
    const auto k = e.keyCode();
    if (k == K::Space) { model.isPlaying() ? model.stop() : playSelection(); }
    else if (k == K::Tab) selectHit (selHit + (e.isAltDown() || e.isShiftDown() ? -1 : 1), model.isPlaying());
    else if (k == K::R && ! e.isMainModifier()) zoomBy (0.5);
    else if (k == K::T) zoomBy (2.0);
    else if (k >= K::Number1 && k <= K::Number5 && ! e.isMainModifier())
    {
        static const double secs[5] = { 0, 20, 5, 1.5, 0.25 };   // 1 fits everything
        const int i = (int) k - (int) K::Number1;
        if (i == 0) fitAll();
        else { pps = laneW() / secs[i]; zoomBy (1.0); }
    }
    else if ((k == K::LeftBracket || k == K::RightBracket) && e.isMainModifier() && e.isAltDown())
        waveScale = k == K::RightBracket ? std::min (16.0f, waveScale * 1.5f) : std::max (1.0f, waveScale / 1.5f);
    else if ((k == K::Up || k == K::Down) && e.isCtrlDown() && e.isAltDown())
        heightIdx = std::clamp (heightIdx + (k == K::Up ? 1 : -1), 0, 3);
    else if (k == K::L && e.isMainModifier() && e.isShiftDown()) { loopOn = ! loopOn; model.setLoop (selA, selB, loopOn); }
    else if (k == K::Z && e.isMainModifier())
    {
        // the session's history: every setting on every step (monitor faders and pans aren't part of it)
        if (e.isShiftDown()) { if (model.canRedo()) { model.redo(); setStatus ("Redone."); } }
        else if (model.canUndo()) { model.undo(); setStatus ("Undone."); }
        else setStatus ("Nothing to undo.");
    }
    else if (k == K::O && e.isMainModifier()) { if (onOpen) onOpen(); }
    else if (k == K::Escape) popTrack = -1;
    else if (k == K::Left || k == K::Right) selectHit (selHit + (k == K::Right ? 1 : -1), false);
    else return false;
    redraw();
    return true;
}

} // namespace ld::samples

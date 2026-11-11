// The steps other than Hits: Setup, Align, Tune, Tone, Clean, Level, Export. Each draws below the shared toolbar and
// audition bar, in the approved layout, and changes the session through SampleModel::change (what you hear follows:
// alignment and tuning at once, levels and the clean-up/tone render in the background while playback carries on).
#include "HitsView.h"
#include "HitsLogic.h"
#include <complex>
#include "Style.h"
#include "hl/Prep.h"
#include "hl/Tune.h"
#include "hl/Session.h"
#include <map>

namespace ld::samples
{
using namespace style;
using visage::Canvas;

namespace
{
struct Range { double lo, hi; };
const std::map<std::string, Range>& ranges()
{
    static const std::map<std::string, Range> r = {
        { "tone.match", { 0, 1 } }, { "tone.bright", { -1, 1 } }, { "tone.crack", { -1, 1 } }, { "tone.smooth", { 0, 1 } }, { "tone.tame", { 0, 1 } },
        { "shape.atk", { -1, 1 } }, { "shape.dec", { -1, 1 } }, { "shape.sus", { -1, 1 } }, { "shape.rel", { -1, 1 } }, { "shape.match", { 0, 1 } },
        { "ref.amt", { 0, 1 } }, { "ref.env", { 0, 1 } },
        { "clean.db", { 0, 36 } }, { "lev.step", { 0.5, 6 } }, { "lev.range", { 6, 48 } }, { "lev.offset", { -24, 6 } }, { "tune.cents", { -100, 100 } },
        { "exp.ceil", { -6, 0 } },
        // Align details: arrival in ms; band frequencies as log2 (Hz), 20 Hz .. 8 kHz; Q
        { "al.lag", { -10, 60 } }, { "al.f0", { 4.3219, 12.9658 } }, { "al.q0", { 0.3, 8 } }, { "al.f1", { 4.3219, 12.9658 } }, { "al.q1", { 0.3, 8 } } };
    return r;
}

std::string pct (double v, bool bipolar)
{
    char b[16];
    std::snprintf (b, sizeof b, bipolar ? "%+d" : "%d %%", (int) std::lround (v * 100));
    return bipolar && std::lround (v * 100) == 0 ? std::string ("0") : std::string (b);
}

std::string fmt (const char* f, double v) { char b[48]; std::snprintf (b, sizeof b, f, v); return b; }

unsigned trackColour (const SampleModel& m, int t) { return t >= 0 && t < (int) m.tracks().size() ? m.tracks()[(size_t) t].colour : ink2; }

// a box with a title, like the design's panels
void panel (Canvas& c, float x, float y, float w, float h, const std::string& title)
{
    c.setColor (bar2); c.roundedRectangle (x, y, w, h, 6);
    c.setColor (0xff38383c); c.roundedRectangleBorder (x, y, w, h, 6, 1);
    if (! title.empty()) txt (c, title, boldFont (12), ink, x + 14, y + 10, w - 28, 18);
}
} // namespace

// ------------------------------------------------------------------------------------------------ widgets
float segmentedW (const std::vector<std::string>& labels)
{
    float w = 4;
    for (auto& l : labels) w += textWidth (boldFont (12), l) + 24;
    return w;
}

void HitsView::segmented (Canvas& c, float x, float y, const std::vector<std::string>& labels, int sel, const std::string& idPrefix, float h)
{
    c.setColor (well); c.roundedRectangle (x, y, segmentedW (labels), h + 4, 5);
    float bx = x + 2;
    for (size_t i = 0; i < labels.size(); ++i)
    {
        const float bw = textWidth (boldFont (12), labels[i]) + 24;
        button (c, bx, y + 2, bw, h, labels[i], (int) i == sel);
        add (bx, y + 2, bw, h, idPrefix + std::to_string (i));
        bx += bw;
    }
}

double HitsView::sliderValue (const std::string& key) const
{
    const auto* S = model.session();
    if (! S) return 0;
    const bool mic = selMic >= 0 && selMic < (int) S->tracks.size();
    const hl::ToneMoves& T = mic ? S->tracks[(size_t) selMic].strip.tone : (const hl::ToneMoves&) S->tone;
    const hl::ShapeMoves& H = mic ? S->tracks[(size_t) selMic].strip.shape : S->shape;
    if (key.rfind ("al.", 0) == 0)
    {
        if (! mic) return 0;
        const auto& tr = S->tracks[(size_t) selMic];
        const size_t b = key.back() == '1' ? 1 : 0;
        if (key == "al.lag") return tr.lag / S->sr * 1000.0;
        if (b >= tr.ap.size()) return key[3] == 'f' ? std::log2 (200.0) : 0.7;
        return key[3] == 'f' ? std::log2 (std::max (1.0, tr.ap[b].fc)) : tr.ap[b].Q;
    }
    if (key == "tone.match") return T.match;
    if (key == "tone.bright") return T.bright;
    if (key == "tone.crack") return T.crack;
    if (key == "tone.smooth") return T.smooth;
    if (key == "tone.tame") return T.tame;
    if (key == "shape.atk") return H.atk;
    if (key == "shape.dec") return H.dec;
    if (key == "shape.sus") return H.sus;
    if (key == "shape.rel") return H.rel;
    if (key == "shape.match") return H.match;
    if (key == "ref.amt") return S->tone.refAmt;
    if (key == "ref.env") return S->tone.refEnvAmt;
    if (key == "clean.db") return S->dnDb;
    if (key == "lev.step") return S->lev.stepDb;
    if (key == "lev.range") return S->lev.rangeDb;
    if (key == "lev.offset") return S->lev.offsetDb;
    if (key == "tune.cents") return S->tune.cents;
    if (key == "exp.ceil") return S->exp.ceilDb;
    return 0;
}

double HitsView::sliderSpan (const std::string& key)
{
    auto it = ranges().find (key);
    return it == ranges().end() ? 1.0 : it->second.hi - it->second.lo;
}

void HitsView::setSliderValue (const std::string& key, double v)
{
    auto it = ranges().find (key);
    if (it == ranges().end() || ! model.session()) return;
    v = std::clamp (v, it->second.lo, it->second.hi);
    const int mic = selMic;
    using R = SampleModel::Redo;
    if (key.rfind ("al.", 0) == 0)
    {
        // arrival and the phase EQ's bands, by hand, on the selected mic: every hit of it
        if (! model.change ([key, v, mic] (hl::Session& S)
            {
                if (mic < 0 || mic >= (int) S.tracks.size()) return;
                const auto& tr = S.tracks[(size_t) mic];
                const int b = key.back() == '1' ? 1 : 0;
                if (key == "al.lag") hl::setLag (S, mic, v * S.sr / 1000.0);
                else if (b <= (int) tr.ap.size())
                {
                    const hl::AllPass cur = b < (int) tr.ap.size() ? tr.ap[(size_t) b] : hl::AllPass { 200, 0.7 };
                    hl::setAllPass (S, mic, b, key[3] == 'f' ? std::pow (2.0, v) : cur.fc, key[3] == 'q' ? v : cur.Q);
                }
            }, R::align, key)) setStatus ("Auto Prep is running: try again when it finishes.");
        return;
    }
    const R redo = key.rfind ("lev.", 0) == 0 || key == "exp.ceil" ? R::levels : key == "tune.cents" ? R::tune : R::mix;
    const bool ok = model.change ([key, v, mic] (hl::Session& S)
    {
        const bool m = mic >= 0 && mic < (int) S.tracks.size();
        hl::ToneMoves& T = m ? S.tracks[(size_t) mic].strip.tone : (hl::ToneMoves&) S.tone;
        hl::ShapeMoves& H = m ? S.tracks[(size_t) mic].strip.shape : S.shape;
        if (key == "tone.match") T.match = v;
        else if (key == "tone.bright") T.bright = v;
        else if (key == "tone.crack") T.crack = v;
        else if (key == "tone.smooth") T.smooth = v;
        else if (key == "tone.tame") T.tame = v;
        else if (key == "shape.atk") H.atk = v;
        else if (key == "shape.dec") H.dec = v;
        else if (key == "shape.sus") H.sus = v;
        else if (key == "shape.rel") H.rel = v;
        else if (key == "shape.match") H.match = v;
        else if (key == "ref.amt") S.tone.refAmt = v;
        else if (key == "ref.env") S.tone.refEnvAmt = v;
        else if (key == "clean.db") S.dnDb = v;
        else if (key == "lev.step") S.lev.stepDb = v;
        else if (key == "lev.range") S.lev.rangeDb = v;
        else if (key == "lev.offset") S.lev.offsetDb = v;
        else if (key == "tune.cents") S.tune.cents = v;
        else if (key == "exp.ceil") S.exp.ceilDb = v;
    }, redo, key);   // one drag undoes as one step
    if (! ok) setStatus ("Auto Prep is running: try again when it finishes.");
}

void HitsView::slider (Canvas& c, float x, float y, float w, const std::string& label, const std::string& key)
{
    auto it = ranges().find (key);
    if (it == ranges().end()) return;
    const auto [lo, hi] = it->second;
    const double v = sliderValue (key);
    const bool bi = lo < 0 && hi > 0;
    txt (c, label, bodyFont (12), ink2, x, y, 120, 22);
    const float tx = x + 124, tw = w - 124 - 70;
    c.setColor (0xff121213); c.roundedRectangle (tx, y + 9, tw, 4, 2);
    const float zero = bi ? (float) ((0 - lo) / (hi - lo)) : 0.0f, pos = (float) ((v - lo) / (hi - lo));
    c.setColor (accent);
    c.roundedRectangle (tx + tw * std::min (zero, pos), y + 9, std::max (2.0f, tw * std::abs (pos - zero)), 4, 2);
    c.setColor (drag == Drag::slider && sliderKey == key ? accentInk : 0xffd8d8da);
    c.circle (tx + tw * pos - 7, y + 4, 14);
    std::string vs;
    if (key == "clean.db" || key.rfind ("lev.", 0) == 0 || key == "exp.ceil") vs = fmt (key == "lev.offset" || key == "exp.ceil" ? "%+.1f dB" : "%.1f dB", v);
    else if (key == "tune.cents") vs = fmt ("%+.0f ct", v);
    else vs = pct (v, bi);
    txt (c, vs, monoFont (11), ink, x + w - 64, y, 64, 22, visage::Font::kRight);
    add (tx - 8, y - 2, tw + 16, 26, "sl:" + key);
}

/** The hits the view covers, as chips to pick from (so a step that shows one hit at a time still reaches the rest). */
void HitsView::hitStrip (Canvas& c, float x, float y, float w)
{
    const auto& H = model.hits();
    if (H.empty()) return;
    const int sel = std::clamp (selHit < 0 ? 0 : selHit, 0, (int) H.size() - 1);
    const std::string what = playMode == 0 ? "Hit" : playMode == 1 ? "Velocity layer " + std::to_string (H[(size_t) sel].vel + 1) : "All layers";
    txt (c, what, boldFont (tok::tSmall), tok::ink2, x, y, 120, 20);
    const float cx0 = x + 124, cw = 26, room = w - 124 - 64;   // room kept for "8 of 40"
    const auto ids = viewHits (H, sel, playMode, std::max (1, (int) (room / (cw + 3))));
    const auto all = viewHits (H, sel, playMode);
    float bx = cx0;
    for (int i : ids)
    {
        const unsigned col = velColour[H[(size_t) i].vel % nVelColours] | 0xff000000;
        c.setColor (i == sel ? tok::coralFill : draw::withAlpha (col, 0.35f)); c.roundedRectangle (bx, y, cw, 20, 3);
        txt (c, std::to_string (i + 1), boldFont (tok::tCaption), i == sel ? 0xffffffff : tok::ink, bx, y, cw, 20, visage::Font::kCenter);
        add (bx, y, cw, 20, "hit:" + std::to_string (i));
        bx += cw + 3;
    }
    if (ids.size() < all.size()) txt (c, std::to_string (ids.size()) + " of " + std::to_string (all.size()), bodyFont (tok::tCaption), tok::ink3, bx + 4, y, 60, 20);
}

/** What the controls beside it change, in words: the view never decides that. */
void HitsView::scopeNote (Canvas& c, float x, float y, float w, const std::string& text)
{
    c.setColor (tok::coral); c.circle (x, y + 6, 6);
    txt (c, text, bodyFont (tok::tSmall), tok::ink2, x + 12, y, w - 12, 18);
}

std::string HitsView::valueText (const std::string& key, double v)
{
    const auto r = ranges().at (key);
    if (key == "clean.db" || key.rfind ("lev.", 0) == 0 || key == "exp.ceil") return fmt (key == "lev.offset" || key == "exp.ceil" ? "%+.1f dB" : "%.1f dB", v);
    if (key == "tune.cents") return fmt ("%+.0f ct", v);
    if (key == "al.lag") return fmt ("%.2f ms", v);
    if (key.rfind ("al.f", 0) == 0) return fmt ("%.0f Hz", std::pow (2.0, v));
    if (key.rfind ("al.q", 0) == 0) return fmt ("%.2f", v);
    return pct (v, r.lo < 0 && r.hi > 0);
}

/** A processing amount: the scalloped knob, its name above, its value below. Drag up or right (Shift: fine),
    Option-click to reset, click the value to type one (Return keeps it, Esc doesn't). */
void HitsView::knobCtl (Canvas& c, float cx, float cy, float d, const std::string& label, const std::string& key)
{
    auto it = ranges().find (key);
    if (it == ranges().end()) return;
    const auto [lo, hi] = it->second;
    const double v = sliderValue (key);
    const bool editing = editKey == key;
    knob (c, cx, cy, d, (float) ((v - lo) / (hi - lo)), label, editing ? std::string() : valueText (key, v), lo < 0 && hi > 0, drag == Drag::slider && sliderKey == key);
    const float vy = cy + d / 2 + 3, vw = 76;
    if (editing)
    {
        c.setColor (tok::lane); c.roundedRectangle (cx - vw / 2, vy, vw, 20, 3);
        focusRing (c, cx - vw / 2, vy, vw, 20);
        txt (c, editText + "|", monoFont (tok::tLabel), tok::ink, cx - vw / 2, vy, vw, 20, visage::Font::kCenter);
    }
    add (cx - d / 2, cy - d / 2, d, d, "sl:" + key);
    add (cx - vw / 2, vy, vw, 20, "ed:" + key);
}

void HitsView::commitEdit (bool keep)
{
    if (editKey.rfind ("name:", 0) == 0)
    {
        // a track's name (empty: back to the recording's own)
        if (keep)
        {
            const auto err = model.renameTrack (std::stoi (editKey.substr (5)), editText);
            if (err.isNotEmpty()) setStatus (err.toStdString());
        }
        editKey.clear(); editText.clear();
        redraw();
        return;
    }
    if (keep && ! editText.empty() && (editKey.rfind ("mon:", 0) == 0 || editKey.rfind ("pan:", 0) == 0))
    {
        // a track's monitor gain (dB) or pan (-100 left .. 100 right)
        const int t = std::stoi (editKey.substr (4));
        char* end = nullptr;
        const double typed = std::strtod (editText.c_str(), &end);
        if (end == editText.c_str() || ! std::isfinite (typed)) setStatus ("That isn't a number.");
        else if (t >= 0 && t < (int) model.tracks().size())
        {
            if (editKey[0] == 'm') model.setMonDb (t, (float) std::clamp (typed, -90.0, 6.0));
            else model.setPan (t, (float) std::clamp (typed / 100.0, -1.0, 1.0));
        }
    }
    else if (keep && ! editKey.empty() && ! editText.empty())
    {
        const auto r = ranges().at (editKey);
        const bool asIs = editKey == "tune.cents" || editKey == "al.lag" || editKey.rfind ("al.q", 0) == 0 || valueText (editKey, 0).find ("dB") != std::string::npos;
        char* end = nullptr;
        const double typed = std::strtod (editText.c_str(), &end);
        // typed in the units shown: Hz for a band's frequency, percent for the amounts, otherwise the number itself
        const double v = editKey.rfind ("al.f", 0) == 0 ? std::log2 (std::max (1.0, typed)) : asIs ? typed : typed / 100;
        if (end != editText.c_str() && std::isfinite (typed)) setSliderValue (editKey, std::clamp (v, r.lo, r.hi));
        else setStatus ("That isn't a number.");
    }
    editKey.clear(); editText.clear();
    redraw();
}

// ------------------------------------------------------------------------------------------------ dispatch
void HitsView::drawStep (Canvas& c)
{
    const float y = tbH + auH;
    c.setColor (bar3); c.fill (0, y, width(), sbH);
    c.setColor (line); c.fill (0, y + sbH - 1, width(), 1);
    if (! model.isOpen())
    {
        txt (c, "Drop the recordings of one take anywhere, or press Cmd-O.", bodyFont (14), ink2, 0, y + 120, width(), 30, visage::Font::kCenter);
        add (0, y + sbH, width(), height() - y - sbH, "open");
        return;
    }
    switch (step)
    {
        case 0: drawSetup (c); break;
        case 2: drawAlign (c); break;
        case 3: case 4: case 5: case 6:
        {
            // the tracks stay down the left with their live monitor controls; the step works in what's right of them
            drawTrackColumn (c);
            stepX = headW;
            const size_t first = hits.size();
            c.saveState();
            c.setPosition (stepX, 0);
            if (step == 3) drawTune (c); else if (step == 4) drawTone (c); else if (step == 5) drawClean (c); else drawLevel (c);
            c.restoreState();
            for (size_t i = first; i < hits.size(); ++i) hits[i].x += stepX;
            barEnd += stepX;
            stepX = 0;
            break;
        }
        case 7: drawExport (c); break;
        default: break;
    }
    // the shared status line: what's left of the step bar, right-aligned (dropped when there's no room)
    std::string st = status;
    if (model.isRendering()) st = "Rendering the change\xe2\x80\xa6 (playback carries on)";
    const float room = width() - 14 - (barEnd + 16);
    if (room > 160) txt (c, st, bodyFont (12), 0xffffc78a, barEnd + 16, y, room, sbH, visage::Font::kRight);
}

/** The track list on the steps whose centre isn't waveform lanes (Tune, Tone, Clean, Level): the same headers,
    the same live monitor controls. Clicking a row picks the mic the step looks at. */
void HitsView::drawTrackColumn (Canvas& c)
{
    const float top = tbH + auH, y0 = top + sbH, rowH = 58, bottom = height();
    const int T = (int) model.tracks().size();
    txt (c, "TRACKS \xc2\xb7 monitoring only", boldFont (10), tok::ink3, 13, top, headW - 20, sbH);
    c.setColor (tok::hairline); c.fill (headW - 1, top, 1, sbH - 1);
    c.setColor (head); c.fill (0, y0, headW, bottom - y0);
    rowScroll = std::clamp (rowScroll, 0.0f, std::max (0.0f, T * rowH - (bottom - y0)));
    const size_t first = hits.size();
    c.setClampBounds (0, y0, headW, bottom - y0);
    for (int t = 0; t < T; ++t)
    {
        const float y = y0 + t * rowH - rowScroll;
        if (y + rowH <= y0 || y >= bottom) continue;
        add (0, y, headW - 14, rowH, "row:" + std::to_string (t));
        drawTrackHeader (c, t, y, rowH, false);
    }
    c.setClampBounds (0, 0, width(), height());
    clipHits (first, y0, bottom);
    c.setColor (tok::hairline); c.fill (headW - 1, y0, 1, bottom - y0);
}

// ------------------------------------------------------------------------------------------------ 1 Setup
void HitsView::drawSetup (Canvas& c)
{
    const auto* S = model.session();
    const float top = tbH + auH, y0 = top + sbH;
    txt (c, "The take: " + model.takeName().toStdString(), boldFont (13), ink, 14, top, 500, sbH);
    barEnd = 14 + textWidth (boldFont (13), "The take: " + model.takeName().toStdString());
    // the recordings
    const float sideW = tok::goldenMinor (width() - 42, 380.0f, 560.0f);   // golden split: recordings : session
    const float px = 14, pw = width() - 14 - sideW - 28;
    panel (c, px, y0 + 14, pw, height() - y0 - 28, "Recordings");
    const float cols[] = { px + 16, px + 250, px + 380, px + 470, px + 560 };
    const char* heads[] = { "Track", "Channels", "Length", "Peak", "Lead" };
    for (int i = 0; i < 5; ++i) txt (c, heads[i], bodyFont (11), ink3, cols[i], y0 + 40, 120, 16);
    float y = y0 + 60;
    for (int t = 0; t < (int) model.tracks().size(); ++t)
    {
        const auto& tr = model.tracks()[(size_t) t];
        const bool lead = t == model.leadTrack();
        c.setColor (t % 2 ? 0xff262628 : 0xff2a2a2d); c.fill (px + 6, y, pw - 12, 30);
        c.setColor (tr.colour); c.fill (px + 6, y, 4, 30);
        txt (c, tr.name, boldFont (12), ink, cols[0], y, 230, 30);
        c.setColor (tr.channels > 1 ? 0xff1f3f5c : 0xff38383c); c.roundedRectangle (cols[1], y + 7, 56, 16, 3);
        txt (c, tr.channels > 1 ? "Stereo" : "Mono", boldFont (10), tr.channels > 1 ? 0xffbcdcff : 0xffd6d6d8, cols[1], y + 7, 56, 16, visage::Font::kCenter);
        txt (c, fmtTime (model.length()), monoFont (11), ink2, cols[2], y, 90, 30);
        txt (c, fmt ("%.1f dB", tr.peakDb), monoFont (11), ink2, cols[3], y, 90, 30);
        c.setColor (lead ? accent : 0xff55555a); c.ring (cols[4], y + 8, 14, lead ? 4.0f : 1.5f);
        add (cols[4] - 4, y, 120, 30, "lead:" + std::to_string (t));
        if (lead) txt (c, "everything lines up to it", bodyFont (11), accentInk, cols[4] + 22, y, px + pw - (cols[4] + 22) - 10, 30);
        y += 32;
    }
    // the session
    const float sx = width() - sideW - 14;
    panel (c, sx, y0 + 14, sideW, height() - y0 - 28, "Session");
    if (! S) return;
    y = y0 + 44;
    txt (c, "Drum", bodyFont (12), ink3, sx + 16, y, 100, 22);
    static const std::vector<std::string> drums { "Kick", "Snare", "Tom", "Hat", "Ride", "Crash" };
    int di = -1;
    for (size_t i = 0; i < drums.size(); ++i) if (juce::String (drums[i]).equalsIgnoreCase (juce::String (S->drum))) di = (int) i;
    segmented (c, sx + 16, y + 22, drums, di, "drum:", 26);
    y += 64;
    txt (c, "Articulation: " + (S->artic.empty() ? std::string ("(from the file names)") : S->artic), bodyFont (12), ink2, sx + 16, y, sideW - 40, 22);
    y += 34;
    for (int k = 0; k < 2; ++k)
    {
        const int v = k ? S->det.RR : S->det.V;
        txt (c, k ? "Round robins" : "Velocities", bodyFont (12), ink3, sx + 16, y, 120, 26);
        c.setColor (btn); c.roundedRectangle (sx + 150, y, 28, 26, 3); txt (c, "\xe2\x88\x92", boldFont (13), ink, sx + 150, y, 28, 26, visage::Font::kCenter);
        add (sx + 150, y, 28, 26, k ? "rr:-1" : "v:-1");
        c.setColor (field); c.roundedRectangle (sx + 182, y, 46, 26, 3); txt (c, std::to_string (v), monoFont (13), ink, sx + 182, y, 46, 26, visage::Font::kCenter);
        c.setColor (btn); c.roundedRectangle (sx + 232, y, 28, 26, 3); txt (c, "+", boldFont (13), ink, sx + 232, y, 28, 26, visage::Font::kCenter);
        add (sx + 232, y, 28, 26, k ? "rr:1" : "v:1");
        y += 34;
    }
    txt (c, "Order", bodyFont (12), ink3, sx + 16, y, 100, 26);
    segmented (c, sx + 150, y - 2, { "Softest first", "Hardest first" }, S->det.order == "loud" ? 1 : 0, "order:", 24);
    y += 36;
    txt (c, "Room mics", bodyFont (12), ink3, sx + 16, y, 100, 26);
    segmented (c, sx + 150, y - 2, { "As recorded", "Keep distance, fix phase" }, model.roomPhase() ? 1 : 0, "rooms:", 24);
    y += 40;
    txt (c, fmt ("%.0f Hz", S->sr) + " \xc2\xb7 " + std::to_string (S->tracks.size()) + " tracks \xc2\xb7 " + fmtTime (model.length()), bodyFont (12), ink3, sx + 16, y, sideW - 40, 22);
    y += 34;
    const int want = S->det.V * S->det.RR;
    txt (c, model.hits().size() == (size_t) want ? "Every hit found (" + std::to_string (want) + ")." : "Expecting " + std::to_string (want) + " hits; found " + std::to_string (model.hits().size()) + ".",
         bodyFont (12), model.hits().size() == (size_t) want ? tok::mintInk : 0xffffc78a, sx + 16, y, sideW - 40, 22);
    y += 34;
    c.setColor (visage::Brush::linear (visage::Color (tok::coralFillTop), visage::Color (tok::coralFill), visage::Point (0, y), visage::Point (0, y + 32)));
    c.roundedRectangle (sx + 16, y, 160, 32, 4);
    txt (c, model.isPrepping() ? "Working\xe2\x80\xa6" : "Auto Prep", boldFont (13), 0xffffffff, sx + 16, y, 160, 32, visage::Font::kCenter);
    add (sx + 16, y, 160, 32, "autoprep");
}

// ------------------------------------------------------------------------------------------------ 3 Align
void HitsView::drawAlign (Canvas& c)
{
    const auto* S = model.session();
    if (! S) return;
    const float top = tbH + auH;
    const int T = (int) S->tracks.size();
    if (selMic < 0 || selMic >= T || selMic == S->refIdx) selMic = T > 1 ? (S->refIdx == 0 ? 1 : 0) : 0;
    // the bar: lead, timing, units, nudge, align all
    float x = 14;
    txt (c, "Lead", bodyFont (12), ink3, x, top, 40, sbH); x += 40;
    c.setColor (field); c.roundedRectangle (x, top + 6, 150, 26, 3);
    txt (c, model.tracks()[(size_t) S->refIdx].name + "  \xe2\x96\xbe", boldFont (12), ink, x + 8, top + 6, 140, 26);
    add (x, top + 6, 150, 26, "leadnext"); x += 164;
    txt (c, "Timing", bodyFont (12), ink3, x, top, 50, sbH); x += 52;
    const int tm = S->timing == "pro" ? 1 : S->timing == "natural" ? 2 : 0;
    segmented (c, x, top + 4, { "Tight", "Standard", "As recorded" }, tm, "timing:", 26); x += segmentedW ({ "Tight", "Standard", "As recorded" }) + 12;
    segmented (c, x, top + 4, { "ms", "samples", "feet" }, alignUnits, "units:", 26); x += segmentedW ({ "ms", "samples", "feet" }) + 12;
    txt (c, "Nudge " + model.tracks()[(size_t) selMic].name, bodyFont (12), ink3, x, top, 150, sbH); x += std::min (150.0f, textWidth (bodyFont (12), "Nudge " + model.tracks()[(size_t) selMic].name) + 8);
    const char* nl[4] = { "\xe2\x88\x92" "1 smp", "\xe2\x88\x92\xc2\xbc", "+\xc2\xbc", "+1 smp" };
    const int nq[4] = { -4, -1, 1, 4 };
    for (int i = 0; i < 4; ++i)
    {
        const float bw = i == 0 || i == 3 ? 58.f : 36.f;
        c.setColor (btn); c.roundedRectangle (x, top + 6, bw, 26, 3);
        txt (c, nl[i], boldFont (11), ink, x, top + 6, bw, 26, visage::Font::kCenter);
        add (x, top + 6, bw, 26, "nudge:" + std::to_string (nq[i]));
        x += bw + 4;
    }
    x += 8;
    button (c, x, top + 6, 70, 26, "Details", alignDetails, tok::coralFill);
    add (x, top + 6, 70, 26, "aldetails");
    x += 70;
    barEnd = x;
    // rows
    const float colW = 380, lx = headW, lw = std::max (200.0f, width() - headW - colW), y0 = top + sbH;
    const float rowH = std::clamp ((height() - y0 - 40 - 24) / std::max (1, T), 78.0f, 120.0f);   // room for the whole header
    const auto& H = model.hits();
    const int hi = std::clamp (selHit < 0 ? 0 : selHit, 0, std::max (0, (int) H.size() - 1));
    const float rowsTop = y0 + 24, rowsBot = height() - 34;
    rowScroll = std::clamp (rowScroll, 0.0f, std::max (0.0f, T * rowH - (rowsBot - rowsTop)));   // rows that don't fit scroll
    const size_t firstHit = hits.size();
    auto columnHeads = [&]
    {
        c.setColor (ruler); c.fill (0, y0, width(), 24);
        txt (c, H.empty() ? std::string ("No hits yet") : "Hit " + std::to_string (hi + 1) + " \xc2\xb7 from the lead's hit", bodyFont (11), ink3, 12, y0, headW - 12, 24);
        txt (c, "\xe2\x88\x92" "2 ms", monoFont (10), ink3, lx + 4, y0, 60, 24);
        txt (c, "+30 ms", monoFont (10), ink3, lx + lw - 64, y0, 60, 24, visage::Font::kRight);
        txt (c, "Status", bodyFont (11), ink3, lx + lw + 12, y0, 100, 24);
        txt (c, "Delay \xc2\xb7 \xc3\x98 \xc2\xb7 EQ", bodyFont (11), ink3, lx + lw + 120, y0, 120, 24);
        txt (c, "Sums with the lead", bodyFont (11), ink3, lx + lw + 250, y0, 130, 24);
    };
    const double sr = S->sr, T0 = -0.002, T1 = 0.030;
    const long on = H.empty() ? 0 : (long) std::llround (H[(size_t) hi].time * sr);
    const auto& leadCh = S->tracks[(size_t) S->refIdx].ch.get();
    float leadPk = 1e-6f;
    for (long n = on + (long) (T0 * sr); n < on + (long) (T1 * sr); ++n) if (n >= 0 && n < (long) leadCh[0].size()) leadPk = std::max (leadPk, std::abs (leadCh[0][(size_t) n]));
    c.setClampBounds (0, rowsTop, width(), rowsBot - rowsTop);   // rows scrolled part-way stay inside their area
    for (int t = 0; t < T; ++t)
    {
        const float y = rowsTop + t * rowH - rowScroll;
        if (y + rowH <= rowsTop || y >= rowsBot) continue;
        const auto& tr = S->tracks[(size_t) t];
        const bool isLead = t == S->refIdx;
        drawTrackHeader (c, t, y, rowH, true);
        c.setColor (line); c.fill (lx, y + rowH - 1, width() - lx, 1);
        if (! isLead)
        {
            const bool apOn = tr.apOn && ! tr.ap.empty();
            c.setColor (apOn ? accentDark : 0xff3a3a3e); c.roundedRectangle (13, y + 56, 74, 18, 9);
            txt (c, "Phase EQ", bodyFont (10), apOn ? accentInk : ink2, 13, y + 56, 74, 18, visage::Font::kCenter);
            add (13, y + 56, 74, 18, "apeq:" + std::to_string (t));
            c.setColor (0xff3a3a3e); c.roundedRectangleBorder (93, y + 56, 52, 18, 9, 1);
            txt (c, "Reset", bodyFont (10), ink2, 93, y + 56, 52, 18, visage::Font::kCenter);
            add (93, y + 56, 52, 18, "reset:" + std::to_string (t));
        }
        add (0, y, lx - 14, rowH, "row:" + std::to_string (t));   // the header's own controls are smaller, so they win
        // the overlay: the lead filled grey, this mic as recorded (dashed), where it sits now (its colour)
        { const float cy0 = std::max (y, rowsTop), cy1 = std::min (y + rowH - 1, rowsBot); c.setClampBounds (lx, cy0, lw, std::max (0.0f, cy1 - cy0)); }
        c.setColor (lane); c.fill (lx, y, lw, rowH - 1);
        const float mid = y + rowH / 2, amp = rowH * 0.42f;
        const auto& ch = tr.ch.get()[0];
        float pk = 1e-6f;
        for (long n = on + (long) (T0 * sr) - 200; n < on + (long) (T1 * sr) + 200; ++n) if (n >= 0 && n < (long) ch.size()) pk = std::max (pk, std::abs (ch[(size_t) n]));
        auto at = [&] (const hl::F32& x, double pos) -> float
        {
            const long i0 = (long) std::floor (pos);
            const float f = (float) (pos - (double) i0);
            if (i0 < 0 || i0 + 1 >= (long) x.size()) return 0.0f;
            return x[(size_t) i0] * (1 - f) + x[(size_t) i0 + 1] * f;
        };
        const int N = (int) lw;
        const float zx = lx + (float) ((0 - T0) / (T1 - T0)) * lw;
        c.setColor (0xff2e2e31); c.fill (zx, y, 1, rowH - 1);
        if (! isLead)
        {
            c.setColor (0x40ffffff);
            for (int i = 0; i < N; ++i)
            {
                const double tt = T0 + (T1 - T0) * i / N;
                const float v = at (leadCh[0], on + tt * sr) / leadPk;
                c.fill (lx + i, std::min (mid, mid - v * amp), 1, std::abs (v * amp) + 1);
            }
        }
        const double lag = isLead ? 0.0 : tr.lag;
        const int pol = tr.pol < 0 ? -1 : 1;
        if (playMode != 0)
        {
            // the other hits the view covers, where they sit now, faint (each scaled to its own peak; at most 8)
            c.setColor (draw::withAlpha (0xff000000 | (trackColour (model, t) & 0xffffff), 0.28f));
            for (int oh : viewHits (H, hi, playMode, 8))
            {
                if (oh == hi) continue;
                const long on2 = (long) std::llround (H[(size_t) oh].time * sr);
                float pk2 = 1e-6f;
                for (long n = on2 + (long) (T0 * sr); n < on2 + (long) (T1 * sr); ++n) if (n >= 0 && n < (long) ch.size()) pk2 = std::max (pk2, std::abs (ch[(size_t) n]));
                float py = mid;
                for (int i = 0; i < N; i += 2)
                {
                    const double tt = T0 + (T1 - T0) * i / N;
                    const float yy = mid - std::clamp (at (ch, on2 + tt * sr + lag) / pk2 * (float) pol, -1.05f, 1.05f) * amp;
                    if (i > 0) c.fill (lx + i, std::min (py, yy), 2.0f, std::abs (yy - py) + 1.0f);
                    py = yy;
                }
            }
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            if (pass == 0 && isLead) continue;
            c.setColor (pass == 0 ? 0x8cffffff : (0xff000000 | (trackColour (model, t) & 0xffffff)));
            float py = mid;
            for (int i = 0; i < N; ++i)
            {
                const double tt = T0 + (T1 - T0) * i / N;
                const double pos = on + tt * sr + (pass == 1 ? lag : 0.0);
                const float v = at (ch, pos) / pk * (pass == 1 ? (float) pol : 1.0f);
                const float yy = mid - std::clamp (v, -1.05f, 1.05f) * amp;
                if (pass == 0 && (i / 4) % 2) { py = yy; continue; }   // dashed
                if (i > 0) c.fill (lx + i, std::min (py, yy), 1.5f, std::abs (yy - py) + 1.5f);
                py = yy;
            }
        }
        c.setClampBounds (0, rowsTop, width(), rowsBot - rowsTop);
        // the columns (the details panel takes their place when it's open)
        const float cx = lx + lw;
        if (alignDetails) continue;
        c.setColor (bar2); c.fill (cx, y, colW, rowH - 1);
        const bool inPhase = tr.alignTo == "phase";   // a room kept in time, set in phase
        std::string stTxt = isLead ? "LEAD" : tr.alignTo == "none" ? "AS RECORDED" : tr.manualLag ? "BY HAND" : inPhase ? "IN PHASE" : "ALIGNED";
        const unsigned stCol = isLead ? accent : tr.alignTo == "none" ? 0xff8a8a90 : 0xff4fc28a;
        c.setColor (draw::withAlpha (stCol, 0.2f)); c.roundedRectangle (cx + 12, y + 10, 96, 18, 3);
        txt (c, stTxt, boldFont (10), stCol, cx + 12, y + 10, 96, 18, visage::Font::kCenter);
        txt (c, isLead ? std::string ("the others follow it") : tr.alignTo == "none" ? std::string ("its own arrival") : inPhase ? std::string ("distance kept") : "to " + model.tracks()[(size_t) S->refIdx].name,
             bodyFont (10), ink3, cx + 12, y + 30, 104, 16);
        const double ms = lag / sr * 1000.0;
        std::string d = alignUnits == 0 ? fmt ("%.2f ms", ms) : alignUnits == 1 ? fmt ("%.2f smp", lag) : fmt ("%.2f ft", ms * 1.125);
        txt (c, d, monoFont (13), ink, cx + 120, y + 8, 130, 20);
        std::string det = std::string (tr.pol < 0 ? "\xc3\x98 on" : "\xc3\x98 off");
        if (! tr.ap.empty() && tr.apOn) det += " \xc2\xb7 EQ " + fmt ("%.0f Hz", tr.ap[0].fc) + (tr.ap.size() > 1 ? fmt (" + %.0f", tr.ap[1].fc) : std::string());
        txt (c, det, monoFont (10), ink3, cx + 120, y + 30, 130, 16);
        bool stale = false;
        const auto sums = isLead ? std::make_pair (std::nan (""), std::nan ("")) : model.sumScore (t, &stale);
        if (! isLead && stale && ! (std::isfinite (sums.first) && std::isfinite (sums.second)))
            txt (c, "Measuring\xe2\x80\xa6", bodyFont (10), ink3, cx + 250, y + 9, 120, 14);
        if (std::isfinite (sums.first) && std::isfinite (sums.second))
        {
            // measured where it sits now (every nudge, flip and phase EQ counts), against as recorded
            const double before = sums.second * 100, after = sums.first * 100;
            for (int k = 0; k < 2; ++k)
            {
                const double v = std::clamp (k ? after : before, -100.0, 100.0);
                const float by = y + 12 + k * 18, bx = cx + 292, bw = 50;
                txt (c, k ? "Now" : "As rec.", bodyFont (10), ink3, cx + 250, by - 3, 50, 14);
                c.setColor (0xff161617); c.fill (bx, by, bw, 8);
                c.setColor (stale ? 0xff5a5a60 : v >= 0 ? 0xff3fbf78 : 0xffd4544b);   // grey while the new score is measured
                c.fill (v >= 0 ? bx + bw / 2 : bx + bw / 2 - (float) (-v / 100 * bw / 2), by, (float) (std::abs (v) / 100 * bw / 2), 8);
                c.setColor (0xff8a8a90); c.fill (bx + bw / 2, by - 1, 1, 10);
                txt (c, stale ? std::string ("\xe2\x80\xa6") : fmt ("%+.0f", v), monoFont (10), ink2, bx + bw + 4, by - 3, 30, 14);
            }
        }
    }
    c.setClampBounds (0, 0, width(), height());
    c.setClampBounds (0, 0, width(), height());
    clipHits (firstHit, rowsTop, rowsBot);
    columnHeads();   // over any row scrolled part-way under them
    if (alignDetails)
    {
        c.setColor (ruler); c.fill (lx + lw, y0, colW, 24);
        drawAlignDetails (c, lx + lw, rowsTop, colW, rowsBot - rowsTop);
    }
    if (T * rowH > rowsBot - rowsTop)
    {
        const float vh = rowsBot - rowsTop, th = vh * vh / (T * rowH);
        c.setColor (0x66ffffff); c.roundedRectangle (lx - 3, rowsTop + (vh - th) * rowScroll / (T * rowH - vh), 3, th, 1.5f);
    }
    // legend
    const float ly = height() - 34;
    c.setColor (bar2); c.fill (0, ly, width(), 34);
    c.setColor (0x40ffffff); c.fill (14, ly + 12, 22, 10);
    txt (c, "What it lines up to", bodyFont (11), ink2, 42, ly, 140, 34);
    c.setColor (0x8cffffff); for (int i = 0; i < 3; ++i) c.fill (190 + i * 8, ly + 16, 5, 2);
    txt (c, "As recorded", bodyFont (11), ink2, 220, ly, 100, 34);
    c.setColor (0xff4f9be8); c.fill (320, ly + 16, 22, 2);
    txt (c, "Where it sits now: shift and polarity (the phase EQ is heard, not drawn)", bodyFont (11), ink2, 348, ly, 420, 34);
    txt (c, playMode == 0 ? "Sums: \xe2\x88\x92" "100 fights, +100 adds fully. Tab: next hit." : "Faint: other hits in view (own peak). Applies to every hit.", bodyFont (11), ink3, width() - 350, ly, 336, 34, visage::Font::kRight);
}

/** Align's details for the selected mic, beside the lanes: arrival, polarity and the phase EQ's real bands, and the
    phase the implemented filters actually have. Everything here applies to every hit of that mic. */
void HitsView::drawAlignDetails (Canvas& c, float x, float y, float w, float h)
{
    const auto* S = model.session();
    if (! S || selMic < 0 || selMic >= (int) S->tracks.size()) return;
    const auto& tr = S->tracks[(size_t) selMic];
    c.setColor (tok::panel); c.fill (x, y, w, h);
    c.setColor (tok::hairline); c.fill (x, y, 1, h);
    const float px = x + 16, pw = w - 32;
    float yy = y + 10;
    txt (c, model.tracks()[(size_t) selMic].name, boldFont (tok::tTitle), tok::ink, px, yy, pw - 60, 22);
    txt (c, tr.manualLag || tr.manualAp || tr.manualPol ? "BY HAND" : "AS MEASURED", boldFont (tok::tCaption), tr.manualLag || tr.manualAp || tr.manualPol ? tok::coralInk : tok::mintInk, px, yy + 2, pw, 18, visage::Font::kRight);
    yy += 26;
    scopeNote (c, px, yy, pw, "Applies to every hit of this mic, whatever is on show.");
    yy += 26;
    const float kd = 55, col = pw / 5;
    knobCtl (c, px + col * 0.5f, yy + 18 + kd / 2, kd, "Arrival", "al.lag");
    // polarity and the phase EQ switch beside it
    toggleSwitch (c, px + col * 1.2f, yy + 22, tr.pol < 0, "Polarity inverted", 130);
    add (px + col * 1.2f, yy + 18, 170, 28, "flipm:" + std::to_string (selMic));
    const bool apOn = tr.apOn && ! tr.ap.empty();
    toggleSwitch (c, px + col * 1.2f, yy + 54, tr.apOn, "Phase EQ", 130);
    add (px + col * 1.2f, yy + 50, 170, 28, "apsw:" + std::to_string (selMic));
    txt (c, "Arrival: how much earlier the mic is played.", bodyFont (tok::tCaption), tok::ink3, px, yy + 18 + kd + 24, pw, 14);
    yy += 18 + kd + 44;
    // the bands that really exist
    txt (c, "PHASE EQ BANDS", boldFont (tok::tCaption), tok::ink3, px, yy, 200, 16);
    if (tr.ap.size() < 2 && tr.apOn)
    {
        c.setColor (tok::control); c.roundedRectangle (px + pw - 84, yy - 3, 84, 20, 10);
        txt (c, "Add a band", bodyFont (tok::tCaption), tok::ink, px + pw - 84, yy - 3, 84, 20, visage::Font::kCenter);
        add (px + pw - 84, yy - 3, 84, 20, "apadd:0");
    }
    yy += 20;
    if (tr.ap.empty())
    {
        txt (c, tr.apOn ? "None on this mic: measuring didn't find one that helps." : "Switched off for this mic.", bodyFont (tok::tSmall), tok::ink2, px, yy + 8, pw, 18);
        yy += 40;
    }
    else
    {
        for (size_t b = 0; b < tr.ap.size() && b < 2; ++b)
        {
            const float bx = px + (float) b * pw / 2;
            knobCtl (c, bx + col * 0.6f, yy + 18 + kd / 2, kd, b == 0 ? "Band 1" : "Band 2", b == 0 ? "al.f0" : "al.f1");
            knobCtl (c, bx + col * 1.75f, yy + 18 + kd / 2, kd, "Q", b == 0 ? "al.q0" : "al.q1");
            txt (c, "remove", bodyFont (tok::tCaption), tok::ink3, bx + col * 0.6f - 30, yy + 18 + kd + 26, 60, 14, visage::Font::kCenter);
            add (bx + col * 0.6f - 30, yy + 18 + kd + 26, 60, 14, "aprem:" + std::to_string (b));
        }
        yy += 18 + kd + 46;
    }
    // the phase of the filters as implemented (the same coefficients the audio path uses), wrapped to +-180 degrees
    const float gy = yy + 18, gh = y + h - gy - 22, gx = px + 30, gw = pw - 30;
    if (gh < 50) return;
    txt (c, "Phase of the phase EQ as it runs (degrees, wrapped)", bodyFont (tok::tCaption), tok::ink3, px, yy, pw, 14);
    c.setColor (tok::lane); c.fill (gx, gy, gw, gh);
    for (int d = -180; d <= 180; d += 90)
    {
        const float ly2 = gy + gh * (0.5f - d / 360.0f);
        c.setColor (d == 0 ? tok::divider : 0xff222225); c.fill (gx, ly2, gw, 1);
        txt (c, std::to_string (d), monoFont (9), tok::ink3, px - 4, ly2 - 7, 30, 14, visage::Font::kRight);
    }
    const double fLo = 20, fHi = 20000;
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const float fx = gx + gw * (float) (std::log (f / fLo) / std::log (fHi / fLo));
        c.setColor (0xff222225); c.fill (fx, gy, 1, gh);
        txt (c, f >= 1000 ? fmt ("%.0fk", f / 1000) : fmt ("%.0f", f), monoFont (9), tok::ink3, fx - 20, gy + gh + 2, 40, 14, visage::Font::kCenter);
    }
    if (apOn)
    {
        c.setColor (tok::coral);
        float prev = 0; bool have = false;
        for (int i = 0; i < (int) gw; ++i)
        {
            const double f = fLo * std::pow (fHi / fLo, (double) i / gw);
            if (f >= S->sr / 2) break;
            std::complex<double> H (1, 0);
            const std::complex<double> z1 = std::polar (1.0, -2 * M_PI * f / S->sr), z2 = z1 * z1;
            for (auto& a : tr.ap)
            {
                if (a.fc <= 0 || a.fc >= S->sr / 2) continue;
                const double w0 = 2 * M_PI * a.fc / S->sr, al = std::sin (w0) / (2 * std::max (0.05, a.Q)), cw = std::cos (w0), a0 = 1 + al;
                const double b0 = (1 - al) / a0, b1 = -2 * cw / a0;
                H *= (b0 + b1 * z1 + z2) / (1.0 + b1 * z1 + b0 * z2);
            }
            const float deg = (float) (std::arg (H) * 180 / M_PI), py = gy + gh * (0.5f - deg / 360.0f);
            if (have && std::abs (py - prev) < gh * 0.5f) c.fill (gx + i, std::min (prev, py), 1.5f, std::abs (py - prev) + 1.5f);   // no line across a wrap
            prev = py; have = true;
        }
    }
    else txt (c, "flat: no phase EQ on this mic", bodyFont (tok::tSmall), tok::ink3, gx, gy, gw, gh, visage::Font::kCenter);
}

// ------------------------------------------------------------------------------------------------ 4 Tune
void HitsView::drawTune (Canvas& c)
{
    const auto* S = model.session();
    if (! S) return;
    const float top = tbH + auH, y0 = top + sbH;
    float x = 14;
    c.setColor (S->tune.on ? accentDark : 0xff3a3a3e); c.roundedRectangle (x, top + 7, 90, 24, 12);
    txt (c, S->tune.on ? "Tuning on" : "Tuning off", boldFont (12), S->tune.on ? accentInk : ink2, x, top + 7, 90, 24, visage::Font::kCenter);
    add (x, top + 7, 90, 24, "tune:on"); x += 104;
    segmented (c, x, top + 4, { "One ratio for the drum", "Even out round robins" }, S->tune.mode == "rr" ? 1 : 0, "tunemode:", 26);
    x += segmentedW ({ "One ratio for the drum", "Even out round robins" }) + 14;
    const auto& ti = S->tuneInfo;
    if (ti && ti->valid)
    {
        txt (c, "Target", bodyFont (12), ink3, x, top, 46, sbH); x += 50;
        c.setColor (btn); c.roundedRectangle (x, top + 6, 26, 26, 3); txt (c, "\xe2\x88\x92", boldFont (12), ink, x, top + 6, 26, 26, visage::Font::kCenter); add (x, top + 6, 26, 26, "target:-1"); x += 30;
        c.setColor (field); c.roundedRectangle (x, top + 6, 110, 26, 3);
        txt (c, hl::noteName (ti->targetMidi) + fmt (" \xc2\xb7 %.1f Hz", ti->targetF), monoFont (11), ink, x, top + 6, 110, 26, visage::Font::kCenter); x += 114;
        c.setColor (btn); c.roundedRectangle (x, top + 6, 26, 26, 3); txt (c, "+", boldFont (12), ink, x, top + 6, 26, 26, visage::Font::kCenter); add (x, top + 6, 26, 26, "target:1"); x += 34;
        txt (c, S->tune.target == "nearest" ? "(nearest note)" : "back to nearest", bodyFont (11), ink3, x, top, 100, sbH);
        add (x, top, 100, sbH, "target:0");
        x += 100;
    }
    barEnd = x;
    const float sideW = tok::goldenMinor (stepW() - 42, 340.0f, 560.0f);   // golden split: pitches : the drum
    const float pw = stepW() - 28 - sideW;
    panel (c, 14, y0 + 14, pw, height() - y0 - 28, "Every hit's pitch");
    const float sx = stepW() - sideW - 14;
    panel (c, sx, y0 + 14, sideW, height() - y0 - 28, "The drum");
    if (! S->tuneAnalyzed || ! ti || ! ti->valid)
    {
        const bool cym = hl::soundsLikeCymbal (*S);
        txt (c, cym ? "This sounds like a cymbal: it has no steady pitch to tune, so tuning is left off." : "No steady pitch was found on the lead mic yet.",
             bodyFont (13), ink2, 34, y0 + 60, pw - 40, 22);
        c.setColor (btn); c.roundedRectangle (34, y0 + 96, 150, 28, 4);
        txt (c, "Measure the pitch", boldFont (12), ink, 34, y0 + 96, 150, 28, visage::Font::kCenter);
        add (34, y0 + 96, 150, 28, "tune:measure");
        return;
    }
    // the chart: before (hollow) and after (filled) per hit, the target line
    const float gx = 70, gy = y0 + 50, gw = pw - 80, gh = height() - y0 - 28 - 60 - 20;
    double lo = 1e9, hiF = 0;
    for (size_t i = 0; i < ti->f.size(); ++i) if (std::isfinite (ti->f[i])) { lo = std::min (lo, ti->f[i]); hiF = std::max (hiF, ti->f[i]); if (i < ti->ratios.size()) { lo = std::min (lo, ti->f[i] * ti->ratios[i]); hiF = std::max (hiF, ti->f[i] * ti->ratios[i]); } }
    if (std::isfinite (ti->targetF)) { lo = std::min (lo, ti->targetF); hiF = std::max (hiF, ti->targetF); }
    if (hiF <= lo) { hiF = lo + 1; }
    const double pad = (hiF - lo) * 0.15 + 0.5;
    lo -= pad; hiF += pad;
    auto yOf = [&] (double f) { return gy + gh - (float) ((f - lo) / (hiF - lo)) * gh; };
    const int N = (int) ti->f.size();
    for (int v = 0; v < model.velocities(); ++v)
    {
        int a = N, b = -1;
        for (int i = 0; i < N && i < (int) model.hits().size(); ++i) if (model.hits()[(size_t) i].vel == v) { a = std::min (a, i); b = std::max (b, i); }
        if (b < 0) continue;
        c.setColor (draw::withAlpha (velColour[v % nVelColours], 0.12f)); c.fill (gx + gw * a / N, gy, gw * (b - a + 1) / N, gh);
        txt (c, "Vel " + std::to_string (v + 1), boldFont (10), velColour[v % nVelColours] | 0xff000000, gx + gw * a / N + 4, gy + 2, 60, 14);
    }
    for (int k = 0; k <= 4; ++k)
    {
        const double f = lo + (hiF - lo) * k / 4;
        c.setColor (0xff333336); c.fill (gx, yOf (f), gw, 1);
        txt (c, fmt ("%.1f Hz", f), monoFont (10), ink3, 20, yOf (f) - 8, 48, 16);
    }
    if (std::isfinite (ti->targetF)) { c.setColor (accent); c.fill (gx, yOf (ti->targetF), gw, 1.5f); }
    for (int i = 0; i < N; ++i)
    {
        if (! std::isfinite (ti->f[(size_t) i])) continue;
        const float px = gx + gw * (i + 0.5f) / N;
        const bool sel = i == selHit;
        const bool shown = inView (model.hits(), selHit < 0 ? 0 : selHit, playMode, i);   // outside the view: still there, dimmed
        const visage::Color col = draw::withAlpha (i < (int) model.hits().size() ? velColour[model.hits()[(size_t) i].vel % nVelColours] | 0xff000000 : ink2, shown ? 1.0f : 0.25f);
        c.setColor (draw::withAlpha (0xff8a8a90, shown ? 1.0f : 0.3f)); c.ring (px - 4, yOf (ti->f[(size_t) i]) - 4, 8, 1.2f);
        if (i < (int) ti->ratios.size())
        {
            const float ya = yOf (ti->f[(size_t) i] * ti->ratios[(size_t) i]);
            c.setColor (sel ? 0xffffffff : col); c.circle (px - (sel ? 6 : 4), ya - (sel ? 6 : 4), sel ? 12.f : 8.f);
        }
        add (px - 6, gy, 12, gh, "hit:" + std::to_string (i));
    }
    // the drum panel
    float y = y0 + 48;
    auto row = [&] (const std::string& k, const std::string& v) { txt (c, k, bodyFont (12), ink3, sx + 16, y, 150, 22); txt (c, v, monoFont (12), ink, sx + 150, y, 174, 22, visage::Font::kRight); y += 26; };
    row ("Measured", fmt ("%.1f Hz", ti->medF) + " \xc2\xb7 " + hl::noteName (ti->medMidi) + fmt (" %+.0f ct", (ti->medMidi - std::round (ti->medMidi)) * 100));
    row ("Target", hl::noteName (ti->targetMidi) + fmt (" \xc2\xb7 %.1f Hz", ti->targetF));
    if (! ti->ratios.empty())
    {
        double rmin = 1e9, rmax = -1e9;
        for (double r : ti->ratios) { rmin = std::min (rmin, r); rmax = std::max (rmax, r); }
        row ("Shift", fmt ("%+.0f", 1200 * std::log2 (rmin)) + fmt (" .. %+.0f ct", 1200 * std::log2 (rmax)));
    }
    y += 6;
    knobCtl (c, sx + 16 + tok::knobLg / 2 + 6, y + 22 + tok::knobLg / 2, tok::knobLg, "Fine tune", "tune.cents");
    txt (c, "Varispeed, as a sampler does: pitch and length", bodyFont (11), ink3, sx + 110, y + 34, sideW - 126, 16);
    txt (c, "move together. Best-quality sinc resampling.", bodyFont (11), ink3, sx + 110, y + 50, sideW - 126, 16);
    y += 110;
    scopeNote (c, sx + 16, y, sideW - 32, S->tune.mode == "rr" ? "Each hit gets its own ratio, the same on every mic." : "One ratio for the whole drum, on every mic.");
    y += 28;
    if (selHit >= 0 && selHit < N && std::isfinite (ti->f[(size_t) selHit]))
    {
        txt (c, "Hit " + std::to_string (selHit + 1), boldFont (12), ink, sx + 16, y, 200, 20); y += 24;
        row ("Measured", fmt ("%.2f Hz", ti->f[(size_t) selHit]));
        if (selHit < (int) ti->ratios.size()) row ("Moves by", fmt ("%+.1f ct", 1200 * std::log2 (ti->ratios[(size_t) selHit])));
    }
}

// ------------------------------------------------------------------------------------------------ 5 Tone
void HitsView::drawTone (Canvas& c)
{
    const auto* S = model.session();
    if (! S) return;
    const float top = tbH + auH, y0 = top + sbH;
    const int T = (int) S->tracks.size();
    float x = 14;
    txt (c, "Applies to", bodyFont (12), ink3, x, top, 70, sbH); x += 74;
    std::vector<std::string> scopes { "All mics" };
    for (auto& tr : model.tracks()) scopes.push_back (tr.name);
    if (selMic >= T) selMic = -1;
    segmented (c, x, top + 4, scopes, selMic + 1, "scope:", 26);
    x += segmentedW (scopes) + 14;
    c.setColor (btn); c.roundedRectangle (x, top + 6, 56, 26, 3); txt (c, "Flat", boldFont (12), ink, x, top + 6, 56, 26, visage::Font::kCenter); add (x, top + 6, 56, 26, "tone:flat");
    barEnd = x + 56;
    // the controls
    const float pw = tok::goldenMinor (stepW() - 42, 400.0f, 560.0f);   // golden split: controls : waveform
    panel (c, 14, y0 + 14, pw, height() - y0 - 28, "");
    float y = y0 + 26;
    // five knobs to a row, phi apart in size from the small ones
    const float kd = 55, col = (pw - 32) / 5;
    auto rowOf = [&] (const char* title, std::initializer_list<std::pair<const char*, const char*>> ks)
    {
        txt (c, title, boldFont (10), tok::ink3, 30, y, 200, 16); y += 22;
        int i = 0;
        for (auto& [label, key] : ks) knobCtl (c, 30 + col * (i++ + 0.5f), y + 18 + kd / 2, kd, label, key);
        y += 18 + kd + 34;
    };
    rowOf ("TONE", { { "Enhance", "tone.match" }, { "Tone", "tone.bright" }, { "Crack", "tone.crack" }, { "Rings", "tone.smooth" }, { "Tame", "tone.tame" } });
    rowOf ("ENVELOPE", { { "Attack", "shape.atk" }, { "Decay", "shape.dec" }, { "Sustain", "shape.sus" }, { "Release", "shape.rel" }, { "Enhance", "shape.match" } });
    txt (c, "REFERENCE MATCH", boldFont (10), 0xff9c9ca1, 30, y, 200, 16); y += 22;
    c.setColor (btn); c.roundedRectangle (30, y, 120, 26, 3);
    txt (c, S->tone.hasRef ? "Load another\xe2\x80\xa6" : "Load a reference\xe2\x80\xa6", boldFont (11), ink, 30, y, 120, 26, visage::Font::kCenter);
    add (30, y, 120, 26, "ref:load");
    if (S->tone.hasRef)
    {
        c.setColor (0xff3a3a3e); c.roundedRectangleBorder (158, y, 70, 26, 3, 1);
        txt (c, "Remove", bodyFont (11), ink2, 158, y, 70, 26, visage::Font::kCenter);
        add (158, y, 70, 26, "ref:remove");
    }
    y += 36;
    if (S->tone.hasRef)
    {
        knobCtl (c, 30 + col * 0.5f, y + 18 + kd / 2, kd, "Tone match", "ref.amt");
        if (S->tone.hasRefEnv) knobCtl (c, 30 + col * 1.5f, y + 18 + kd / 2, kd, "Envelope", "ref.env");
    }
    else txt (c, "A finished sample you like: the tone and envelope move toward it.", bodyFont (11), ink3, 30, y, pw - 32, 16);
    // the selected hit, before and after, on the lead (or the mic the moves apply to)
    const float gx = 14 + pw + 14, gw = stepW() - gx - 14;
    panel (c, gx, y0 + 14, gw, height() - y0 - 28, "");
    if (viewMic >= (int) S->tracks.size()) viewMic = -1;
    const int t = viewMic >= 0 ? viewMic : selMic >= 0 ? selMic : S->refIdx;
    const auto& H = model.hits();
    const int hi = std::clamp (selHit < 0 ? 0 : selHit, 0, std::max (0, (int) H.size() - 1));
    hitStrip (c, gx + 14, y0 + 48, gw - 28);
    scopeNote (c, 30, height() - 14 - 30, pw - 32, selMic >= 0 ? "Applies to every hit of " + model.tracks()[(size_t) selMic].name + ", whatever is on show." : std::string ("Applies to every hit of every mic, whatever is on show."));
    txt (c, model.tracks()[(size_t) t].name + " \xc2\xb7 hit " + std::to_string (hi + 1) + ": as recorded (grey) and with the tone moves (" + std::string (model.isRendering() ? "rendering\xe2\x80\xa6" : "as it plays now") + ")",
         bodyFont (12), ink2, gx + 14, y0 + 22, gw - 28, 20);
    const auto* M = model.mixRender();
    if (H.empty()) return;
    const double sr = S->sr;
    const long on = (long) std::llround (H[(size_t) hi].time * sr), len = (long) (0.4 * sr), pre = (long) (0.006 * sr);
    const auto& orig = S->tracks[(size_t) t].ch.get()[0];
    const hl::F32* proc = nullptr;
    if (M && t < (int) M->proc.size()) { const auto& pr = M->bus[(size_t) t].empty() ? M->proc[(size_t) t] : M->bus[(size_t) t]; if (! pr.empty()) proc = &pr[0]; }
    const float wx = gx + 14, ww = gw - 28, wy = y0 + 80, wh = (height() - y0 - 28 - 94) * 0.62f, mid = wy + wh / 2;
    c.setColor (lane); c.fill (wx, wy, ww, wh);
    float pk = 1e-6f;
    for (long n = on - pre; n < on + len; ++n) if (n >= 0 && n < (long) orig.size()) { pk = std::max (pk, std::abs (orig[(size_t) n])); if (proc && n < (long) proc->size()) pk = std::max (pk, std::abs ((*proc)[(size_t) n])); }
    for (int pass = 0; pass < (proc ? 2 : 1); ++pass)
    {
        const auto& x = pass == 0 ? orig : *proc;
        c.setColor (pass == 0 ? 0x66ffffff : (0xd0000000 | (trackColour (model, t) & 0xffffff)));
        for (int i = 0; i < (int) ww; ++i)
        {
            const long a = on - pre + (long) ((double) (len + pre) * i / ww), b = on - pre + (long) ((double) (len + pre) * (i + 1) / ww);
            float mn = 0, mxv = 0;
            for (long n = a; n < std::max (a + 1, b); ++n) if (n >= 0 && n < (long) x.size()) { mn = std::min (mn, x[(size_t) n]); mxv = std::max (mxv, x[(size_t) n]); }
            c.fill (wx + i, mid - mxv / pk * wh * 0.46f, 1, std::max (1.0f, (mxv - mn) / pk * wh * 0.46f));
        }
    }
    // the envelope in dB underneath
    const float ey = wy + wh + 14, eh = height() - 14 - 14 - ey;
    c.setColor (lane); c.fill (wx, ey, ww, eh);
    txt (c, "Envelope (dB)", bodyFont (10), ink3, wx + ww - 126, ey + 4, 120, 14, visage::Font::kRight);
    for (int pass = 0; pass < (proc ? 2 : 1); ++pass)
    {
        const auto& x = pass == 0 ? orig : *proc;
        c.setColor (pass == 0 ? 0x99ffffff : (0xff000000 | (trackColour (model, t) & 0xffffff)));
        float py = 0;
        for (int i = 0; i < (int) ww; i += 2)
        {
            const long a = on - pre + (long) ((double) (len + pre) * i / ww), b = on - pre + (long) ((double) (len + pre) * (i + 2) / ww);
            float m = 0;
            for (long n = a; n < b; ++n) if (n >= 0 && n < (long) x.size()) m = std::max (m, std::abs (x[(size_t) n]));
            const float db = m > 0 ? 20 * std::log10 (m / pk) : -90;
            const float yy = ey + eh * std::clamp (-db / 72.0f, 0.0f, 1.0f);
            if (i > 0) c.fill (wx + i, std::min (py, yy), 2, std::abs (yy - py) + 1.5f);
            py = yy;
        }
    }
}

// ------------------------------------------------------------------------------------------------ 6 Clean
void HitsView::drawClean (Canvas& c)
{
    const auto* S = model.session();
    if (! S) return;
    const float top = tbH + auH, y0 = top + sbH;
    txt (c, "Noise under the tails, taken down only where the tail has faded into it. Heard on B.", bodyFont (12), ink2, 14, top, 700, sbH);
    barEnd = 14 + textWidth (bodyFont (12), "Noise under the tails, taken down only where the tail has faded into it. Heard on B.");
    const float pw = tok::goldenMinor (stepW() - 42, 400.0f, 560.0f);   // golden split: controls : what it does
    panel (c, 14, y0 + 14, pw, 170, "Noise");
    c.setColor (S->dnOn ? accentDark : 0xff3a3a3e); c.roundedRectangle (pw - 60, y0 + 24, 56, 22, 11);
    txt (c, S->dnOn ? "On" : "Off", boldFont (11), S->dnOn ? accentInk : ink2, pw - 60, y0 + 24, 56, 22, visage::Font::kCenter);
    add (pw - 60, y0 + 24, 56, 22, "clean:on");
    knobCtl (c, 30 + tok::knobLg / 2 + 6, y0 + 68 + tok::knobLg / 2, tok::knobLg, "Reduce by", "clean.db");
    txt (c, "The hit itself is never touched; the noise floor", bodyFont (11), ink3, 124, y0 + 84, pw - 126, 16);
    txt (c, "is measured on each mic between the hits.", bodyFont (11), ink3, 124, y0 + 100, pw - 126, 16);
    // the tail of the selected hit, in dB: as recorded and cleaned
    const float gx = 14 + pw + 14, gw = stepW() - gx - 14;
    panel (c, gx, y0 + 14, gw, height() - y0 - 28, "");
    if (viewMic >= (int) S->tracks.size()) viewMic = -1;
    const int t = viewMic >= 0 ? viewMic : S->refIdx;
    const auto& H = model.hits();
    if (H.empty()) return;
    hitStrip (c, gx + 14, y0 + 48, gw - 28);
    scopeNote (c, 30, y0 + 150, pw - 32, "Applies to every hit on every mic, whatever is on show.");
    const int hi = std::clamp (selHit < 0 ? 0 : selHit, 0, (int) H.size() - 1);
    txt (c, model.tracks()[(size_t) t].name + " \xc2\xb7 hit " + std::to_string (hi + 1) + " \xc2\xb7 the whole tail in dB: as recorded (grey) and cleaned (colour). Click a track to look at another.",
         bodyFont (12), ink2, gx + 14, y0 + 22, gw - 28, 20);
    const double sr = S->sr;
    const long on = (long) std::llround (H[(size_t) hi].time * sr), end = (long) std::llround ((H[(size_t) hi].end - 0.01) * sr);
    const auto& orig = S->tracks[(size_t) t].ch.get()[0];
    const auto* M = model.mixRender();
    const hl::F32* proc = nullptr;
    if (M && t < (int) M->proc.size()) { const auto& pr = M->bus[(size_t) t].empty() ? M->proc[(size_t) t] : M->bus[(size_t) t]; if (! pr.empty()) proc = &pr[0]; }
    const float wx = gx + 14, ww = gw - 28, wy = y0 + 80, wh = height() - y0 - 28 - 94;
    c.setColor (lane); c.fill (wx, wy, ww, wh);
    for (int db = 0; db >= -90; db -= 20) { const float yy = wy + wh * (-db / 96.0f); c.setColor (0xff2e2e31); c.fill (wx, yy, ww, 1); txt (c, std::to_string (db) + " dB", monoFont (10), ink3, wx + 4, yy, 60, 14); }
    float pk = 1e-6f;
    for (long n = on; n < end && n < (long) orig.size(); ++n) pk = std::max (pk, std::abs (orig[(size_t) n]));
    for (int pass = 0; pass < (proc ? 2 : 1); ++pass)
    {
        const auto& x = pass == 0 ? orig : *proc;
        c.setColor (pass == 0 ? 0x80ffffff : (0xff000000 | (trackColour (model, t) & 0xffffff)));
        float py = 0;
        for (int i = 0; i < (int) ww; i += 2)
        {
            const long a = on + (long) ((double) (end - on) * i / ww), b = on + (long) ((double) (end - on) * (i + 2) / ww);
            float m = 0;
            for (long n = a; n < b; ++n) if (n >= 0 && n < (long) x.size()) m = std::max (m, std::abs (x[(size_t) n]));
            const float db = m > 0 ? 20 * std::log10 (m / pk) : -96;
            const float yy = wy + wh * std::clamp (-db / 96.0f, 0.0f, 1.0f);
            if (i > 0) c.fill (wx + i, std::min (py, yy), 2, std::abs (yy - py) + 1.5f);
            py = yy;
        }
    }
}

// ------------------------------------------------------------------------------------------------ 7 Level
void HitsView::drawLevel (Canvas& c)
{
    const auto* S = model.session();
    if (! S) return;
    const float top = tbH + auH, y0 = top + sbH;
    float x = 14;
    segmented (c, x, top + 4, { "Each mic on its own", "Linked" }, S->lev.mode == "linked" ? 1 : 0, "levmode:", 26);
    x += segmentedW ({ "Each mic on its own", "Linked" }) + 14;
    static const std::vector<std::string> curves { "midi", "even", "step", "range" };
    int ci = 0;
    for (size_t i = 0; i < curves.size(); ++i) if (S->lev.curve == curves[i]) ci = (int) i;
    segmented (c, x, top + 4, { "Velocity curve", "Even steps", "Fixed step", "Range" }, ci, "levcurve:", 26);
    barEnd = x + segmentedW ({ "Velocity curve", "Even steps", "Fixed step", "Range" });
    const auto* L = model.levelResult();
    const float sideW = tok::goldenMinor (stepW() - 42, 360.0f, 560.0f);   // golden split: levels : layers
    const float pw = stepW() - 28 - sideW;
    panel (c, 14, y0 + 14, pw, height() - y0 - 28, "What the exported hits will be");
    const float sx = stepW() - sideW - 14;
    panel (c, sx, y0 + 14, sideW, height() - y0 - 28, "Layers");
    if (! L || ! L->valid) { txt (c, "Run Auto Prep: the levels are measured once every hit is found.", bodyFont (13), ink2, 34, y0 + 60, pw - 40, 22); return; }
    if (viewMic >= (int) S->tracks.size()) viewMic = -1;
    const int t = viewMic >= 0 ? viewMic : S->refIdx;
    txt (c, model.tracks()[(size_t) t].name + " \xc2\xb7 grey: as played, colour: as exported. Click a bar to hear the hit.", bodyFont (11), ink3, 300, y0 + 24, pw - 300, 16, visage::Font::kRight);
    const float gx = 70, gy = y0 + 56, gw = pw - 80, gh = height() - y0 - 28 - 80;
    const double LO = -48;
    auto yOf = [&] (double db) { return gy + (float) std::clamp (-db / -LO, 0.0, 1.0) * gh; };
    for (int db = 0; db >= (int) LO; db -= 6) { c.setColor (0xff333336); c.fill (gx, yOf (db), gw, 1); txt (c, std::to_string (db) + " dB", monoFont (10), ink3, 20, yOf (db) - 8, 48, 16); }
    const auto& H = model.hits();
    const int N = (int) H.size();
    const auto& raw = L->lvlRaw.size() > (size_t) t ? L->lvlRaw[(size_t) t] : hl::F64 {};
    for (int i = 0; i < N && i < (int) raw.size(); ++i)
    {
        const double before = raw[(size_t) i], after = before + model.hitGainDb (t, (size_t) i);
        const float slot = gw / N, bw = slot * 0.62f, bx = gx + i * slot + (slot - bw) / 2;
        const unsigned col = velColour[H[(size_t) i].vel % nVelColours];
        const bool shown = inView (H, selHit < 0 ? 0 : selHit, playMode, i);   // outside the view: still there, dimmed
        c.setColor (shown ? 0x33ffffff : 0x14ffffff); c.fill (bx, yOf (before), bw, gy + gh - yOf (before));
        c.setColor (i == selHit ? accent : draw::withAlpha (col | 0xff000000, shown ? 1.0f : 0.25f)); c.fill (bx + bw * 0.2f, yOf (after), bw * 0.6f, gy + gh - yOf (after));
        add (bx, gy, bw, gh, "hit:" + std::to_string (i));
    }
    // the layers
    float y = y0 + 48;
    txt (c, "Layer", bodyFont (11), ink3, sx + 16, y, 60, 16); txt (c, "Lands at", bodyFont (11), ink3, sx + 90, y, 90, 16); txt (c, "Spread before \xe2\x86\x92 after", bodyFont (11), ink3, sx + 190, y, 160, 16);
    y += 22;
    for (int v = 0; v < model.velocities(); ++v)
    {
        double sum = 0, lo0 = 1e9, hi0 = -1e9, lo1 = 1e9, hi1 = -1e9; int n = 0;
        for (int i = 0; i < N && i < (int) raw.size(); ++i)
            if (H[(size_t) i].vel == v)
            {
                const double b = raw[(size_t) i], a = b + model.hitGainDb (t, (size_t) i);
                sum += a; ++n; lo0 = std::min (lo0, b); hi0 = std::max (hi0, b); lo1 = std::min (lo1, a); hi1 = std::max (hi1, a);
            }
        if (n == 0) continue;
        c.setColor (velColour[v % nVelColours] | 0xff000000); c.roundedRectangle (sx + 16, y + 3, 40, 18, 3);
        txt (c, "V" + std::to_string (v + 1), boldFont (10), 0xff101011, sx + 16, y + 3, 40, 18, visage::Font::kCenter);
        txt (c, fmt ("%.1f dB", sum / n), monoFont (11), ink, sx + 90, y, 90, 24);
        txt (c, fmt ("\xc2\xb1%.1f", (hi0 - lo0) / 2) + fmt (" \xe2\x86\x92 \xc2\xb1%.1f dB", (hi1 - lo1) / 2), monoFont (11), ink2, sx + 190, y, 160, 24);
        y += 26;
    }
    y += 12;
    {
        const float kd = 55, col = (sideW - 32) / 3;
        const std::pair<const char*, const char*> ks[3] = { { "Step", "lev.step" }, { "Range", "lev.range" }, { "Offset", "lev.offset" } };
        for (int i = 0; i < 3; ++i) knobCtl (c, sx + 16 + col * (i + 0.5f), y + 22 + kd / 2, kd, ks[i].first, ks[i].second);
        y += 22 + kd + 38;
    }
    scopeNote (c, sx + 16, y, sideW - 32, S->lev.mode == "linked" ? "Applies to every hit, the mics moving together." : "Applies to every hit, each mic on its own.");
    y += 26;
    double peakMax = -100;
    for (double p : L->peakAfter) peakMax = std::max (peakMax, p);
    txt (c, std::isfinite (L->peakMax) ? fmt ("Loudest hit after levelling: %.1f dBFS", L->peakMax + L->trimDb) : std::string(), bodyFont (12), ink2, sx + 16, y, 328, 20);
    y += 22;
    txt (c, "Mon and Pan never change these: the files", bodyFont (11), ink3, sx + 16, y, 328, 16);
    txt (c, "come out exactly as shown.", bodyFont (11), ink3, sx + 16, y + 16, 328, 16);
}

// ------------------------------------------------------------------------------------------------ 8 Export
void HitsView::drawExport (Canvas& c)
{
    const auto* S = model.session();
    if (! S) return;
    const float top = tbH + auH, y0 = top + sbH;
    float x = 14;
    txt (c, "Bits", bodyFont (12), ink3, x, top, 30, sbH); x += 34;
    segmented (c, x, top + 4, { "16", "24", "32 float" }, S->exp.bits == 16 ? 0 : S->exp.bits == 24 ? 1 : 2, "bits:", 26); x += segmentedW ({ "16", "24", "32 float" }) + 14;
    txt (c, "Rate", bodyFont (12), ink3, x, top, 34, sbH); x += 38;
    static const std::vector<std::string> rates { "same", "44100", "48000", "96000" };
    int ri = 0; for (size_t i = 0; i < rates.size(); ++i) if (S->exp.rate == rates[i]) ri = (int) i;
    segmented (c, x, top + 4, { "As recorded", "44.1k", "48k", "96k" }, ri, "rate:", 26); x += segmentedW ({ "As recorded", "44.1k", "48k", "96k" }) + 14;
    txt (c, "Tails", bodyFont (12), ink3, x, top, 38, sbH); x += 40;
    segmented (c, x, top + 4, { "Natural", "Fade" }, S->exp.tailMode == "fade" ? 1 : 0, "tails:", 26);
    barEnd = x + segmentedW ({ "Natural", "Fade" });
    // the checks
    const float pw = tok::goldenMinor (width() - 42, 420.0f, 560.0f);   // golden split: checks : file list
    panel (c, 14, y0 + 14, pw, height() - y0 - 28, "Before it writes");
    float y = y0 + 44;
    const int want = S->det.V * S->det.RR;
    const auto* L = model.levelResult();
    int aligned = 0, flipped = 0;
    for (int t = 0; t < (int) S->tracks.size(); ++t) { if (t != S->refIdx && (std::abs (S->tracks[(size_t) t].lag) > 0.01 || ! S->tracks[(size_t) t].ap.empty())) ++aligned; if (S->tracks[(size_t) t].pol < 0) ++flipped; }
    const bool toneOn = S->tone.match + std::abs (S->tone.bright) + std::abs (S->tone.crack) + S->tone.smooth + S->tone.tame + std::abs (S->shape.atk) + std::abs (S->shape.sus) + S->shape.match > 0;
    struct Check { bool ok; std::string title, detail; };
    std::vector<Check> checks {
        { (int) model.hits().size() == want, "All " + std::to_string (want) + " hits", std::to_string (S->det.V) + (S->det.V == 1 ? " velocity \xc3\x97 " : " velocities \xc3\x97 ") + std::to_string (S->det.RR) + (S->det.RR == 1 ? " round robin" : " round robins") },
        { S->phaseAnalyzed, "Aligned", std::to_string (aligned) + " mics moved, " + std::to_string (flipped) + " flipped" },
        { true, S->tune.on ? "Tuned" : "Not tuned", S->tune.on && S->tuneInfo && S->tuneInfo->valid ? "to " + hl::noteName (S->tuneInfo->targetMidi) : std::string ("as played") },
        { true, toneOn ? "Toned" : "Tone flat", toneOn ? "the Tone step's moves" : "as recorded" },
        { true, S->dnOn ? "Cleaned" : "Not cleaned", S->dnOn ? fmt ("noise down %.0f dB", S->dnDb) : std::string ("as recorded") },
        { L && L->valid && L->peakMax + L->trimDb <= 0.0, "Levelled \xc2\xb7 nothing clips", L && L->valid ? fmt ("loudest %.1f dBFS", L->peakMax + L->trimDb) : std::string ("run Auto Prep") },
        { model.exportBlocker().isEmpty(), model.exportBlocker().isEmpty() ? std::string ("Ready to write") : std::string ("Not ready yet"),
          model.exportBlocker().isEmpty() ? std::string ("rendered through the same engine and plugins B plays") : model.exportBlocker().toStdString() } };
    for (auto& k : checks)
    {
        c.setColor (k.ok ? 0xff3fbf78 : 0xffe0663c); c.circle (30, y + 4, 12);
        txt (c, k.ok ? "\xe2\x9c\x93" : "!", boldFont (9), 0xff101011, 30, y + 4, 12, 12, visage::Font::kCenter);
        txt (c, k.title, boldFont (12), ink, 52, y, pw - 60, 18);
        txt (c, k.detail, bodyFont (11), ink3, 52, y + 18, pw - 60, 16);
        y += 40;
    }
    y += 4;
    txt (c, "Name", bodyFont (12), ink3, 30, y, 60, 22);
    c.setColor (field); c.roundedRectangle (30, y + 24, pw - 32, 26, 3);
    txt (c, S->exp.hitTemplate, monoFont (11), ink, 38, y + 24, pw - 48, 26);
    y += 58;
    segmented (c, 30, y, { "Folder per hit", "Folder per mic", "Flat" }, S->exp.hitTemplate.find ("/{MIC}") != std::string::npos ? 0 : S->exp.hitTemplate.rfind ("{MIC}/", 0) == 0 ? 1 : 2, "tpl:", 24);
    y += 44;
    txt (c, "To: " + (exportFolder.empty() ? std::string ("(you'll pick a folder)") : exportFolder), bodyFont (11), ink2, 30, y, pw - 32, 18);
    y += 28;
    c.setColor (visage::Brush::linear (visage::Color (tok::coralFillTop), visage::Color (tok::coralFill), visage::Point (0, y), visage::Point (0, y + 34)));
    c.roundedRectangle (30, y, 180, 34, 4);
    txt (c, exportProgress >= 0 && exportProgress < 1 ? fmt ("Cancel \xc2\xb7 %.0f %%", exportProgress * 100) : std::string ("Export"), boldFont (13), 0xffffffff, 30, y, 180, 34, visage::Font::kCenter);
    add (30, y, 180, 34, "export");
    // the files
    const float gx = 14 + pw + 14, gw = width() - gx - 14;
    // (the list is rebuilt only when what it depends on changes: 30 times a second it would cost 25 ms a frame)
    const std::string key = S->exp.hitTemplate + "|" + S->exp.fullTemplate + "|" + S->exp.mode + "|" + S->drum + "|" + S->artic + "|" + std::to_string (S->onsets.size()) + "|" + std::to_string (S->det.V) + "x" + std::to_string (S->det.RR);
    if (key != exportKey) { exportKey = key; exportCache = hl::exportList (*S); }
    const auto& list = exportCache;
    panel (c, gx, y0 + 14, gw, height() - y0 - 28, std::to_string (list.size()) + " files");
    y = y0 + 40;
    const double srOut = S->exp.rate == "same" ? S->sr : std::stod (S->exp.rate);
    for (size_t k = 0; k < list.size() && y < height() - 40; ++k)
    {
        const auto& it = list[k];
        c.setColor (k % 2 ? 0xff262628 : 0xff2a2a2d); c.fill (gx + 6, y, gw - 12, 22);
        if (it.t >= 0) { c.setColor (trackColour (model, it.t)); c.fill (gx + 6, y, 4, 22); }
        txt (c, it.path, monoFont (10), ink2, gx + 16, y, gw - 200, 22);
        const bool st = it.t >= 0 && model.tracks()[(size_t) it.t].channels > 1;
        txt (c, std::string (st ? "Stereo" : "Mono") + " \xc2\xb7 " + fmt ("%.0f", srOut / 1000) + "k", bodyFont (10), ink3, gx + gw - 180, y, 170, 22, visage::Font::kRight);
        y += 23;
    }
}

// ------------------------------------------------------------------------------------------------ actions
bool HitsView::actStep (const std::string& id, const visage::MouseEvent& e)
{
    using R = SampleModel::Redo;
    auto arg = [&] { return std::stoi (id.substr (id.find (':') + 1)); };
    auto has = [&] (const char* p) { return id.rfind (p, 0) == 0; };
    if (has ("sl:"))
    {
        sliderKey = id.substr (3);
        if (e.isAltDown())
        {
            const auto r = ranges().at (sliderKey);
            setSliderValue (sliderKey, r.lo < 0 ? 0.0 : (sliderKey == "clean.db" ? 18.0 : sliderKey == "lev.step" ? 2.0 : sliderKey == "lev.range" ? 24.0 : 0.0));
            return true;
        }
        drag = Drag::slider;
        dragY0 = e.position.x; dragY1 = e.position.y;
        dragV0 = (float) sliderValue (sliderKey);
        return true;
    }
    if (has ("ed:")) { editKey = id.substr (3); editText.clear(); return true; }
    if (has ("scope:")) { selMic = arg() - 1; if (selMic >= 0) viewMic = selMic; return true; }
    if (has ("row:")) { if (step == 2) selMic = arg(); else viewMic = arg(); return true; }   // off Align, a row only changes what's shown
    if (has ("units:")) { alignUnits = arg(); return true; }
    if (has ("apsw:")) { const int t = arg(); model.change ([t] (hl::Session& S) { hl::setPhaseEq (S, t, ! S.tracks[(size_t) t].apOn); }, R::align); return true; }
    if (id == "aldetails") { alignDetails = ! alignDetails; return true; }
    if (has ("apadd:")) { const int t = selMic; model.change ([t] (hl::Session& S) { if (t >= 0 && t < (int) S.tracks.size()) hl::setAllPass (S, t, (int) S.tracks[(size_t) t].ap.size(), 200, 0.7); }, R::align); return true; }
    if (has ("aprem:")) { const int t = selMic, b = arg(); model.change ([t, b] (hl::Session& S) { hl::removeAllPass (S, t, b); }, R::align); return true; }
    if (has ("lead:"))
    {
        const int t = arg();
        if (model.change ([t] (hl::Session& S) { S.refIdx = t; }, R::reprep) && onAutoPrep) onAutoPrep();   // everything is measured against the lead: measure again
        return true;
    }
    if (id == "leadnext")
    {
        const int T = (int) model.tracks().size();
        const int t = (model.leadTrack() + 1) % std::max (1, T);
        if (model.change ([t] (hl::Session& S) { S.refIdx = t; }, R::reprep) && onAutoPrep) onAutoPrep();
        return true;
    }
    if (has ("drum:"))
    {
        static const char* d[] = { "kick", "snare", "tom", "hat", "ride", "crash" };
        const std::string drum = d[std::clamp (arg(), 0, 5)];
        model.change ([drum] (hl::Session& S) { S.drum = drum; hl::applyDrumDefaults (S, drum); }, R::reprep);
        setStatus ("Drum set. Run Auto Prep: export waits until it has.");
        return true;
    }
    if (has ("v:") || has ("rr:"))
    {
        const bool rr = id[0] == 'r';
        const int d = arg();
        model.change ([rr, d] (hl::Session& S) { int& n = rr ? S.det.RR : S.det.V; n = std::clamp (n + d, 1, 32); }, R::reprep);
        setStatus ("Counts changed. Run Auto Prep to find that many hits: export waits until it has.");
        return true;
    }
    if (has ("rooms:"))
    {
        const bool on = arg() == 1;
        model.setRoomPhase (on);
        setStatus (on ? "Rooms keep their distance; polarity and phase EQ are set against the lead. Auto Prep does this from now on."
                      : "Rooms are left exactly as recorded.");
        return true;
    }
    if (has ("order:")) { const bool loud = arg() == 1; model.change ([loud] (hl::Session& S) { S.det.order = loud ? "loud" : "soft"; }, R::reprep); setStatus ("Order changed. Run Auto Prep: export waits until it has."); return true; }
    if (has ("timing:"))
    {
        static const char* m[] = { "tight", "pro", "natural" };
        const std::string mode = m[std::clamp (arg(), 0, 2)];
        model.change ([mode] (hl::Session& S) { hl::setTiming (S, mode); }, R::align);
        return true;
    }
    if (has ("nudge:"))
    {
        const int q = arg(), t = selMic;
        model.change ([q, t] (hl::Session& S) { if (t >= 0 && t < (int) S.tracks.size()) hl::nudgeLag (S, t, q); }, R::align);
        return true;
    }
    if (has ("flipm:")) { const int t = arg(); model.change ([t] (hl::Session& S) { hl::flipMic (S, t); }, R::align); return true; }
    if (has ("apeq:"))
    {
        const int t = arg();
        model.change ([t] (hl::Session& S) { hl::setPhaseEq (S, t, ! (S.tracks[(size_t) t].apOn && ! S.tracks[(size_t) t].ap.empty())); }, R::align);
        return true;
    }
    if (has ("reset:")) { const int t = arg(); model.change ([t] (hl::Session& S) { hl::resetMic (S, t); }, R::align); return true; }
    if (id == "tune:on") { model.change ([] (hl::Session& S) { S.tune.on = ! S.tune.on; }, R::tune); return true; }
    if (id == "tune:measure") { model.change ([] (hl::Session& S) { S.tuneAnyway = true; hl::runPitch (S); }, R::tune); return true; }
    if (has ("tunemode:")) { const bool rr = arg() == 1; model.change ([rr] (hl::Session& S) { S.tune.mode = rr ? "rr" : "drum"; }, R::tune); return true; }
    if (has ("target:"))
    {
        const int d = arg();
        model.change ([d] (hl::Session& S)
        {
            if (d == 0 || ! S.tuneInfo) { S.tune.target = "nearest"; return; }
            S.tune.target = std::to_string (S.tuneInfo->targetMidi + d);
        }, R::tune);
        return true;
    }
    if (id == "tone:flat")
    {
        const int mic = selMic;
        model.change ([mic] (hl::Session& S)
        {
            const bool m = mic >= 0 && mic < (int) S.tracks.size();
            hl::ToneMoves& T = m ? S.tracks[(size_t) mic].strip.tone : (hl::ToneMoves&) S.tone;
            hl::ShapeMoves& H = m ? S.tracks[(size_t) mic].strip.shape : S.shape;
            T.match = T.bright = T.crack = T.smooth = T.tame = 0;
            H = {};
        }, R::mix);
        return true;
    }
    if (id == "ref:load") { if (onLoadReference) onLoadReference(); return true; }
    if (id == "ref:remove") { model.change ([] (hl::Session& S) { S.tone.hasRef = false; S.tone.refCurve.clear(); S.tone.hasRefEnv = false; }, R::mix); return true; }
    if (id == "clean:on") { model.change ([] (hl::Session& S) { S.dnOn = ! S.dnOn; }, R::mix); return true; }
    if (has ("levmode:")) { const bool linked = arg() == 1; model.change ([linked] (hl::Session& S) { S.lev.mode = linked ? "linked" : "permic"; }, R::levels); return true; }
    if (has ("levcurve:")) { static const char* cv[] = { "midi", "even", "step", "range" }; const std::string k = cv[std::clamp (arg(), 0, 3)]; model.change ([k] (hl::Session& S) { S.lev.curve = k; }, R::levels); return true; }
    if (has ("bits:")) { const int b = arg() == 0 ? 16 : arg() == 1 ? 24 : 32; model.change ([b] (hl::Session& S) { S.exp.bits = b; }, R::none); return true; }
    if (has ("rate:")) { static const char* r[] = { "same", "44100", "48000", "96000" }; const std::string k = r[std::clamp (arg(), 0, 3)]; model.change ([k] (hl::Session& S) { S.exp.rate = k; }, R::none); return true; }
    if (has ("tails:")) { const bool f = arg() == 1; model.change ([f] (hl::Session& S) { S.exp.tailMode = f ? "fade" : "natural"; }, R::none); return true; }
    if (has ("tpl:"))
    {
        static const char* tp[] = { "{DRUM}_{ARTIC}_RR{RR}_VEL{VEL}/{MIC}.wav", "{MIC}/{DRUM}_{ARTIC}_{MIC}_V{VEL}_RR{RR}.wav", "{DRUM}_{ARTIC}_{MIC}_V{VEL}_RR{RR}.wav" };
        const std::string k = tp[std::clamp (arg(), 0, 2)];
        model.change ([k] (hl::Session& S) { S.exp.hitTemplate = k; }, R::none);
        return true;
    }
    if (id == "export") { if (onExport) onExport(); return true; }
    return false;
}

} // namespace ld::samples

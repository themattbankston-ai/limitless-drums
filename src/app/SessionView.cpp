#include "SessionView.h"
#include <cmath>

namespace ld
{
namespace te = tracktion;

SessionView::SessionView (te::Edit& e, std::vector<MicRole> roles, std::function<theme::Palette()> p)
    : edit (e), palette (std::move (p))
{
    auto tracks = te::getAudioTracks (edit);

    for (int i = 0; i < tracks.size(); ++i)
    {
        Lane lane;
        lane.track = tracks[i];
        if (i < (int) roles.size()) lane.role = roles[(size_t) i];

        for (auto* c : tracks[i]->getClips())
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
                lane.clips.emplace_back (c, std::make_unique<te::SmartThumbnail> (edit.engine, ac->getPlaybackFile(), *this, &edit));

        lanes.push_back (std::move (lane));
    }

    setWantsKeyboardFocus (false);
    startTimerHz (60);
}

SessionView::~SessionView() { stopTimer(); }

//==============================================================================
double SessionView::xToTime (float x) const { return viewStart + (x - headerWidth) / pixelsPerSecond; }
float SessionView::timeToX (double t) const { return (float) (headerWidth + (t - viewStart) * pixelsPerSecond); }

juce::Rectangle<int> SessionView::laneBounds (int i) const
{
    return { 0, rulerHeight + i * laneHeight - scrollY, getWidth(), laneHeight };
}

juce::Rectangle<int> SessionView::soloBox (int i) const
{
    auto b = laneBounds (i);
    return { headerWidth - 66, b.getY() + laneHeight - 30, 26, 20 };
}

juce::Rectangle<int> SessionView::muteBox (int i) const
{
    auto b = laneBounds (i);
    return { headerWidth - 36, b.getY() + laneHeight - 30, 26, 20 };
}

std::pair<juce::Colour, juce::Colour> SessionView::roleColours (const MicRole& r) const
{
    using K = MicRole::Kind;
    namespace n = theme::neon;
    switch (r.kind)
    {
        case K::kickIn: case K::kickOut: case K::kickSub: case K::kick:      return { n::amber, n::orange };
        case K::snareTop: case K::snare:                                     return { n::cyan, n::blue };
        case K::snareBottom:                                                 return { n::blue, n::cyan };
        case K::hiHat: case K::ride: case K::cymbal:                         return { n::gold, n::lime };
        case K::tom: case K::floorTom:                                       return { n::magenta, n::orange };
        case K::overheads:                                                   return { n::blue, n::violet };
        case K::roomMono: case K::roomClose: case K::room:                   return { n::violet, n::magenta };
        case K::roomFar:                                                     return { n::violet, n::cyan };
        case K::roomCrush:                                                   return { n::magenta, n::pink };
        case K::ambient:                                                     return { n::pink, n::violet };
        default:                                                             return { juce::Colour (0xffb8c0cc), juce::Colour (0xff7d8794) };
    }
}

juce::Colour SessionView::roleColour (const MicRole& r, const theme::Palette& p) const
{
    auto c = roleColours (r).first;
    // On the light theme, deepen the colours so waveforms keep their contrast.
    return p.ground.getPerceivedBrightness() > 0.5f ? c.darker (0.6f) : c;
}

void SessionView::clampView()
{
    const double len = std::max (1.0, edit.getLength().inSeconds());
    const double visible = (getWidth() - headerWidth) / pixelsPerSecond;
    viewStart = juce::jlimit (0.0, std::max (0.0, len - visible * 0.5), viewStart);

    const int contentH = (int) lanes.size() * laneHeight;
    scrollY = juce::jlimit (0, std::max (0, contentH - (getHeight() - rulerHeight)), scrollY);
}

void SessionView::zoomToFit()
{
    const double len = std::max (1.0, edit.getLength().inSeconds());
    const int w = std::max (100, getWidth() - headerWidth - 16);
    pixelsPerSecond = w / len;
    viewStart = 0.0;
    repaint();
}

void SessionView::zoomBy (double factor, double anchorTime)
{
    const float anchorX = timeToX (anchorTime);
    pixelsPerSecond = juce::jlimit (2.0, 48000.0 / 2.0, pixelsPerSecond * factor);
    viewStart = anchorTime - (anchorX - headerWidth) / pixelsPerSecond;
    clampView();
    repaint();
}

void SessionView::resized()
{
    if (pixelsPerSecond <= 50.0 && viewStart == 0.0)
        zoomToFit();
    clampView();
}

//==============================================================================
void SessionView::paint (juce::Graphics& g)
{
    const auto p = palette();
    g.fillAll (p.ground);

    for (int i = 0; i < (int) lanes.size(); ++i)
        if (laneBounds (i).getBottom() > rulerHeight && laneBounds (i).getY() < getHeight())
            paintLane (g, p, i);

    paintRuler (g, p);

    // Playhead
    const double pos = edit.getTransport().getPosition().inSeconds();
    const float x = timeToX (pos);
    if (x >= headerWidth && x < getWidth())
    {
        g.setColour (p.signal.withAlpha (0.18f));
        g.fillRect (x - 3.0f, 0.0f, 7.0f, (float) getHeight());
        g.setColour (p.signal);
        g.fillRect (x - 0.5f, 0.0f, 1.5f, (float) getHeight());
        juce::Path tri;
        tri.addTriangle (x - 6, 0, x + 6, 0, x, 8);
        g.fillPath (tri);
    }
}

void SessionView::paintRuler (juce::Graphics& g, const theme::Palette& p)
{
    auto r = getLocalBounds().removeFromTop (rulerHeight);
    g.setColour (p.panel);
    g.fillRect (r);
    g.setColour (p.line);
    g.fillRect (r.removeFromBottom (1));

    // Header corner: session length
    g.setColour (p.inkQuiet);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (juce::String (lanes.size()) + " tracks  ·  " + juce::String (edit.getLength().inSeconds(), 1) + " s",
                juce::Rectangle<int> (12, 0, headerWidth - 12, rulerHeight), juce::Justification::centredLeft);

    // Tick spacing that leaves about 80 px between labels
    static const double steps[] = { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300 };
    double step = 300;
    for (auto s : steps) if (s * pixelsPerSecond >= 80) { step = s; break; }

    const double t0 = std::floor (viewStart / step) * step;
    g.saveState();
    g.reduceClipRegion (headerWidth, 0, getWidth() - headerWidth, rulerHeight);
    for (double t = t0; timeToX (t) < getWidth(); t += step)
    {
        const float x = timeToX (t);
        g.setColour (p.line);
        g.fillRect (x, rulerHeight - 9.0f, 1.0f, 8.0f);
        for (int k = 1; k < 5; ++k)
            g.fillRect (x + (float) (k * step / 5 * pixelsPerSecond), rulerHeight - 5.0f, 1.0f, 4.0f);

        juce::String label;
        if (step >= 1) label = juce::String ((int) (t / 60)) + ":" + juce::String ((int) std::fmod (t, 60.0)).paddedLeft ('0', 2);
        else label = juce::String (t, step >= 0.1 ? 1 : step >= 0.01 ? 2 : 3) + " s";
        g.setColour (p.inkQuiet);
        g.drawText (label, juce::Rectangle<float> (x + 4, 4, 90, 14), juce::Justification::centredLeft);
    }
    g.restoreState();
}

void SessionView::paintLane (juce::Graphics& g, const theme::Palette& p, int i)
{
    auto& lane = lanes[(size_t) i];
    auto b = laneBounds (i);
    const bool day = p.ground.getPerceivedBrightness() > 0.5f;
    auto [c1, c2] = roleColours (lane.role);
    if (day) { c1 = c1.darker (0.55f); c2 = c2.darker (0.55f); }
    const bool muted = lane.track->isMuted (false);
    const bool soloed = lane.track->isSolo (false);
    const bool anySolo = std::any_of (lanes.begin(), lanes.end(), [] (auto& l) { return l.track->isSolo (false); });
    const bool audible = ! muted && (! anySolo || soloed);

    // --- Header: a machined faceplate strip ---------------------------------------------
    auto h = b.withWidth (headerWidth).toFloat();
    g.setGradientFill (juce::ColourGradient (p.raised.brighter (day ? 0.05f : 0.06f), 0, h.getY(),
                                             p.panel.darker (day ? 0.02f : 0.25f), 0, h.getBottom(), false));
    g.fillRect (h);
    g.setColour (juce::Colours::white.withAlpha (day ? 0.5f : 0.05f));
    g.fillRect (h.getX(), h.getY(), h.getWidth(), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (day ? 0.15f : 0.5f));
    g.fillRect (h.getX(), h.getBottom() - 1, h.getWidth(), 1.0f);

    // Colour LED bar on the left edge
    auto led = juce::Rectangle<float> (h.getX() + 6, h.getY() + 10, 3, h.getHeight() - 20);
    if (audible && ! day)
    {
        g.setColour (c1.withAlpha (0.25f));
        g.fillRoundedRectangle (led.expanded (3, 2), 3.0f);
    }
    g.setGradientFill (juce::ColourGradient (c1.withAlpha (audible ? 1.0f : 0.35f), 0, led.getY(),
                                             c2.withAlpha (audible ? 1.0f : 0.35f), 0, led.getBottom(), false));
    g.fillRoundedRectangle (led, 1.5f);

    g.setColour (audible ? p.ink : p.inkQuiet);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (lane.track->getName(), (int) h.getX() + 18, (int) h.getY() + 10, headerWidth - 28, 18, juce::Justification::centredLeft, true);

    g.setColour (p.inkQuiet.withAlpha (0.8f));
    g.setFont (juce::FontOptions (10.5f));
    juce::String sub;
    if (! lane.clips.empty())
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (lane.clips.front().first.get()))
            sub = ac->getAudioFile().getFile().getFileName();
    g.drawText (sub, (int) h.getX() + 18, (int) h.getY() + 28, headerWidth - 28, 14, juce::Justification::centredLeft, true);

    // LED push buttons: lit when on
    auto drawLedButton = [&] (juce::Rectangle<int> r, const juce::String& t, bool on, juce::Colour lit)
    {
        auto rf = r.toFloat();
        if (on && ! day)
        {
            g.setColour (lit.withAlpha (0.3f));
            g.fillRoundedRectangle (rf.expanded (3), theme::radius::s + 2);
        }
        g.setGradientFill (juce::ColourGradient (on ? lit.brighter (0.2f) : p.raised.brighter (0.08f), 0, rf.getY(),
                                                 on ? lit.darker (0.3f) : p.raised.darker (0.2f), 0, rf.getBottom(), false));
        g.fillRoundedRectangle (rf, theme::radius::s);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (rf, theme::radius::s, 1.0f);
        g.setColour (on ? juce::Colour (0xff101114) : p.inkQuiet);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (t, r, juce::Justification::centred);
    };
    drawLedButton (soloBox (i), "S", soloed, theme::neon::gold);
    drawLedButton (muteBox (i), "M", muted, theme::neon::cyan);

    // --- Screen: recessed dark glass ---------------------------------------------------------
    auto screen = juce::Rectangle<float> ((float) headerWidth + 4, (float) b.getY() + 3, (float) getWidth() - headerWidth - 8, (float) laneHeight - 6);
    g.setColour (day ? juce::Colour (0xffe9eae6) : theme::glass::deep);
    g.fillRoundedRectangle (screen, 5.0f);

    g.saveState();
    g.reduceClipRegion (screen.toNearestInt());

    // faint grid: a line per ruler tick
    static const double steps[] = { 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60 };
    double step = 60;
    for (auto st : steps) if (st * pixelsPerSecond >= 80) { step = st; break; }
    g.setColour (day ? juce::Colour (0x14000000) : theme::glass::grid);
    for (double t = std::floor (viewStart / step) * step; timeToX (t) < getWidth(); t += step)
        g.fillRect (timeToX (t), screen.getY(), 1.0f, screen.getHeight());
    g.fillRect (screen.getX(), screen.getCentreY(), screen.getWidth(), 1.0f);

    for (auto& [clip, thumb] : lane.clips)
    {
        const auto pos = clip->getPosition();
        if (thumb == nullptr || thumb->isGeneratingProxy())
        {
            g.setColour (p.inkQuiet);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("Reading waveform\u2026", screen.reduced (10, 0), juce::Justification::centredLeft);
            continue;
        }

        const double visStart = std::max (pos.getStart().inSeconds(), xToTime ((float) headerWidth));
        const double visEnd = std::min (pos.getEnd().inSeconds(), xToTime ((float) getWidth()));
        if (visEnd <= visStart) continue;

        const double off = pos.getOffset().inSeconds() - pos.getStart().inSeconds();
        const te::TimeRange src (te::TimePosition::fromSeconds (visStart + off), te::TimePosition::fromSeconds (visEnd + off));
        auto r = juce::Rectangle<int> ((int) timeToX (visStart), (int) screen.getY() + 4,
                                       std::max (1, (int) (timeToX (visEnd) - timeToX (visStart))), (int) screen.getHeight() - 8);
        const float alpha = audible ? 1.0f : 0.3f;
        // Display-only vertical zoom: the loudest moment of this file fills ~90% of the lane.
        const float peak = thumb->getApproximatePeak();
        const float vz = peak > 0.0001f ? juce::jlimit (1.0f, 12.0f, 0.9f / peak) : 1.0f;

        // Glow: a wider, faint pass under the waveform
        if (! day && audible)
        {
            g.setColour (c1.withAlpha (0.18f));
            thumb->drawChannels (g, r.expanded (0, 3), src, vz);
        }

        // Gradient fill across time: colour one into colour two
        juce::ColourGradient grad (c1.withAlpha (alpha), (float) r.getX(), 0, c2.withAlpha (alpha), (float) r.getRight(), 0, false);
        if (r.getWidth() > getWidth() / 2)
        {
            grad = juce::ColourGradient (c1.withAlpha (alpha), (float) headerWidth, 0, c2.withAlpha (alpha), (float) getWidth(), 0, false);
            grad.addColour (0.5, c1.interpolatedWith (c2, 0.5f).brighter (0.25f).withAlpha (alpha));
        }
        g.setGradientFill (grad);
        thumb->drawChannels (g, r, src, vz);
    }

    // Glass: inner shadow at the top, reflection line
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (day ? 0.08f : 0.55f), 0, screen.getY(),
                                             juce::Colours::transparentBlack, 0, screen.getY() + 10, false));
    g.fillRect (screen.withHeight (10));
    g.restoreState();

    g.setColour (day ? juce::Colour (0x22000000) : theme::glass::rim);
    g.drawRoundedRectangle (screen, 5.0f, 1.0f);
}

//==============================================================================
void SessionView::timerCallback()
{
    const double pos = edit.getTransport().getPosition().inSeconds();
    const float x = timeToX (pos);

    // Keep the playhead on screen while playing
    if (edit.getTransport().isPlaying() && x > getWidth() - 40)
    {
        viewStart = pos - 40.0 / pixelsPerSecond;
        clampView();
        repaint();
        return;
    }

    if (std::abs (x - lastPlayheadX) >= 0.5)
    {
        repaint ((int) std::min ((double) x, lastPlayheadX) - 8, 0, (int) std::abs (x - lastPlayheadX) + 18, getHeight());
        lastPlayheadX = x;
    }

    for (auto& l : lanes)
        for (auto& c : l.clips)
            if (c.second != nullptr && c.second->isGeneratingProxy())
            {
                repaint();
                return;
            }
}

void SessionView::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) lanes.size(); ++i)
    {
        auto* t = lanes[(size_t) i].track;
        if (soloBox (i).contains (e.getPosition())) { t->setSolo (! t->isSolo (false)); repaint(); return; }
        if (muteBox (i).contains (e.getPosition())) { t->setMute (! t->isMuted (false)); repaint(); return; }
    }

    if (e.x > headerWidth)
        mouseDrag (e);
}

void SessionView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getMouseDownX() <= headerWidth)
        return;

    const double t = juce::jlimit (0.0, edit.getLength().inSeconds(), xToTime ((float) e.x));
    edit.getTransport().setPosition (te::TimePosition::fromSeconds (t));
    repaint();
}

void SessionView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        zoomBy (std::pow (1.0025, w.deltaY * 600.0), xToTime ((float) std::max (e.x, headerWidth)));
        return;
    }

    const float dx = e.mods.isShiftDown() ? w.deltaY : w.deltaX;
    const float dy = e.mods.isShiftDown() ? 0.0f : w.deltaY;
    viewStart -= dx * 300.0 / pixelsPerSecond;
    scrollY -= (int) std::lround (dy * 300.0);
    clampView();
    repaint();
}

void SessionView::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) lanes.size(); ++i)
        if (laneBounds (i).withWidth (headerWidth).withHeight (34).contains (e.getPosition()))
            return beginRename (i);
}

void SessionView::beginRename (int index)
{
    auto* track = lanes[(size_t) index].track;
    const auto p = palette();
    renameEditor = std::make_unique<juce::TextEditor>();
    auto& ed = *renameEditor;
    ed.setText (track->getName(), false);
    ed.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    ed.setColour (juce::TextEditor::backgroundColourId, p.raised);
    ed.setColour (juce::TextEditor::textColourId, p.ink);
    ed.setColour (juce::TextEditor::outlineColourId, p.select);
    ed.setColour (juce::TextEditor::focusedOutlineColourId, p.select);
    ed.setBounds (laneBounds (index).getX() + 10, laneBounds (index).getY() + 7, headerWidth - 20, 24);
    addAndMakeVisible (ed);
    ed.selectAll();
    ed.grabKeyboardFocus();

    auto commit = [this, track]
    {
        if (renameEditor == nullptr) return;
        const auto newName = renameEditor->getText().trim();
        if (newName.isNotEmpty() && newName != track->getName())
        {
            edit.getUndoManager().beginNewTransaction ("Rename " + track->getName() + " to " + newName);
            track->setName (newName);
        }
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<SessionView> (this)] { if (sp) { sp->renameEditor = nullptr; sp->repaint(); } });
    };
    ed.onReturnKey = commit;
    ed.onFocusLost = commit;
    ed.onEscapeKey = [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<SessionView> (this)] { if (sp) sp->renameEditor = nullptr; }); };
}

void SessionView::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    zoomBy (scale, xToTime ((float) std::max (e.x, headerWidth)));
}

} // namespace ld

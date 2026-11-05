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

juce::Colour SessionView::roleColour (const MicRole& r, const theme::Palette& p) const
{
    auto c = [&]() -> juce::Colour {
    using K = MicRole::Kind;
    switch (r.kind)
    {
        case K::kickIn: case K::kickOut: case K::kickSub: case K::kick:      return p.signal;
        case K::snareTop: case K::snareBottom: case K::snare:                return p.select;
        case K::hiHat: case K::ride: case K::cymbal:                         return juce::Colour (0xffe8c75a);
        case K::tom: case K::floorTom:                                       return juce::Colour (0xffe07a5f);
        case K::overheads:                                                   return juce::Colour (0xff8fb8ff);
        case K::roomMono: case K::roomClose: case K::room: case K::roomFar:
        case K::roomCrush: case K::ambient:                                  return p.reference;
        default:                                                             return p.inkQuiet;
    }
    }();
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
    const auto colour = roleColour (lane.role, p);
    const bool muted = lane.track->isMuted (false);
    const bool soloed = lane.track->isSolo (false);
    const bool anySolo = std::any_of (lanes.begin(), lanes.end(), [] (auto& l) { return l.track->isSolo (false); });
    const bool audible = ! muted && (! anySolo || soloed);

    // Lane background and separator
    g.setColour (i % 2 == 0 ? p.ground : p.ground.interpolatedWith (p.panel, 0.35f));
    g.fillRect (b);
    g.setColour (p.line);
    g.fillRect (b.getX(), b.getBottom() - 1, b.getWidth(), 1);

    // Header
    auto h = b.withWidth (headerWidth);
    g.setColour (p.panel);
    g.fillRect (h);
    g.setColour (colour);
    g.fillRect (h.getX(), h.getY() + 6, 3, h.getHeight() - 12);
    g.setColour (p.line);
    g.fillRect (h.getRight() - 1, h.getY(), 1, h.getHeight());

    g.setColour (audible ? p.ink : p.inkQuiet);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (lane.track->getName(), h.getX() + 14, h.getY() + 10, headerWidth - 24, 18, juce::Justification::centredLeft, true);

    g.setColour (p.inkQuiet);
    g.setFont (juce::FontOptions (11.0f));
    juce::String sub;
    if (! lane.clips.empty())
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (lane.clips.front().first.get()))
            sub = ac->getAudioFile().getFile().getFileName();
    g.drawText (sub, h.getX() + 14, h.getY() + 28, headerWidth - 24, 14, juce::Justification::centredLeft, true);

    auto drawToggle = [&] (juce::Rectangle<int> r, const juce::String& t, bool on, juce::Colour onColour)
    {
        g.setColour (on ? onColour : p.raised);
        g.fillRoundedRectangle (r.toFloat(), theme::radius::s);
        g.setColour (on ? p.ground : p.inkQuiet);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (t, r, juce::Justification::centred);
    };
    drawToggle (soloBox (i), "S", soloed, juce::Colour (0xffe8c75a));
    drawToggle (muteBox (i), "M", muted, p.select);

    // Waveforms
    g.saveState();
    g.reduceClipRegion (headerWidth, b.getY(), getWidth() - headerWidth, b.getHeight() - 1);
    for (auto& [clip, thumb] : lane.clips)
    {
        const auto pos = clip->getPosition();
        const float x1 = timeToX (pos.getStart().inSeconds()), x2 = timeToX (pos.getEnd().inSeconds());
        auto area = juce::Rectangle<float> (x1, (float) b.getY() + 4, x2 - x1, (float) laneHeight - 9);

        g.setColour (colour.withAlpha (0.08f));
        g.fillRoundedRectangle (area, 3.0f);

        if (thumb != nullptr && ! thumb->isGeneratingProxy())
        {
            // Only draw the visible part of the clip
            const double visStart = std::max (pos.getStart().inSeconds(), xToTime ((float) headerWidth));
            const double visEnd = std::min (pos.getEnd().inSeconds(), xToTime ((float) getWidth()));
            if (visEnd > visStart)
            {
                const double off = pos.getOffset().inSeconds() - pos.getStart().inSeconds();
                auto r = juce::Rectangle<int> ((int) timeToX (visStart), (int) area.getY() + 2,
                                               std::max (1, (int) (timeToX (visEnd) - timeToX (visStart))), (int) area.getHeight() - 4);
                g.setColour (colour.withAlpha (audible ? 0.95f : 0.35f));
                thumb->drawChannels (g, r, te::TimeRange (te::TimePosition::fromSeconds (visStart + off),
                                                          te::TimePosition::fromSeconds (visEnd + off)), 1.0f);
            }
        }
        else
        {
            g.setColour (p.inkQuiet);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("Reading waveform…", area.reduced (8, 0), juce::Justification::centredLeft);
        }
    }
    g.restoreState();
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

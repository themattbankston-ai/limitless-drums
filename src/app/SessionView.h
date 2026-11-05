#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theme.h"
#include "core/Naming.h"

namespace ld
{
/** The multitrack view: a ruler, one lane per mic (header with solo/mute, waveform), and the
    playhead. Scroll: two-finger vertical moves through tracks, horizontal or shift-scroll moves
    in time; pinch or Cmd-scroll zooms around the pointer. Click in the ruler or a lane to move
    the playhead. */
class SessionView : public juce::Component, private juce::Timer
{
public:
    SessionView (tracktion::Edit&, std::vector<MicRole> roles, std::function<theme::Palette()> palette);
    ~SessionView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void zoomToFit();
    void zoomBy (double factor, double anchorTime);

    static constexpr int rulerHeight = 30, headerWidth = 210;

private:
    struct Lane
    {
        tracktion::AudioTrack* track = nullptr;
        MicRole role;
        std::vector<std::pair<tracktion::Clip::Ptr, std::unique_ptr<tracktion::SmartThumbnail>>> clips;
    };

    tracktion::Edit& edit;
    std::function<theme::Palette()> palette;
    std::vector<Lane> lanes;
    double pixelsPerSecond = 50.0, viewStart = 0.0;
    int laneHeight = 76, scrollY = 0;
    double lastPlayheadX = -1;
    std::unique_ptr<juce::TextEditor> renameEditor;
    void beginRename (int index);

    void timerCallback() override;
    double xToTime (float x) const;
    float timeToX (double t) const;
    juce::Rectangle<int> laneBounds (int index) const;
    juce::Rectangle<int> soloBox (int index) const;
    juce::Rectangle<int> muteBox (int index) const;
    juce::Colour roleColour (const MicRole&, const theme::Palette&) const;
    void clampView();
    void paintRuler (juce::Graphics&, const theme::Palette&);
    void paintLane (juce::Graphics&, const theme::Palette&, int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionView)
};
} // namespace ld

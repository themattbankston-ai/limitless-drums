#pragma once

#include "samples/SampleModel.h"
#include <visage/ui.h>
#include <functional>
#include <string>
#include <vector>

namespace ld::samples
{

/** The sample build's window contents: the toolbar and audition bar shared by every step, and the Hits step (the
    edit window). Drawn on the GPU; behaves like a standard DAW edit window (select and play or loop it, zoom, track
    and waveform height, pop-up monitor faders). Layout and colours follow the approved design canvas. */
class HitsView : public visage::Frame
{
public:
    explicit HitsView (SampleModel&);

    void draw (visage::Canvas&) override;
    void mouseDown (const visage::MouseEvent&) override;
    void mouseDrag (const visage::MouseEvent&) override;
    void mouseUp (const visage::MouseEvent&) override;
    bool mouseWheel (const visage::MouseEvent&) override;
    bool keyPress (const visage::KeyEvent&) override;

    /** Call after a take opens or Auto Prep finishes. */
    void takeChanged();
    void setStatus (const std::string& s) { status = s; redraw(); }
    void tick();   ///< 30 times a second: playhead, meters

    std::function<void()> onOpen, onAutoPrep;

private:
    SampleModel& model;
    struct Hit { float x, y, w, h; std::string id; };
    std::vector<Hit> hits;   ///< what's clickable this frame
    void add (float x, float y, float w, float h, const std::string& id) { hits.push_back ({ x, y, w, h, id }); }
    const Hit* hitAt (float x, float y) const;
    void act (const std::string& id, const visage::MouseEvent&);

    // view
    double scroll = 0, pps = 40;      ///< seconds at the left edge, pixels per second
    float waveScale = 1;              ///< waveform height (the drawing, not the audio)
    int heightIdx = 1;                ///< Small, Medium, Large, Jumbo
    int selHit = -1;
    bool needsFit = true;            ///< fit the take once the frame has its size
    double selA = 0, selB = 0;        ///< the selection, seconds
    int playMode = 0;
    bool loopOn = true, followOn = true, lmOn = true, padsOn = false;
    int ab = 1;                       ///< 0 A, 1 B, 2 A then B
    std::string status;

    // gestures
    enum class Drag { none, select, fader, pan } drag = Drag::none;
    int dragTrack = -1;
    double dragFrom = 0;
    float dragY0 = 0, dragV0 = 0;
    int popTrack = -1;                ///< the track whose fader is popped up
    bool popPan = false;

    // layout
    static constexpr float tbH = 52, auH = 50, sbH = 38, rulH = 22, headW = 268, inspW = 286, closeH = 168;
    float laneTop() const { return tbH + auH + sbH + 3 * rulH; }
    float laneX() const { return headW; }
    float laneW() const { return std::max (100.0f, width() - headW - inspW); }
    float trackH (int t) const;
    float trackY (int t) const;
    double timeAt (float x) const { return scroll + (x - laneX()) / pps; }
    float xOf (double t) const { return laneX() + (float) ((t - scroll) * pps); }

    void selectHit (int i, bool play, bool andRedraw = true);
    void zoomBy (double f);
    void fitAll();
    void playSelection();

    void drawToolbar (visage::Canvas&);
    void drawAudition (visage::Canvas&);
    void drawStepBar (visage::Canvas&);
    void drawRulers (visage::Canvas&);
    void drawTracks (visage::Canvas&);
    void drawInspector (visage::Canvas&);
    void drawCloseUp (visage::Canvas&);
    void drawPopup (visage::Canvas&);
};

} // namespace ld::samples

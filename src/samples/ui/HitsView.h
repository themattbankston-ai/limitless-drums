#pragma once

#include "samples/SampleModel.h"
#include "hl/Session.h"
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

    /** Call after a take opens or Auto Prep finishes (refit: fit the view to the take again). */
    void takeChanged (bool refit = true);
    void setStatus (const std::string& s) { status = s; redraw(); }
    void tick();   ///< 30 times a second: playhead, meters

    std::function<void()> onOpen, onAutoPrep;
    std::function<void()> onExport;         ///< the Export step's button (the shell picks the folder)
    std::function<void()> onLoadReference;  ///< the Tone step's reference (the shell picks the file)
    int currentStep() const { return step; }
    void setStep (int s) { step = std::clamp (s, 0, 7); rowScroll = 0; redraw(); }
    void setExportProgress (float p) { exportProgress = p; redraw(); }
    std::string exportFolder;               ///< where the export goes (the shell sets it)

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

    int step = 1;                      ///< 0 Setup .. 7 Export
    int alignUnits = 0;               ///< ms, samples, feet
    int selMic = -1;                  ///< the mic the Align, Tone and Level steps act on (-1: all / the lead)
    float exportProgress = -1;
    float barEnd = 0;
    std::string exportKey;
    std::vector<ld::hl::ExportItem> exportCache;
    bool lastActive = true;
    double lastPos = 0;           ///< something moved last tick (playing, rendering, a meter)                 ///< where the step bar's controls end (the status goes after)

    // gestures
    enum class Drag { none, select, fader, pan, slider } drag = Drag::none;
    std::string sliderKey;
    int dragTrack = -1;
    double dragFrom = 0;
    float dragY0 = 0, dragV0 = 0, dragY1 = 0;
    std::string editKey, editText;    ///< the control whose value is being typed, and what's typed so far
    void commitEdit (bool keep);
    float rowScroll = 0;              ///< how far the track rows are scrolled down (when they don't all fit)
    void clipHits (size_t from, float top, float bottom);   ///< keep this frame's later targets inside a scrolled area
    int ctlTrack = -1;                ///< the track whose fader or pan is being dragged

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
    void drawTrackHeader (visage::Canvas&, int t, float y, float h, bool withPolarity);
    void drawTrackColumn (visage::Canvas&);
    float stepX = 0;                  ///< where the step's own area starts (right of the track column, when there is one)
    float stepW() const { return width() - stepX; }

    // the other steps (StepViews.cpp)
    void drawStep (visage::Canvas&);
    void drawSetup (visage::Canvas&);
    void drawAlign (visage::Canvas&);
    void drawTune (visage::Canvas&);
    void drawTone (visage::Canvas&);
    void drawClean (visage::Canvas&);
    void drawLevel (visage::Canvas&);
    void drawExport (visage::Canvas&);
    bool actStep (const std::string& id, const visage::MouseEvent&);   ///< true when it was one of theirs
    void slider (visage::Canvas&, float x, float y, float w, const std::string& label, const std::string& key);
    void segmented (visage::Canvas&, float x, float y, const std::vector<std::string>& labels, int sel, const std::string& idPrefix, float h = 28);
    double sliderValue (const std::string& key) const;
    static double sliderSpan (const std::string& key);
    void setSliderValue (const std::string& key, double v);
    void knobCtl (visage::Canvas&, float cx, float cy, float d, const std::string& label, const std::string& key);
    static std::string valueText (const std::string& key, double v);
};

} // namespace ld::samples

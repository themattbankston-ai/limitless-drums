#pragma once

#include <visage/app.h>
#include "Wave.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ld::ui
{
/** Everything the interface shows, as plain data. MainComponent fills it from the edit; the
    interface never touches the engine directly and reports what you do through callbacks. */
struct TrackInfo
{
    std::string name, model;          ///< "Snare Top", "SM57"
    bool stereo = false, isRoom = false, isDrumIcon = false;
    unsigned colourA = 0xff2ee6ff, colourB = 0xff4c8dff;
    bool solo = false, mute = false;
    float volumeDb = 0.0f, pan = 0.0f;
    float panL = -1.0f, panR = 1.0f;  ///< a stereo mic's two pans (Pro Tools)
    std::vector<float> peaks;         ///< max |sample| per block, the recording (A)
    std::vector<float> peaksB;        ///< the same, processed and on its levels (B); empty until ready
    double blocksPerSecond = 750.0;
    std::shared_ptr<const Wave> wave; ///< the audio, for the lane at any zoom
};

struct HitInfo { double time = 0.0; int layer = 0, roundRobin = 0; std::vector<float> micDb; /**< its level per track, once measured */ };

struct LayerInfo
{
    double start = 0.0, end = 0.0;
    int midiLow = 1, midiHigh = 127, numHits = 0;
    std::string note;
    std::vector<float> micDb;         ///< mean peak per track, in track order
};

struct StudioModel
{
    bool hasSession = false;
    std::string title;                ///< take or project name
    double length = 0.0, bpm = 120.0;
    std::vector<TrackInfo> tracks;
    std::vector<HitInfo> hits;
    std::vector<LayerInfo> layers;
    std::vector<float> arrivalMs;     ///< per track, relative to the reference mic (NaN = unknown)
    int referenceTrack = 0;
    /** The Level step as the engine has it, for the rack's Level module. */
    struct LevelView
    {
        bool valid = false;
        bool linked = false, pro = false;
        float shape = 0.9f, offsetDb = 0, rangeDb = 24, winMs = 60;
        int onStandard = 0, total = 0;
        std::vector<float> curveDb;   ///< the lead's level per velocity layer
    } level;
};

enum class Step { session, hits, align, tune, mix, level, exportStep };

/** What the header shows (Hitline's steps, Auto Prep and transport). */
struct HeaderState
{
    std::string sessionLine = "No session", detailLine = "Drop a folder of mics";
    std::string status[7];            ///< per step: "7 MICS", "36 HITS", ...
    bool done[7] {};
    bool exportReady = false, canUndo = false, canRedo = false;
    int historyCount = 0;
    int vel = -1, rr = -1, hit = -1, hits = 0, midiLo = 0, midiHi = 0;
    std::vector<float> hitWave;       ///< the selected hit, peak per point, for the readout
    bool processed = true, chain = false, playing = false, prepping = false;
    std::string mode = "hit";
    bool loop = false, follow = true, lm = true, pads = false, rudiments = false;
    float phase = NAN, meterL = 0, meterR = 0;
    std::string monitorMic;           ///< the mic the heart monitor follows
    int velocities = 6;
};

class Studio : public visage::Frame
{
public:
    Studio();
    ~Studio() override;

    void setModel (StudioModel);
    const StudioModel& model() const { return data; }
    void setTrackState (int index, bool solo, bool mute, float volumeDb, float pan, float panL = -1.0f, float panR = 1.0f);
    void setTransport (double seconds, bool playing);
    void setDayMode (bool day);
    bool isDayMode() const { return day; }
    void setNotice (std::string text) { notice = std::move (text); redrawAll(); }
    void setDeviceText (std::string text) { device = std::move (text); redrawAll(); }
    void setHistory (std::vector<std::string> entries, int current);
    void setHistoryVisible (bool);
    bool isHistoryVisible() const;
    void setStep (Step);
    void setHeader (HeaderState);
    HeaderState& headerState() { return hdr; }
    /** The heart monitor: the selected mic's level and the velocity of the hit it belongs to, ~600 points/s. */
    void pushMonitor (const float* level, const int* vel, int n);
    Step step() const { return currentStep; }
    void zoomToFit();
    void zoomBy (double factor, double anchorSeconds);
    /** The big fader beside a track's header while its mini fader is held (-1 hides it). */
    void showBigFader (int track);
    void flashPad (int velocity);

    /** Called ~60 times a second for the playhead, meters and the transient pulses. */
    void tick (double nowSeconds);

    // What you did
    std::function<void()> onImport, onPlay, onRewind, onRecord, onThemeToggled, onHistoryToggled;
    // the header
    std::function<void (Step)> onStepClicked;
    std::function<void()> onAutoPrep, onUndo, onRedo, onPrevHit, onNextHit;
    std::function<void (int)> onAB;                    ///< 0 = A, 1 = B, 2 = A then B
    std::function<void (const std::string&)> onMode, onToggle;
    std::function<void (int)> onSelectTrack;           ///< the mic the heart monitor follows
    std::function<void (int, int)> onMoveTrack;        ///< from, to (track order)
    std::function<void (double)> onSeek;
    std::function<void (int, bool)> onSolo, onMute;
    std::function<void (int, float)> onVolume, onPan;
    std::function<void (int, int, float)> onPanLR;
    std::function<void (int)> onPad;                  ///< a velocity pad (0-based): plays a random round robin of it     ///< track, 0 = left channel / 1 = right channel, pan
    std::function<void (int)> onRename, onHistoryJump;
    std::function<void (const std::vector<std::string>&)> onFilesDropped;
    std::function<bool (const visage::KeyEvent&)> onKey;

    // visage::Frame
    void draw (visage::Canvas&) override;
    void resized() override;
    bool keyPress (const visage::KeyEvent&) override;
    void mouseDown (const visage::MouseEvent&) override;
    bool receivesDragDropFiles() override { return true; }
    std::string dragDropFileExtensionRegex() override { return ".*"; }
    void dragFilesEnter (const std::vector<std::string>&) override { dropHover = true; redrawAll(); }
    void dragFilesExit() override { dropHover = false; redrawAll(); }
    void dropFiles (const std::vector<std::string>&) override;

    // Shared with the panels
    struct View { double pixelsPerSecond = 20.0, start = 0.0; float scrollY = 0.0f; };
    View view;
    int selectedHit = -1;
    double playhead = 0.0;
    bool playing = false, dropHover = false;
    std::vector<double> pulseStart;   ///< per hit, when the playhead last crossed it
    double now = 0.0;
    std::string notice, device;
    std::vector<std::string> history;
    int historyIndex = 0;
    bool day = false;
    Step currentStep = Step::session;
    StudioModel data;
    HeaderState hdr;
    std::vector<float> monLevel; std::vector<int> monVel; int monHead = 0;
    int selectedTrack = 0;
    float abMix = 1;                  ///< lanes: 0 = the recording, 1 = processed

    float laneHeight() const;
    float screenContentTop() const;   ///< y (in screen coords) where lanes start
    double screenWidthForZoom() const;

private:
    struct Header; struct Strips; struct Screen; struct Rack; struct Footer; struct HistoryPanel; struct Console; struct BigFader;
    std::unique_ptr<Header> header;
    std::unique_ptr<Strips> strips;
    std::unique_ptr<BigFader> bigFader;
    std::unique_ptr<Screen> screen;
    std::unique_ptr<Console> console;
    std::unique_ptr<Rack> rack;
    std::unique_ptr<Footer> footer;
    std::unique_ptr<HistoryPanel> historyPanel;
    double lastPlayhead = 0.0;
    bool atFit = true;
    void layoutPanels();
    friend struct Screen;
};
} // namespace ld::ui

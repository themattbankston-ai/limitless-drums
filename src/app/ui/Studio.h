#pragma once

#include <visage/app.h>
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
    std::vector<float> peaks;         ///< max |sample| per block
    double blocksPerSecond = 750.0;
};

struct HitInfo { double time = 0.0; int layer = 0, roundRobin = 0; };

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
};

enum class Step { importStep, align, levels, tune, mix, exportStep };

class Studio : public visage::Frame
{
public:
    Studio();
    ~Studio() override;

    void setModel (StudioModel);
    const StudioModel& model() const { return data; }
    void setTrackState (int index, bool solo, bool mute, float volumeDb, float pan);
    void setTransport (double seconds, bool playing);
    void setDayMode (bool day);
    bool isDayMode() const { return day; }
    void setNotice (std::string text) { notice = std::move (text); redrawAll(); }
    void setDeviceText (std::string text) { device = std::move (text); redrawAll(); }
    void setHistory (std::vector<std::string> entries, int current);
    void setHistoryVisible (bool);
    bool isHistoryVisible() const;
    void setStep (Step);
    Step step() const { return currentStep; }
    void zoomToFit();
    void zoomBy (double factor, double anchorSeconds);

    /** Called ~60 times a second for the playhead, meters and the transient pulses. */
    void tick (double nowSeconds);

    // What you did
    std::function<void()> onImport, onPlay, onRewind, onRecord, onThemeToggled, onHistoryToggled;
    std::function<void (double)> onSeek;
    std::function<void (int, bool)> onSolo, onMute;
    std::function<void (int, float)> onVolume, onPan;
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
    Step currentStep = Step::levels;
    StudioModel data;

    float laneHeight() const;
    float screenContentTop() const;   ///< y (in screen coords) where lanes start
    double screenWidthForZoom() const;

private:
    struct Header; struct Strips; struct Screen; struct Rack; struct Footer; struct HistoryPanel; struct Console;
    std::unique_ptr<Header> header;
    std::unique_ptr<Strips> strips;
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

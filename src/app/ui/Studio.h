#pragma once

#include <visage/app.h>
#include "Wave.h"
#include <array>
#include <cmath>
#include <functional>
#include <map>
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
    /** The Align step: every mic's alignment, and the scope on the selected mic and hit. */
    struct AlignView
    {
        bool valid = false;
        std::string timing = "tight";             ///< tight, pro, natural
        int fightingBefore = 0, fightingAfter = 0;
        float sumBefore = NAN, sumAfter = NAN;    ///< the mics' average sum with what they line up to
        struct Mic
        {
            std::string name;                     ///< the mic
            std::string status;                   ///< lead, inphase, corrected, manual
            std::string to;                       ///< what it lines up to, as the menu says it
            float delayMs = 0, delaySmp = 0;
            bool flipped = false, eq = false, eqAvail = false, lr = false, lrAvail = false, changed = false;
            float before = NAN, after = NAN;
            std::array<float, 8> bandsBefore {}, bandsAfter {};
            std::string eqText;                   ///< "all-pass at 190 + 410 Hz"
            // its row: the track over what it lines up to around the selected hit (as it sits now, and as recorded)
            std::vector<float> ref, cur, rec;
            float spanMs = 8;
        };
        std::vector<Mic> mics;                    ///< in track order
        int mic = -1;                             ///< the mic on the scope
        bool spread = true;                       ///< the heat over a spread of hits (else this hit)
        bool samples = false;                     ///< delays in samples (else ms)
        // the scope, around the selected hit: the reference, the mic as it sits now and as recorded
        std::vector<float> ref, cur, rec;
        float preMs = 1.5f, spanMs = 8, sr = 44100;
        std::vector<float> heat;                  ///< the heat curve around the current delay
        float heatFrom = 0, heatStep = 0.25f;     ///< samples
        float best = NAN, bestHeat = NAN, curHeat = NAN, lag = 0;
        int hit = -1;                             ///< the hit the rows and scope show
    } align;
    /** The Tune step: where the drum sits, where it's going, every hit's pitch. */
    struct TuneView
    {
        bool tunable = false, read = false, on = false, rr = false;   ///< rr: even out round robins
        std::string advice;                       ///< "", "cymbal" or "nopitch": tuning isn't recommended
        bool anyway = false;                      ///< tuned though it isn't recommended
        std::string from = "Top", target = "Nearest";                 ///< read from; the target as the menu says it
        float cents = 0, a4 = 440, fmin = 120, fmax = 400, skipMs = 15;
        // the drum
        float medF = NAN, medCents = NAN;          ///< cents from the nearest note
        std::string sitsAt, tunedTo;               ///< "A#3", "B3 +12"
        float ratio = 1, shiftSemis = 0;
        float spreadBefore = NAN, spreadAfter = NAN;   ///< widest spread inside one velocity, cents
        int worstLayer = -1; float worstCents = NAN;
        std::vector<float> layerHz;                ///< per velocity
        // every hit: cents from the drum's pitch, as recorded and tuned; its layer
        std::vector<float> before, after; std::vector<int> layer; int layers = 0;
        // the selected hit
        int hit = -1; float hitF = NAN, hitTunedF = NAN, hitCents = NAN, hitCentsBefore = NAN; std::string hitNote;
        float targetPos = 0.5f;                    ///< the target knob, 0..1 over its notes
    } tune;
    /** The Mix step: the channel strip of the selected track (or the bus) and the reference deck. */
    struct MixView
    {
        bool valid = false;
        int sel = -1;                              ///< -1: the bus, else a track
        std::string selName = "Bus";
        bool studioAvail = false, studio = false;  ///< the measured source the pro tone pulls toward
        std::array<float, 5> tone {}, shape {};    ///< match, bright, crack, smooth, tame / atk, dec, sus, rel, match
        bool cymbal = false;
        // the selected hit on the selected channel: third-octave shape before and after, and the envelope (dB)
        std::vector<float> specBefore, specAfter, refDelta, envBefore, envAfter;
        bool hasRef = false, refAudio = false; std::string refName; float refAmt = 0;
        bool hasRefEnv = false; float refEnvAmt = 0;  ///< Match envelope
        struct Verb { std::string type = "room"; float decay = 1, pre = 0, size = 1, tone = 0, width = 1; };
        std::array<Verb, 2> verbs;                 ///< R1, R2
        std::vector<float> refWave;                ///< peaks of the reference's first hit
        struct Ins { std::string name; bool on = false; };
        std::vector<Ins> inserts;                  ///< the selected channel's insert slots
        /** One mixer strip: its five insert slots (empty name = empty slot) and its reverb sends (dB, -60 = off). */
        struct Chan { std::array<Ins, 5> ins; float r1 = -60, r2 = -60; };
        std::vector<Chan> chans;                   ///< per track, in track order
        Chan bus;                                  ///< the drum bus
        float busFader = 0; bool busMute = false;
        bool fxOn = false;
    } mix;
};

/** An open plugin window: which slot, its type, and its settings flattened to paths ("decap.drive"). */
struct PluginView
{
    bool open = false;
    int who = -1, slot = 0;           ///< who: track index, -1 the drum bus
    std::string type, track;
    bool on = true;
    std::map<std::string, double> num;      ///< numbers and switches (1 / 0)
    std::map<std::string, std::string> str; ///< choices ("model", "decap.style")
    double get (const std::string& k, double d) const { auto i = num.find (k); return i == num.end() ? d : i->second; }
    std::string gets (const std::string& k, const std::string& d) const { auto i = str.find (k); return i == str.end() ? d : i->second; }
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
    /** Opens, updates or closes (open = false) the floating plugin window. */
    void setPlugin (PluginView);
    float pluginLevel (int who) const;
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
    /** The Align module: action ("again", "timing", "select", "to", "nudge", "lag", "snap", "flip", "eq", "lr",
        "reset", "pair", "gain", "spread", "units"), the mic, a value; for the two menus also where the selector is. */
    std::function<void (const std::string&, int, double)> onAlign;
    std::function<void (const std::string&, float, float, float, float)> onAlignMenu;
    /** The Tune module: action ("on", "read", "mode", "fine", "a4", "target", "nearest", "play", "tone"), a value;
        and the menus (read from, target, range) with where they open. */
    std::function<void (const std::string&, double)> onTune;
    std::function<void (const std::string&, float, float, float, float)> onTuneMenu;
    /** The Mix module: action, value; and its menus with where they open. */
    std::function<void (const std::string&, double)> onMix;
    std::function<void (const std::string&, float, float, float, float)> onMixMenu;
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
    struct Header; struct Strips; struct Screen; struct Rack; struct Footer; struct HistoryPanel; struct Console; struct BigFader; struct PluginWindow;
    std::unique_ptr<Header> header;
    std::unique_ptr<Strips> strips;
    std::unique_ptr<BigFader> bigFader;
    std::unique_ptr<Screen> screen;
    std::unique_ptr<Console> console;
    std::unique_ptr<Rack> rack;
    std::unique_ptr<Footer> footer;
    std::unique_ptr<HistoryPanel> historyPanel;
    std::unique_ptr<PluginWindow> pluginWin;
    double lastPlayhead = 0.0, lastTick = 0.0;
    bool atFit = true;
    void layoutPanels();
    friend struct Screen;
    friend struct PluginWindow;
};
} // namespace ld::ui

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "Engine.h"
#include <atomic>
#include <memory>
#include <thread>

namespace ld
{
/** One hit in a phrase: when it starts (seconds from the start), which hit, its velocity layer. */
struct PlayEvent { double at = 0; int hit = 0; int vel = 0; };

/** Hitline's play modes, as phrases of hits (40_audio.js and 28_extras.js). */
namespace modes
{
    std::vector<PlayEvent> hit (const hl::Session&, int sel);
    std::vector<PlayEvent> layer (const hl::Session&, int sel);
    std::vector<PlayEvent> sweep (const hl::Session&, int sel);
    /** Groove: two bars of hard quarter notes, then sixteenths climbing through every velocity and round robin. */
    std::vector<PlayEvent> groove (const hl::Session&, double bpm = 90, double unused = 100);
    /** The rudiment build: paradiddles, ghost notes, a triplet crescendo, flams and a roll, round robins at random. */
    std::vector<PlayEvent> rudiments (const hl::Session&, double bpm = 100);
    struct MidiNote { double at; int note, vel; };
    std::vector<MidiNote> parseMidi (const juce::MemoryBlock&, juce::String& error);
    std::vector<PlayEvent> midi (const hl::Session&, const std::vector<MidiNote>&);
}

/** The audition player: plays phrases of hits through the audio device, rendered from the engine (A, the recording
    as it is, and B, processed), with the console live (faders, mute, solo, pan, bus, room return), B flipping
    while it plays, A then B, loudness-matched A/B, and a safety limiter on the preview only. Feeds the meters, the
    phase meter and the heart monitor. */
class Audition : private juce::AudioIODeviceCallback
{
public:
    explicit Audition (juce::AudioDeviceManager&);
    ~Audition() override;

    void setEngine (std::shared_ptr<EngineState>);
    struct Options
    {
        bool chain = false, mono = false, loudnessMatch = true;
        std::string mode = "hit";
        std::vector<int> only;   ///< just these tracks (empty: all)
        int side = -1;           ///< -1: A or B as the A/B switch says; 0: always A (as recorded); 1: always B
        bool console = true;     ///< false: every track at unity, centred, ignoring faders, mute and solo (pair checks)
        bool gain = false;       ///< hear what you gain: B minus A, what the processing changed and nothing else
    };
    /** Message thread. Renders what the start needs (the loudest hits for the shared scale, a few for the
        loudness match), then plays; the rest renders ahead of the playhead. */
    void play (std::vector<PlayEvent>, const Options&);
    void stop();
    bool isPlaying() const { return playing.load(); }
    /** A pure tone (the Tune step's recorded and target notes), on top of anything playing. */
    void playTone (double hz, double seconds) { toneHz.store (hz); toneLen.store ((int) (seconds * (deviceRate > 0 ? deviceRate : 48000))); toneLeft.store (toneLen.load()); }
    std::string mode() const { return currentMode; }

    void setProcessed (bool b);   ///< the B side (crossfades while playing)
    bool processed() const { return wantB.load(); }

    /** The console as it stands: per track gain (mute/solo/fader resolved) and pan, the bus and the room return. */
    void setConsole (const std::vector<float>& trackGain, const std::vector<float>& pan, const std::vector<bool>& stereo, float bus, float room,
                     const std::vector<float>& panL = {}, const std::vector<float>& panR = {});

    // what is playing, for the screen (any thread)
    int currentHit() const { return curHit.load(); }
    int currentVel() const { return curVel.load(); }
    float peakL() const { return pkL.load(); }
    float peakR() const { return pkR.load(); }
    float correlation() const { return corr.load(); }

    /** The heart monitor: the selected mic's level, about 600 points a second, with the velocity layer of the hit
        it belongs to. The screen drains it. */
    void setMonitorTrack (int t) { monTrack.store (t); }
    struct MonitorPoint { float level; int vel; };
    int drainMonitor (MonitorPoint* out, int max);

    std::function<void()> onFinished;   ///< message thread, when a phrase ends on its own

    /** Renders one block as the audio device would (tests). */
    void renderBlock (float* L, float* R, int frames, double rate) { deviceRate = rate; float* o[2] { L, R }; audioDeviceIOCallbackWithContext (nullptr, 0, o, 2, frames, {}); }
    double position() const { return t; }
private:
    std::atomic<double> toneHz { 0 };
    std::atomic<int> toneLeft { 0 }, toneLen { 0 };
    double tonePhase = 0;
public:
    double phraseEnd() const { return phrase ? phrase->end : 0; }
    double debugScale() const { return phrase ? phrase->scale * 1000 + phrase->kA : 0; }

private:
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const*, int, int, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

    struct Rendered { hl::HitStems a, b; };
    struct Phrase
    {
        std::vector<PlayEvent> events;
        std::vector<std::unique_ptr<Rendered>> store;
        std::unique_ptr<std::atomic<Rendered*>[]> ready;
        double scale = 1, kA = 1, chainAt = 0, end = 0, sr = 44100;
        bool chain = false, mono = false, console = true, gain = false;
        int nT = 0, ref = 0, side = -1;
        std::atomic<bool> cancel { false };
    };
    std::unique_ptr<Rendered> renderOne (const EngineState&, int hit, bool mono, const std::vector<int>& only) const;
    void finishLater();

    juce::AudioDeviceManager& dm;
    std::shared_ptr<EngineState> engine;
    std::shared_ptr<Phrase> phrase;          // owned by the message thread, read by the audio thread through `live`
    std::atomic<Phrase*> live { nullptr };
    std::thread worker;
    std::string currentMode;

    std::atomic<bool> playing { false }, wantB { true };
    double deviceRate = 44100, t = 0;        // audio thread: session seconds since the start
    float gA = 0, gB = 0, limGain = 1;
    static constexpr int maxTracks = 64;
    std::atomic<float> tGain[maxTracks], tPan[maxTracks], tPanL[maxTracks], tPanR[maxTracks];
    std::atomic<bool> tStereo[maxTracks];
    std::atomic<float> busGain { 1 }, roomGain { 0 };
    std::atomic<int> curHit { -1 }, curVel { 0 }, monTrack { 0 };
    std::atomic<float> pkL { 0 }, pkR { 0 }, corr { NAN };
    juce::AbstractFifo monFifo { 4096 };
    std::vector<MonitorPoint> monBuf = std::vector<MonitorPoint> (4096);
    double monAcc = 0; float monPeak = 0;
    double cxy = 0, cxx = 0, cyy = 0; int cn = 0;
    std::atomic<bool> finishedFlag { false }, inCallback { false }, snapGains { true };
};
} // namespace ld

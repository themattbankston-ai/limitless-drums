#pragma once

#include <array>
#include <vector>

namespace ld::live
{

/** Alignment for one track, as it plays: a sub-sample time shift, polarity and gain.

    Every track carries the same fixed latency (latencySamples()), and a shift moves this track within it, so a
    positive advance plays it earlier than recorded and the host's latency compensation lines everything back up
    with the timeline. The fractional part uses a 128-tap Kaiser-windowed sinc, so whole-sample shifts are exact and
    fractional ones are flat across the audible band.

    Phase EQ: up to two all-pass filters (frequency and Q, the same filters the Align analysis chooses) after the
    shift. They change phase only, never level.

    Any change (shift, polarity, gain or phase EQ) crossfades from the old setting to the new one over 10 ms with equal-gain
    fades (the two settings are the same audio a moment apart, so their gains must sum to 1). Changes take effect at
    the start of the next process() call, and the output depends only on the input and when changes arrive, never on
    how the audio is split into blocks: playback and an offline render of the same settings give identical samples. */
class AlignDsp
{
public:
    static constexpr int taps = 128;

    struct AllPass
    {
        double fc = 0, Q = 0.707;    ///< fc 0 = off
        bool operator== (const AllPass& o) const { return fc == o.fc && (fc == 0 || Q == o.Q); }
    };

    static constexpr double kFadeSeconds = 0.010;   ///< every change crossfades over this long

    struct Settings
    {
        double advanceSamples = 0;   ///< + plays the track earlier, - later
        bool flip = false;
        double gainDb = 0;
        std::array<AllPass, 2> ap {};
        bool operator== (const Settings& o) const { return advanceSamples == o.advanceSamples && flip == o.flip && gainDb == o.gainDb && ap[0] == o.ap[0] && ap[1] == o.ap[1]; }
        bool operator!= (const Settings& o) const { return ! (*this == o); }
    };

    /** The fixed latency every track carries at this rate (also the largest advance allowed, plus half the taps). */
    static int latencySamples (double sampleRate);
    /** How far a track can be moved either way at this rate. */
    static double maxShiftSamples (double sampleRate);

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setTarget (const Settings& s) { target = s; }
    const Settings& current() const { return cur.s; }
    void process (float* const* channels, int numChannels, int numSamples);

private:
    struct Kernel
    {
        Settings s;
        long whole = 0;                 ///< whole samples of delay
        std::array<float, taps> h {};   ///< interpolation taps (gain and polarity folded in)
        bool impulse = false;           ///< a whole-sample delay: one tap
        float g = 1;                    ///< gain and polarity for the impulse case
        struct Biquad { double b0, b1, b2, a1, a2; };
        std::array<Biquad, 2> ap {};    ///< the phase EQ, in order (fixed size: nothing allocates on the audio thread)
        int nAp = 0;
    };
    using State = std::array<std::array<std::array<double, 4>, 2>, 2>;   ///< [channel][all-pass] x1 x2 y1 y2
    float pathOut (const std::vector<float>& ring, long at, const Kernel&, std::array<std::array<double, 4>, 2>& st) const;
    void prime (Kernel&, State&) const;
    Kernel make (const Settings&) const;
    float tapOut (const std::vector<float>& ring, long at, const Kernel&) const;
    void startFadeIfNeeded();

    double sr = 48000;
    int L = 0;
    std::vector<std::vector<float>> ring;
    long mask = 0, pos = 0;
    Settings target;
    Kernel cur, next;
    State curState {}, nextState {};
    int primeLen = 4096;
    bool fading = false;
    int fadePos = 0, fadeLen = 480;
    std::vector<float> fadeCurve;
};

} // namespace ld::live

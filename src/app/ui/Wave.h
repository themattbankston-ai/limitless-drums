#pragma once
#include <memory>
#include <utility>
#include <vector>

namespace ld::ui
{
/** The audio behind a lane, for drawing the waveform at any zoom: the recording (A), and the processed audio
    as it gets exported (B: sample n is b[n + lagB] times the Level gain of its hit), each with a min/max
    pyramid per channel. */
struct Wave
{
    using Chans = std::vector<std::vector<float>>;
    std::shared_ptr<const Chans> a, b;
    long lagB = 0;
    std::vector<long> regionStart;    ///< B: where each hit's gain starts (samples)
    std::vector<float> gainB;         ///< B: per hit, linear
    double sampleRate = 44100;
    static constexpr int base = 16;   ///< samples per block at level 0; each level doubles
    std::vector<std::vector<std::vector<float>>> pyrA, pyrB;   ///< [level][channel] = min, max, min, max...
    int channels() const { return a ? (int) a->size() : 0; }
    long length() const { return a && ! a->empty() ? (long) (*a)[0].size() : 0; }
    float sampleB (int ch, long n) const;
    /** min and max of channel ch over samples [s0, s1), morphing A to B by mix. */
    std::pair<float, float> range (int ch, long s0, long s1, float mix) const;
    float peak (float mix) const;     ///< the loudest sample, either side
    void build();                     ///< fills the pyramids
};

} // namespace ld::ui

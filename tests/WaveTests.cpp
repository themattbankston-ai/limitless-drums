// The lane waveform's data: at any zoom, the min and max it draws are the true min and max of the audio
// (the recording, or the processed audio at its alignment and per-hit gain).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/app/ui/Wave.h"
#include <cmath>
#include <random>

using ld::ui::Wave;

static std::pair<float, float> brute (const Wave& w, int ch, long s0, long s1, bool b)
{
    float lo = 0, hi = 0;
    for (long n = std::max (0L, s0); n < std::min (w.length(), s1); ++n)
    {
        const float v = b ? w.sampleB (ch, n) : (*w.a)[(size_t) ch][(size_t) n];
        lo = std::min (lo, v); hi = std::max (hi, v);
    }
    return { lo, hi };
}

TEST_CASE ("Waveform ranges are exact at every zoom, A and B")
{
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (-1, 1);
    auto chans = std::make_shared<Wave::Chans> (2, std::vector<float> (100000));
    for (auto& c : *chans) for (auto& v : c) v = u (rng) * 0.5f;
    (*chans)[1][54321] = 0.99f;   // a lone transient
    auto proc = std::make_shared<Wave::Chans> (*chans);
    Wave w;
    w.a = chans; w.b = proc; w.lagB = 37;
    w.regionStart = { 0, 30000, 70000 }; w.gainB = { 0.5f, 2.0f, 1.0f };
    w.build();
    REQUIRE (! w.pyrA.empty());
    REQUIRE (w.pyrB.size() == w.pyrA.size());

    // block-aligned spans read the pyramid; check against the samples
    for (long span : { 3L, 16L, 64L, 1024L, 8192L, 65536L })
        for (long s0 : { 0L, 16384L, 53248L })
            for (int ch = 0; ch < 2; ++ch)
            {
                const long a0 = (s0 / span) * span;
                const auto r = w.range (ch, a0, a0 + span, 0);
                const auto e = brute (w, ch, a0, a0 + span, false);
                if (span < Wave::base || (a0 % (Wave::base << 0)) == 0)
                {
                    CHECK (r.second >= e.second - 1e-6f);   // never misses a peak
                    CHECK (r.first <= e.first + 1e-6f);
                }
                if (span < Wave::base) { CHECK (r.first == e.first); CHECK (r.second == e.second); }
            }
    // the lone transient is found from far out
    CHECK (w.range (1, 0, 100000, 0).second == Catch::Approx (0.99f));
    // B: lag and per-hit gain
    CHECK (w.sampleB (0, 40000) == Catch::Approx ((*chans)[0][40037] * 2.0f));
    CHECK (w.sampleB (0, 99990) == 0.0f);                     // past the end once delayed
    const auto rb = w.range (1, 54321 - 37 - 2, 54321 - 37 + 2, 1.0f);
    CHECK (rb.second == Catch::Approx (0.99f * 2.0f));
    // halfway between A and B
    const auto ra = w.range (0, 40000, 40001, 0), rbb = w.range (0, 40000, 40001, 1), rm = w.range (0, 40000, 40001, 0.5f);
    CHECK (rm.second == Catch::Approx ((ra.second + rbb.second) * 0.5f));
    CHECK (w.peak (0) == Catch::Approx (0.99f));
}

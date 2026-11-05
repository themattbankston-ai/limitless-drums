#include "Fft.h"
#include <cmath>
#include <map>
#include <mutex>

namespace ld::hl
{
namespace
{
    struct Plan { std::vector<int> rev; F64 tw; };
    const Plan& planFor (size_t N)
    {
        static std::map<size_t, Plan> plans;
        static std::mutex m;
        std::lock_guard<std::mutex> lock (m);
        auto it = plans.find (N);
        if (it != plans.end()) return it->second;
        Plan p;
        // twiddles per stage: cos then sin of 2*pi*j/s, j < s/2 (as Hitline's WASM plan)
        for (size_t s = 2; s <= N; s <<= 1)
        {
            const size_t h = s / 2;
            for (size_t j = 0; j < h; ++j) p.tw.push_back (std::cos (2 * M_PI * (double) j / (double) s));
            for (size_t j = 0; j < h; ++j) p.tw.push_back (std::sin (2 * M_PI * (double) j / (double) s));
        }
        int lev = (int) std::lround (std::log2 ((double) N));
        p.rev.resize (N);
        for (size_t i = 0; i < N; ++i) { int r = 0, x = (int) i; for (int j = 0; j < lev; ++j) { r = (r << 1) | (x & 1); x >>= 1; } p.rev[i] = r; }
        return plans.emplace (N, std::move (p)).first->second;
    }
}

void fft (F64& re, F64& im, bool inverse)
{
    const size_t N = re.size();
    const auto& p = planFor (N);
    for (size_t i = 0; i < N; ++i) { const size_t j = (size_t) p.rev[i]; if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); } }
    size_t off = 0;
    for (size_t s = 2; s <= N; s <<= 1)
    {
        const size_t h = s / 2;
        const double* c = p.tw.data() + off;
        const double* sn = c + h;
        for (size_t i = 0; i < N; i += s)
            for (size_t j = 0; j < h; ++j)
            {
                const double cc = c[j], ss = inverse ? sn[j] : -sn[j];
                const size_t a = i + j, b = a + h;
                const double tr = re[b] * cc - im[b] * ss, ti = re[b] * ss + im[b] * cc;
                re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
            }
        off += 2 * h;
    }
    if (inverse) for (size_t i = 0; i < N; ++i) { re[i] /= (double) N; im[i] /= (double) N; }
}
} // namespace ld::hl

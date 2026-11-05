#pragma once
// Helpers that reproduce JavaScript number behaviour, so ported Hitline code gives the same results.
#include <cmath>
#include <vector>

namespace ld::hl
{
/** JavaScript Math.round: halves round up (toward +infinity), unlike std::round. */
inline double jsRound (double x) { return std::floor (x + 0.5); }
inline int jsRoundI (double x) { return (int) std::floor (x + 0.5); }

using F32 = std::vector<float>;   // Hitline's Float32Array
using F64 = std::vector<double>;  // Hitline's Float64Array or plain arrays of numbers
} // namespace ld::hl

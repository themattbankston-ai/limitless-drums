#pragma once
#include "Js.h"

namespace ld::hl
{
/** In-place radix-2 FFT on doubles (N a power of two), the same algorithm Hitline uses. The inverse
    divides by N. */
void fft (F64& re, F64& im, bool inverse);
} // namespace ld::hl

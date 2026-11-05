#include "Wav.h"
#include <cstring>

namespace ld::hl
{
static uint16_t u16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }
static uint32_t u32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }

WavData parseWav (const juce::MemoryBlock& mb)
{
    WavData w;
    const auto* u8 = static_cast<const uint8_t*> (mb.getData());
    const size_t n = mb.getSize();
    auto tag = [&] (size_t o) { return std::string ((const char*) u8 + o, 4); };
    if (n < 12) { w.error = "File is too short to be a WAV."; return w; }
    const auto riff = tag (0);
    if ((riff != "RIFF" && riff != "RF64") || tag (8) != "WAVE") { w.error = "Not a WAV file (no RIFF/WAVE header)."; return w; }

    size_t p = 12;
    bool haveFmt = false;
    int code = 0, nch = 0, ba = 0, bps = 0;
    uint32_t sr = 0;
    long long dataOff = -1;
    double dataLen = 0, ds64 = -1;
    while (p + 8 <= n)
    {
        const auto id = tag (p);
        const uint32_t size = u32 (u8 + p + 4);
        if (id == "ds64" && size >= 16 && p + 8 + 16 <= n) ds64 = (double) u32 (u8 + p + 16) + (double) u32 (u8 + p + 20) * 4294967296.0;
        if (id == "fmt ")
        {
            code = u16 (u8 + p + 8); nch = u16 (u8 + p + 10); sr = u32 (u8 + p + 12); ba = u16 (u8 + p + 20); bps = u16 (u8 + p + 22);
            if (code == 0xFFFE && size >= 40) code = u16 (u8 + p + 8 + 24);
            haveFmt = true;
        }
        else if (id == "data")
        {
            dataOff = (long long) p + 8;
            dataLen = (size == 0xFFFFFFFFu && ds64 >= 0) ? ds64 : (double) size;
            if ((double) dataOff + dataLen > (double) n) dataLen = (double) n - (double) dataOff;
            break;
        }
        p += 8 + (size_t) size + (size & 1);
    }
    if (! haveFmt) { w.error = "WAV has no format chunk."; return w; }
    if (dataOff < 0) { w.error = "WAV has no audio data chunk."; return w; }

    const int bytes = bps / 8;
    if (ba == 0) ba = nch * bytes;
    const size_t frames = (size_t) std::floor (dataLen / ba);
    w.channels.assign ((size_t) nch, F32 (frames, 0.0f));
    const uint8_t* d = u8 + dataOff;
    if (code == 3 && bps == 32)
    {
        for (size_t i = 0; i < frames; ++i)
            for (int c = 0; c < nch; ++c) { float v; std::memcpy (&v, d + i * (size_t) ba + (size_t) c * 4, 4); w.channels[(size_t) c][i] = v; }
    }
    else if (code == 3 && bps == 64)
    {
        for (size_t i = 0; i < frames; ++i)
            for (int c = 0; c < nch; ++c) { double v; std::memcpy (&v, d + i * (size_t) ba + (size_t) c * 8, 8); w.channels[(size_t) c][i] = (float) v; }
    }
    else if (code == 1)
    {
        for (size_t i = 0; i < frames; ++i)
            for (int c = 0; c < nch; ++c)
            {
                const uint8_t* q = d + i * (size_t) ba + (size_t) c * (size_t) bytes;
                double v;
                if (bps == 16) v = (int16_t) u16 (q) / 32768.0;
                else if (bps == 24) { int x = q[0] | (q[1] << 8) | (q[2] << 16); if (x & 0x800000) x -= 0x1000000; v = x / 8388608.0; }
                else if (bps == 32) v = (int32_t) u32 (q) / 2147483648.0;
                else if (bps == 8) v = (q[0] - 128) / 128.0;
                else { w.error = "Unsupported PCM bit depth: " + std::to_string (bps) + "-bit."; return w; }
                w.channels[(size_t) c][i] = (float) v;
            }
    }
    else { w.error = "Unsupported WAV encoding (format code " + std::to_string (code) + "). Use PCM or 32-bit float."; return w; }

    w.sampleRate = sr;
    w.bits = bps;
    w.isFloat = code == 3;
    w.label = std::to_string (bps) + (code == 3 ? "-bit float" : "-bit");
    return w;
}

WavData readWav (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { WavData w; w.error = "Couldn't read " + f.getFileName().toStdString(); return w; }
    return parseWav (mb);
}
} // namespace ld::hl

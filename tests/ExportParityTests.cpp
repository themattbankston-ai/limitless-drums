// End-to-end parity with Hitline 4.8: a whole export with the Mix step switched on (clean-up, tone, shape,
// tame, channel and bus inserts, the shared room, tuning) through Hitline headless (tools/parity/golden_export.js,
// keys tests/parity/export_*.json) and through the C++ engine must write the same files.
// Needs LD_PARITY_SESSIONS like the other parity tests.
#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include "hl/Session.h"
#include "hl/Wav.h"

namespace
{
juce::File root() { return juce::File (LD_SOURCE_DIR); }

uint32_t crc32 (const uint8_t* p, size_t n)
{
    static uint32_t T[256];
    static bool init = false;
    if (! init) { for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; T[i] = c; } init = true; }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = T[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

struct Decoded { int nch = 0, bits = 0; double sr = 0; std::vector<ld::hl::F32> ch; };
Decoded decode (const juce::MemoryBlock& mb)
{
    Decoded d;
    auto w = ld::hl::parseWav (mb);
    d.nch = (int) w.channels.size(); d.sr = w.sampleRate; d.bits = w.bits; d.ch = std::move (w.channels);
    return d;
}

void runCase (const juce::String& keyName, const juce::String& folder)
{
    const auto sessions = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    const auto dir = juce::File (sessions).getChildFile (folder);
    const auto keyFile = root().getChildFile ("tests/parity").getChildFile (keyName);
    if (! dir.isDirectory() || ! keyFile.existsAsFile()) { WARN ("missing " << keyName << " or " << folder); return; }
    const auto key = juce::JSON::parse (keyFile);
    const auto st = key["state"];
    const int STEP = key["STEP"];

    std::vector<ld::hl::Chs> audio;
    for (auto& t : *st["tracks"].getArray())
    {
        auto w = ld::hl::readWav (dir.getChildFile (t["name"].toString() + ".wav"));
        REQUIRE (w.error.empty());
        audio.push_back (std::move (w.channels));
    }
    auto S = ld::hl::sessionFromHitline (st, std::move (audio));
    S.tameWorkers = 3;   // golden_export.js fixes navigator.hardwareConcurrency at 4

    const bool kit = S.mode == "kit";
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const auto M = ld::hl::renderMix (S);
    const auto L = kit ? ld::hl::LevelResult() : ld::hl::measureLevels (S, M);
    REQUIRE ((kit || L.valid));
    const auto t1 = juce::Time::getMillisecondCounterHiRes();

    // levels measured after the mix
    double worstGain = 0;
    if (! kit)
    {
        for (size_t t = 0; t < S.tracks.size(); ++t)
            for (size_t i = 0; i < S.onsets.size(); ++i)
                worstGain = std::max (worstGain, std::abs (L.gainsDbT[t][i] - (double) st["res"]["gainsDbT"][(int) t][(int) i]));
        CHECK (std::abs (L.trimDb - (double) st["res"]["trimDb"]) < 1e-6);
    }

    std::map<std::string, const juce::var*> gold;
    for (auto& f : *key["files"].getArray()) gold[f["path"].toString().toStdString()] = &f;

    int n = 0, identical = 0, missing = 0;
    double worstRel = 0, worstAbs = 0; std::string worstAt;
    std::vector<std::string> order;
    auto onFile = [&] (const std::string& path, const juce::MemoryBlock& bytes)
    {
        order.push_back (path);
        ++n;
        auto it = gold.find (path);
        if (it == gold.end()) { ++missing; UNSCOPED_INFO ("not in Hitline's export: " << path); return; }
        const auto& g = *it->second;
        CHECK ((int64_t) bytes.getSize() == (int64_t) (double) g["bytes"]);
        if (crc32 (static_cast<const uint8_t*> (bytes.getData()), bytes.getSize()) == (uint32_t) (juce::int64) g["crc"]) { ++identical; return; }
        const auto d = decode (bytes);
        CHECK (d.nch == (int) g["nch"]);
        CHECK (d.sr == (double) g["sr"]);
        const double fs = d.bits == 24 ? 8388608.0 : 1.0;   // the key keeps 24-bit samples as integers
        for (int c = 0; c < d.nch && c < (int) g["nch"]; ++c)
        {
            const auto& x = d.ch[(size_t) c];
            REQUIRE ((int64_t) x.size() == (int64_t) (double) g["n"]);
            double e = 0; for (float v : x) e += (double) v * v * fs * fs;
            const double ge = g["energy"][c];
            const double rel = std::abs (e - ge) / std::max (1e-30, ge);
            const auto* s = g["s"][c].getArray();
            double pk = 0;
            for (int k = 0; k < s->size(); ++k)
            {
                const double a = (double) x[(size_t) k * (size_t) STEP] * fs, b = (*s)[k];
                pk = std::max (pk, std::abs (a - b) / fs);
            }
            if (rel > worstRel) { worstRel = rel; worstAt = path; }
            worstAbs = std::max (worstAbs, pk);
        }
    };
    if (kit) ld::hl::writeKit (S, M, onFile); else ld::hl::writeSession (S, M, L, onFile);
    const auto t2 = juce::Time::getMillisecondCounterHiRes();

    int goldAudio = 0; for (auto& [p, _] : gold) if (juce::String (p).endsWithIgnoreCase (".wav")) ++goldAudio;
    INFO (keyName << ": " << n << " files, " << identical << " bit-identical; worst energy difference " << worstRel
                  << " (" << worstAt << "), worst sample difference " << worstAbs << "; worst level gain difference " << worstGain
                  << " dB; mix " << juce::roundToInt (t1 - t0) << " ms, export " << juce::roundToInt (t2 - t1) << " ms");
    WARN (keyName << ": " << n << " files, " << identical << " bit-identical; worst energy difference " << worstRel
                  << " (" << worstAt << "), worst sample difference " << worstAbs << "; worst level gain difference " << worstGain
                  << " dB; mix " << juce::roundToInt (t1 - t0) << " ms, export " << juce::roundToInt (t2 - t1) << " ms");
    CHECK (missing == 0);
    CHECK (n == goldAudio);
    // same files in the same order as Hitline wrote them
    int k = 0;
    for (auto& f : *key["files"].getArray())
    {
        const auto p = f["path"].toString().toStdString();
        if (! juce::String (p).endsWithIgnoreCase (".wav")) continue;
        if (k < (int) order.size()) CHECK (order[(size_t) k] == p);
        ++k;
    }
    CHECK (worstGain < 1e-6);
    CHECK (worstRel < 1e-5);
    CHECK (worstAbs < 1e-5);

    // the one-shot, with the console set as Hitline had it
    const auto os = key["oneShot"];
    if (os.isObject())
    {
        const auto con = os["console"];
        for (int t = 0; t < (int) S.tracks.size(); ++t)
        {
            const auto c = con["tracks"][t];
            auto& tr = S.tracks[(size_t) t];
            tr.fader = c["fader"]; tr.mute = c["mute"]; tr.solo = c["solo"]; tr.strip.pan = c["pan"];
        }
        S.busFader = con["busFader"]; S.busMute = con["busMute"];
        CHECK ((int) ld::hl::defaultOneShot (S, L) == (int) os["i"]);
        const auto h = ld::hl::renderHitStems (S, M, L, (size_t) (int) os["i"]);
        const auto [Lc, Rc] = ld::hl::mixToStereo (S, h);
        for (auto [y, g] : { std::pair<const ld::hl::F32*, juce::var> { &Lc, os["L"] }, { &Rc, os["R"] } })
        {
            REQUIRE ((int) y->size() == (int) g["n"]);
            double e = 0; for (float v : *y) e += (double) v * v;
            CHECK (std::abs (e - (double) g["energy"]) / (double) g["energy"] < 1e-6);
            const auto* s = g["s"].getArray();
            double pk = 0;
            for (int k = 0; k < s->size(); ++k) pk = std::max (pk, std::abs ((double) (*y)[(size_t) k * (size_t) STEP] - (double) (*s)[k]));
            CHECK (pk < 1e-5);
        }
        const auto shot = ld::hl::exportOneShot (S, M, L);
        CHECK (! shot.name.empty());
    }
}
} // namespace

TEST_CASE ("Parity: a full export with the mix on matches Hitline 4.8")
{
    if (juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {}).isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    ld::hl::setLibraryReference (juce::JSON::parse (root().getChildFile ("assets/libref.json")));
    ld::hl::setReverbLibrary (juce::JSON::parse (root().getChildFile ("assets/irlib.json")));
    SECTION ("everything on, hits and full tracks") { runCase ("export_A.json", "SNARE 2 - BB/Center"); }
    SECTION ("Panda reference, kit levels, gated tails, 48 kHz 24-bit") { runCase ("export_B.json", "SNARE 1 - MH/Center"); }
    SECTION ("whole-kit take: bleed gate, clean-up on strong hits, 48 kHz 24-bit") { runCase ("export_K.json", "SNARE 1 - MH/Center"); }
}

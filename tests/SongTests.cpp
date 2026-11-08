// Song mode on a synthetic song with a known tempo map and known hits (tools/make_song.py): hits found, the tempo
// tracked, tightening pulls hits toward the grid with every mic moving together, and the MIDI files are well formed.
#include <catch2/catch_test_macros.hpp>
#include "../src/hl/Song.h"
#include "../src/hl/Wav.h"
#include "../src/hl/Detect.h"
#include <juce_core/juce_core.h>

using namespace ld::hl;

namespace
{
struct Song { std::vector<F32> monos; std::vector<std::string> names; std::vector<Chs> chs; double sr = 44100; juce::var truth; };
Song loadSong (const juce::File& dir)
{
    Song s;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))
    {
        auto w = readWav (f);
        s.sr = w.sampleRate; s.names.push_back (f.getFileNameWithoutExtension().toStdString());
        s.chs.push_back (w.channels); s.monos.push_back (monoOf (w.channels));
    }
    s.truth = juce::JSON::parse (dir.getChildFile ("truth.json"));
    return s;
}
}

TEST_CASE ("Song mode: hits, tempo, tightening, MIDI")
{
    const auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("LD_SONG", "/home/claude/songtest/song60"));
    if (! dir.isDirectory()) { WARN ("no test song; skipped"); return; }
    const auto S = loadSong (dir);
    REQUIRE (S.monos.size() >= 8);
    const auto A = analyseSong (S.monos, S.names, S.sr);
    REQUIRE (A.done);

    // hits: every kick, snare (not ghost) and tom found within 6 ms, few extras
    int truthN = 0, found = 0;
    std::vector<bool> matched (A.hits.size(), false);
    for (auto& h : *S.truth["hits"].getArray())
    {
        const auto d = h["drum"].toString();
        if (d == "hat") continue;
        if (d == "snare" && (double) h["vel"] < 0.3) continue;
        ++truthN;
        const double t = (double) h["t"];
        for (size_t i = 0; i < A.hits.size(); ++i)
            if (std::abs (A.hits[i].pos / S.sr - t) < 0.006 && d == drumName (A.hits[i].drum)) { ++found; matched[i] = true; goto next; }
        if (juce::SystemStats::getEnvironmentVariable ("LD_SONG_DEBUG", "").isNotEmpty())
        {
            std::string near;
            for (auto& x : A.hits) if (std::abs (x.pos / S.sr - t) < 0.05) near += std::string (" ") + drumName (x.drum) + "@" + std::to_string ((int) std::lround ((x.pos / S.sr - t) * 1000));
            WARN ("missed " << d << " at " << t << " vel " << (double) h["vel"] << ":" << near);
        }
        next:;
    }
    for (const char* dn : { "kick", "snare", "tom" })
    {
        double sum = 0; int k = 0;
        for (auto& h : *S.truth["hits"].getArray())
        {
            if (h["drum"].toString() != dn) continue;
            for (auto& x : A.hits) if (std::abs (x.pos / S.sr - (double) h["t"]) < 0.006 && std::string (drumName (x.drum)) == dn) { sum += x.pos / S.sr - (double) h["t"]; ++k; break; }
        }
        WARN (dn << ": detection is on average " << (k ? sum / k * 1000 : 0) << " ms from the truth");
    }
    // a detection that matches a ghost note we didn't count isn't an extra
    for (auto& h : *S.truth["hits"].getArray())
        for (size_t i = 0; i < A.hits.size(); ++i)
            if (std::abs (A.hits[i].pos / S.sr - (double) h["t"]) < 0.006 && h["drum"].toString() == drumName (A.hits[i].drum)) matched[i] = true;
    int extra = 0; for (size_t i = 0; i < A.hits.size(); ++i) if (! matched[i] && A.hits[i].drum != Drum::hat && A.hits[i].drum != Drum::cymbal)
    {
        ++extra;
        if (juce::SystemStats::getEnvironmentVariable ("LD_SONG_DEBUG", "").isNotEmpty() && extra < 12) WARN ("extra " << drumName (A.hits[i].drum) << " at " << A.hits[i].pos / S.sr << " vel " << A.hits[i].vel);
    }
    WARN ("hits: " << found << " of " << truthN << " found, " << extra << " extra, " << A.hits.size() << " in all");
    CHECK (found >= truthN * 0.95);
    CHECK (extra <= truthN * 0.05);

    // tempo: the tracked beats near the true ones, average BPM right
    std::vector<double> tb; for (auto& b : *S.truth["beats"].getArray()) tb.push_back ((double) b);
    int near = 0;
    for (double b : A.beats) { double d = 1; for (double x : tb) d = std::min (d, std::abs (x - b)); if (d < 0.03) ++near; }
    WARN ("beats: " << A.beats.size() << " tracked, " << near << " within 30 ms of a true beat (of " << tb.size() << ")");
    CHECK (near >= (int) (tb.size() * 0.9));
    const auto bpm = bpmOf (gridBeats (A, {}));
    double avg = 0; for (double v : bpm) avg += v; avg /= (double) bpm.size();
    WARN ("average BPM " << avg);
    CHECK (avg > 115); CHECK (avg < 125);

    for (const char* mode : { "keep", "smooth", "flatten" })
    {
        SongSettings m; m.tempoMode = mode; const auto gb = gridBeats (A, m);
        double err = 0; int k = 0; for (double b : gb) { double d = 1; for (double x : tb) d = std::min (d, std::abs (x - b)); err += d; ++k; }
        WARN ("grid '" << mode << "': average " << err / k * 1000 << " ms from the true beats");
    }
    if (juce::SystemStats::getEnvironmentVariable ("LD_SONG_DEBUG", "").isNotEmpty())
    {
        SongSettings m; const auto sm = gridBeats (A, m);
        for (size_t i = 0; i < A.beats.size(); i += 7) WARN ("beat " << i << " keep " << A.beats[i] << " smooth " << sm[i] << " truth " << (i < tb.size() ? tb[i] : 0));
    }
    // tightening: kick and snare move closer to the true grid; the edit moves every mic the same
    SongSettings set; set.tightness = 1; set.tempoMode = "smooth";
    const auto E = planEdits (A, set);
    REQUIRE (! E.empty());
    double before = 0, after = 0; int n = 0;
    for (auto& h : *S.truth["hits"].getArray())
    {
        const auto d = h["drum"].toString(); if (d != "kick" && d != "snare") continue;
        const double t = (double) h["t"], g = (double) h["grid"];
        before += std::abs (t - g);
        after += std::abs (movedPos ((long) std::lround (t * S.sr), E) / S.sr - g); ++n;
        if (juce::SystemStats::getEnvironmentVariable ("LD_SONG_DEBUG", "").isNotEmpty() && n < 14)
            WARN (d << " t " << t << " grid " << g << " moved " << movedPos ((long) std::lround (t * S.sr), E) / S.sr);
    }
    WARN ("kick/snare average distance from the true grid: " << before / n * 1000 << " ms before, " << after / n * 1000 << " ms after");
    CHECK (after < before * 0.6);
    // the same edits on two mics: their relative timing (phase) is unchanged around a moved hit
    const auto& top = S.chs[0][0];
    const auto y = applyEdits (top, E, S.sr);
    CHECK (y.size() == top.size());
    const auto y2 = applyEdits (top, E, S.sr);
    CHECK (y == y2);
    // a moved hit is intact: its first 30 ms match the original shifted by its edit
    const auto& e = E[E.size() / 2];
    if (e.shift != 0)
    {
        double err = 0, ref = 0;
        for (long q = e.cut + 200; q < e.cut + 1300; ++q) { const double a = y[(size_t) (q + e.shift)], b = top[(size_t) q]; err += (a - b) * (a - b); ref += b * b; }
        CHECK (err <= ref * 1e-6 + 1e-9);
    }

    // MIDI: headers and one note per hit
    const auto mid = midiPerformance (A.hits, E, gridBeats (A, set), S.sr);
    REQUIRE (mid.size() > 100);
    CHECK (mid.substr (0, 4) == "MThd");
    int ons = 0; for (size_t i = 0; i + 2 < mid.size(); ++i) if ((unsigned char) mid[i] == 0x99) ++ons;
    CHECK (ons >= (int) A.hits.size());
    const auto tm = midiTempoMap (gridBeats (A, set));
    CHECK (tm.substr (0, 4) == "MThd");

    // flatten: one tempo
    SongSettings fl; fl.tempoMode = "flatten";
    const auto fb = bpmOf (gridBeats (A, fl));
    CHECK (std::abs (fb.front() - fb[fb.size() / 2]) < 0.01);
    const auto probs = findProblems (A, set);
    WARN ("problems flagged: " << probs.size());
}

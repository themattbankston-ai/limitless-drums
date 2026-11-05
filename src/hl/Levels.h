#pragma once
#include "Js.h"
#include <optional>
#include <string>

namespace ld::hl
{
/** Port of Hitline 4.8's levels (dsp.js computeTargets and helpers; app computeAll, houseTargets,
    the house standard per drum and mic family). Levels are measured on what gets exported (after the
    mix) and every hit gets the gain that lands it on its velocity's target. */
struct Det { int V = 6, RR = 6; std::string order = "soft"; double prerollMs = 2; };
struct LevelSettings
{
    std::string mode = "permic";      ///< "permic" (each mic on its own standard) or "linked" (one gain for all mics)
    std::string source = "ref";       ///< linked mode: "ref", "sum" or a track index
    double winMs = 60;
    std::string curve = "midi";       ///< midi, even, step, rr, range, preset
    double shape = 0.9;
    std::string midiRange = "natural";
    double stepDb = 2, rangeDb = 24;
    std::string anchor = "loud";
    double offsetDb = 0;
    std::string std = "house";        ///< "house" (the library standard) or "kit"/"saved"
    std::string steps = "even";       ///< house steps: "even" or "pro" (Standard)
};
struct ExportLevelSettings { double maxLenS = 12; std::string trim = "off"; double ceilDb = -1; };

struct LevelTrack
{
    std::string mic, name;
    std::vector<F32> ch;              ///< the signal levels are measured on (after the mix)
    F32 mono;                         ///< for display peaks
    long lagInt = 0;                  ///< applied delay, rounded
    int pol = 1;
    F64 clipDb;                       ///< per-hit clip gain (empty = 0)
    double arrivalMs = NAN;           ///< for naming unknown mics (Hitline's soundKey)
    double autoAsis = NAN;
};

struct Targets { std::vector<int> velOf, rrOf; F64 means, T, gains; };
Targets computeTargets (const F64& L, int V, int RR, const std::string& order, const LevelSettings&, const F64* presetT = nullptr);

struct Region { long start, end; };
std::vector<Region> sliceRegions (const std::vector<long>& onsets, long pre, long maxLenS, long total);

struct LevelResult
{
    bool valid = false;
    std::vector<F64> lvl, lvlRaw, gainsDbT, badge;
    std::vector<int> velOf;
    std::vector<Targets> groupsT;
    std::optional<Targets> linked;
    F64 srcL, peakAfter, dispPeak, targetsOut, spreadBefore, spreadAfter;
    std::vector<Region> regions;
    double peakMax = -INFINITY, trimDb = 0, maxBoost = -INFINITY;
    bool house = false, locked = false;
};

/** Hitline's house standard for one mic family of one drum: {top dB, range dB}. */
std::pair<double, double> houseStd (const std::string& drum, const std::string& key);
/** Hitline's houseKey: lead, bottom, sub, oh, room, or what the mic sounds like. */
std::string houseKey (const LevelTrack&, bool isRef);
F64 houseTargets (const std::string& drum, const std::string& key, int V, const std::string& steps);

LevelResult computeLevels (const std::vector<LevelTrack>& tracks, int refIdx, const std::vector<long>& onsets, double sr,
                           const Det&, const LevelSettings&, const ExportLevelSettings&, const std::string& drum,
                           const F64& tuneRatios, const std::vector<int>& rejects = {});
} // namespace ld::hl

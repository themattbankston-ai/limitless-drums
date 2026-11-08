#pragma once
#include <functional>
#include "FxCore.h"

namespace ld::hl
{
/** Ports of Hitline 4.8's saturation (Decapitator, Devil-Loc, Saturn, FATSO models), reverb
    (measured EMT 140, Lexicon 224, Sound City programs and VintageVerb-style programs) and the insert
    chain that runs every insert in slot order. */
Chs satRender (const Chs& chs, double sr, const juce::var& P);

/** The reverb library: Hitline's measured programs (assets/irlib.json). Call once at startup. */
void setReverbLibrary (const juce::var& irlib);
std::pair<F32, F32> verbIR (const juce::var& p, double sr);
Chs verbWet (const Chs& chs, double sr, const juce::var& p);
Chs verbRender (const Chs& chs, double sr, const juce::var& p);
Chs roomRender (const F32& send, double sr, const juce::var& p);

struct InsertOptions
{
    double trim = 0;
    std::map<int, Chs> keys;          ///< gate side-chains by track index
    std::vector<long> ons;            ///< hits, for the comp's gain-reduction meter
    /** When set, each compressor adds its gain reduction (dB, >= 0) at GR_RATE per second, keyed by its slot. */
    std::map<int, F32>* grCurves = nullptr;
};
/** Hitline's insRender: trim, then every insert that is switched on, in slot order. */
/** Third-party plugins (insert type "ext"): the app supplies how to run one offline (the engine doesn't host plugins
    itself). Called from render threads, one call per insert per render; returns the input unchanged when it can't. */
using ExternalRenderer = std::function<Chs (const juce::var& insert, const Chs& in, double sr)>;
void setExternalRenderer (ExternalRenderer);
Chs insRender (const Chs& chs, double sr, const juce::var& inserts, const InsertOptions& = {}, std::vector<F64>* compGr = nullptr);
} // namespace ld::hl

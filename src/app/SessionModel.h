#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "core/Naming.h"
#include "Engine.h"
#include "ui/Studio.h"

namespace ld
{
/** Everything the interface shows, from the edit (names, mute, solo, faders) and, when it is there, the
    engine's state (hits, velocity layers, round robins, per-mic levels per layer, pitch, alignment). */
ui::StudioModel buildStudioModel (tracktion::Edit&, const std::vector<MicRole>& roles, const juce::String& title, std::shared_ptr<const EngineState>);

/** The Align step's view: every mic's alignment, and the scope on one mic around one hit. */
ui::StudioModel::AlignView buildAlignView (const EngineState&, int mic, int hit, bool spread, bool samples);

/** The Tune step's view. */
ui::StudioModel::TuneView buildTuneView (const EngineState&, int hit);

/** The Mix step's view of the selected channel (-1: the bus) around the selected hit. */
ui::StudioModel::MixView buildMixView (const EngineState&, int sel, int hit, const hl::Chs* refAudio, const std::string& refName);

/** The audio files behind the edit's tracks, in track order. */
std::vector<juce::File> editFiles (tracktion::Edit&);
} // namespace ld

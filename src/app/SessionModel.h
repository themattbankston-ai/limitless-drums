#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "core/Naming.h"
#include "Engine.h"
#include "ui/Studio.h"

namespace ld
{
/** Everything the interface shows, from the edit (names, mute, solo, faders) and, when it is there, the
    engine's state (hits, velocity layers, round robins, per-mic levels per layer, pitch, alignment). */
ui::StudioModel buildStudioModel (tracktion::Edit&, const std::vector<MicRole>& roles, const juce::String& title, const EngineState*);

/** The audio files behind the edit's tracks, in track order. */
std::vector<juce::File> editFiles (tracktion::Edit&);
} // namespace ld

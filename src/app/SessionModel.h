#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "core/Naming.h"
#include "hl/Prep.h"
#include "ui/Studio.h"

namespace ld
{
/** The engine's view of the open take: Hitline's session (hits, alignment, tuning), the processing chain's
    renders and the levels, once they are ready. */
struct EngineState
{
    hl::Session session;
    std::optional<hl::MixRender> mix;
    std::optional<hl::LevelResult> levels;
    bool prepared = false;            ///< Auto prep ran and found every hit
    std::vector<std::string> errors;
    std::string advice;               ///< what to change when the hit count doesn't match
};

/** Reads the take, opens it as Hitline would (names, lead, counts, drum defaults, hits) and runs Auto prep.
    Blocking: call it off the message thread. */
std::shared_ptr<EngineState> prepareTake (const std::vector<juce::File>& files, const juce::String& folderName);
/** Renders the processing chain and measures the levels on it. Blocking. */
void renderAndMeasure (EngineState&);

/** Everything the interface shows, from the edit (names, mute, solo, faders) and, when it is there, the
    engine's state (hits, velocity layers, round robins, per-mic levels per layer, pitch, alignment). */
ui::StudioModel buildStudioModel (tracktion::Edit&, const std::vector<MicRole>& roles, const juce::String& title, const EngineState*);

/** The audio files behind the edit's tracks, in track order. */
std::vector<juce::File> editFiles (tracktion::Edit&);
} // namespace ld

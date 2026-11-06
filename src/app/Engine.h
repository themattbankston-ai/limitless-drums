#pragma once

#include "hl/Prep.h"
#include <juce_core/juce_core.h>
#include <memory>

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

} // namespace ld

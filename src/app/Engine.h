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

/** What the New Session window decided. Empty / zero fields: as read from the files. */
struct TakeOptions
{
    std::string drum, artic;          ///< drum key: kick, snare, tom, hat, ride, crash, china, splash, kit
    int vel = 0, rr = 0;
    juce::var settings;               ///< hl::settingsToVar of the session to start from (void = the standard)
};

/** Reads the take and opens it as Hitline would (names, lead, counts, drum defaults, hits found), with the
    options on top. No Auto prep. Blocking: call it off the message thread. */
std::shared_ptr<EngineState> openTake (const std::vector<juce::File>& files, const juce::String& folderName, const TakeOptions& = {});
/** openTake, then Auto prep. Blocking. */
std::shared_ptr<EngineState> prepareTake (const std::vector<juce::File>& files, const juce::String& folderName, const TakeOptions& = {});
/** Renders the processing chain and measures the levels on it. Blocking. */
void renderAndMeasure (EngineState&);

} // namespace ld

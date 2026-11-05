#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "core/Naming.h"
#include "ui/Studio.h"

namespace ld
{
/** Reads the session's audio and works out everything the interface shows: waveform peaks,
    the hits, their velocity layers and round robins, per-mic levels per layer, pitch and the
    arrival time of each mic relative to the reference (the first close mic). */
ui::StudioModel buildStudioModel (tracktion::Edit&, const std::vector<MicRole>& roles, const juce::String& title);

/** The mic Matt uses for a role, shown under the track name until you set your own. */
juce::String defaultMicModel (const MicRole&);
} // namespace ld

#pragma once
#include "Align.h"
#include "Session.h"

namespace ld::hl
{
/** How a mic of this role is lined up: Hitline's own options for that role (a sub kick is a close mic). */
AlignOptions alignOptionsFor (const std::string& role);

/** What a mic is, from its sound: "sub", "bottom", "far" or "near" ("" when it can't tell). */
std::string detectRole (const Session&, int track);

/** The role a track is treated as: the user's choice, else what the sound says, else the name. */
std::string effectiveRole (const Session&, int track);
} // namespace ld::hl

#pragma once
#include "Align.h"
#include "Session.h"

namespace ld::hl
{
/** The rule book: how an engineer treats each kind of drum mic when lining it up. The measurement always
    decides; the rules say what to listen to and which way to lean in a close call.
      sub     a sub kick (speaker-cone mic): judged on the lows only, searched wider, the delay that matches
              when it first hears the hit
      bottom  a snare/tom bottom or reso mic: the heads move opposite ways, so it leans to flipped; the
              upper bands (the wires) count more
      near    any other close mic (kick in/out, toms, overheads): the delay that matches when it first hears
              the hit, not one a cycle off
      far     rooms: as Hitline (found by arrival, kept natural by default) */
AlignOptions rulesFor (const std::string& role);

/** What a mic is, from its sound: "sub", "bottom", "far" or "near" ("" when it can't tell). */
std::string detectRole (const Session&, int track);

/** The role the rule book uses for a track: the user's choice, else what the sound says, else the name. */
std::string effectiveRole (const Session&, int track);
} // namespace ld::hl

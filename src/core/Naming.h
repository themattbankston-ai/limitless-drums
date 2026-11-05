#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace ld
{

/** What a mic is, worked out from its file name. `rank` sorts tracks into Matt's order:
    close mics top to bottom (kick, snare top then bottom, hat, toms, ride, cymbals),
    then overheads, room mono, room close, room far, room crush, other rooms,
    and finally prints and effects. */
struct MicRole
{
    enum class Kind
    {
        kickIn, kickOut, kickSub, kick,
        snareTop, snareBottom, snare,
        hiHat, tom, floorTom, ride, cymbal,
        overheads,
        roomMono, roomClose, room, roomFar, roomCrush,
        ambient,
        print,
        other
    };

    Kind kind = Kind::other;
    int index = 0;            ///< 1, 2, 3 for numbered mics (Tom 2), 0 if none
    juce::String side;        ///< "L", "R" or empty
    juce::String detail;      ///< extra words kept for the name (e.g. "Hat" in "Overheads Hat")
    bool confident = false;   ///< false = keep the file's own (cleaned) name
    double rank = 1000.0;
    juce::String displayName;
};

/** Classifies one file name (with or without extension or track number). */
MicRole classifyMic (const juce::String& fileName);

/** Classifies, sorts and names a whole session. Returns indices into `fileNames` in
    display order; `rolesOut` (same order as the result) gets the final roles, with
    duplicate names numbered (Ambient 1, Ambient 2). Files that tie keep their
    original order. */
std::vector<int> orderSession (const juce::StringArray& fileNames, std::vector<MicRole>& rolesOut);

/** "01 Kick1.wav" -> "Kick1", "SNARE_CENTER_TOP_V06_RR06.wav" -> "SNARE CENTER TOP" */
juce::String cleanFileName (const juce::String& fileName);

} // namespace ld

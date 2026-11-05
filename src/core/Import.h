#pragma once

#include "Session.h"
#include "Naming.h"

namespace ld
{

/** One folder's worth of mics that belong together (a song, or one articulation of a sample session). */
struct ImportTake
{
    juce::String title;                   ///< Folder name, e.g. "Saosin Drums" or "Center"
    juce::File folder;
    std::vector<MicFile> mics;            ///< In display order, named to the standard
    std::vector<MicRole> roles;           ///< Same order as mics
    std::vector<juce::File> references;   ///< One-shot references found beside the mics
    double sampleRate = 0;
    double lengthSeconds = 0;
    juce::StringArray warnings;
};

/** Works out what to open from dropped files and folders. Each folder that holds audio
    files becomes one take (subfolders included, so a sample session's Center, Rimshot,
    Sidestick folders each become a take). Loose files dropped together form one take. */
std::vector<ImportTake> planImport (const juce::Array<juce::File>& filesOrFolders);

bool isAudioFile (const juce::File&);

} // namespace ld

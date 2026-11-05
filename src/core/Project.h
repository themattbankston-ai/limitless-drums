#pragma once

#include "Session.h"
#include "Naming.h"

namespace ld
{

/** A Limitless Drums project is one ".ldproj" file: the full edit (tracks, clips, edits,
    tempo map, mixer) plus each track's mic role. Your audio files are referenced, never
    copied or changed. */
namespace project
{
    inline constexpr const char* extension = ".ldproj";

    /** Stores each track's mic role on the track so it survives save/load. */
    void storeRoles (tracktion::Edit&, const std::vector<MicRole>&);
    std::vector<MicRole> readRoles (tracktion::Edit&);

    /** Writes the project. Returns false (and leaves any existing file alone) on failure. */
    bool save (tracktion::Edit&, const juce::File& file);

    /** Opens a project. Returns nullptr if the file can't be read. Missing audio files are
        listed in `missingFiles` so the app can ask where they went. */
    std::unique_ptr<tracktion::Edit> open (tracktion::Engine&, const juce::File& file,
                                           std::vector<MicRole>& roles, juce::StringArray& missingFiles);

    /** Folder for autosaves and crash recovery. */
    juce::File autosaveFolder();

    /** The autosave file for a project (or for an untitled session, keyed by `sessionKey`). */
    juce::File autosaveFileFor (const juce::File& projectFile, const juce::String& sessionKey);

    /** Writes an autosave if the edit changed since the last save/autosave. */
    bool autosave (tracktion::Edit&, const juce::File& autosaveFile);

    /** Autosaves left behind by a crash (newer than their project, or untitled). */
    juce::Array<juce::File> findRecoverableAutosaves();
}

} // namespace ld

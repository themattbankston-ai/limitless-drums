#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace ld
{
/** Libraries: named collections of sessions (Limitless Snares, Kicks…). Each session keeps what it was (drum,
    articulation, counts), where its project lives, and its settings, so a new session can start from them.
    Kept in one JSON file in the app's data folder. */
struct LibrarySession
{
    juce::String id, name, drum, artic, project;
    int vel = 0, rr = 0;
    juce::var settings;               ///< hl::settingsToVar
    juce::int64 modified = 0;
};

struct Library
{
    juce::String name;
    juce::uint32 colour = 0xff9b6bff;
    std::vector<LibrarySession> sessions;
};

class LibraryStore
{
public:
    explicit LibraryStore (juce::File file = defaultFile());
    static juce::File defaultFile();

    std::vector<Library> libraries;
    juce::String current;             ///< the library new sessions go into

    void load();
    bool save() const;
    Library* find (const juce::String& name);
    /** Adds a library (a new colour each time). Returns false if the name is taken or empty. */
    bool add (const juce::String& name);
    /** Adds or replaces a session (matched by id) in a library. */
    void put (const juce::String& library, const LibrarySession&);
    const LibrarySession* findSession (const juce::String& id) const;

private:
    juce::File file;
};
} // namespace ld

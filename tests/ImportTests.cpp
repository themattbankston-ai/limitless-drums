#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "core/Import.h"

using namespace ldtest;

TEST_CASE ("Import: a sample session folder becomes one take per articulation, in order", "[import]")
{
    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_tests/import/SNARE 2 - BB");
    root.getParentDirectory().deleteRecursively();

    // Two articulation folders written with SPLIT-style names, plus a one-shot folder.
    for (auto art : { "Center", "Rimshot" })
    {
        auto dir = root.getChildFile (art);
        auto take = writeTake (dir, { 0.5, 1.0 }, 7, 2.0);
        const char* mics[] = { "TOP", "BOTTOM", "OH", "ROOM-MONO", "ROOM-CLOSE", "ROOM-FAR", "ROOM-CRUSH" };
        for (int i = 0; i < 7; ++i)
            take.mics[(size_t) i].file.moveFileTo (dir.getChildFile (juce::String ("SNARE_") + juce::String (art).toUpperCase()
                                                                     + "_" + mics[6 - i] + "_RR06_V06.wav"));
    }
    auto oneShot = root.getChildFile ("One Shot");
    writeSine (oneShot, 200.0, 0.5f, 0.5).file.moveFileTo (oneShot.getChildFile ("LUDWIG BLACK BEAUTY ONE SHOT.wav"));

    auto takes = ld::planImport ({ root });
    REQUIRE (takes.size() == 2);
    CHECK (takes[0].title.contains ("Center"));
    CHECK (takes[1].title.contains ("Rimshot"));
    REQUIRE (takes[0].mics.size() == 7);

    juce::StringArray names;
    for (auto& m : takes[0].mics) names.add (m.name);
    CHECK (names == juce::StringArray { "Snare Top", "Snare Bottom", "Overheads", "Room Mono", "Room Close", "Room Far", "Room Crush" });
    CHECK (takes[0].references.size() == 1);
    CHECK (takes[0].sampleRate == 48000.0);
    CHECK (takes[0].warnings.isEmpty());
}

TEST_CASE ("Import: loose files dropped together form one take", "[import]")
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_tests/import_loose");
    dir.deleteRecursively();
    auto take = writeTake (dir, { 0.5 }, 3, 1.0);

    juce::Array<juce::File> files;
    for (auto& m : take.mics) files.add (m.file);
    auto takes = ld::planImport (files);
    REQUIRE (takes.size() == 1);
    CHECK (takes[0].mics.size() == 3);
    CHECK (takes[0].mics[0].name == "Kick In");   // kick sorts before snare
}

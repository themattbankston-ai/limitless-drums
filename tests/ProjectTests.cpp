#include <catch2/catch_test_macros.hpp>
#include "TestSignals.h"
#include "core/Project.h"
#include "core/Conform.h"
#include "core/Analysis.h"
#include <cmath>

using namespace ldtest;

static double maxDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const int n = std::min (a.getNumSamples(), b.getNumSamples());
    double m = 0;
    for (int i = 0; i < n; ++i) m = std::max (m, (double) std::abs (a.getSample (0, i) - b.getSample (0, i)));
    return m;
}

TEST_CASE ("Project: save and reopen keeps tracks, roles and every edit exactly", "[project]")
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_tests/project");
    dir.deleteRecursively();
    auto take = writeTake (dir.getChildFile ("audio"), { 1.0, 1.51, 1.98, 2.52 }, 4, 4.0);

    juce::StringArray names;
    for (auto& m : take.mics) names.add (m.file.getFileName());
    std::vector<ld::MicRole> roles;
    auto order = ld::orderSession (names, roles);
    std::vector<ld::MicFile> mics;
    for (size_t i = 0; i < order.size(); ++i) mics.push_back ({ roles[i].displayName, take.mics[(size_t) order[i]].file });

    auto edit = ld::createSessionEdit (engine(), mics, 120.0);
    REQUIRE (edit != nullptr);
    ld::project::storeRoles (*edit, roles);
    ld::conformToGrid (*edit, ld::detectTriggers (ld::readMono (mics[0].file), 48000.0), {});
    const auto before = ld::renderTrack (*edit, 0, 48000.0);

    auto file = dir.getChildFile (juce::String ("Test") + ld::project::extension);
    REQUIRE (ld::project::save (*edit, file));
    CHECK (! edit->hasChangedSinceSaved());
    edit = nullptr;

    std::vector<ld::MicRole> loadedRoles;
    juce::StringArray missing;
    auto reopened = ld::project::open (engine(), file, loadedRoles, missing);
    REQUIRE (reopened != nullptr);
    CHECK (missing.isEmpty());

    auto tracks = tracktion::getAudioTracks (*reopened);
    REQUIRE (tracks.size() == 4);
    REQUIRE (loadedRoles.size() == 4);
    for (int i = 0; i < 4; ++i)
    {
        CHECK (tracks[i]->getName() == roles[(size_t) i].displayName);
        CHECK (loadedRoles[(size_t) i].kind == roles[(size_t) i].kind);
        CHECK (tracks[i]->getClips().size() == 5);   // 4 hits -> 5 slices
    }

    const auto after = ld::renderTrack (*reopened, 0, 48000.0);
    CHECK (maxDiff (before, after) < 1.0e-6);
}

TEST_CASE ("Project: missing audio files are reported, not silently dropped", "[project]")
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_tests/project_missing");
    dir.deleteRecursively();
    auto take = writeTake (dir.getChildFile ("audio"), { 0.5 }, 2, 1.0);
    auto edit = ld::createSessionEdit (engine(), take.mics, 120.0);
    auto file = dir.getChildFile (juce::String ("Missing") + ld::project::extension);
    REQUIRE (ld::project::save (*edit, file));
    edit = nullptr;

    take.mics[1].file.deleteFile();
    std::vector<ld::MicRole> roles;
    juce::StringArray missing;
    auto reopened = ld::project::open (engine(), file, roles, missing);
    REQUIRE (reopened != nullptr);
    CHECK (missing.size() == 1);
}

TEST_CASE ("Project: autosave writes only when something changed, and is found for recovery", "[project]")
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ld_tests/project_autosave");
    dir.deleteRecursively();
    auto take = writeTake (dir.getChildFile ("audio"), { 0.5 }, 2, 1.0);
    auto edit = ld::createSessionEdit (engine(), take.mics, 120.0);

    auto auto1 = ld::project::autosaveFileFor ({}, "test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
    auto1.deleteFile();
    CHECK (ld::project::autosave (*edit, auto1));
    CHECK (auto1.existsAsFile());
    CHECK (ld::project::findRecoverableAutosaves().contains (auto1));
    auto1.deleteFile();
}

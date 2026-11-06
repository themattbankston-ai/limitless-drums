// What each mic is, told by its sound: a sub kick, a snare bottom, a far room, whatever it's called.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/Rules.h"

TEST_CASE ("Mic roles come from the sound")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_RULES_TAKES", {});
    if (root.isEmpty()) { WARN ("LD_RULES_TAKES not set; skipped"); return; }
    for (auto& d : juce::File (root).findChildFiles (juce::File::findDirectories, false))
    {
        auto files = d.findChildFiles (juce::File::findFiles, false, "*.wav");
        std::vector<juce::File> fv (files.begin(), files.end());
        auto st = ld::openTake (fv, "Center");
        auto& S = st->session;
        ld::hl::runDetect (S);
        for (int i = 0; i < (int) S.tracks.size(); ++i)
        {
            if (i == S.refIdx) continue;
            auto& tr = S.tracks[(size_t) i];
            const auto r = ld::hl::detectRole (S, i);
            const auto up = juce::String (tr.mic).toUpperCase();
            INFO (d.getFileName() << " / " << tr.mic << " heard as " << r);
            if (up.contains ("ROOM")) CHECK (r == "far");
            else if (up.contains ("SUB")) CHECK (r == "sub");
            else if (up.contains ("BOTTOM")) CHECK (r == "bottom");
            else CHECK (r == "near");
            // what it's called no longer matters: a sub named anything is still a sub
            tr.mic = "Mic " + std::to_string (i); tr.name = tr.mic;
            CHECK (ld::hl::detectRole (S, i) == r);
        }
    }
}

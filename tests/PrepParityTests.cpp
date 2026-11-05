// Parity with Hitline 4.8's import and Auto prep: the session folders dropped in, named, set up and prepared
// by the C++ port must come out as Hitline's answer keys (tests/parity/*.json) describe them.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>
#include "hl/Prep.h"
using Catch::Matchers::WithinAbs;

TEST_CASE ("Mic names, drum and articulation are read from file names like Hitline")
{
    using namespace ld::hl;
    CHECK (cleanMicName ("SNARE_CENTER_TOP_V06_RR06.wav", "snare", "Center") == "Top");
    CHECK (cleanMicName ("SNARE_CENTER_ROOM-FAR_RR06_V06.wav", "snare", "Center") == "Room Far");
    CHECK (cleanMicName ("SNARE_CENTER_OH_V06_RR06.wav", "snare", "Center") == "Overheads");
    CHECK (cleanMicName ("Kick In.wav", "kick", "Hit") == "Kick In");
    CHECK (cleanMicName ("Snare Top 01.wav", "kit", "Performance") == "Snare Top 01");
    CHECK (guessDrum ("Toms/Floor 2") == "tom");
    CHECK (guessArtic ("snare rimshot_v06_rr06") == "Rimshot");
    CHECK (guessArtic ("SNARE RIMSHOT_V06_RR06") == "Snare Rimshot");   // as Hitline: drum words only match in lower case
    const auto g = groupSessions ({ "Center/a_TOP.wav", "Center/a_OH.wav", "Rim/b.wav" });
    REQUIRE (g.size() == 2);
    CHECK (g[0].artic == "Center");
    CHECK (g[0].files.size() == 2);
}

TEST_CASE ("Parity: import and Auto prep match Hitline 4.8")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    const auto dir = juce::File (LD_SOURCE_DIR).getChildFile ("tests/parity");
    const auto index = juce::JSON::parse (dir.getChildFile ("sessions.json"));
    for (auto& e : *index.getArray())
    {
        const auto key = juce::JSON::parse (dir.getChildFile (e["key"].toString()))["state"];
        const auto folder = juce::File (root).getChildFile (e["folder"].toString());
        if (! folder.isDirectory()) continue;
        INFO (e["key"].toString());
        auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
        std::sort (files.begin(), files.end(), [] (auto& a, auto& b) { return a.getFileName().toStdString() < b.getFileName().toStdString(); });
        std::vector<std::string> rel;
        for (auto& f : files) rel.push_back ((folder.getFileName() + "/" + f.getFileName()).toStdString());
        const auto recs = ld::hl::groupSessions (rel);
        REQUIRE (recs.size() == 1);
        CHECK (recs[0].drum == key["drum"].toString().toStdString());
        CHECK (recs[0].artic == key["artic"].toString().toStdString());
        std::vector<ld::hl::SourceFile> src;
        for (auto i : recs[0].files) src.push_back ({ files[(int) i].getFileName().toStdString(), rel[i], ld::hl::readWav (files[(int) i]) });
        auto opened = ld::hl::openSession (std::move (src), recs[0].drum, recs[0].artic);
        CHECK (opened.errors.empty());
        auto& S = opened.session;
        REQUIRE (ld::hl::autoPrep (S));

        CHECK (S.refIdx == (int) key["refIdx"]);
        CHECK (S.det.V == (int) key["det"]["V"]);
        CHECK (S.det.RR == (int) key["det"]["RR"]);
        CHECK (S.thrDb == (double) key["det"]["thrDb"]);
        CHECK (S.exp.maxLenS == (double) key["exp"]["maxLenS"]);
        CHECK (S.lev.shape == (double) key["lev"]["shape"]);
        const auto* ko = key["onsets"].getArray();
        REQUIRE ((int) S.onsets.size() == ko->size());
        for (int i = 0; i < ko->size(); ++i) CHECK (S.onsets[(size_t) i] == (long) (int) (*ko)[i]);
        CHECK (S.flipped == (bool) key["phase"]["flipped"]);
        const auto* kt = key["tracks"].getArray();
        REQUIRE ((int) S.tracks.size() == kt->size());
        for (int t = 0; t < kt->size(); ++t)
        {
            const auto k = (*kt)[t];
            const auto& tr = S.tracks[(size_t) t];
            INFO (k["mic"].toString());
            CHECK (tr.name == k["name"].toString().toStdString());
            CHECK (tr.mic == k["mic"].toString().toStdString());
            CHECK (tr.alignTo == k["alignTo"].toString().toStdString());
            CHECK_THAT (tr.lag, WithinAbs ((double) k["lag"], 1e-9));
            CHECK (tr.pol == (int) k["pol"]);
            const auto* ap = k["ap"].getArray();
            REQUIRE ((int) tr.ap.size() == ap->size());
            for (int j = 0; j < ap->size(); ++j) { CHECK (tr.ap[(size_t) j].fc == (double) (*ap)[j]["fc"]); CHECK (tr.ap[(size_t) j].Q == (double) (*ap)[j]["Q"]); }
        }
        const auto ti = key["tinfo"];
        REQUIRE (S.tuneInfo.has_value() == ti.isObject());
        if (S.tuneInfo)
        {
            CHECK_THAT (S.tuneInfo->medF, WithinAbs ((double) ti["medF"], 1e-9));
            CHECK (S.tuneInfo->nearest == (int) ti["nearest"]);
            for (int i = 0; i < ti["ratios"].getArray()->size(); ++i) CHECK_THAT (S.tuneInfo->ratios[(size_t) i], WithinAbs ((double) ti["ratios"][i], 1e-12));
        }
    }
}

TEST_CASE ("Parity: a whole-kit take opens like Hitline 4.8")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    const auto keyFile = juce::File (LD_SOURCE_DIR).getChildFile ("tests/parity/export_K.json");
    if (root.isEmpty() || ! keyFile.existsAsFile()) { WARN ("LD_PARITY_SESSIONS not set; parity skipped"); return; }
    const auto key = juce::JSON::parse (keyFile)["state"];
    const auto folder = juce::File (root).getChildFile ("SNARE 1 - MH/Center");
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::sort (files.begin(), files.end(), [] (auto& a, auto& b) { return a.getFileName().toStdString() < b.getFileName().toStdString(); });
    std::vector<std::string> rel;
    for (auto& f : files) rel.push_back (("Center/" + f.getFileName()).toStdString());
    const auto recs = ld::hl::groupSessions (rel, true);
    REQUIRE (recs.size() == 1);
    CHECK (recs[0].artic == key["artic"].toString().toStdString());
    std::vector<ld::hl::SourceFile> src;
    for (auto i : recs[0].files) src.push_back ({ files[(int) i].getFileName().toStdString(), rel[i], ld::hl::readWav (files[(int) i]) });
    auto opened = ld::hl::openSession (std::move (src), "kit", recs[0].artic);
    auto& S = opened.session;
    CHECK (S.mode == "kit");
    CHECK (S.exp.mode == "full");
    CHECK (! S.dnOn);
    const auto* ko = key["kitOn"].getArray();
    REQUIRE ((int) S.kitOn.size() == ko->size());
    for (int i = 0; i < ko->size(); ++i) CHECK (S.kitOn[(size_t) i] == (long) (double) (*ko)[i]);
    const auto* kt = key["tracks"].getArray();
    REQUIRE ((int) S.tracks.size() == kt->size());
    for (int t = 0; t < kt->size(); ++t)
    {
        CHECK (S.tracks[(size_t) t].mic == (*kt)[t]["mic"].toString().toStdString());
        CHECK (S.tracks[(size_t) t].alignTo == (*kt)[t]["alignTo"].toString().toStdString());
    }
    CHECK (ld::hl::autoPrep (S));
    CHECK (S.phaseAnalyzed);
}

TEST_CASE ("A take opened from files goes through the whole chain")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) { WARN ("LD_PARITY_SESSIONS not set; skipped"); return; }
    const auto folder = juce::File (root).getChildFile ("SNARE 1 - MH/Center");
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<ld::hl::SourceFile> src;
    for (auto& f : files) src.push_back ({ f.getFileName().toStdString(), ("Center/" + f.getFileName()).toStdString(), ld::hl::readWav (f) });
    auto opened = ld::hl::openSession (std::move (src), "snare", "Center");
    auto& S = opened.session;
    REQUIRE (ld::hl::autoPrep (S));
    const auto M = ld::hl::renderMix (S);
    const auto L = ld::hl::measureLevels (S, M);
    CHECK (L.valid);
}


// Not a pass/fail test: prints how much memory each stage of a session takes ([.mem], run on purpose).
#include <catch2/catch_test_macros.hpp>
#include "../src/app/Engine.h"
#include "../src/hl/Prep.h"
#include "../src/hl/FxSat.h"
#include <fstream>

static long rssMb()
{
    std::ifstream f ("/proc/self/status"); std::string l;
    while (std::getline (f, l)) if (l.rfind ("VmRSS:", 0) == 0) return std::stol (l.substr (6)) / 1024;
    return -1;
}
static size_t bytesOf (const std::vector<ld::hl::Chs>& v) { size_t b = 0; for (auto& c : v) for (auto& x : c) b += x.capacity() * 4; return b; }

TEST_CASE ("Memory probe", "[.mem]")
{
    const auto root = juce::SystemStats::getEnvironmentVariable ("LD_PARITY_SESSIONS", {});
    if (root.isEmpty()) return;
    ld::hl::setReverbLibrary (juce::JSON::parse (juce::File (LD_SOURCE_DIR).getChildFile ("assets/irlib.json")));
    WARN ("start " << rssMb() << " MB");
    auto files = juce::File (root).getChildFile ("SNARE 1 - MH/Center").findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    auto st = ld::prepareTake (fv, "Center");
    WARN ("after prepare (session + mix + levels) " << rssMb() << " MB");
    size_t raw = 0; for (auto& t : st->session.tracks) for (auto& x : t.ch) raw += x.capacity() * 4;
    WARN ("raw audio in the session: " << raw / 1048576 << " MB, " << st->session.tracks.size() << " tracks");
    if (st->mix)
    {
        auto& M = *st->mix;
        WARN ("mix stages MB: dn " << bytesOf (M.dn) / 1048576 << " tone " << bytesOf (M.tone) / 1048576 << " ins " << bytesOf (M.ins) / 1048576
              << " bus " << bytesOf (M.bus) / 1048576 << " proc " << bytesOf (M.proc) / 1048576);
    }
    { auto M2 = ld::hl::renderMix (st->session); WARN ("during a second render " << rssMb() << " MB");
      WARN ("its stages MB: dn " << bytesOf (M2.dn) / 1048576 << " tone " << bytesOf (M2.tone) / 1048576 << " ins " << bytesOf (M2.ins) / 1048576
            << " bus " << bytesOf (M2.bus) / 1048576 << " proc " << bytesOf (M2.proc) / 1048576); }
    WARN ("after it " << rssMb() << " MB");
}

#include "../src/hl/Song.h"
TEST_CASE ("Song render timing probe", "[.perf]")
{
    const auto dir = juce::File (juce::SystemStats::getEnvironmentVariable ("LD_SONG", "/home/claude/songtest/song240"));
    if (! dir.isDirectory()) return;
    ld::hl::setReverbLibrary (juce::JSON::parse (juce::File (LD_SOURCE_DIR).getChildFile ("assets/irlib.json")));
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> fv (files.begin(), files.end());
    ld::TakeOptions o; o.drum = "kit";
    auto st = ld::prepareTake (fv, dir.getFileName(), o);
    auto& S = st->session;
    auto timeIt = [&] (const char* what)
    {
        const auto t = juce::Time::getMillisecondCounterHiRes();
        { auto M = ld::hl::renderMix (S); }
        WARN (what << ": " << (juce::Time::getMillisecondCounterHiRes() - t) / 1000 << " s, RSS " << rssMb() << " MB");
    };
    timeIt ("plain");
    S.song.set.tightness = 0.8; timeIt ("tightened");
    S.tracks[0].strip.r1 = -10; timeIt ("tightened + R1 send");
    S.song.set.tightness = 0; timeIt ("R1 send only");
}

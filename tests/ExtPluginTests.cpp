// Third-party plugin hosting: the test plugin (tools: /home/claude/testplug, a gain with 64 samples of latency and
// its gain in its state) is found by a scan, runs offline with its latency taken out, and keeps its state.
#include <catch2/catch_test_macros.hpp>
#include "../src/app/ExtPlugins.h"

TEST_CASE ("Third-party plugins: found, run offline in time, state kept")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("LD_PLUGIN_PATH", "/home/claude/testplug/build/LDTestGain_artefacts/Release/VST3");
    if (! juce::File (path).isDirectory()) { WARN ("no test plugin; skipped"); return; }
    juce::ScopedJuceInitialiser_GUI gui;
    juce::FileSearchPath sp (path);
    juce::KnownPluginList list;
    juce::AudioPluginFormat* vst3 = nullptr;
    for (auto* f : ld::ext::formats().getFormats()) if (f->getName() == "VST3") vst3 = f;
    REQUIRE (vst3 != nullptr);
    juce::PluginDirectoryScanner sc (list, *vst3, sp, true, juce::File(), false);
    juce::String name; while (sc.scanNextFile (true, name)) {}
    REQUIRE (list.getNumTypes() >= 1);
    const auto desc = list.getTypes()[0];
    CHECK (desc.name == "LD Test Gain");
    auto ins = ld::ext::makeInsert (desc);
    // a click at 1000 on both channels; default gain 0.5; latency removed -> the click stays at 1000, halved
    ld::hl::Chs in (2, ld::hl::F32 (4000, 0.0f)); in[0][1000] = 1; in[1][1000] = 1;
    auto out = ld::ext::render (ins, in, 48000);
    REQUIRE (out.size() == 2);
    CHECK (std::abs (out[0][1000] - 0.5f) < 1e-5f);
    CHECK (std::abs (out[0][936]) < 1e-6f);
    // a mono mic through it: still in time
    ld::hl::Chs mono (1, ld::hl::F32 (4000, 0.0f)); mono[0][2000] = 1;
    auto om = ld::ext::render (ins, mono, 48000);
    REQUIRE (om.size() == 1);
    CHECK (std::abs (om[0][2000] - 0.5f) < 1e-5f);
    // its state: gain 1.5 saved as the plugin saves it
    // (a hosted plugin's state is whatever its wrapper hands back, so make it the way the editor does: set the
    // parameter on an instance and save that instance's state)
    {
        juce::String err;
        auto inst = ld::ext::formats().createPluginInstance (desc, 48000, 512, err);
        REQUIRE (inst != nullptr);
        for (auto* prm : inst->getParameters()) if (prm->getName (32) == "Gain") prm->setValueNotifyingHost (1.5f / 2.0f);
        juce::MemoryBlock m; inst->getStateInformation (m);
        ins["p"].getDynamicObject()->setProperty ("state", m.toBase64Encoding());
    }
    out = ld::ext::render (ins, in, 48000);
    CHECK (std::abs (out[0][1000] - 1.5f) < 1e-5f);
    // a plugin that isn't there: the audio passes through
    auto bad = ld::ext::makeInsert (desc);
    bad["p"].getDynamicObject()->setProperty ("desc", "<PLUGIN name=\"Nope\" format=\"VST3\" file=\"/nowhere/nope.vst3\" uniqueId=\"1\"/>");
    const auto pass = ld::ext::render (bad, in, 48000);
    CHECK (pass[0][1000] == 1.0f);
}

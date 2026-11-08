#pragma once
// Third-party plugins (VST3, and AU on the Mac) as inserts: finding them, running them offline inside the engine's
// render (with their latency taken out, so nothing shifts against the other mics), and their own editor windows.
// An insert holds { "type": "ext", "on": true, "p": { "desc": <the plugin's description, XML>, "name": "...",
// "state": <its saved state, base64> } }.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "hl/FxCore.h"
#include <functional>

namespace ld::ext
{
/** The plugins found so far (kept between runs). */
juce::KnownPluginList& known();
juce::AudioPluginFormatManager& formats();
/** Looks for plugins in the usual places (and LD_PLUGIN_PATH), on a worker thread; done (on the message thread) with
    how many there are now. A plugin that crashes the scan is skipped next time. */
void scan (std::function<void (int)> done);
bool scanning();

/** A new insert for a plugin from the list. */
juce::var makeInsert (const juce::PluginDescription&);
/** The engine's renderer for "ext" inserts (registered at start-up). */
hl::Chs render (const juce::var& insert, const hl::Chs& in, double sr);

/** Opens (or brings forward) the plugin's own editor. Every change is reported as its new state (base64), at most a
    few times a second, and once more when the window closes. */
void openEditor (const juce::var& insert, const juce::String& title, std::function<void (const juce::String& state)> onState);
void closeEditors();
} // namespace ld::ext

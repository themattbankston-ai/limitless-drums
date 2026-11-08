#include "ExtPlugins.h"
#include <atomic>
#include <map>
#include <thread>

namespace ld::ext
{
namespace
{
juce::File dataDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Limitless Drums");
    d.createDirectory();
    return d;
}
juce::File listFile() { return dataDir().getChildFile ("plugins.xml"); }
std::atomic<bool> busy { false };
std::mutex listLock;

/** A plugin instance, made on the message thread when there is one (some formats insist), else right here. */
std::unique_ptr<juce::AudioPluginInstance> instantiate (const juce::PluginDescription& d, double sr, int block, juce::String& err)
{
    auto make = [&] { return formats().createPluginInstance (d, sr, block, err); };
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm == nullptr || mm->isThisTheMessageThread()) return make();
    std::unique_ptr<juce::AudioPluginInstance> p;
    juce::WaitableEvent done;
    juce::MessageManager::callAsync ([&] { p = make(); done.signal(); });
    if (! done.wait (20000)) { err = "timed out"; return nullptr; }
    return p;
}

bool descOf (const juce::var& insert, juce::PluginDescription& d)
{
    const auto xml = juce::parseXML (insert["p"]["desc"].toString());
    return xml != nullptr && d.loadFromXml (*xml);
}

void restore (juce::AudioPluginInstance& p, const juce::var& insert)
{
    juce::MemoryBlock m;
    if (m.fromBase64Encoding (insert["p"]["state"].toString()) && m.getSize() > 0) p.setStateInformation (m.getData(), (int) m.getSize());
}
juce::String stateOf (juce::AudioPluginInstance& p)
{
    juce::MemoryBlock m; p.getStateInformation (m);
    return m.toBase64Encoding();
}
} // namespace

juce::AudioPluginFormatManager& formats()
{
    static juce::AudioPluginFormatManager fm;
    static const bool init = [] { juce::addDefaultFormatsToManager (fm); return true; }();
    (void) init;
    return fm;
}

juce::KnownPluginList& known()
{
    static juce::KnownPluginList list;
    static const bool loaded = []
    {
        if (auto xml = juce::parseXML (listFile())) list.recreateFromXml (*xml);
        return true;
    }();
    (void) loaded;
    return list;
}

bool scanning() { return busy.load(); }

void scan (std::function<void (int)> done)
{
    if (busy.exchange (true)) return;
    formats(); known();
    std::thread ([done]
    {
        for (auto* f : formats().getFormats())
        {
            auto paths = f->getDefaultLocationsToSearch();
            const auto extra = juce::SystemStats::getEnvironmentVariable ("LD_PLUGIN_PATH", {});
            if (extra.isNotEmpty()) paths.add (juce::File (extra));
            // a plugin that takes the scan down is named in the pedal file and left out next time
            juce::PluginDirectoryScanner sc (known(), *f, paths, true, dataDir().getChildFile ("scan-crash.txt"), false);
            juce::String name;
            while (sc.scanNextFile (true, name)) {}
        }
        if (auto xml = known().createXml()) xml->writeTo (listFile());
        const int n = known().getNumTypes();
        busy = false;
        juce::MessageManager::callAsync ([done, n] { if (done) done (n); });
    }).detach();
}

juce::var makeInsert (const juce::PluginDescription& d)
{
    auto* p = new juce::DynamicObject();
    if (auto xml = d.createXml()) p->setProperty ("desc", xml->toString (juce::XmlElement::TextFormat().singleLine()));
    p->setProperty ("name", d.name); p->setProperty ("maker", d.manufacturerName); p->setProperty ("state", "");
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", "ext"); o->setProperty ("on", true); o->setProperty ("p", juce::var (p));
    return juce::var (o);
}

hl::Chs render (const juce::var& insert, const hl::Chs& in, double sr)
{
    if (in.empty() || in[0].empty()) return in;
    juce::PluginDescription d;
    if (! descOf (insert, d)) return in;
    constexpr int B = 512;
    juce::String err;
    auto p = instantiate (d, sr, B, err);
    if (p == nullptr) return in;   // not found or won't load: the insert passes audio through
    restore (*p, insert);
    const int nIn = (int) in.size();
    // the channel layout: the track's own if the plugin takes it, else stereo
    auto layoutFor = [] (int ch) { return ch == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); };
    juce::AudioProcessor::BusesLayout L;
    L.inputBuses.add (layoutFor (nIn)); L.outputBuses.add (layoutFor (nIn));
    if (! p->checkBusesLayoutSupported (L)) { L.inputBuses.clear(); L.outputBuses.clear(); L.inputBuses.add (juce::AudioChannelSet::stereo()); L.outputBuses.add (juce::AudioChannelSet::stereo()); }
    p->setBusesLayout (L);
    p->setNonRealtime (true);
    p->setRateAndBufferSizeDetails (sr, B);
    p->prepareToPlay (sr, B);
    const int chans = std::max (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
    const int outCh = std::max (1, p->getTotalNumOutputChannels());
    const long n = (long) in[0].size(), lat = std::max (0, p->getLatencySamples());
    hl::Chs out ((size_t) nIn, hl::F32 ((size_t) n, 0.0f));
    juce::AudioBuffer<float> buf (std::max (chans, 1), B);
    juce::MidiBuffer midi;
    // run past the end by the latency, then drop the first `lat` samples: the output lines up with the input
    for (long s = 0; s < n + lat; s += B)
    {
        const int len = (int) std::min<long> (B, n + lat - s);
        buf.clear();
        for (int c = 0; c < std::min (chans, std::max (nIn, 2)); ++c)
        {
            const auto& x = in[(size_t) std::min (c, nIn - 1)];
            for (int i = 0; i < len; ++i) { const long q = s + i; if (q < n) buf.setSample (c, i, x[(size_t) q]); }
        }
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), buf.getNumChannels(), len);
        p->processBlock (view, midi);
        midi.clear();
        for (int c = 0; c < nIn; ++c)
        {
            const int sc = nIn == 1 && outCh > 1 ? -1 : std::min (c, outCh - 1);   // a mono mic through a stereo plugin: the two sides averaged
            for (int i = 0; i < len; ++i)
            {
                const long q = s + i - lat;
                if (q < 0 || q >= n) continue;
                out[(size_t) c][(size_t) q] = sc < 0 ? 0.5f * (view.getSample (0, i) + view.getSample (1, i)) : view.getSample (sc, i);
            }
        }
    }
    p->releaseResources();
    // free it on the message thread too, as some formats want
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && ! mm->isThisTheMessageThread())
    {
        juce::WaitableEvent gone;
        auto* raw = p.release();
        juce::MessageManager::callAsync ([raw, &gone] { delete raw; gone.signal(); });
        gone.wait (20000);
    }
    return out;
}

// ---------------------------------------------------------------- editor windows
namespace
{
struct EditorWindow : juce::DocumentWindow, private juce::Timer, private juce::AudioProcessorListener
{
    EditorWindow (std::unique_ptr<juce::AudioPluginInstance> inst, const juce::String& title, std::function<void (const juce::String&)> cb)
        : DocumentWindow (title, juce::Colour (0xff141518), DocumentWindow::closeButton), plugin (std::move (inst)), onState (std::move (cb))
    {
        setUsingNativeTitleBar (true);
        plugin->addListener (this);
        if (auto* ed = plugin->createEditorIfNeeded()) setContentNonOwned (ed, true);
        else { auto* g = new juce::GenericAudioProcessorEditor (*plugin); owned.reset (g); setContentNonOwned (g, true); }
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        startTimerHz (4);
    }
    ~EditorWindow() override
    {
        stopTimer();
        plugin->removeListener (this);
        flush();
        clearContentComponent();
        owned.reset();
        if (auto* ed = plugin->getActiveEditor()) delete ed;
    }
    void closeButtonPressed() override;
    void flush() { if (dirty.exchange (false) && onState) onState (stateOf (*plugin)); }
    void timerCallback() override { flush(); }
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { dirty = true; }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override { dirty = true; }
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::unique_ptr<juce::Component> owned;
    std::function<void (const juce::String&)> onState;
    std::atomic<bool> dirty { false };
    juce::String key;
};
std::map<juce::String, std::unique_ptr<EditorWindow>>& editors() { static std::map<juce::String, std::unique_ptr<EditorWindow>> m; return m; }
void EditorWindow::closeButtonPressed()
{
    const auto k = key;
    juce::MessageManager::callAsync ([k] { editors().erase (k); });
}
} // namespace

void openEditor (const juce::var& insert, const juce::String& title, std::function<void (const juce::String&)> onState)
{
    const auto key = title;
    if (auto it = editors().find (key); it != editors().end()) { it->second->toFront (true); return; }
    juce::PluginDescription d;
    if (! descOf (insert, d)) return;
    juce::String err;
    auto p = formats().createPluginInstance (d, 48000, 512, err);
    if (p == nullptr) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Plugin", "Couldn't open " + d.name + ": " + err); return; }
    restore (*p, insert);
    auto w = std::make_unique<EditorWindow> (std::move (p), title, std::move (onState));
    w->key = key;
    editors()[key] = std::move (w);
}

void closeEditors() { editors().clear(); }
} // namespace ld::ext

#include "MainComponent.h"
#include "core/Session.h"
#if defined (__GLIBC__)
 #include <malloc.h>
#endif

namespace ld
{
class Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Limitless Drums"; }
    const juce::String getApplicationVersion() override { return LD_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
       #if defined (__GLIBC__)
        // audio buffers (1 MB and up) straight from and back to the system: without this glibc raises its threshold
        // after the first frees and keeps hundreds of MB of freed renders resident (macOS already behaves this way)
        mallopt (M_MMAP_THRESHOLD, 1 << 20);
       #endif
        engine = std::make_unique<tracktion::Engine> ("Limitless Drums", std::make_unique<SyncUIBehaviour>(), nullptr);
        window = std::make_unique<Window> (getApplicationName(), *engine);
        openFromCommandLine (commandLine);
        juce::MessageManager::callAsync ([this] { if (auto* mc = content()) mc->offerRecovery(); });
    }

    void anotherInstanceStarted (const juce::String& commandLine) override { openFromCommandLine (commandLine); }

    // Opening a folder or files from Finder ("Open With") or the command line.
    void openFromCommandLine (const juce::String& commandLine)
    {
        juce::Array<juce::File> items;
        for (auto& arg : juce::StringArray::fromTokens (commandLine, true))
        {
            auto path = arg.unquoted();
            if (juce::File::isAbsolutePath (path) && juce::File (path).exists())
                items.add (juce::File (path));
        }
        if (! items.isEmpty())
            if (auto* mc = content())
                mc->importItems (items);
    }

    void shutdown() override
    {
        window = nullptr;
        engine = nullptr;
    }

    void systemRequestedQuit() override
    {
        if (auto* mc = content())
            mc->confirmClose ([] (bool ok) { if (ok) juce::JUCEApplication::getInstance()->quit(); });
        else
            quit();
    }

    MainComponent* content() const { return window ? dynamic_cast<MainComponent*> (window->getContentComponent()) : nullptr; }

private:
    struct Window : public juce::DocumentWindow
    {
        Window (const juce::String& name, tracktion::Engine& e)
            : DocumentWindow (name, theme::night().ground, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            auto* mc = new MainComponent (e);
            mc->onTitleChanged = [this] (const juce::String& t) { setName (t); };
            setContentOwned (mc, true);
            setResizable (true, true);
            setResizeLimits (900, 560, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<tracktion::Engine> engine;
    std::unique_ptr<Window> window;
};
} // namespace ld

START_JUCE_APPLICATION (ld::Application)

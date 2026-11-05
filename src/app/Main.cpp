#include "MainComponent.h"

namespace ld
{
class Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Limitless Drums"; }
    const juce::String getApplicationVersion() override { return LD_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        engine = std::make_unique<tracktion::Engine> ("Limitless Drums", std::make_unique<SyncUIBehaviour>(), nullptr);
        window = std::make_unique<Window> (getApplicationName(), *engine);
    }

    void shutdown() override
    {
        window = nullptr;
        engine = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

private:
    struct Window : public juce::DocumentWindow
    {
        Window (const juce::String& name, tracktion::Engine& e)
            : DocumentWindow (name, theme::night().ground, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (e), true);
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

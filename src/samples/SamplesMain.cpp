// The sample build of Limitless Drums: the app shell. JUCE keeps the window, dialogs and audio device; the whole
// interface is drawn on the GPU by the step views.
#include "SampleModel.h"
#include "ui/HitsView.h"
#include "app/ui/VisageHost.h"
#include "core/Session.h"
#include "BinaryData.h"

namespace ld::samples
{

class Shell : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit Shell (te::Engine& e) : model (e), view (model)
    {
        hl::setReverbLibrary (juce::JSON::parse (juce::String::fromUTF8 (BinaryData::irlib_json, BinaryData::irlib_jsonSize)));
        hl::setLibraryReference (juce::JSON::parse (juce::String::fromUTF8 (BinaryData::libref_json, BinaryData::libref_jsonSize)));
        setOpaque (true);
        root.addChild (&view);
        root.onResize() += [this] { view.setBounds (0, 0, root.width(), root.height()); };
        host = std::make_unique<ui::VisageHost> (root);
        addAndMakeVisible (*host);
        view.onOpen = [this] { chooseFiles(); };
        view.onAutoPrep = [this] { autoPrep(); };
        setSize (1440, 900);
        startTimerHz (30);
    }

    ~Shell() override
    {
        stopTimer();
        if (host) host->shutdown();
        root.removeChild (&view);
    }

    void resized() override { if (host) host->setBounds (getLocalBounds()); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff232325)); }

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& paths, int, int) override
    {
        juce::Array<juce::File> fs;
        for (auto& p : paths) fs.add (juce::File (p));
        open (fs);
    }

    void open (const juce::Array<juce::File>& items)
    {
        model.stop();
        const auto err = model.open (items);
        view.setStatus (err.toStdString());
        if (err.isEmpty()) view.takeChanged();
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setName ("Limitless Drums \xe2\x80\x94 " + model.takeName());
    }

    void autoPrep()
    {
        if (! model.isOpen() || model.isPrepping()) return;
        view.setStatus ("Finding the hits and lining up the mics\xe2\x80\xa6");
        model.autoPrep ([this] (juce::String err)
        {
            view.setStatus (err.isEmpty() ? std::string ("Every hit found, and the mics are lined up. A/B to hear the difference.") : err.toStdString());
            view.takeChanged();
        });
    }

private:
    SampleModel model;
    visage::ApplicationWindow root;
    HitsView view;
    std::unique_ptr<ui::VisageHost> host;
    std::unique_ptr<juce::FileChooser> chooser;

    void chooseFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open the recordings of one take", juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) { if (! fc.getResults().isEmpty()) open (fc.getResults()); });
    }

    void timerCallback() override
    {
        view.tick();
       #if JUCE_LINUX
        // embedded on Linux nothing else wakes the GPU window for frames marked stale (macOS has the display link)
        if (root.window() != nullptr) root.drawWindow();
       #endif
    }
};

class Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Limitless Drums"; }
    const juce::String getApplicationVersion() override { return LD_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        engine = std::make_unique<te::Engine> ("Limitless Drums", std::make_unique<SyncUIBehaviour>(), nullptr);
        window = std::make_unique<Window> (*engine);
        juce::Array<juce::File> items;
        for (auto& arg : juce::StringArray::fromTokens (commandLine, true))
            if (juce::File::isAbsolutePath (arg.unquoted()) && juce::File (arg.unquoted()).exists()) items.add (juce::File (arg.unquoted()));
        if (! items.isEmpty())
            juce::MessageManager::callAsync ([this, items]
            {
                if (auto* s = shell()) { s->open (items); if (std::getenv ("LD_AUTOPREP")) s->autoPrep(); }
            });
    }

    void shutdown() override { window = nullptr; engine = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    struct Window : juce::DocumentWindow
    {
        explicit Window (te::Engine& e) : DocumentWindow ("Limitless Drums", juce::Colour (0xff232325), allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new Shell (e), true);
            setResizable (true, true);
            setResizeLimits (1100, 700, 5000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    Shell* shell() const { return window ? dynamic_cast<Shell*> (window->getContentComponent()) : nullptr; }
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<Window> window;
};

} // namespace ld::samples

START_JUCE_APPLICATION (ld::samples::Application)

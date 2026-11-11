// The sample build of Limitless Drums: the app shell. JUCE keeps the window, dialogs and audio device; the whole
// interface is drawn on the GPU by the step views.
#include "SampleModel.h"
#include "ui/HitsView.h"
#include "app/ui/VisageHost.h"
#include "core/Session.h"
#include "hl/Prep.h"
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
        view.onExport = [this] { exportTake(); };
        view.onLoadReference = [this] { loadReference(); };
        model.onChanged = [this]
        {
            view.takeChanged (false);
            // an export asked for while a change was still rendering goes now, on the settings it was asked with
            if (pendingExport != juce::File() && ! model.isBusy()) { auto d = pendingExport; pendingExport = juce::File(); writeExport (d); }
        };
        if (const char* step = std::getenv ("LD_STEP")) view.setStep (std::atoi (step));
        setSize (1440, 900);
        if (const char* sz = std::getenv ("LD_WINDOW"))   // e.g. 1100x700, for checking the smallest layout
            setSize (juce::jmax (1100, juce::String (sz).upToFirstOccurrenceOf ("x", false, false).getIntValue()),
                     juce::jmax (700, juce::String (sz).fromFirstOccurrenceOf ("x", false, false).getIntValue()));
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
        pendingExport = juce::File();
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
            view.setStatus (err.isEmpty() ? std::string ("Every hit found, the mics lined up and the levels set. A/B to hear the difference.") : err.toStdString());
            view.takeChanged();
            if (const char* to = std::getenv ("LD_EXPORT_TO")) if (err.isEmpty()) exportTake();
        });
    }

    void writeExport (juce::File dir)
    {
        {
            view.exportFolder = dir.getFullPathName().toStdString();
            if (model.isBusy() && ! model.isPrepping())
            {
                // export one settled revision, never the one before the last change: wait for the render
                pendingExport = dir;
                view.setStatus ("Exporting as soon as the last change has rendered\xe2\x80\xa6");
                return;
            }
            if (auto why = model.exportBlocker(); why.isNotEmpty()) { view.setStatus (why.toStdString()); return; }
            view.setExportProgress (0);
            view.setStatus ("Writing the files\xe2\x80\xa6");
           #if JUCE_LINUX
            if (root.window() != nullptr) root.drawWindow();
           #endif
            int count = 0;
            const auto err = model.exportTo (dir, count, [this] (float p) { view.setExportProgress (p); });
            view.setExportProgress (-1);
            view.setStatus (err.isEmpty() ? "Wrote " + std::to_string (count) + " files to " + dir.getFullPathName().toStdString() : err.toStdString());
        }
    }

    void exportTake()
    {
        if (auto why = model.exportBlocker(); why.isNotEmpty() && ! model.isBusy()) { view.setStatus (why.toStdString()); return; }
        auto go = [this] (juce::File dir) { writeExport (dir); };
        if (const char* to = std::getenv ("LD_EXPORT_TO")) { go (juce::File (to)); return; }
        chooser = std::make_unique<juce::FileChooser> ("Export to", juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [go] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) go (fc.getResult()); });
    }

    void loadReference()
    {
        if (! model.mixRender()) { view.setStatus ("Run Auto Prep first."); return; }
        chooser = std::make_unique<juce::FileChooser> ("A finished sample to move toward", juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile() || ! model.session() || ! model.mixRender()) return;
            auto R = hl::loadReference (*model.session(), *model.mixRender(), f, hl::kReferenceEngine);
            if (! R.error.empty() || R.curve.empty()) { view.setStatus ("Couldn't read that reference: " + R.error); return; }
            model.change ([R] (hl::Session& s)
            {
                s.tone.hasRef = true; s.tone.refCurve = R.curve; if (s.tone.refAmt == 0) s.tone.refAmt = 0.6;
                s.tone.hasRefEnv = R.hasEnv; s.tone.refEnv = R.env; if (R.hasEnv && s.tone.refEnvAmt == 0) s.tone.refEnvAmt = 0.5;
            }, SampleModel::Redo::mix);
            view.setStatus ("Reference loaded: " + R.name + " (" + std::to_string (R.hitsUsed) + (R.hitsUsed == 1 ? " hit" : " hits") + " measured).");
        });
    }

private:
    SampleModel model;
    visage::ApplicationWindow root;
    HitsView view;
    std::unique_ptr<ui::VisageHost> host;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File pendingExport;

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

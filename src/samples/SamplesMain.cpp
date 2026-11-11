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
        view.onSave = [this] { save(); };
        view.onCancelExport = [this] { if (exporting) { cancelExport = true; view.setStatus ("Cancelling\xe2\x80\xa6"); } };
        // what you chose for room mics is the app's, kept between launches (LD_NO_PREFS: tests and captures start clean)
        if (! std::getenv ("LD_NO_PREFS"))
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "Limitless Drums Samples"; o.filenameSuffix = "settings"; o.osxLibrarySubFolder = "Application Support"; o.folderName = "Limitless Drums";
            prefs = std::make_unique<juce::PropertiesFile> (o);
            model.setRoomPhase (prefs->getBoolValue ("roomPhase", false));
        }
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

    /** Runs `then` once there's nothing unsaved to lose: straight away, after a save, or after you say not to save.
        Cancel (or a save that fails) leaves everything as it is. */
    void guard (std::function<void()> then)
    {
        if (exporting) { view.setStatus ("Still writing the export: cancel it first, or wait."); return; }
        if (! model.modified()) { then(); return; }
        juce::NativeMessageBox::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save the changes to " + model.takeName() + "?",
            "The project is saved beside the recordings. If you don't save, what you changed since the last save is lost.", this,
            juce::ModalCallbackFunction::create ([this, then] (int r)
            {
                if (r == 1) save ([then] (bool ok) { if (ok) then(); });
                else if (r == 2) then();
            }));
    }

    void open (const juce::Array<juce::File>& items) { guard ([this, items] { openNow (items); }); }

    void openNow (const juce::Array<juce::File>& items)
    {
        model.stop();
        pendingExport = juce::File();
        // a project, or recordings with a project saved for exactly them: recalled
        juce::File project;
        if (items.size() == 1 && items[0].hasFileExtension ("ldproj")) project = items[0];
        else project = SampleModel::projectFor (items);
        juce::String err;
        if (project.existsAsFile())
        {
            err = model.openProject (project);
            if (err.isEmpty() && model.recallNeedsPrep())
            {
                view.takeChanged();
                view.setStatus ("Recalling " + project.getFileName().toStdString() + "\xe2\x80\xa6");
                model.autoPrep ([this, project] (juce::String e)
                {
                    const auto note = model.recallNote();
                    view.setStatus (e.isNotEmpty() ? e.toStdString() : note.isNotEmpty() ? note.toStdString() : "Recalled " + project.getFileName().toStdString() + ": everything as you saved it.");
                    view.takeChanged();
                });
            }
            else view.setStatus (err.isEmpty() ? "Recalled " + project.getFileName().toStdString() + ". Run Auto Prep to carry on." : err.toStdString());
        }
        else
        {
            err = model.open (items);
            view.setStatus (err.toStdString());
        }
        if (model.isOpen()) view.takeChanged();
        retitle();
    }

    /** Saves the project beside the recordings (asks where only if that can't be written). */
    void save (std::function<void (bool)> done = {})
    {
        if (! model.isOpen()) { if (done) done (false); return; }
        const auto file = model.projectFile() != juce::File() ? model.projectFile() : model.defaultProjectFile();
        const auto err = model.saveProject (file);
        if (err.isEmpty()) { view.setStatus ("Saved " + file.getFileName().toStdString() + " beside the recordings."); retitle(); if (done) done (true); return; }
        if (! err.startsWith ("Couldn't write")) { view.setStatus (err.toStdString()); if (done) done (false); return; }
        chooser = std::make_unique<juce::FileChooser> ("Save the project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (file.getFileName()), "*.ldproj");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, done] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File()) { if (done) done (false); return; }
            const auto f = fc.getResult().withFileExtension ("ldproj");
            const auto e = model.saveProject (f);
            view.setStatus (e.isEmpty() ? "Saved " + f.getFullPathName().toStdString() + "." : e.toStdString());
            retitle();
            if (done) done (e.isEmpty());
        });
    }

    void retitle()
    {
        shownModified = model.modified();
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
            w->setName (model.isOpen() ? "Limitless Drums \xe2\x80\x94 " + model.takeName() + (shownModified ? " (edited)" : "") : juce::String ("Limitless Drums"));
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
            exporting = true; cancelExport = false;
            const auto err = model.exportTo (dir, count, [this] (float p) { view.setExportProgress (std::min (p, 0.999f)); },
                [this]
                {
                    // between renders: let a click on Cancel (or Esc) through. While it writes, the view answers nothing else.
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
                    return cancelExport;
                });
            exporting = false;
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

    void useReference (const juce::File& f)
    {
        const auto err = model.loadReference (f);
        std::string msg = "Reference loaded: " + model.referenceName().toStdString() + " (" + std::to_string (model.referenceHits()) + (model.referenceHits() == 1 ? " hit" : " hits") + " measured).";
        if (const auto n = model.referenceCancelled().size(); n > 0)
            msg += " " + std::to_string (n) + (n == 1 ? " band" : " bands") + " left alone where your mics cancel each other: that's for Align to fix.";
        view.setStatus (err.isNotEmpty() ? err.toStdString() : msg);
    }

    void loadReference()
    {
        if (! model.mixRender()) { view.setStatus ("Run Auto Prep first."); return; }
        if (const char* ref = std::getenv ("LD_REFERENCE")) { useReference (juce::File (ref)); return; }
        chooser = std::make_unique<juce::FileChooser> ("A finished sample to move toward", juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            useReference (f);
        });
    }

private:
    SampleModel model;
    visage::ApplicationWindow root;
    HitsView view;
    std::unique_ptr<ui::VisageHost> host;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File pendingExport;
    std::unique_ptr<juce::PropertiesFile> prefs;
    bool exporting = false, cancelExport = false, shownModified = false;

    void chooseFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open the recordings of one take, or a project", juce::File(), "*.wav;*.aif;*.aiff;*.ldproj");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) { if (! fc.getResults().isEmpty()) open (fc.getResults()); });
    }

    void timerCallback() override
    {
        view.tick();
        if (model.modified() != shownModified) retitle();
        if (prefs && prefs->getBoolValue ("roomPhase", false) != model.roomPhaseDefault()) { prefs->setValue ("roomPhase", model.roomPhaseDefault()); prefs->saveIfNeeded(); }
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
    void systemRequestedQuit() override { if (auto* s = shell()) s->guard ([] { quit(); }); else quit(); }

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

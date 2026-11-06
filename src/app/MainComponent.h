#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theme.h"
#include "core/Import.h"
#include "core/Project.h"
#include "ui/Studio.h"
#include "ui/VisageHost.h"
#include "Audition.h"

namespace ld
{
/** Colours for the few native pieces left (menus, dialogs). */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setPalette (const theme::Palette&);
};

/** The main window. The whole interface is drawn on the GPU by ui::Studio; this component
    owns the session (edit, project file, autosave), the menus and native dialogs, and turns
    what you do in the interface into edits. */
class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    explicit MainComponent (tracktion::Engine&);
    ~MainComponent() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Opens files/folders as if dropped. A .ldproj opens as a project. */
    void importItems (const juce::Array<juce::File>&);

    // Menus
    juce::StringArray getMenuBarNames() override { return { "File", "Edit", "View" }; }
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int itemID, int) override;

    /** Called when the app wants to quit: offers to save unsaved work, then calls done(true) to quit. */
    void confirmClose (std::function<void (bool)> done);
    /** Offers to restore work left behind by a crash. */
    void offerRecovery();
    std::function<void (const juce::String&)> onTitleChanged;

    /** Tears the GPU view down before the window goes away. */
    void shutdownUi();

private:
    tracktion::Engine& engine;
    LookAndFeel lnf;
    bool dayMode = false;

    visage::ApplicationWindow root;
    ui::Studio studio;
    std::unique_ptr<ui::VisageHost> host;

    std::vector<ImportTake> takes;
    int currentTake = -1;
    std::unique_ptr<tracktion::Edit> edit;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File projectFile;
    juce::String sessionKey = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64());
    std::vector<MicRole> currentRoles;
    std::shared_ptr<struct EngineState> engineState;   ///< the ported Hitline engine's view of the open take
    int prepGeneration = 0;
    bool exporting = false;
    std::unique_ptr<Audition> audition;
    std::string playModeName = "hit";
    bool rudimentsOn = false;
    bool chainAB = false, loopOn = false, followOn = true, lmOn = true, padsOn = false, prepping = false;
    int lastSelected = -2;
    std::vector<modes::MidiNote> midiNotes;
    juce::String midiName;
    int autosaveTicks = 0;
    bool lastDirty = false;
    juce::String deviceText;

    theme::Palette palette() const { return dayMode ? theme::day() : theme::night(); }
    void openTake (int index);
    void startPrep();
    void refreshModel();
    void adoptEngineOrder();
    void exportTake();
    void exportOneShotFile();
    void playMode (const std::string& mode);
    void startPhrase (const std::string& mode);
    void stepHit (int delta);
    void updateHeader();
    void pushConsole();
    int engineIndexOf (int editTrack) const;
    void showEdit (std::unique_ptr<tracktion::Edit>, std::vector<MicRole> roles);
    void closeEdit();
    void openProject (const juce::File&);
    void chooseProject();
    void saveProject (bool saveAs, std::function<void (bool)> done = nullptr);
    void undo (bool redo);
    void jumpHistory (int row);
    void updateTitle();
    void updateHistory();
    void togglePlay();
    void chooseFiles();
    void applyTheme();
    void renameTrack (int index);
    void setNotice (const juce::String&);
    void pushTrackStates();
    std::vector<tracktion::AudioTrack*> audioTracks() const;
    bool handleKey (const visage::KeyEvent&);
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace ld

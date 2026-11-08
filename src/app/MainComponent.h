#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theme.h"
#include "core/Import.h"
#include "core/Project.h"
#include "ui/Studio.h"
#include "ui/NewSession.h"
#include "Library.h"
#include "Engine.h"
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
    ui::NewSession newSession;
    LibraryStore libraries;
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
    // the Align step
    int alignMic = -1, alignHit = -2, renderGeneration = 0;
    bool renderBusy = false, renderPending = false;   ///< one background render at a time; changes during it ask for one more
    bool alignSpread = true, alignSamples = false, alignDirty = true;
    void refreshAlign();
    void alignOp (std::function<void (hl::Session&)> op, const juce::String& what);
    void rerender();
    void onAlignAction (const std::string& action, int mic, double v);
    void onAlignMenu (const std::string& which, float x, float y, float w, float h);
    // the Mix step
    int mixSel = -1, mixHit = -2;
    hl::Chs refAudio; std::string refName;
    std::unique_ptr<juce::FileChooser> refChooser;
    void refreshMix();
    void refreshPlugin();
    int plugT = -2, plugK = 0;
    double songFrom = 0; bool songPlaying = false;   ///< Song mode playback: where it started   ///< the insert whose window is open (-2 none, -1 the bus)
    void onMixAction (const std::string& action, double v);
    void onSongAction (const std::string& action, double v);
public:
    /** Undo for the engine's settings (mix, plugins, Song mode): a deep copy of them, and putting one back. */
    juce::var settingsSnapshot() const;
    void restoreSettings (const juce::var&);
private:
    void recordChange (const juce::var& before, const juce::String& what);
    void onMixMenu (const std::string& which, float x, float y, float w, float h);
    // the Tune step
    int tuneHit = -2;
    void refreshTune();
    void onTuneAction (const std::string& action, double v);
    void onTuneMenu (const std::string& which, float x, float y, float w, float h);
    std::vector<modes::MidiNote> midiNotes;
    juce::String midiName;
    int autosaveTicks = 0;
    bool lastDirty = false;
    juce::String deviceText;

    theme::Palette palette() const { return dayMode ? theme::day() : theme::night(); }
    void openTake (int index);
    void startPrep();
    void loadTake();
    juce::String takeTitle() const;
    // New Session
    void openNewSession (const juce::Array<juce::File>& items = {});
    void dialogAddItems (const juce::Array<juce::File>& items);
    void dialogUseTake (int index);
    void createSession();
    void storeInLibrary();
    std::vector<ImportTake> dialogTakes;
    int dialogTake = -1;
    juce::String dialogStartId;            ///< the library session Start From copies (empty: the standard)
    TakeOptions takeOptions;               ///< what the open session was created with
    juce::String sessionName, sessionLibrary, librarySessionId;
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

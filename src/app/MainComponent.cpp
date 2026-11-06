#include "MainComponent.h"
#include "SessionModel.h"
#include <thread>

namespace ld
{
namespace te = tracktion;

//==============================================================================
void LookAndFeel::setPalette (const theme::Palette& p)
{
    setColour (juce::PopupMenu::backgroundColourId, p.panel);
    setColour (juce::PopupMenu::textColourId, p.ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.select.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, p.ink);
    setColour (juce::AlertWindow::backgroundColourId, p.panel);
    setColour (juce::AlertWindow::textColourId, p.ink);
    setColour (juce::TextEditor::backgroundColourId, p.raised);
    setColour (juce::TextEditor::textColourId, p.ink);
    setColour (juce::TextButton::buttonColourId, p.raised);
    setColour (juce::TextButton::textColourOffId, p.ink);
}

//==============================================================================
MainComponent::MainComponent (te::Engine& e) : engine (e)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    root.addChild (&studio);
    root.onResize() += [this] { studio.setBounds (0, 0, root.width(), root.height()); };
    host = std::make_unique<ui::VisageHost> (root);
    addAndMakeVisible (*host);

    studio.onImport = [this] { if (! edit) chooseFiles(); };
    studio.onPlay = [this] { togglePlay(); };
    studio.onStepClicked = [this] (ui::Step st) { studio.setStep (st); updateHeader(); };
    studio.onAutoPrep = [this] { if (edit && ! prepping) startPrep(); };
    studio.onUndo = [this] { undo (false); };
    studio.onRedo = [this] { undo (true); };
    studio.onPrevHit = [this] { stepHit (-1); };
    studio.onNextHit = [this] { stepHit (1); };
    studio.onAB = [this] (int i)
    {
        if (i == 2) chainAB = ! chainAB;
        else { chainAB = false; if (audition) audition->setProcessed (i == 1); }
        updateHeader();
    };
    studio.onMode = [this] (const std::string& m) { playMode (m); };
    studio.onToggle = [this] (const std::string& k)
    {
        if (k == "loop") loopOn = ! loopOn;
        else if (k == "follow") followOn = ! followOn;
        else if (k == "lm") lmOn = ! lmOn;
        else if (k == "pads") { padsOn = false; setNotice ("The pads come with the screen, next."); }
        updateHeader();
    };
    studio.onSelectTrack = [this] (int i) { if (audition) audition->setMonitorTrack (engineIndexOf (i)); updateHeader(); };
    studio.onRewind = [this] { if (edit) edit->getTransport().setPosition (te::TimePosition()); };
    studio.onRecord = [this] { setNotice ("Recording comes with Song mode."); };
    studio.onSeek = [this] (double t) { if (edit) edit->getTransport().setPosition (te::TimePosition::fromSeconds (t)); };
    studio.onThemeToggled = [this] { dayMode = ! dayMode; applyTheme(); };
    studio.onHistoryToggled = [this] { studio.setHistoryVisible (! studio.isHistoryVisible()); };
    studio.onHistoryJump = [this] (int row) { jumpHistory (row); };
    studio.onRename = [this] (int i) { renameTrack (i); };
    studio.onKey = [this] (const visage::KeyEvent& k) { return handleKey (k); };
    audition = std::make_unique<Audition> (engine.getDeviceManager().deviceManager);
    audition->onFinished = [this]
    {
        if (loopOn && playModeName != "hit" && playModeName != "mono") startPhrase (playModeName);
        updateHeader();
    };
    studio.onFilesDropped = [this] (const std::vector<std::string>& paths)
    {
        juce::Array<juce::File> items;
        for (auto& p : paths) items.add (juce::File (juce::String (p)));
        // Let the drop finish before any dialog opens.
        juce::MessageManager::callAsync ([this, items] { importItems (items); });
    };
    studio.onSolo = [this] (int i, bool on) { auto t = audioTracks(); if (i < (int) t.size()) { t[(size_t) i]->setSolo (on); pushTrackStates(); } };
    studio.onMute = [this] (int i, bool on) { auto t = audioTracks(); if (i < (int) t.size()) { t[(size_t) i]->setMute (on); pushTrackStates(); } };
    studio.onVolume = [this] (int i, float db)
    {
        auto t = audioTracks();
        if (i < (int) t.size()) if (auto* vp = t[(size_t) i]->getVolumePlugin()) { vp->setVolumeDb (db); pushTrackStates(); }
    };
    studio.onPan = [this] (int i, float pan)
    {
        auto t = audioTracks();
        if (i < (int) t.size()) if (auto* vp = t[(size_t) i]->getVolumePlugin()) { vp->setPan (pan); pushTrackStates(); }
    };
    studio.setStep (ui::Step::session);

    auto& dm = engine.getDeviceManager().deviceManager;
    if (auto* dev = dm.getCurrentAudioDevice())
        deviceText = dev->getName() + "  ·  " + juce::String (dev->getCurrentSampleRate() / 1000.0, 1) + " kHz";
    else
        deviceText = "No audio output";
    studio.setDeviceText ((deviceText + "  ·  Limitless Drums " + LD_VERSION_STRING).toStdString());

    setWantsKeyboardFocus (true);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
#endif
    applyTheme();
    startTimerHz (60);
    setSize (1600, 1000);
}

MainComponent::~MainComponent()
{
    stopTimer();
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
#endif
    shutdownUi();
    if (edit) edit->getTransport().stop (false, true);
    if (edit) edit->getUndoManager().removeChangeListener (this);
    edit = nullptr;
    setLookAndFeel (nullptr);
}

void MainComponent::shutdownUi()
{
    // Stop the GPU render loop before any frame it draws goes away.
    if (host) host->shutdown();
    root.removeChild (&studio);
    host = nullptr;
}

void MainComponent::applyTheme()
{
    lnf.setPalette (palette());
    studio.setDayMode (dayMode);
    sendLookAndFeelChange();
}

void MainComponent::setNotice (const juce::String& s) { studio.setNotice (s.toStdString()); }

std::vector<te::AudioTrack*> MainComponent::audioTracks() const
{
    if (! edit) return {};
    auto list = te::getAudioTracks (*edit);
    return { list.begin(), list.end() };
}

void MainComponent::pushTrackStates()
{
    auto tracks = audioTracks();
    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        auto* t = tracks[(size_t) i];
        auto* vp = t->getVolumePlugin();
        studio.setTrackState (i, t->isSolo (false), t->isMuted (false), vp ? vp->getVolumeDb() : 0.0f, vp ? vp->getPan() : 0.0f);
    }
}

//==============================================================================
void MainComponent::chooseFiles()
{
    chooser = std::make_unique<juce::FileChooser> ("Import drum tracks or a folder",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                   "*.wav;*.aif;*.aiff;*.bwf;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (! fc.getResults().isEmpty())
                                  importItems (fc.getResults());
                          });
}

void MainComponent::importItems (const juce::Array<juce::File>& items)
{
    for (auto& f : items)
        if (f.hasFileExtension (project::extension))
            return openProject (f);

    auto planned = planImport (items);
    if (planned.empty())
    {
        setNotice ("No WAV or AIFF files found there.");
        return;
    }

    takes = std::move (planned);
    currentTake = -1;
    if (takes.size() == 1)
        return openTake (0);

    // Several takes: pick one now. Which take a session is gets decided at import, not in the header.
    juce::PopupMenu m;
    m.addSectionHeader ("Open which take?");
    for (int i = 0; i < (int) takes.size(); ++i)
        m.addItem (i + 1, takes[(size_t) i].title + "  (" + juce::String (takes[(size_t) i].mics.size()) + " mics)");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (getScreenBounds().withSizeKeepingCentre (2, 2)),
                     [this] (int r) { if (r > 0) openTake (r - 1); });
}

void MainComponent::closeEdit()
{
    if (edit)
    {
        edit->getTransport().stop (false, true);
        edit->getUndoManager().removeChangeListener (this);
        project::autosaveFileFor (projectFile, sessionKey).deleteFile();
    }
    edit = nullptr;
    engineState = nullptr;
    ++prepGeneration;
    studio.setModel ({});
    studio.setHistory ({}, 0);
}

void MainComponent::showEdit (std::unique_ptr<te::Edit> newEdit, std::vector<MicRole> roles)
{
    edit = std::move (newEdit);
    currentRoles = std::move (roles);
    project::storeRoles (*edit, currentRoles);
    edit->resetChangedStatus();
    edit->getUndoManager().clearUndoHistory();
    edit->getUndoManager().addChangeListener (this);

    const auto title = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension()
                     : (currentTake >= 0 && currentTake < (int) takes.size() ? takes[(size_t) currentTake].title : juce::String ("Untitled"));
    studio.setModel (buildStudioModel (*edit, currentRoles, title, nullptr));
    studio.setStep (ui::Step::align);
    startPrep();
    updateHistory();
    grabKeyboardFocus();
    updateTitle();

    // The engine settles a few internal values just after loading; drop those from the
    // history so it starts clean and only lists your own edits.
    juce::Timer::callAfterDelay (500, [sp = juce::Component::SafePointer<MainComponent> (this), e = edit.get()]
    {
        if (sp == nullptr || sp->edit.get() != e) return;
        auto& um = e->getUndoManager();
        bool onlyUnnamed = true;
        for (auto& d : um.getUndoDescriptions()) if (d.isNotEmpty()) onlyUnnamed = false;
        if (onlyUnnamed) um.clearUndoHistory();
        e->resetChangedStatus();
        sp->updateTitle();
        sp->updateHistory();
    });
}

void MainComponent::refreshModel()
{
    if (! edit) return;
    const auto title = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension()
                     : (currentTake >= 0 && currentTake < (int) takes.size() ? takes[(size_t) currentTake].title : juce::String ("Untitled"));
    const auto view = studio.view;
    studio.setModel (buildStudioModel (*edit, currentRoles, title, engineState.get()));
    studio.view = view;
}

/** Hitline's Auto prep on the open take, off the message thread: hits, alignment and tuning first (shown as
    soon as they are ready), then the processing chain and the levels measured on it. */
void MainComponent::startPrep()
{
    if (! edit) return;
    const auto files = editFiles (*edit);
    if (files.empty() || std::any_of (files.begin(), files.end(), [] (auto& f) { return ! f.existsAsFile(); })) return;
    const int gen = ++prepGeneration;
    const auto folder = files[0].getParentDirectory().getFileName();
    setNotice (juce::CharPointer_UTF8 ("Finding the hits and lining up the mics\xe2\x80\xa6"));
    prepping = true;
    if (audition) audition->setEngine (nullptr);
    updateHeader();
    std::thread ([files, folder, gen, sp = juce::Component::SafePointer<MainComponent> (this)]
    {
        std::shared_ptr<EngineState> st = prepareTake (files, folder);
        juce::MessageManager::callAsync ([sp, st, gen]
        {
            if (sp == nullptr || sp->prepGeneration != gen) return;
            sp->engineState = st;
            sp->refreshModel();
            const auto& S = st->session;
            if (! st->prepared) sp->prepping = false;
            sp->updateHeader();
            if (! st->errors.empty()) sp->setNotice (juce::String (st->errors.front()));
            else if (! st->prepared) sp->setNotice (juce::String (st->advice.empty() ? "Couldn't find the hits." : st->advice));
            else
            {
                int moved = 0;
                for (auto& t : S.tracks) if (t.autoResult && ! t.autoResult->kept && (t.lag != 0 || t.pol < 0 || ! t.ap.empty())) ++moved;
                sp->setNotice (juce::String (S.onsets.size()) + " hits found, " + juce::String (moved) + " mic" + (moved == 1 ? "" : "s") + " realigned"
                               + (S.flipped ? ", " + juce::String (S.tracks[(size_t) S.refIdx].mic) + " polarity flipped" : juce::String())
                               + juce::String (juce::CharPointer_UTF8 (". Rendering the mix and setting the levels\xe2\x80\xa6")));
            }
        });
        if (! st->prepared && st->session.mode != "kit") return;
        auto mix = std::make_shared<hl::MixRender> (hl::renderMix (st->session));
        auto lv = std::make_shared<std::optional<hl::LevelResult>>();
        if (st->session.mode != "kit") *lv = hl::measureLevels (st->session, *mix);
        juce::MessageManager::callAsync ([sp, st, gen, mix, lv]
        {
            if (sp == nullptr || sp->prepGeneration != gen || sp->engineState != st) return;
            st->mix = std::move (*mix);
            st->levels = std::move (*lv);
            sp->prepping = false;
            if (sp->audition) sp->audition->setEngine (st);
            sp->refreshModel();
            sp->updateHeader();
            sp->setNotice ("Ready. Every hit is on the level standard. File > Export writes the samples.");
        });
    }).detach();
}

void MainComponent::exportTake()
{
    if (! engineState || ! engineState->mix || exporting) { setNotice ("Still preparing the take."); return; }
    const bool kit = engineState->session.mode == "kit";
    if (! kit && ! (engineState->levels && engineState->levels->valid)) { setNotice ("Export needs every hit found: the levels need the full count."); return; }
    chooser = std::make_unique<juce::FileChooser> ("Export to a folder", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, kit] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (! dir.isDirectory()) return;
        exporting = true;
        setNotice (juce::CharPointer_UTF8 ("Exporting\xe2\x80\xa6"));
        std::thread ([st = engineState, dir, kit, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            int n = 0; juce::String err;
            try
            {
                auto sink = [&] (const std::string& path, const juce::MemoryBlock& bytes)
                {
                    const auto f = dir.getChildFile (juce::String (path));
                    f.getParentDirectory().createDirectory();
                    if (! f.replaceWithData (bytes.getData(), bytes.getSize())) throw std::runtime_error ("Couldn't write " + f.getFullPathName().toStdString());
                };
                n = kit ? hl::writeKit (st->session, *st->mix, sink) : hl::writeSession (st->session, *st->mix, *st->levels, sink);
            }
            catch (const std::exception& e) { err = e.what(); }
            juce::MessageManager::callAsync ([sp, n, err, dir]
            {
                if (sp == nullptr) return;
                sp->exporting = false;
                sp->setNotice (err.isNotEmpty() ? "Export failed: " + err : "Exported " + juce::String (n) + " files to " + dir.getFileName() + ".");
                if (err.isEmpty()) dir.revealToUser();
            });
        }).detach();
    });
}

void MainComponent::exportOneShotFile()
{
    if (! engineState || ! engineState->mix || ! engineState->levels) { setNotice ("Still preparing the take."); return; }
    // the console as it stands in the edit: faders, mute, solo, pan
    auto& S = engineState->session;
    const auto files = editFiles (*edit);
    auto tracks = tracktion::getAudioTracks (*edit);
    for (size_t i = 0; i < tracks.size(); ++i)
        for (auto& tr : S.tracks)
            if (tr.name == files[i].getFileNameWithoutExtension().toStdString())
            {
                tr.mute = tracks[i]->isMuted (false); tr.solo = tracks[i]->isSolo (false);
                if (auto* vp = tracks[i]->getVolumePlugin()) { tr.fader = vp->getVolumeDb(); tr.strip.pan = vp->getPan(); }
            }
    const auto shot = hl::exportOneShot (S, *engineState->mix, *engineState->levels);
    if (shot.name.empty()) { setNotice ("The bus is silent: every channel is muted or down."); return; }
    chooser = std::make_unique<juce::FileChooser> ("Save the one-shot", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::String (shot.name)), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, wav = shot.wav, hit = shot.hit] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        setNotice (f.replaceWithData (wav.getData(), wav.getSize()) ? "Saved the one-shot (hit " + juce::String ((int) hit + 1) + ") as " + f.getFileName() + "."
                                                                       : "Couldn't write " + f.getFullPathName());
    });
}

void MainComponent::openTake (int index)
{
    if (index < 0 || index >= (int) takes.size())
        return;

    closeEdit();
    projectFile = juce::File();
    sessionKey = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64());

    auto& take = takes[(size_t) index];
    auto newEdit = createSessionEdit (engine, take.mics, 120.0, te::Edit::EditRole::forEditing);
    currentTake = index;

    if (newEdit == nullptr)
    {
        setNotice ("Couldn't open " + take.title + ".");
        return;
    }

    juce::String notice = take.warnings.joinIntoString ("  ");
    if (! take.references.empty())
        notice = (notice.isNotEmpty() ? notice + "  " : juce::String())
                 + juce::String ((int) take.references.size()) + " one-shot reference found (used later for reference matching).";
    setNotice (notice);
    showEdit (std::move (newEdit), take.roles);
}

void MainComponent::openProject (const juce::File& f)
{
    std::vector<MicRole> roles;
    juce::StringArray missing;
    auto loaded = project::open (engine, f, roles, missing);
    if (loaded == nullptr)
    {
        setNotice ("Couldn't open " + f.getFileName() + ".");
        return;
    }

    closeEdit();
    takes.clear();
    projectFile = f;
    setNotice (missing.isEmpty() ? juce::String()
                                 : juce::String (missing.size()) + " audio file(s) are missing: " + missing.joinIntoString (", "));
    showEdit (std::move (loaded), roles);
}

void MainComponent::chooseProject()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a Limitless Drums project",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   juce::String ("*") + project::extension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) openProject (fc.getResult()); });
}

void MainComponent::saveProject (bool saveAs, std::function<void (bool)> done)
{
    if (! edit) { if (done) done (true); return; }

    auto write = [this, done] (juce::File f)
    {
        f = f.withFileExtension (project::extension);
        project::storeRoles (*edit, currentRoles);
        const auto oldAutosave = project::autosaveFileFor (projectFile, sessionKey);
        const bool ok = project::save (*edit, f);
        if (ok)
        {
            oldAutosave.deleteFile();
            projectFile = f;
            setNotice ("Saved " + f.getFileName());
        }
        else
            setNotice ("Couldn't save to " + f.getFullPathName());
        updateTitle();
        if (done) done (ok);
    };

    if (! saveAs && projectFile != juce::File())
        return write (projectFile);

    auto suggested = projectFile != juce::File() ? projectFile
                   : (currentTake >= 0 && currentTake < (int) takes.size()
                          ? takes[(size_t) currentTake].folder.getSiblingFile (takes[(size_t) currentTake].folder.getFileName() + project::extension)
                          : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::String ("Untitled") + project::extension));

    chooser = std::make_unique<juce::FileChooser> ("Save project", suggested, juce::String ("*") + project::extension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [write, done] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() != juce::File()) write (fc.getResult());
                              else if (done) done (false);
                          });
}

void MainComponent::undo (bool redo)
{
    if (! edit) return;
    auto& um = edit->getUndoManager();
    if (redo) um.redo(); else um.undo();
}

void MainComponent::jumpHistory (int row)
{
    if (! edit) return;
    auto& um = edit->getUndoManager();
    int delta = row - um.getUndoDescriptions().size();
    while (delta < 0 && um.undo()) ++delta;
    while (delta > 0 && um.redo()) --delta;
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateHistory();
    // Undo can rename tracks; keep the strips in step.
    if (edit)
    {
        auto tracks = audioTracks();
        auto m = studio.model();
        bool renamed = false;
        for (size_t i = 0; i < tracks.size() && i < m.tracks.size(); ++i)
            if (m.tracks[i].name != tracks[i]->getName().toStdString()) { m.tracks[i].name = tracks[i]->getName().toStdString(); renamed = true; }
        if (renamed)
        {
            const auto hit = studio.selectedHit;
            const auto step = studio.step();
            const auto view = studio.view;
            studio.setModel (std::move (m));
            studio.selectedHit = hit;
            studio.view = view;
            studio.setStep (step);
        }
    }
}

void MainComponent::updateHistory()
{
    if (! edit) return studio.setHistory ({}, 0);
    auto& um = edit->getUndoManager();
    std::vector<std::string> rows { "Opened session" };
    auto past = um.getUndoDescriptions();
    for (int i = past.size(); --i >= 0;) rows.push_back ((past[i].isNotEmpty() ? past[i] : juce::String ("Edit")).toStdString());
    for (auto& f : um.getRedoDescriptions()) rows.push_back ((f.isNotEmpty() ? f : juce::String ("Edit")).toStdString());
    studio.setHistory (std::move (rows), past.size());
}

void MainComponent::renameTrack (int index)
{
    auto tracks = audioTracks();
    if (index < 0 || index >= (int) tracks.size()) return;
    auto* track = tracks[(size_t) index];

    auto* w = new juce::AlertWindow ("Rename track", {}, juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", track->getName());
    w->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, index] (int r)
    {
        const auto newName = w->getTextEditorContents ("name").trim();
        auto t = audioTracks();
        if (r == 1 && newName.isNotEmpty() && index < (int) t.size() && newName != t[(size_t) index]->getName())
        {
            edit->getUndoManager().beginNewTransaction ("Rename " + t[(size_t) index]->getName() + " to " + newName);
            t[(size_t) index]->setName (newName);
            changeListenerCallback (nullptr);
        }
    }), true);
}

void MainComponent::updateTitle()
{
    juce::String t = "Limitless Drums";
    if (edit)
    {
        const auto name = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension()
                        : (currentTake >= 0 && currentTake < (int) takes.size() ? takes[(size_t) currentTake].title : juce::String ("Untitled"));
        t = name + (edit->hasChangedSinceSaved() || projectFile == juce::File() ? " •" : "") + "  —  Limitless Drums";
    }
    if (onTitleChanged) onTitleChanged (t);
}

void MainComponent::confirmClose (std::function<void (bool)> done)
{
    if (! edit || (projectFile != juce::File() && ! edit->hasChangedSinceSaved()))
    {
        closeEdit();
        return done (true);
    }

    auto options = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Save changes?")
                       .withMessage ("Do you want to save this session before quitting?")
                       .withButton ("Save", 1).withButton ("Don't Save", 2).withButton ("Cancel", 0);
    juce::AlertWindow::showAsync (options, [this, done] (int result)
    {
        if (result == 1) saveProject (false, [this, done] (bool ok) { if (ok) { closeEdit(); done (true); } else done (false); });
        else if (result == 2) { closeEdit(); done (true); }
        else done (false);
    });
}

void MainComponent::offerRecovery()
{
    auto files = project::findRecoverableAutosaves();
    if (files.isEmpty()) return;

    auto newest = files.getFirst();
    for (auto& f : files) if (f.getLastModificationTime() > newest.getLastModificationTime()) newest = f;

    auto options = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                       .withTitle ("Restore unsaved work?")
                       .withMessage ("Limitless Drums didn't close properly last time. Restore the session autosaved "
                                     + newest.getLastModificationTime().toString (true, true, false) + "?")
                       .withButton ("Restore", 1).withButton ("Discard", 0);
    juce::AlertWindow::showAsync (options, [this, newest, files] (int result)
    {
        if (result == 1)
        {
            juce::File original;
            if (auto xml = juce::parseXML (newest)) original = juce::File (xml->getStringAttribute ("ldProjectFile"));
            openProject (newest);
            projectFile = original.existsAsFile() ? original : juce::File();
            if (edit) edit->markAsChanged();
            setNotice ("Restored unsaved work. Save to keep it.");
            updateTitle();
        }
        for (auto& f : files) if (f != newest || result != 1) f.deleteFile();
    });
}

//==============================================================================
enum MenuIDs { miOpen = 1, miImport, miSave, miSaveAs, miUndo, miRedo, miHistory, miTheme, miFit, miExport, miOneShot };

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    const bool hasEdit = edit != nullptr;
    if (index == 0)
    {
        m.addItem (miOpen, juce::String (juce::CharPointer_UTF8 ("Open Project\xe2\x80\xa6")));
        m.addItem (miImport, juce::String (juce::CharPointer_UTF8 ("Import Tracks\xe2\x80\xa6")));
        m.addSeparator();
        m.addItem (miSave, "Save", hasEdit);
        m.addItem (miSaveAs, juce::String (juce::CharPointer_UTF8 ("Save As\xe2\x80\xa6")), hasEdit);
        m.addSeparator();
        const bool ready = hasEdit && engineState && engineState->mix && ! exporting;
        m.addItem (miExport, juce::String (juce::CharPointer_UTF8 ("Export\xe2\x80\xa6")), ready);
        m.addItem (miOneShot, juce::String (juce::CharPointer_UTF8 ("Export One-Shot\xe2\x80\xa6")), ready && engineState->levels.has_value());
    }
    else if (index == 1)
    {
        const bool canUndo = hasEdit && edit->getUndoManager().canUndo();
        const bool canRedo = hasEdit && edit->getUndoManager().canRedo();
        m.addItem (miUndo, canUndo ? "Undo " + edit->getUndoManager().getUndoDescription() + "" : juce::String ("Undo"), canUndo);
        m.addItem (miRedo, canRedo ? "Redo " + edit->getUndoManager().getRedoDescription() + "" : juce::String ("Redo"), canRedo);
    }
    else
    {
        m.addItem (miHistory, "Show History", true, studio.isHistoryVisible());
        m.addItem (miTheme, dayMode ? "Night Mode" : "Day Mode");
        m.addItem (miFit, "Zoom to Fit", hasEdit);
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    switch (id)
    {
        case miOpen:    chooseProject(); break;
        case miImport:  chooseFiles(); break;
        case miSave:    saveProject (false); break;
        case miSaveAs:  saveProject (true); break;
        case miUndo:    undo (false); break;
        case miRedo:    undo (true); break;
        case miHistory: studio.setHistoryVisible (! studio.isHistoryVisible()); break;
        case miTheme:   dayMode = ! dayMode; applyTheme(); break;
        case miFit:     studio.zoomToFit(); break;
        case miExport:  exportTake(); break;
        case miOneShot: exportOneShotFile(); break;
        default: break;
    }
}

void MainComponent::togglePlay()
{
    // Space / Play: the lit play mode (Hitline's playSel); again stops
    if (audition && audition->isPlaying()) { audition->stop(); updateHeader(); return; }
    playMode (playModeName);
}

int MainComponent::engineIndexOf (int editTrack) const
{
    if (! edit || ! engineState) return 0;
    const auto files = editFiles (*edit);
    if (editTrack < 0 || editTrack >= (int) files.size()) return engineState->session.refIdx;
    for (size_t t = 0; t < engineState->session.tracks.size(); ++t)
        if (engineState->session.tracks[t].name == files[(size_t) editTrack].getFileNameWithoutExtension().toStdString()) return (int) t;
    return engineState->session.refIdx;
}

void MainComponent::playMode (const std::string& m)
{
    if (! audition) return;
    if (audition->isPlaying() && audition->mode() == m) { audition->stop(); updateHeader(); return; }
    playModeName = m;
    startPhrase (m);
    updateHeader();
}

void MainComponent::startPhrase (const std::string& m)
{
    if (! engineState || ! engineState->mix) { setNotice ("Still preparing the take."); return; }
    const auto& S = engineState->session;
    const int sel = std::max (0, studio.selectedHit);
    if (m != "hit" && m != "mono" && ! hl::countsOk (S)) { setNotice ("That needs the full hit count. Hit and Mono work now."); return; }
    if (edit) edit->getTransport().stop (false, false);
    pushConsole();
    std::vector<PlayEvent> ev;
    Audition::Options o;
    o.mode = m; o.chain = chainAB && m != "hit" && m != "mono"; o.loudnessMatch = lmOn;
    if (m == "hit") ev = modes::hit (S, sel);
    else if (m == "layer") ev = modes::layer (S, sel);
    else if (m == "sweep") ev = modes::sweep (S, sel);
    else if (m == "groove") ev = modes::groove (S);
    else if (m == "mono") { ev = modes::hit (S, sel); o.mono = true; }
    else if (m == "midi")
    {
        if (midiNotes.empty())
        {
            chooser = std::make_unique<juce::FileChooser> ("Play a MIDI file on this drum", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                juce::MemoryBlock mb; f.loadFileAsData (mb);
                juce::String err;
                auto notes = modes::parseMidi (mb, err);
                if (notes.empty()) { setNotice (err); return; }
                midiNotes = std::move (notes); midiName = f.getFileName();
                startPhrase ("midi");
                updateHeader();
            });
            return;
        }
        ev = modes::midi (S, midiNotes);
    }
    if (ev.empty()) return;
    audition->play (std::move (ev), o);
}

void MainComponent::stepHit (int delta)
{
    const int n = (int) studio.model().hits.size();
    if (n == 0) return;
    studio.selectedHit = std::clamp (studio.selectedHit + delta, 0, n - 1);
    studio.redrawAll();
}

void MainComponent::pushConsole()
{
    if (! audition || ! edit || ! engineState) return;
    const auto& S = engineState->session;
    std::vector<float> g (S.tracks.size(), 1.0f), pan (S.tracks.size(), 0.0f);
    std::vector<bool> st (S.tracks.size(), false);
    auto tracks = audioTracks();
    bool anySolo = false;
    for (auto* t : tracks) anySolo = anySolo || t->isSolo (false);
    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const int e = engineIndexOf (i);
        if (e < 0 || e >= (int) S.tracks.size()) continue;
        auto* t = tracks[(size_t) i];
        const bool audible = anySolo ? t->isSolo (false) : ! t->isMuted (false);
        float db = 0, p = 0;
        if (auto* vp = t->getVolumePlugin()) { db = vp->getVolumeDb(); p = vp->getPan(); }
        g[(size_t) e] = audible ? (db <= -60 ? 0.0f : std::pow (10.0f, db / 20.0f)) : 0.0f;
        pan[(size_t) e] = p;
        st[(size_t) e] = S.tracks[(size_t) e].ch.size() > 1;
    }
    const float room = hl::roomLive (S) ? std::pow (10.0f, (float) S.room.ret / 20.0f) : 0.0f;
    audition->setConsole (g, pan, st, 1.0f, room);
}

void MainComponent::updateHeader()
{
    ui::HeaderState h = studio.headerState();
    const auto* es = engineState.get();
    const auto& m = studio.model();
    for (int i = 0; i < 7; ++i) { h.status[i].clear(); h.done[i] = false; }
    h.prepping = prepping;
    if (es && ! es->session.tracks.empty())
    {
        const auto& S = es->session;
        const auto di = juce::String (S.drum).substring (0, 1).toUpperCase() + juce::String (S.drum).substring (1);
        h.sessionLine = (di + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + juce::String (S.artic)).toStdString();
        h.detailLine = std::to_string (S.tracks.size()) + " mics \xc2\xb7 " + std::to_string (S.det.V) + " vel \xc3\x97 " + std::to_string (S.det.RR) + " RR \xc2\xb7 "
                     + juce::String (S.sr / 1000.0, 1).toStdString() + " kHz";
        const bool ok = hl::countsOk (S);
        h.status[0] = std::to_string (S.tracks.size()) + " MICS"; h.done[0] = true;
        h.status[1] = std::to_string (S.onsets.size()) + (ok ? " HITS" : " FOUND"); h.done[1] = ok;
        int moved = 0;
        for (auto& t : S.tracks) if (t.autoResult && ! t.autoResult->kept && (t.lag != 0 || t.pol < 0 || ! t.ap.empty())) ++moved;
        h.status[2] = S.phaseAnalyzed ? std::to_string (moved) + " LINED UP" : "NOT YET"; h.done[2] = S.phaseAnalyzed;
        h.status[3] = S.tuneInfo ? hl::noteName (S.tuneInfo->nearest) + (S.tune.on ? " \xc2\xb7 ON" : " \xc2\xb7 OFF") : "NO PITCH"; h.done[3] = S.tuneInfo.has_value();
        h.status[4] = hl::fxOn (S) || hl::busLive (S) || hl::roomLive (S) ? "ON" : "FLAT";
        const bool lv = es->levels && es->levels->valid;
        h.status[5] = lv ? "ON STANDARD" : (es->mix ? "NEEDS HITS" : "MEASURING"); h.done[5] = lv;
        h.exportReady = es->mix && (lv || S.mode == "kit");
        static const EngineState* countedFor = nullptr; static size_t files = 0;
        if (countedFor != es || ! h.exportReady) { files = hl::exportList (S).size(); countedFor = h.exportReady ? es : nullptr; }
        h.status[6] = std::to_string (files) + " FILES";
        h.velocities = S.det.V;
        h.hits = (int) S.onsets.size();
        const int sel = audition && audition->isPlaying() && audition->currentHit() >= 0 ? audition->currentHit() : studio.selectedHit;
        h.hit = sel;
        h.vel = sel >= 0 && ok ? S.velOfHit ((size_t) sel) : -1;
        h.rr = sel >= 0 && ok ? sel % S.det.RR : -1;
        if (h.vel >= 0) { h.midiLo = (int) std::floor (h.vel * 127.0 / S.det.V) + 1; h.midiHi = (int) std::floor ((h.vel + 1) * 127.0 / S.det.V); } else h.midiLo = h.midiHi = 0;
        h.hitWave.clear();
        if (sel >= 0 && sel < (int) m.hits.size() && m.referenceTrack < (int) m.tracks.size())
        {
            const auto& tr = m.tracks[(size_t) m.referenceTrack];
            const int b0 = (int) (m.hits[(size_t) sel].time * tr.blocksPerSecond), nb = (int) (0.6 * tr.blocksPerSecond);
            float pk = 1e-6f;
            for (int q = 0; q < nb && b0 + q < (int) tr.peaks.size(); ++q) pk = std::max (pk, tr.peaks[(size_t) (b0 + q)]);
            for (int q = 0; q < 48; ++q)
            {
                float v = 0;
                for (int r = q * nb / 48; r < (q + 1) * nb / 48 && b0 + r < (int) tr.peaks.size(); ++r) v = std::max (v, tr.peaks[(size_t) (b0 + r)]);
                h.hitWave.push_back (v / pk);
            }
        }
        const int mon = audition ? engineIndexOf (studio.selectedTrack) : S.refIdx;
        h.monitorMic = juce::String (S.tracks[(size_t) std::clamp (mon, 0, (int) S.tracks.size() - 1)].mic).toUpperCase().toStdString();
        if (audition) audition->setMonitorTrack (mon);
    }
    h.processed = audition ? audition->processed() : true;
    h.chain = chainAB;
    h.playing = audition && audition->isPlaying();
    h.mode = playModeName;
    h.loop = loopOn; h.follow = followOn; h.lm = lmOn; h.pads = padsOn;
    h.canUndo = edit && edit->getUndoManager().canUndo();
    h.canRedo = edit && edit->getUndoManager().canRedo();
    h.historyCount = edit ? edit->getUndoManager().getNumActionsInCurrentTransaction() + (int) edit->getUndoManager().getUndoDescriptions().size() : 0;
    studio.setHeader (std::move (h));
}
bool MainComponent::handleKey (const visage::KeyEvent& k)
{
    using KC = visage::KeyCode;
    const auto code = k.keyCode();
    const double pos = edit ? edit->getTransport().getPosition().inSeconds() : 0.0;
    if (k.isMainModifier())
    {
        if (code == KC::O) { chooseProject(); return true; }
        if (code == KC::I) { chooseFiles(); return true; }
        if (code == KC::S) { saveProject (k.isShiftDown()); return true; }
        if (code == KC::Z) { undo (k.isShiftDown()); return true; }
        if (code == KC::E) { exportTake(); return true; }
        return false;
    }
    if (code == KC::Space)  { togglePlay(); return true; }
    // Hitline's keys: play modes, loop, A/B, previous and next hit, Auto Prep
    if (code == KC::H) { playMode ("hit"); return true; }
    if (code == KC::L) { playMode ("layer"); return true; }
    if (code == KC::V) { playMode ("sweep"); return true; }
    if (code == KC::G) { playMode ("groove"); return true; }
    if (code == KC::M) { playMode ("mono"); return true; }
    if (code == KC::O) { loopOn = ! loopOn; updateHeader(); return true; }
    if (code == KC::A) { if (edit && ! prepping) startPrep(); return true; }
    if (code == KC::B)
    {
        if (k.isShiftDown()) chainAB = ! chainAB;
        else if (audition) { chainAB = false; audition->setProcessed (! audition->processed()); }
        updateHeader();
        return true;
    }
    if (code == KC::Left)  { stepHit (-1); return true; }
    if (code == KC::Right) { stepHit (1); return true; }
    if (code == KC::Return && edit) { edit->getTransport().setPosition (te::TimePosition()); return true; }
    if (code == KC::Equals) { studio.zoomBy (1.5, pos); return true; }
    if (code == KC::Minus)  { studio.zoomBy (1.0 / 1.5, pos); return true; }
    if (code == KC::F)      { studio.zoomToFit(); return true; }
    return false;
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    // Keys reach here only when the GPU view doesn't have focus; route them the same way.
    const auto mods = k.getModifiers();
    int vm = 0;
    if (mods.isCommandDown()) vm |= visage::kModifierCmd;
    if (mods.isShiftDown()) vm |= visage::kModifierShift;
    auto ch = (int) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode());
    if (k == juce::KeyPress::spaceKey) ch = ' ';
    if (k.getKeyCode() == juce::KeyPress::leftKey) { stepHit (-1); return true; }
    if (k.getKeyCode() == juce::KeyPress::rightKey) { stepHit (1); return true; }
    if (k == juce::KeyPress::returnKey) ch = '\n';
    if (k.getTextCharacter() == '=' || k.getTextCharacter() == '-') ch = (int) k.getTextCharacter();
    return handleKey (visage::KeyEvent ((visage::KeyCode) ch, vm, true));
}

void MainComponent::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (edit)
    {
        auto& t = edit->getTransport();
        studio.setTransport (t.getPosition().inSeconds(), t.isPlaying());

        const bool dirty = edit->hasChangedSinceSaved();
        if (dirty != lastDirty) { lastDirty = dirty; updateTitle(); }
        if (++autosaveTicks >= 60 * 30)   // every 30 s
        {
            autosaveTicks = 0;
            project::autosave (*edit, project::autosaveFileFor (projectFile, sessionKey));
        }
    }
    if (audition)
    {
        auto& h = studio.headerState();
        h.meterL = audition->peakL(); h.meterR = audition->peakR(); h.phase = audition->correlation();
        Audition::MonitorPoint pts[512];
        const int n = audition->drainMonitor (pts, 512);
        if (n > 0)
        {
            float lv[512]; int vv[512];
            for (int q = 0; q < n; ++q) { lv[q] = pts[q].level; vv[q] = pts[q].vel; }
            studio.pushMonitor (lv, vv, n);
        }
        const bool playingNow = audition->isPlaying();
        if (playingNow && audition->currentHit() >= 0 && audition->currentHit() != studio.selectedHit) { studio.selectedHit = audition->currentHit(); lastSelected = studio.selectedHit; studio.redrawAll(); }
        if (studio.selectedHit != lastSelected)
        {
            lastSelected = studio.selectedHit;
            if (followOn && ! playingNow && engineState && engineState->mix) startPhrase ("hit");
        }
        if (playingNow != h.playing || playingNow || (++autosaveTicks % 15) == 0) { if (playingNow) pushConsole(); updateHeader(); }
    }
    studio.tick (now);
}

//==============================================================================
void MainComponent::resized()
{
    if (host) host->setBounds (getLocalBounds());
}

} // namespace ld

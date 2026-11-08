#include "MainComponent.h"
#include "SessionModel.h"
#include "hl/Detect.h"
#include "hl/FxSat.h"
#include "hl/FxInserts.h"
#include "hl/Song.h"
#include <set>
#if defined (__GLIBC__)
 #include <malloc.h>
#endif
#include "BinaryData.h"
#include <thread>

/** Hands memory the renders freed back to the system. glibc keeps freed blocks in each thread's arena, and the
    renders run on worker threads, so without this a session's footprint only ever grows. */
static void releaseFreedMemory()
{
   #if defined (__GLIBC__)
    malloc_trim (0);
   #endif
}

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
    // the measured reverb models and the pro-library reference the engine draws on
    hl::setReverbLibrary (juce::JSON::parse (juce::String::fromUTF8 (BinaryData::irlib_json, BinaryData::irlib_jsonSize)));
    hl::setLibraryReference (juce::JSON::parse (juce::String::fromUTF8 (BinaryData::libref_json, BinaryData::libref_jsonSize)));
    setLookAndFeel (&lnf);
    setOpaque (true);

    root.addChild (&studio);
    root.addChild (&newSession);
    root.onResize() += [this]
    {
        studio.setBounds (0, 0, root.width(), root.height());
        newSession.setBounds (0, 0, root.width(), root.height());
    };
    host = std::make_unique<ui::VisageHost> (root);
    addAndMakeVisible (*host);

    studio.onImport = [this] { openNewSession(); };
    studio.onPlay = [this] { togglePlay(); };
    studio.onStepClicked = [this] (ui::Step st) { studio.setStep (st); if (st == ui::Step::align) refreshAlign(); if (st == ui::Step::tune) refreshTune(); if (st == ui::Step::mix) refreshMix(); updateHeader(); };
    studio.onAlign = [this] (const std::string& a, int mic, double v) { onAlignAction (a, mic, v); };
    studio.onAlignMenu = [this] (const std::string& w, float x, float y, float ww, float h) { onAlignMenu (w, x, y, ww, h); };
    studio.onTune = [this] (const std::string& a, double v) { onTuneAction (a, v); };
    studio.onMix = [this] (const std::string& a, double v) { onMixAction (a, v); };
    studio.onSong = [this] (const std::string& a, double v) { onSongAction (a, v); };
    studio.onMixMenu = [this] (const std::string& w, float x, float y, float ww, float h) { onMixMenu (w, x, y, ww, h); };
    studio.onTuneMenu = [this] (const std::string& w, float x, float y, float ww, float h) { onTuneMenu (w, x, y, ww, h); };
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
        else if (k == "rudiments") { rudimentsOn = ! rudimentsOn; playModeName = "groove"; }
        else if (k == "pads") { padsOn = false; setNotice ("The pads come with the screen, next."); }
        updateHeader();
    };
    studio.onMoveTrack = [this] (int from, int to)
    {
        auto tracks = audioTracks();
        if (! edit || from < 0 || from >= (int) tracks.size() || to < 0 || to >= (int) tracks.size()) return;
        // preceding track after the move: the one that will sit just above `to`
        te::Track* preceding = nullptr;
        if (to > 0) preceding = tracks[(size_t) (to > from ? to : to - 1)];
        edit->moveTrack (tracks[(size_t) from], te::TrackInsertPoint (nullptr, preceding));
        if (from < (int) currentRoles.size() && to < (int) currentRoles.size())
        {
            auto r = currentRoles[(size_t) from];
            currentRoles.erase (currentRoles.begin() + from);
            currentRoles.insert (currentRoles.begin() + to, r);
        }
        studio.selectedTrack = to;
        refreshModel();
        updateHeader();
    };
    studio.onSelectTrack = [this] (int i)
    {
        if (audition) audition->setMonitorTrack (engineIndexOf (i));
        if (studio.step() == ui::Step::align && engineState && engineIndexOf (i) != engineState->session.refIdx) { alignMic = engineIndexOf (i); refreshAlign(); }
        updateHeader();
    };
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
    studio.onPad = [this] (int v)
    {
        // a random round robin of that velocity (Hitline's pads)
        const auto& hits = studio.model().hits;
        std::vector<int> pick;
        for (int h = 0; h < (int) hits.size(); ++h) if (hits[(size_t) h].layer == v) pick.push_back (h);
        if (pick.empty()) return;
        studio.selectedHit = pick[(size_t) juce::Random::getSystemRandom().nextInt ((int) pick.size())];
        studio.flashPad (v);
        studio.redrawAll();
        playModeName = "hit";
        startPhrase ("hit");
        updateHeader();
    };
    studio.onPanLR = [this] (int i, int side, float v)
    {
        auto t = audioTracks();
        if (i >= (int) t.size() || ! edit) return;
        t[(size_t) i]->state.setProperty (side ? "ldPanR" : "ldPanL", juce::jlimit (-1.0f, 1.0f, v), &edit->getUndoManager());
        pushTrackStates();
        pushConsole();
    };
    studio.onPan = [this] (int i, float pan)
    {
        auto t = audioTracks();
        if (i < (int) t.size()) if (auto* vp = t[(size_t) i]->getVolumePlugin()) { vp->setPan (pan); pushTrackStates(); }
    };
    newSession.onCancel = [this] { newSession.close(); grabKeyboardFocus(); };
    newSession.onCreate = [this] { createSession(); };
    newSession.onBrowse = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Add the take: a folder of mics", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                       "*.wav;*.aif;*.aiff;*.bwf;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) { if (! fc.getResults().isEmpty()) dialogAddItems (fc.getResults()); });
    };
    newSession.onFilesDropped = [this] (const std::vector<std::string>& paths)
    {
        juce::Array<juce::File> items;
        for (auto& p : paths) items.add (juce::File (juce::String (p)));
        juce::MessageManager::callAsync ([this, items] { dialogAddItems (items); });
    };
    auto menuAt = [this] (float x, float y, float w, float h)
    {
        return juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (x, y, w, h).toNearestInt()))
                                         .withMinimumWidth ((int) w);
    };
    newSession.onPickLibrary = [this, menuAt] (float x, float y, float w, float h)
    {
        juce::PopupMenu m;
        for (int i = 0; i < (int) libraries.libraries.size(); ++i)
        {
            const auto& L = libraries.libraries[(size_t) i];
            m.addItem (i + 1, L.name + "  (" + juce::String ((int) L.sessions.size()) + ")", true, L.name == juce::String (newSession.state().library));
        }
        m.addSeparator();
        m.addItem (1000, juce::String (juce::CharPointer_UTF8 ("New Library\xe2\x80\xa6")));
        m.showMenuAsync (menuAt (x, y, w, h), [this] (int r)
        {
            if (r == 1000) { if (newSession.onNewLibrary) newSession.onNewLibrary(); return; }
            if (r < 1 || r > (int) libraries.libraries.size()) return;
            const auto& L = libraries.libraries[(size_t) r - 1];
            newSession.state().library = L.name.toStdString();
            newSession.state().libraryColour = L.colour;
            newSession.refresh();
        });
    };
    newSession.onPickStart = [this, menuAt] (float x, float y, float w, float h)
    {
        juce::PopupMenu m;
        m.addItem (1, "The library's standard", true, dialogStartId.isEmpty());
        auto* lib = libraries.find (juce::String (newSession.state().library));
        std::vector<const LibrarySession*> mine, other;
        for (auto& L : libraries.libraries)
            for (auto& e : L.sessions)
                if (e.settings.isObject()) (lib == &L ? mine : other).push_back (&e);
        auto newest = [] (auto* a, auto* b) { return a->modified > b->modified; };
        std::sort (mine.begin(), mine.end(), newest);
        std::sort (other.begin(), other.end(), newest);
        std::vector<const LibrarySession*> all;
        auto line = [] (const LibrarySession* e) { return e->name + "  \xc2\xb7  " + e->artic; };
        if (! mine.empty()) m.addSectionHeader ("Copy the levels and tone of");
        for (auto* e : mine) { all.push_back (e); m.addItem ((int) all.size() + 1, juce::String (juce::CharPointer_UTF8 (line (e).toRawUTF8())), true, e->id == dialogStartId); }
        if (! other.empty())
        {
            juce::PopupMenu sub;
            for (auto* e : other) { all.push_back (e); sub.addItem ((int) all.size() + 1, juce::String (juce::CharPointer_UTF8 (line (e).toRawUTF8())), true, e->id == dialogStartId); }
            m.addSubMenu ("Other libraries", sub);
        }
        if (all.empty()) m.addItem (-1, "No sessions in your libraries yet", false);
        m.showMenuAsync (menuAt (x, y, w, h), [this, all] (int r)
        {
            if (r == 1) { dialogStartId.clear(); newSession.state().startFrom = "The library's standard"; }
            else if (r >= 2 && r - 2 < (int) all.size()) { dialogStartId = all[(size_t) r - 2]->id; newSession.state().startFrom = all[(size_t) r - 2]->name.toStdString(); }
            else return;
            newSession.refresh();
        });
    };
    newSession.onNewLibrary = [this]
    {
        auto* w = new juce::AlertWindow ("New library", "A collection of sessions, like Limitless Snares.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", {});
        w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            const auto name = w->getTextEditorContents ("name").trim();
            if (r != 1 || name.isEmpty()) return;
            if (! libraries.add (name) && libraries.find (name) == nullptr) return;
            libraries.save();
            if (auto* L = libraries.find (name))
            {
                newSession.state().library = L->name.toStdString();
                newSession.state().libraryColour = L->colour;
                newSession.refresh();
            }
        }), true);
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
    root.removeChild (&newSession);
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
        studio.setTrackState (i, t->isSolo (false), t->isMuted (false), vp ? vp->getVolumeDb() : 0.0f, vp ? vp->getPan() : 0.0f,
                              (float) t->state.getProperty ("ldPanL", -1.0f), (float) t->state.getProperty ("ldPanR", 1.0f));
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

    // Audio always starts a session through the New Session window.
    if (newSession.isOpen()) dialogAddItems (items);
    else openNewSession (items);
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
    {
        // what New Session decided travels with the project; a project opened from disk brings its own back
        static const juce::Identifier key ("ldSession");
        if (edit->state.hasProperty (key) && sessionName.isEmpty())
        {
            const auto v = juce::JSON::parse (edit->state[key].toString());
            sessionName = v["name"].toString(); sessionLibrary = v["library"].toString(); librarySessionId = v["id"].toString();
            takeOptions = { v["drum"].toString().toStdString(), v["artic"].toString().toStdString(), (int) v["vel"], (int) v["rr"], {} };
        }
        auto o = juce::var (new juce::DynamicObject()); auto* d = o.getDynamicObject();
        d->setProperty ("name", sessionName); d->setProperty ("library", sessionLibrary); d->setProperty ("id", librarySessionId);
        d->setProperty ("drum", juce::String (takeOptions.drum)); d->setProperty ("artic", juce::String (takeOptions.artic));
        d->setProperty ("vel", takeOptions.vel); d->setProperty ("rr", takeOptions.rr);
        edit->state.setProperty (key, juce::JSON::toString (o, true), nullptr);
    }
    edit->resetChangedStatus();
    edit->getUndoManager().clearUndoHistory();
    edit->getUndoManager().addChangeListener (this);

    const auto title = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension()
                     : takeTitle();
    studio.setModel (buildStudioModel (*edit, currentRoles, title, nullptr));
    studio.setStep (ui::Step::session);
    loadTake();
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

/** The tracks as Hitline lays them out: the lead first, every mic named for what it is (Top, Room Far), so the
    strips, the console and the files agree with the engine. Only while nothing has been edited yet. */
void MainComponent::adoptEngineOrder()
{
    if (! edit || ! engineState || edit->hasChangedSinceSaved()) return;
    const auto& S = engineState->session;
    const auto files = editFiles (*edit);
    std::vector<MicFile> mics;
    std::vector<MicRole> roles;
    for (auto& tr : S.tracks)
        for (auto& f : files)
            if (f.getFileNameWithoutExtension().toStdString() == tr.name)
            {
                mics.push_back ({ juce::String (tr.mic), f });
                roles.push_back (classifyMic (juce::String (S.drum == "kit" ? std::string() : S.drum + " ") + juce::String (tr.mic)));
            }
    if (mics.size() != files.size()) return;
    auto names = [&] { juce::StringArray a; for (auto* t : audioTracks()) a.add (t->getName()); return a; };
    juce::StringArray want; for (auto& m : mics) want.add (m.name);
    if (names() == want) return;
    auto fresh = createSessionEdit (engine, mics, 120.0, te::Edit::EditRole::forEditing);
    if (fresh == nullptr) return;
    edit->getTransport().stop (false, true);
    edit->getUndoManager().removeChangeListener (this);
    edit = std::move (fresh);
    currentRoles = std::move (roles);
    project::storeRoles (*edit, currentRoles);
    edit->resetChangedStatus();
    edit->getUndoManager().clearUndoHistory();
    edit->getUndoManager().addChangeListener (this);
    studio.selectedTrack = S.refIdx;
}

void MainComponent::refreshModel()
{
    if (! edit) return;
    const auto title = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension()
                     : takeTitle();
    const auto view = studio.view;
    auto m = buildStudioModel (*edit, currentRoles, title, engineState);
    if (engineState) m.align = buildAlignView (*engineState, alignMic, studio.selectedHit, alignSpread, alignSamples);
    if (engineState) m.tune = buildTuneView (*engineState, studio.selectedHit);
    if (engineState) m.mix = buildMixView (*engineState, mixSel, studio.selectedHit, refAudio.empty() ? nullptr : &refAudio, refName);
    if (engineState) m.song = buildSongView (*engineState);
    if (engineState)
    {
        const auto& S = engineState->session;
        m.exp.bits = S.exp.bits; m.exp.kitBus = S.exp.kitBus; m.exp.verbs = S.exp.verbs;
        m.exp.ready = engineState->mix && (S.mode == "kit" || (engineState->levels && engineState->levels->valid));
        m.exp.files = (int) hl::exportList (S).size();
    }
    mixHit = studio.selectedHit;
    tuneHit = studio.selectedHit;
    alignMic = m.align.mic; alignHit = studio.selectedHit;
    studio.setModel (std::move (m));
    studio.view = view;
}

void MainComponent::refreshAlign()
{
    if (! engineState) return;
    studio.data.align = buildAlignView (*engineState, alignMic, studio.selectedHit, alignSpread, alignSamples);
    alignMic = studio.data.align.mic; alignHit = studio.selectedHit;
    studio.redrawAll();
}

/** A change on the Align step: made on the session (playback stopped, so nothing reads it meanwhile), shown at
    once, then the mix renders again behind it for the B side and the levels. */
void MainComponent::alignOp (std::function<void (hl::Session&)> op, const juce::String& what)
{
    if (! engineState || ! engineState->session.phaseAnalyzed) return;
    if (audition) audition->stop();
    op (engineState->session);
    refreshModel();
    updateHeader();
    setNotice (what);
    rerender();
}

void MainComponent::rerender()
{
    if (! engineState) return;
    // dragging a knob sends a stream of changes: render one at a time, then once more with the latest state
    if (renderBusy) { renderPending = true; ++renderGeneration; return; }
    renderBusy = true; renderPending = false;
    const int gen = ++renderGeneration;
    auto copy = std::make_shared<hl::Session> (engineState->session);
    if (copy->mode == "kit") for (auto& t : copy->tracks) hl::F32().swap (t.mono);   // a song's render doesn't read the monos: half a copy less
    std::thread ([copy, gen, st = engineState, sp = juce::Component::SafePointer<MainComponent> (this)]
    {
        auto mix = std::make_shared<hl::MixRender> (hl::renderMix (*copy));
        auto lv = std::make_shared<std::optional<hl::LevelResult>>();
        if (copy->mode != "kit") *lv = hl::measureLevels (*copy, *mix);
        juce::MessageManager::callAsync ([sp, st, gen, mix, lv]
        {
            if (sp == nullptr) return;
            sp->renderBusy = false;
            if (sp->renderPending) { sp->renderPending = false; sp->rerender(); return; }
            if (sp->renderGeneration != gen || sp->engineState != st) return;
            if (sp->audition) sp->audition->stop();
            st->mix = std::move (*mix);
            st->levels = std::move (*lv);
            if (sp->audition) sp->audition->setEngine (st);
            sp->refreshModel();
            sp->refreshPlugin();
            sp->updateHeader();
            releaseFreedMemory();
        });
    }).detach();
}

void MainComponent::onAlignAction (const std::string& a, int mic, double v)
{
    if (! engineState) return;
    auto& S = engineState->session;
    const int nT = (int) S.tracks.size();
    const bool ok = mic >= 0 && mic < nT && mic != S.refIdx;
    const auto micName = ok ? juce::String (S.tracks[(size_t) mic].mic) : juce::String();
    if (a == "again")
    {
        if (prepping) return;
        // measure every mic again, off the message thread, on a copy; then it replaces the session's alignment
        if (audition) audition->stop();
        setNotice (juce::CharPointer_UTF8 ("Lining up the tracks again\xe2\x80\xa6"));
        const int gen = ++renderGeneration;
        auto copy = std::make_shared<hl::Session> (S);
        std::thread ([copy, gen, st = engineState, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            for (auto& tr : copy->tracks) { tr.manualLag = tr.manualPol = false; }
            hl::runAnalyze (*copy);
            juce::MessageManager::callAsync ([sp, st, gen, copy]
            {
                if (sp == nullptr || sp->renderGeneration != gen || sp->engineState != st) return;
                if (sp->audition) sp->audition->stop();
                st->session = std::move (*copy);
                sp->refreshModel(); sp->updateHeader();
                sp->setNotice ("Lined up again.");
                sp->rerender();
            });
        }).detach();
        return;
    }
    if (a.rfind ("timing:", 0) == 0)
    {
        const auto mode = a.substr (7);
        alignOp ([mode] (hl::Session& s) { hl::setTiming (s, mode); },
                 mode == "pro" ? "Standard phase: every track as late as it sits on a real record." : mode == "natural" ? "Every track as recorded." : "Tight phase: every track lined up tight with the lead.");
        return;
    }
    if (a == "spread") { alignSpread = v != 0; refreshAlign(); return; }
    if (a == "units") { alignSamples = v != 0; refreshAlign(); return; }
    if (! ok) return;
    if (a == "nudge") { const int d = (int) v; alignOp ([mic, d] (hl::Session& s) { hl::nudgeLag (s, mic, d); }, micName + (d < 0 ? " earlier" : " later") + " by " + (std::abs (d) == 4 ? "a sample" : "a quarter sample") + "."); return; }
    if (a == "lag") { alignOp ([mic, v] (hl::Session& s) { hl::setLag (s, mic, v); }, micName + " moved by hand."); return; }
    if (a == "snap")
    {
        const auto best = studio.data.align.best;
        if (! std::isfinite (best)) return;
        alignOp ([mic, best] (hl::Session& s) { hl::setLag (s, mic, best); }, micName + " snapped to where it sums hottest.");
        return;
    }
    if (a == "flip") { alignOp ([mic] (hl::Session& s) { hl::flipMic (s, mic); }, micName + " polarity flipped."); return; }
    if (a == "eq") { const bool on = ! S.tracks[(size_t) mic].apOn; alignOp ([mic, on] (hl::Session& s) { hl::setPhaseEq (s, mic, on); }, micName + (on ? " phase EQ on." : " phase EQ off.")); return; }
    if (a == "lr")
    {
        if (! S.tracks[(size_t) mic].lr) return;
        const bool on = ! S.tracks[(size_t) mic].lrLock;
        alignOp ([mic, on] (hl::Session& s) { hl::setLrLock (s, mic, on); }, micName + (on ? " right side lined up with the left." : " left and right as recorded."));
        return;
    }
    if (a == "reset") { alignOp ([mic] (hl::Session& s) { hl::resetMic (s, mic); }, micName + " back to where it measured."); return; }
    if (a == "pair" || a == "gain")
    {
        if (! audition || ! engineState->mix) { setNotice ("Still preparing the take."); return; }
        if (a == "gain" && v == 0) { audition->stop(); updateHeader(); return; }
        if (edit) edit->getTransport().stop (false, false);
        Audition::Options o;
        o.mono = true; o.console = false; o.loudnessMatch = false; o.mode = "pair";
        const int r = hl::alignRef (S, mic) >= 0 ? hl::alignRef (S, mic) : S.refIdx;
        o.only = { r, mic };
        if (a == "gain") o.gain = true; else o.side = v != 0 ? 1 : 0;
        audition->play (modes::hit (S, std::max (0, studio.selectedHit)), o);
        setNotice (a == "gain" ? juce::String ("What lining up ") + micName + " changed, and nothing else."
                               : juce::String (S.tracks[(size_t) r].mic) + " + " + micName + " in mono, " + (v != 0 ? "aligned." : "as recorded."));
        updateHeader();
        return;
    }
}

void MainComponent::onAlignMenu (const std::string& which, float x, float y, float w, float h)
{
    if (! engineState) return;
    const auto& S = engineState->session;
    const auto opts = juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (x, y + 0, w, h).toNearestInt()))
                                                .withMinimumWidth ((int) w);
    juce::PopupMenu m;
    if (which == "mic")
    {
        for (int t = 0; t < (int) S.tracks.size(); ++t)
            if (t != S.refIdx) m.addItem (t + 1, juce::String (S.tracks[(size_t) t].mic), true, t == alignMic);
        m.showMenuAsync (opts, [this] (int r) { if (r <= 0) return; alignMic = r - 1; studio.selectedTrack = alignMic; refreshAlign(); });
        return;
    }
    const int mic = alignMic;
    if (mic < 0 || mic >= (int) S.tracks.size() || mic == S.refIdx) return;
    const auto& tr = S.tracks[(size_t) mic];
    std::vector<std::string> keys;
    auto add = [&] (const std::string& key, const juce::String& label) { keys.push_back (key); m.addItem ((int) keys.size(), label, true, tr.alignTo == key); };
    add ("lead", juce::String (S.tracks[(size_t) S.refIdx].mic) + " (lead)");
    {
        static const std::vector<std::optional<hl::MixRender::Scan>> none;
        hl::ToneEngine E (S, none);
        const double T = E.proTarget (mic);
        if (std::isfinite (T)) add ("pro", "Standard phase (" + juce::String (T / S.sr * 1000, 1) + " ms late)");
    }
    m.addSeparator();
    for (const char* g : { "A", "B", "C", "D" })
    {
        juce::String label = juce::String ("Group ") + g;
        juce::StringArray mem;
        for (int t = 0; t < (int) S.tracks.size(); ++t) if (t != mic && S.tracks[(size_t) t].alignTo == g) mem.add (S.tracks[(size_t) t].mic);
        if (! mem.isEmpty()) label += juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + mem.joinIntoString (", ");
        add (g, label);
    }
    m.addSeparator();
    add ("none", "As recorded");
    m.showMenuAsync (opts, [this, keys, mic] (int r)
    {
        if (r <= 0 || r > (int) keys.size()) return;
        const auto to = keys[(size_t) r - 1];
        alignOp ([mic, to] (hl::Session& s) { hl::setAlignTo (s, mic, to); }, "Lines up to " + juce::String (to == "none" ? "nothing: as recorded" : to == "lead" ? "the lead" : to == "pro" ? "standard phase" : "group " + to) + ".");
    });
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
    std::thread ([files, folder, gen, opts = takeOptions, sp = juce::Component::SafePointer<MainComponent> (this)]
    {
        std::shared_ptr<EngineState> st = prepareTake (files, folder, opts);
        juce::MessageManager::callAsync ([sp, st, gen]
        {
            if (sp == nullptr || sp->prepGeneration != gen) return;
            sp->engineState = st;
            sp->adoptEngineOrder();
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
            sp->refreshPlugin();
            sp->updateHeader();
            releaseFreedMemory();
            sp->setNotice (st->session.mode == "kit" ? "Ready. Hits and tempo found: Edit tightens the timing; File > Export writes the tracks and the tempo map."
                                                     : "Ready. Every hit is on the level standard. File > Export writes the samples.");
            sp->storeInLibrary();
        });
    }).detach();
}

juce::String MainComponent::takeTitle() const
{
    if (sessionName.isNotEmpty()) return sessionName;
    return currentTake >= 0 && currentTake < (int) takes.size() ? takes[(size_t) currentTake].title : juce::String ("Untitled");
}

/** Opens the take as Hitline would (names, the lead first, counts, the drum's defaults) and stops there:
    Auto prep waits for you. */
void MainComponent::loadTake()
{
    if (! edit) return;
    const auto files = editFiles (*edit);
    if (files.empty() || std::any_of (files.begin(), files.end(), [] (auto& f) { return ! f.existsAsFile(); })) return;
    const int gen = ++prepGeneration;
    const auto folder = files[0].getParentDirectory().getFileName();
    prepping = false;
    if (audition) audition->setEngine (nullptr);
    setNotice (juce::CharPointer_UTF8 ("Loading the mics\xe2\x80\xa6"));
    std::thread ([files, folder, gen, opts = takeOptions, sp = juce::Component::SafePointer<MainComponent> (this)]
    {
        auto st = ld::openTake (files, folder, opts);
        // Limitless's own tuning: the stick attack keeps its pitch on big shifts, and exports use the finer filter
        st->session.keepAttack = true; st->session.tuneTaps = 64;
        juce::MessageManager::callAsync ([sp, st, gen]
        {
            if (sp == nullptr || sp->prepGeneration != gen) return;
            sp->engineState = st;
            sp->adoptEngineOrder();
            sp->refreshModel();
            sp->updateHeader();
            const auto& S = st->session;
            if (! st->errors.empty()) { sp->setNotice (juce::String (st->errors.front())); return; }
            if (S.tracks.empty()) return;
            sp->setNotice (juce::String (S.tracks.size()) + " mics loaded, " + juce::String (S.tracks[(size_t) S.refIdx].mic) + " leads, "
                           + juce::String (S.det.V) + juce::String (juce::CharPointer_UTF8 (" \xc3\x97 ")) + juce::String (S.det.RR)
                           + " hits expected. Press Auto Prep (A) when you're ready.");
            sp->storeInLibrary();
        });
    }).detach();
}

//==============================================================================
// New Session
void MainComponent::openNewSession (const juce::Array<juce::File>& items)
{
    for (auto& f : items)
        if (f.hasFileExtension (project::extension)) return openProject (f);

    ui::NewSessionState s;
    s.library = libraries.current.toStdString();
    if (auto* L = libraries.find (libraries.current)) s.libraryColour = L->colour;
    dialogTakes.clear();
    dialogTake = -1;
    dialogStartId.clear();
    newSession.open (s);
    if (! items.isEmpty()) dialogAddItems (items);
}

void MainComponent::dialogAddItems (const juce::Array<juce::File>& items)
{
    for (auto& f : items)
        if (f.hasFileExtension (project::extension)) { newSession.close(); return openProject (f); }
    auto planned = planImport (items);
    if (planned.empty())
    {
        newSession.state().folder.clear();
        newSession.state().folderDetail = "No WAV or AIFF files there.";
        newSession.refresh();
        return;
    }
    dialogTakes = std::move (planned);
    if (dialogTakes.size() == 1) return dialogUseTake (0);
    juce::PopupMenu m;
    m.addSectionHeader ("Which articulation is this session?");
    for (int i = 0; i < (int) dialogTakes.size(); ++i)
        m.addItem (i + 1, dialogTakes[(size_t) i].title + "  (" + juce::String (dialogTakes[(size_t) i].mics.size()) + " mics)");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (getScreenBounds().withSizeKeepingCentre (2, 2)),
                     [this] (int r) { if (r > 0) dialogUseTake (r - 1); });
}

void MainComponent::dialogUseTake (int index)
{
    if (index < 0 || index >= (int) dialogTakes.size() || ! newSession.isOpen()) return;
    dialogTake = index;
    const auto& take = dialogTakes[(size_t) index];
    auto& s = newSession.state();
    s.folder = take.title.toStdString();
    juce::String detail = juce::String ((int) take.mics.size()) + " mic" + (take.mics.size() == 1 ? "" : "s");
    if (take.sampleRate > 0) detail << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) << juce::String (take.sampleRate / 1000.0, 1) << " kHz";
    if (take.lengthSeconds > 0) detail << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) << juce::String (take.lengthSeconds, 1) << " s";
    detail << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) << take.folder.getParentDirectory().getFileName();
    s.folderDetail = detail.toStdString();

    // what the names say: drum, articulation, counts
    std::vector<std::string> rel, names;
    for (auto& m : take.mics) { rel.push_back ((take.folder.getFileName() + "/" + m.file.getFileName()).toStdString()); names.push_back (rel.back()); }
    const auto recs = hl::groupSessions (rel);
    static const std::vector<std::string> known { "kick", "snare", "tom", "hat", "ride", "crash", "china", "splash", "kit" };
    if (! recs.empty())
    {
        if (std::find (known.begin(), known.end(), recs[0].drum) != known.end()) s.drum = recs[0].drum;
        if (! recs[0].artic.empty()) s.artic = recs[0].artic;
    }
    // a take with close mics on more than one drum is a performance: Song mode
    {
        std::set<int> drums;
        for (auto& m : take.mics) { const auto d = hl::drumOfMic (m.file.getFileNameWithoutExtension().toStdString()); if (d != hl::Drum::other && d != hl::Drum::cymbal) drums.insert ((int) d); }
        if (drums.size() >= 2) s.drum = "kit";
    }
    if (const auto c = hl::guessCounts (names)) { s.vel = c->first; s.rr = c->second; s.countsFromNames = true; }
    else s.countsFromNames = false;
    if (s.name.empty())
    {
        const auto parent = take.folder.getParentDirectory().getFileName();
        s.name = (dialogTakes.size() > 1 || take.title.equalsIgnoreCase (juce::String (s.artic))) && parent.isNotEmpty() ? parent.toStdString() : take.title.toStdString();
    }
    newSession.refresh();
}

void MainComponent::createSession()
{
    if (dialogTake < 0 || dialogTake >= (int) dialogTakes.size()) return;
    const auto s = newSession.state();
    takeOptions = {};
    takeOptions.drum = s.drum;
    takeOptions.artic = s.artic.empty() ? std::string ("Hit") : s.artic;
    takeOptions.vel = s.vel; takeOptions.rr = s.rr;
    if (dialogStartId.isNotEmpty())
        if (auto* from = libraries.findSession (dialogStartId)) takeOptions.settings = from->settings;
    sessionName = juce::String (s.name).trim();
    if (sessionName.isEmpty()) sessionName = dialogTakes[(size_t) dialogTake].title;
    sessionLibrary = juce::String (s.library);
    librarySessionId = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64());
    libraries.current = sessionLibrary;
    newSession.close();
    takes = dialogTakes;
    openTake (dialogTake);
    storeInLibrary();
}

/** Keeps the library's record of the open session current: what it is, where its project lives, its settings. */
void MainComponent::storeInLibrary()
{
    if (librarySessionId.isEmpty() || sessionLibrary.isEmpty()) return;
    LibrarySession e;
    if (auto* old = libraries.findSession (librarySessionId)) e = *old;
    e.id = librarySessionId;
    e.name = sessionName;
    e.drum = takeOptions.drum; e.artic = takeOptions.artic; e.vel = takeOptions.vel; e.rr = takeOptions.rr;
    if (engineState && ! engineState->session.tracks.empty())
    {
        const auto& S = engineState->session;
        e.drum = S.drum; e.artic = S.artic; e.vel = S.det.V; e.rr = S.det.RR;
        e.settings = hl::settingsToVar (S);
    }
    if (projectFile != juce::File()) e.project = projectFile.getFullPathName();
    e.modified = juce::Time::currentTimeMillis();
    libraries.put (sessionLibrary, e);
    libraries.save();
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
                if (kit && st->session.song.an.done)
                {
                    // Song mode: the tempo map for Pro Tools, and every hit as MIDI on it
                    const auto& S = st->session;
                    const auto beats = hl::gridBeats (S.song.an, S.song.set);
                    const auto E = hl::planEdits (S.song.an, S.song.set);
                    auto put = [&] (const std::string& name, const std::string& bytes) { sink (name, juce::MemoryBlock (bytes.data(), bytes.size())); ++n; };
                    if (beats.size() > 1)
                    {
                        put ("Tempo Map.mid", hl::midiTempoMap (beats));
                        put ("Drums (MIDI).mid", hl::midiPerformance (S.song.an.hits, E, beats, S.sr));
                    }
                }
            }
            catch (const std::exception& e) { err = e.what(); }
            juce::MessageManager::callAsync ([sp, n, err, dir]
            {
                if (sp == nullptr) return;
                sp->exporting = false;
                sp->setNotice (err.isNotEmpty() ? "Export failed: " + err : "Exported " + juce::String (n) + " files to " + dir.getFileName() + ".");
                if (err.isEmpty()) { dir.revealToUser(); sp->storeInLibrary(); }
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
                tr.panL = (double) tracks[i]->state.getProperty ("ldPanL", -1.0); tr.panR = (double) tracks[i]->state.getProperty ("ldPanR", 1.0);
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
    sessionName.clear(); sessionLibrary.clear(); librarySessionId.clear(); takeOptions = {};
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
            storeInLibrary();
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

// ---------------------------------------------------------------- undo for the engine's settings
// Mix, plugins, Song mode and Clean change the engine's session, not the Tracktion edit; each change goes onto the
// edit's undo list as an action holding the settings before and after (no audio, a few KB), so Undo, Redo and the
// History panel cover them alongside everything else.
namespace
{
struct SettingsChange : juce::UndoableAction
{
    SettingsChange (MainComponent& m, juce::var b, juce::var a) : mc (m), before (std::move (b)), after (std::move (a)) {}
    bool perform() override { if (first) { first = false; return true; } mc.restoreSettings (after); return true; }
    bool undo() override { mc.restoreSettings (before); return true; }
    int getSizeInUnits() override { return 10; }
    MainComponent& mc; juce::var before, after; bool first = true;
};
}

juce::var MainComponent::settingsSnapshot() const
{
    if (! engineState) return {};
    // a deep copy: the session's insert lists are shared vars that later changes edit in place
    return juce::JSON::parse (juce::JSON::toString (hl::settingsToVar (engineState->session), true));
}

void MainComponent::recordChange (const juce::var& before, const juce::String& what)
{
    if (! edit || ! engineState || before.isVoid()) return;
    auto& um = edit->getUndoManager();
    um.beginNewTransaction (what);
    um.perform (new SettingsChange (*this, before, settingsSnapshot()), what);
}

void MainComponent::restoreSettings (const juce::var& v)
{
    if (! engineState || v.isVoid()) return;
    if (audition) audition->stop();
    hl::applySettings (engineState->session, juce::JSON::parse (juce::JSON::toString (v, true)));
    refreshModel(); refreshMix(); updateHeader();
    rerender();
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
                        : takeTitle();
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
        m.addItem (miImport, juce::String (juce::CharPointer_UTF8 ("New Session\xe2\x80\xa6")));
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
        case miImport:  openNewSession(); break;
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
    if (S.mode == "kit")
    {
        // Song mode: play the performance from the playhead (from the top when it's at the end)
        if (edit) edit->getTransport().stop (false, false);
        pushConsole();
        long len = 0; for (auto& t : S.tracks) len = std::max (len, t.len);
        songFrom = studio.playhead < len / S.sr - 0.5 ? std::max (0.0, studio.playhead) : 0.0;
        audition->playSong (songFrom);
        songPlaying = true;
        return;
    }
    if (m != "hit" && m != "mono" && ! hl::countsOk (S)) { setNotice ("That needs the full hit count. Hit and Mono work now."); return; }
    if (edit) edit->getTransport().stop (false, false);
    pushConsole();
    std::vector<PlayEvent> ev;
    Audition::Options o;
    o.mode = m; o.chain = chainAB && m != "hit" && m != "mono"; o.loudnessMatch = lmOn;
    if (m == "hit") ev = modes::hit (S, sel);
    else if (m == "layer") ev = modes::layer (S, sel);
    else if (m == "sweep") ev = modes::sweep (S, sel);
    else if (m == "groove") ev = rudimentsOn ? modes::rudiments (S) : modes::groove (S);
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
    std::vector<float> g (S.tracks.size(), 1.0f), pan (S.tracks.size(), 0.0f), panL (S.tracks.size(), -1.0f), panR (S.tracks.size(), 1.0f);
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
        panL[(size_t) e] = (float) t->state.getProperty ("ldPanL", -1.0f);
        panR[(size_t) e] = (float) t->state.getProperty ("ldPanR", 1.0f);
        st[(size_t) e] = S.tracks[(size_t) e].ch.size() > 1;
    }
    const float room = hl::roomLive (S) ? std::pow (10.0f, (float) S.room.ret / 20.0f) : 0.0f;
    audition->setConsole (g, pan, st, 1.0f, room, panL, panR);
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
        h.song = S.mode == "kit";
        if (h.song) { h.sessionLine = "Song \xc2\xb7 " + sessionName.toStdString(); h.detailLine = std::to_string (S.tracks.size()) + " mics \xc2\xb7 " + juce::String (S.maxLen() / S.sr, 1).toStdString() + " s \xc2\xb7 " + juce::String (S.sr / 1000.0, 1).toStdString() + " kHz"; }
        h.status[1] = std::to_string (S.onsets.size()) + (ok ? " HITS" : " FOUND"); h.done[1] = ok;
        int moved = 0;
        for (auto& t : S.tracks) if (t.autoResult && ! t.autoResult->kept && (t.lag != 0 || t.pol < 0 || ! t.ap.empty())) ++moved;
        h.status[2] = S.phaseAnalyzed ? std::to_string (moved) + " LINED UP" : "NOT YET"; h.done[2] = S.phaseAnalyzed;
        {
            const auto adv = hl::tuneAdvice (S);
            if (adv == "cymbal" && ! S.tune.on) { h.status[3] = "OFF \xc2\xb7 CYMBAL"; h.done[3] = true; }
            else if (adv == "nopitch" && ! S.tune.on) { h.status[3] = "NO STEADY PITCH"; h.done[3] = true; }
            else { h.status[3] = S.tuneInfo ? hl::noteName (S.tuneInfo->nearest) + (S.tune.on ? " \xc2\xb7 ON" : " \xc2\xb7 OFF") : "NO PITCH"; h.done[3] = S.tuneInfo.has_value(); }
        }
        h.status[4] = ! es->mix ? (prepping ? "RENDERING" : "NOT YET") : hl::fxOn (S) || hl::busLive (S) || hl::roomLive (S) ? "ON" : "FLAT";
        const bool lv = es->levels && es->levels->valid;
        h.status[5] = lv ? "ON STANDARD" : (es->mix ? "NEEDS HITS" : prepping ? "MEASURING" : "NOT YET"); h.done[5] = lv;
        if (h.song)
        {
            // Song mode: Edit & Tempo, Clean; Tune is for samples
            const auto bp = hl::bpmOf (hl::gridBeats (S.song.an, S.song.set));
            double avg = 0; for (double b : bp) avg += b; if (! bp.empty()) avg /= (double) bp.size();
            h.status[1] = S.song.an.done ? juce::String (avg, 1).toStdString() + " BPM" : std::string ("NOT YET");
            h.done[1] = S.song.an.done;
            h.status[3] = "SAMPLES ONLY"; h.done[3] = false;
            h.status[5] = S.kitBleed > 0.05 ? "BLEED -" + juce::String ((int) std::lround (S.kitBleed)).toStdString() + " dB" : std::string ("OFF");
            h.done[5] = S.kitBleed > 0.05;
        }
        h.exportReady = es->mix && (lv || S.mode == "kit");
        static const EngineState* countedFor = nullptr; static size_t files = 0;
        if (countedFor != es || ! h.exportReady) { files = hl::exportList (S).size(); countedFor = h.exportReady ? es : nullptr; }
        h.status[6] = h.exportReady ? std::to_string (files) + " FILES" : std::string ("NOT YET");
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
    h.loop = loopOn; h.follow = followOn; h.lm = lmOn; h.pads = padsOn; h.rudiments = rudimentsOn;
    h.canUndo = edit && edit->getUndoManager().canUndo();
    h.canRedo = edit && edit->getUndoManager().canRedo();
    h.historyCount = edit ? edit->getUndoManager().getNumActionsInCurrentTransaction() + (int) edit->getUndoManager().getUndoDescriptions().size() : 0;
    studio.setHeader (std::move (h));
}
bool MainComponent::handleKey (const visage::KeyEvent& k)
{
    using KC = visage::KeyCode;
    const auto code = k.keyCode();
    if (newSession.isOpen()) return true;   // the window takes its own keys
    if (! k.isMainModifier() && ! k.isAltDown() && ! k.isShiftDown() && code >= KC::Number1 && code <= KC::Number6)
    {
        if (studio.onPad) studio.onPad ((int) code - (int) KC::Number1);
        return true;
    }
    const double pos = edit ? edit->getTransport().getPosition().inSeconds() : 0.0;
    if (k.isMainModifier())
    {
        if (code == KC::O) { chooseProject(); return true; }
        if (code == KC::I || code == KC::N) { openNewSession(); return true; }
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
    if (code == KC::G) { if (k.isShiftDown()) { rudimentsOn = ! rudimentsOn; playModeName = "groove"; updateHeader(); } else playMode ("groove"); return true; }
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
    if (newSession.isOpen()) return k == juce::KeyPress::escapeKey ? (newSession.close(), true) : true;
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
        if (songPlaying && audition && audition->isPlaying()) studio.setTransport (songFrom + audition->position(), true);
        else if (songPlaying) { songPlaying = false; studio.setTransport (studio.playhead, false); }
        else studio.setTransport (t.getPosition().inSeconds(), t.isPlaying());

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
        if (studio.step() == ui::Step::align && ! playingNow && studio.selectedHit != alignHit) refreshAlign();
        if (studio.step() == ui::Step::tune && studio.selectedHit != tuneHit) refreshTune();
        if (studio.step() == ui::Step::mix && ! playingNow && studio.selectedHit != mixHit) refreshMix();
        if (studio.selectedHit != lastSelected)
        {
            lastSelected = studio.selectedHit;
            if (followOn && engineState && engineState->mix) { playModeName = playModeName == "mono" ? "mono" : "hit"; startPhrase (playModeName); }
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

namespace ld
{
// ------------------------------------------------------------------------------------------- the Tune step
static int tuneBase (const hl::Session& S)
{
    if (S.tuneInfo && S.tuneInfo->valid) return S.tuneInfo->nearest;
    return (int) std::lround (hl::hzToMidi (std::sqrt (S.pitchOpt.fmin * S.pitchOpt.fmax), S.tune.a4));
}

void MainComponent::refreshTune()
{
    if (! engineState) return;
    auto v = buildTuneView (*engineState, studio.selectedHit);
    // the target knob: Nearest, then seven notes either side of where the drum sits
    const auto& S = engineState->session;
    if (S.tune.target == "nearest") v.targetPos = 0;
    else v.targetPos = std::clamp ((float) (juce::String (S.tune.target).getIntValue() - (tuneBase (S) - 7) + 1) / 15.0f, 0.0f, 1.0f);
    studio.data.tune = std::move (v);
    tuneHit = studio.selectedHit;
    studio.redrawAll();
}

void MainComponent::onTuneAction (const std::string& a, double v)
{
    if (! engineState) return;
    auto& S = engineState->session;
    auto change = [this] (std::function<void (hl::Session&)> op, const juce::String& what)
    {
        if (audition) audition->stop();
        op (engineState->session);
        hl::computeTune (engineState->session);
        refreshModel(); refreshTune(); updateHeader(); setNotice (what);
        rerender();
    };
    if (a == "read")
    {
        if (prepping || S.onsets.empty()) { setNotice ("Find the hits first."); return; }
        if (audition) audition->stop();
        setNotice (juce::CharPointer_UTF8 ("Reading the pitch of every hit\xe2\x80\xa6"));
        const int gen = ++renderGeneration;
        auto copy = std::make_shared<hl::Session> (S);
        std::thread ([copy, gen, st = engineState, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            hl::runPitch (*copy);
            juce::MessageManager::callAsync ([sp, st, gen, copy]
            {
                if (sp == nullptr || sp->renderGeneration != gen || sp->engineState != st) return;
                if (sp->audition) sp->audition->stop();
                st->session = std::move (*copy);
                sp->refreshModel(); sp->refreshTune(); sp->updateHeader();
                const auto& s2 = st->session;
                sp->setNotice (s2.tuneInfo && s2.tuneInfo->valid
                    ? juce::String ("Sits at ") + juce::String (s2.tuneInfo->medF, 1) + " Hz, " + hl::noteName (s2.tuneInfo->nearest) + "."
                    : juce::String ("No clear pitch. Try another track to read from, or another range."));
                sp->rerender();
            });
        }).detach();
        return;
    }
    if (a == "anyway")
    {
        // not recommended: read the pitch if it hasn't been, then tune
        S.tuneAnyway = true;
        if (! S.tuneInfo || ! S.tuneInfo->valid) { onTuneAction ("read", 0); setNotice ("Reading the pitch. Tuning this isn't recommended: press Tune It when it's read."); return; }
        change ([] (hl::Session& s) { s.tune.on = true; }, "Tuned anyway. Not recommended: there's no steady note here to tune to.");
        return;
    }
    if (! S.tuneInfo || ! S.tuneInfo->valid) return;
    const auto note = [&] { return S.tuneInfo ? hl::noteName (S.tuneInfo->targetMidi) : std::string(); };
    if (a == "on") { const bool on = v != 0; change ([on] (hl::Session& s) { s.tune.on = on; }, on ? "Tuned. Varispeed: the attack stays intact." : "Tuning off: as recorded."); return; }
    if (a == "mode") { const bool rr = v != 0; change ([rr] (hl::Session& s) { s.tune.mode = rr ? "rr" : "drum"; }, rr ? "Round robins evened out too, each toward its own layer." : "One ratio for the whole drum: soft and hard hits keep their natural pitch."); return; }
    if (a == "fine") { const double c = v; change ([c] (hl::Session& s) { s.tune.cents = c; }, juce::String ("Fine tune ") + (c > 0 ? "+" : "") + juce::String ((int) c) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2."))); return; }
    if (a == "a4") { const double f = v; change ([f] (hl::Session& s) { s.tune.a4 = f; }, "A4 = " + juce::String ((int) f) + " Hz."); return; }
    if (a == "nearest") { change ([] (hl::Session& s) { s.tune.target = "nearest"; }, "Target: the nearest note."); return; }
    if (a == "target")
    {
        const int idx = (int) std::lround (v * 15);
        const int base = tuneBase (S);
        const std::string tgt = idx <= 0 ? std::string ("nearest") : std::to_string (base - 7 + idx - 1);
        change ([tgt] (hl::Session& s) { s.tune.target = tgt; }, tgt == "nearest" ? juce::String ("Target: the nearest note.") : juce::String ("Target: ") + hl::noteName (std::stod (tgt)) + ".");
        return;
    }
    if (a == "play")
    {
        if (! audition || ! engineState->mix) return;
        if (edit) edit->getTransport().stop (false, false);
        Audition::Options o;
        if (v == 2) o.chain = true; else o.side = (int) v;
        audition->play (modes::hit (S, std::max (0, studio.selectedHit)), o);
        setNotice (v == 0 ? "The hit as recorded." : v == 1 ? "The hit tuned." : "As recorded, then tuned.");
        updateHeader();
        return;
    }
    if (a == "tone")
    {
        if (! audition) return;
        const auto& ti = *S.tuneInfo;
        const double hz = v != 0 ? ti.targetF : ti.medF;
        audition->playTone (hz, 1.6);
        setNotice ((v != 0 ? juce::String ("Target note ") + note() : juce::String ("Recorded note ") + hl::noteName (std::round (hl::hzToMidi (hz, S.tune.a4)))) + ", " + juce::String (hz, 1) + " Hz.");
        return;
    }
}

void MainComponent::onTuneMenu (const std::string& which, float x, float y, float w, float h)
{
    if (! engineState) return;
    const auto& S = engineState->session;
    const auto opts = juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (x, y, w, h).toNearestInt())).withMinimumWidth ((int) w);
    juce::PopupMenu m;
    if (which == "from")
    {
        m.addItem (1, juce::String (S.tracks[(size_t) S.refIdx].mic) + " (lead)", true, S.tuneMic == "ref");
        for (int t = 0; t < (int) S.tracks.size(); ++t)
            if (t != S.refIdx) m.addItem (t + 2, juce::String (S.tracks[(size_t) t].mic), true, S.tuneMic == std::to_string (t));
        m.showMenuAsync (opts, [this] (int r)
        {
            if (r <= 0 || ! engineState) return;
            engineState->session.tuneMic = r == 1 ? std::string ("ref") : std::to_string (r - 2);
            onTuneAction ("read", 0);
        });
        return;
    }
    // the reading range
    struct R { const char* name; double lo, hi; };
    static const R ranges[] = { { "Kick", 40, 160 }, { "Floor tom", 60, 200 }, { "Rack tom", 80, 320 }, { "Snare", 120, 480 }, { "Piccolo snare", 200, 800 } };
    int id = 1;
    for (auto& r : ranges) m.addItem (id++, juce::String (r.name) + "  " + juce::String ((int) r.lo) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + juce::String ((int) r.hi) + " Hz", true,
                                      std::abs (S.pitchOpt.fmin - r.lo) < 0.5 && std::abs (S.pitchOpt.fmax - r.hi) < 0.5);
    m.addSeparator();
    for (int ms : { 5, 10, 15, 25, 40 }) m.addItem (100 + ms, "Skip " + juce::String (ms) + " ms of attack", true, std::abs (S.pitchOpt.skipMs - ms) < 0.5);
    m.showMenuAsync (opts, [this] (int r)
    {
        if (r <= 0 || ! engineState) return;
        auto& s = engineState->session;
        if (r >= 100) { s.pitchOpt.skipMs = r - 100; s.tuneAutoWin = false; }
        else { s.pitchOpt.fmin = ranges[r - 1].lo; s.pitchOpt.fmax = ranges[r - 1].hi; }
        onTuneAction ("read", 0);
    });
}

// ------------------------------------------------------------------------------------------- the Mix step
void MainComponent::refreshMix()
{
    if (! engineState) return;
    studio.data.mix = buildMixView (*engineState, mixSel, studio.selectedHit, refAudio.empty() ? nullptr : &refAudio, refName);
    mixHit = studio.selectedHit;
    refreshPlugin();
    studio.redrawAll();
}

void MainComponent::refreshPlugin()
{
    // the open plugin window shows its insert's settings, flattened ("decap.drive" = 3, "model" = "decap")
    ui::PluginView pv;
    if (engineState && plugT >= -1)
    {
        auto& S = engineState->session;
        const juce::var* ins = plugT < 0 ? &S.busIns : (plugT < (int) S.tracks.size() ? &S.tracks[(size_t) plugT].strip.inserts : nullptr);
        if (ins && ins->isArray() && plugK < ins->getArray()->size())
        {
            const auto& it = (*ins)[plugK];
            pv.open = true; pv.who = plugT; pv.slot = plugK;
            pv.type = it["type"].toString().toStdString(); pv.on = (bool) it["on"];
            pv.track = plugT < 0 ? std::string ("DRUM BUS") : juce::String (S.tracks[(size_t) plugT].mic).toUpperCase().toStdString();
            std::function<void (const juce::var&, const std::string&)> flat = [&] (const juce::var& o, const std::string& pre)
            {
                if (auto* d = o.getDynamicObject())
                    for (auto& kv : d->getProperties())
                    {
                        const auto key = pre + kv.name.toString().toStdString();
                        const auto& x = kv.value;
                        if (auto* arr = x.getArray())   // first: in JUCE an array also says isObject()
                        {
                            pv.num[key + ".count"] = arr->size();
                            for (int i = 0; i < arr->size(); ++i)
                            {
                                const auto& el = (*arr)[i]; const auto k2 = key + "." + std::to_string (i);
                                if (el.isObject()) flat (el, k2 + ".");
                                else if (el.isString()) pv.str[k2] = el.toString().toStdString();
                                else pv.num[k2] = (double) el;
                            }
                        }
                        else if (x.isObject()) flat (x, key + ".");
                        else if (x.isBool()) pv.num[key] = (bool) x ? 1.0 : 0.0;
                        else if (x.isInt() || x.isInt64() || x.isDouble()) pv.num[key] = (double) x;
                        else if (x.isString()) pv.str[key] = x.toString().toStdString();
                    }
            };
            flat (it["p"], "");
            if (pv.type == "pultec" || pv.type == "eq")
            {
                auto curveOf = [&] (int band)
                {
                    std::vector<float> c ((size_t) ui::PluginView::CURVE_N);
                    for (int i = 0; i < ui::PluginView::CURVE_N; ++i)
                        c[(size_t) i] = (float) hl::insertCurveDb (it["type"].toString(), it["p"], 20 * std::pow (1000.0, i / (double) (ui::PluginView::CURVE_N - 1)), band);
                    return c;
                };
                pv.curve = curveOf (-1);
                if (auto* bands = it["p"]["bands"].getArray()) for (int b = 0; b < bands->size(); ++b) pv.bandCurves.push_back (curveOf (b));
            }
            if (engineState->mix)
            {
                const auto key = (plugT < 0 ? std::string ("bus") : std::to_string (plugT)) + ":" + std::to_string (plugK);
                const auto g = engineState->mix->gr.find (key);
                if (g != engineState->mix->gr.end()) pv.gr = std::make_shared<const std::vector<float>> (g->second);
            }
        }
        else plugT = -2;
    }
    studio.setPlugin (std::move (pv));
}

void MainComponent::onSongAction (const std::string& a, double v)
{
    if (! engineState) return;
    auto& S = engineState->session;
    if (a.rfind ("prob:", 0) == 0)
    {
        // step to the next (or previous) flagged problem and play from just before it
        const auto P = hl::findProblems (S.song.an, S.song.set);
        if (P.empty()) return;
        const double now = studio.playhead; const int dir = std::stoi (a.substr (5));
        double to = -1;
        if (dir > 0) { for (auto& p : P) if (p.pos / S.sr > now + 0.07) { to = p.pos / S.sr; break; } if (to < 0) to = P.front().pos / S.sr; }
        else { for (auto it = P.rbegin(); it != P.rend(); ++it) if (it->pos / S.sr < now - 0.07) { to = it->pos / S.sr; break; } if (to < 0) to = P.back().pos / S.sr; }
        studio.onSeek (to);
        studio.setTransport (to, false);
        for (auto& p : P) if (std::abs (p.pos / S.sr - to) < 0.001) { setNotice (juce::String (p.text) + " at " + juce::String (to, 2) + " s."); break; }
        studio.view.start = std::max (0.0, to - 1.5);   // bring it into view
        studio.redrawAll();
        return;
    }
    auto change = [this] (std::function<void (hl::Session&)> op, const juce::String& what)
    {
        if (audition) audition->stop();
        const auto before = settingsSnapshot();
        op (engineState->session);
        recordChange (before, what);
        refreshModel(); updateHeader(); setNotice (what);
        rerender();
    };
    if (a.rfind ("tempo:", 0) == 0)
    {
        const auto m = a.substr (6);
        change ([m] (hl::Session& s) { s.song.set.tempoMode = m; }, m == "keep" ? "Tempo: following every push and pull." : m == "smooth" ? "Tempo: a gentle map that follows the drummer." : "Tempo: one tempo for the whole song.");
        return;
    }
    if (a == "half" || a == "x2")
    {
        const double cur = S.song.set.bpmScale, to = a == "half" ? (cur < 0.75 ? 1.0 : 0.5) : (cur > 1.5 ? 1.0 : 2.0);
        change ([to] (hl::Session& s) { s.song.set.bpmScale = to; }, to == 1.0 ? "Tempo as detected." : to < 1 ? "Half time." : "Double time.");
        return;
    }
    if (a == "tight") { change ([v] (hl::Session& s) { s.song.set.tightness = std::clamp (v, 0.0, 1.0); }, "Tightness: " + juce::String ((int) std::lround (v * 100)) + " %."); return; }
    if (a == "swing") { change ([v] (hl::Session& s) { s.song.set.swing = std::clamp (v, 0.0, 1.0); }, v <= 0 ? juce::String ("Swing off.") : "Swing: " + juce::String ((int) std::lround (v * 100)) + " %."); return; }
    if (a.rfind ("div:", 0) == 0) { const int d = std::stoi (a.substr (4)); change ([d] (hl::Session& s) { s.song.set.division = d; }, d == 8 ? "Grid: eighths." : d == 12 ? "Grid: eighth-note triplets." : "Grid: sixteenths."); return; }
    if (a == "feel") { change ([] (hl::Session& s) { s.song.set.keepFeel = ! s.song.set.keepFeel; }, S.song.set.keepFeel ? "Keep Feel off: ghost notes and flams tighten too." : "Keep Feel on: ghost notes and flams stay loose."); return; }
    if (a == "hats") { change ([] (hl::Session& s) { s.song.set.swingHatsOnly = ! s.song.set.swingHatsOnly; }, S.song.set.swingHatsOnly ? "Swing on everything." : "Swing on the hats only."); return; }
    if (a == "bleed") { change ([v] (hl::Session& s) { s.kitBleed = std::clamp (v, 0.0, 24.0); }, v <= 0 ? juce::String ("Bleed control off.") : "Bleed: close mics down " + juce::String ((int) std::lround (v)) + " dB between their own hits."); return; }
    if (a == "export") { exportTake(); return; }
    if (a.rfind ("bits:", 0) == 0) { const int b = std::stoi (a.substr (5)); change ([b] (hl::Session& s) { s.exp.bits = b; }, b == 24 ? "Export: 24-bit." : "Export: 32-bit float."); return; }
    if (a == "bus") { change ([] (hl::Session& s) { s.exp.kitBus = ! s.exp.kitBus; }, S.exp.kitBus ? "Export: no drum bus." : "Export: with the drum bus."); return; }
    if (a.rfind ("verbs:", 0) == 0) { const auto m = a.substr (6); change ([m] (hl::Session& s) { s.exp.verbs = m; }, m == "print" ? "Export: reverbs printed into the bus." : "Export: reverbs as their own files."); return; }
    if (a == "reanalyse")
    {
        if (audition) audition->stop();
        hl::analyseSongOf (S); refreshModel(); updateHeader(); rerender(); setNotice ("Hits and tempo found again.");
    }
}

void MainComponent::onMixAction (const std::string& a, double v)
{
    if (! engineState) return;
    auto& S = engineState->session;
    const int sel = mixSel >= 0 && mixSel < (int) S.tracks.size() ? mixSel : -1;
    auto change = [this] (std::function<void (hl::Session&)> op, const juce::String& what)
    {
        if (audition) audition->stop();
        const auto before = settingsSnapshot();
        op (engineState->session);
        recordChange (before, what);
        refreshModel(); refreshMix(); updateHeader(); setNotice (what);
        rerender();
    };
    const juce::String where = sel < 0 ? juce::String ("every channel") : juce::String (S.tracks[(size_t) sel].mic);
    if (a.rfind ("knob:", 0) == 0 && std::stoi (a.substr (5)) >= 11)
    {
        const int i = std::stoi (a.substr (5));
        if (i == 11)
        {
            change ([v] (hl::Session& s) { s.tone.refEnvAmt = std::clamp (v, 0.0, 1.0); }, "Match envelope: " + juce::String ((int) std::lround (v * 100)) + " %.");
            return;
        }
        const int u = i >= 17 ? 1 : 0, w = (i - 12) % 5;
        static const char* names[5] = { "decay", "pre-delay", "size", "tone", "width" };
        change ([u, w, v] (hl::Session& s)
        {
            auto& R = s.verbs[(size_t) u];
            double* f[5] = { &R.decay, &R.pre, &R.size, &R.tone, &R.width };
            *f[w] = v;
        }, juce::String (u ? "R2 " : "R1 ") + names[w] + ": " + (w == 0 ? "x" + juce::String (v, 2) : w == 1 ? juce::String ((int) std::lround (v)) + " ms" : w == 3 ? juce::String (v, 1) : juce::String ((int) std::lround (v * 100)) + " %") + ".");
        return;
    }
    if (a.rfind ("knob:", 0) == 0)
    {
        const int i = std::stoi (a.substr (5));
        static const char* names[11] = { "Pro tone", "Tone", "Crack", "Rings", "Tame", "Attack", "Decay", "Sustain", "Release", "Pro envelope", "Reference amount" };
        change ([i, v, sel] (hl::Session& s)
        {
            hl::ToneMoves& T = sel < 0 ? (hl::ToneMoves&) s.tone : s.tracks[(size_t) sel].strip.tone;
            hl::ShapeMoves& H = sel < 0 ? s.shape : s.tracks[(size_t) sel].strip.shape;
            double* f[11] = { &T.match, &T.bright, &T.crack, &T.smooth, &T.tame, &H.atk, &H.dec, &H.sus, &H.rel, &H.match, &s.tone.refAmt };
            *f[i] = v;
        }, juce::String (names[i]) + (i == 10 ? juce::String() : juce::String (" on ") + where) + ": " + (i == 1 || i == 2 || (i >= 5 && i < 9) ? (v > 0 ? "+" : "") + juce::String ((int) std::lround (v * 100)) : juce::String ((int) std::lround (v * 100)) + " %") + ".");
        return;
    }
    if (a == "src:lib" || a == "src:studio")
    {
        const bool st = a == "src:studio";
        if (st && ! hl::studioReferenceAvailable()) return;
        change ([st] (hl::Session& s) { s.tone.proSrc = st ? "panda" : ""; }, st ? "Pro tone pulls toward the measured studio set." : "Pro tone pulls toward the average of the pro libraries.");
        return;
    }
    if (a == "start")
    {
        // Standard: the knobs set to land near where the pro libraries put this channel, then yours to move
        // (it listens to every mic first, off the message thread, on a copy)
        if (S.onsets.empty()) { setNotice ("Run Auto Prep first."); return; }
        setNotice (juce::CharPointer_UTF8 ("Standard: listening to the mics\xe2\x80\xa6"));
        auto copy = std::make_shared<hl::Session> (S);
        std::thread ([copy, sel, where, st = engineState, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            auto L = *copy; L.tone.match = std::max (0.01, L.tone.match);   // the tone pass only listens while tone is live
            const auto M = hl::renderMix (L);
            const auto res = hl::standardMoves (*copy, M, sel);
            juce::MessageManager::callAsync ([sp, st, res, sel, where]
            {
                if (sp == nullptr || sp->engineState != st) return;
                const auto [T, H] = res;
                if (sp->audition) sp->audition->stop();
                auto& s = st->session;
                hl::ToneMoves& tm = sel < 0 ? (hl::ToneMoves&) s.tone : s.tracks[(size_t) sel].strip.tone;
                hl::ShapeMoves& hm = sel < 0 ? s.shape : s.tracks[(size_t) sel].strip.shape;
                tm.match = 0; tm.bright = T.bright; tm.crack = T.crack; tm.smooth = T.smooth; tm.tame = T.tame;
                hm = H;
                sp->refreshModel(); sp->refreshMix(); sp->updateHeader();
                sp->setNotice ("Standard on " + where + ": tone " + juce::String ((int) std::lround (T.bright * 100)) + ", crack " + juce::String ((int) std::lround (T.crack * 100))
                               + ", rings " + juce::String ((int) std::lround (T.smooth * 100)) + " %, tame " + juce::String ((int) std::lround (T.tame * 100))
                               + " %, sustain " + juce::String ((int) std::lround (H.sus * 100)) + ". Move any knob from here.");
                sp->rerender();
            });
        }).detach();
        return;
    }
    if (a == "flat")
    {
        change ([] (hl::Session& s)
        {
            s.tone.match = s.tone.bright = s.tone.crack = s.tone.smooth = s.tone.tame = 0;
            s.shape.atk = s.shape.dec = s.shape.sus = s.shape.rel = s.shape.match = 0;
        }, "Everything flat.");
        return;
    }
    if (a == "ab")
    {
        if (! audition || ! engineState->mix) return;
        if (edit) edit->getTransport().stop (false, false);
        Audition::Options o; o.chain = true;
        audition->play (modes::hit (S, std::max (0, studio.selectedHit)), o);
        setNotice ("As recorded, then with the chain.");
        updateHeader();
        return;
    }
    // the mixer: select a strip, sends, the drum bus fader and mute, insert bypass
    if (a.rfind ("sel:", 0) == 0)
    {
        const auto w = a.substr (4);
        mixSel = w == "bus" ? -1 : std::stoi (w);
        refreshMix();
        return;
    }
    auto whoOf = [&] (const std::string& w) { return w == "bus" ? -1 : std::stoi (w); };
    if (a.rfind ("send:", 0) == 0)
    {
        const auto rest = a.substr (5); const auto c = rest.find (':');
        const int t = whoOf (rest.substr (0, c)), k = std::stoi (rest.substr (c + 1));
        const double db = std::round (std::clamp (v, -60.0, 6.0) * 2) / 2;
        change ([t, k, db] (hl::Session& s)
        {
            double& d = t < 0 ? (k ? s.busR2 : s.busR1) : (k ? s.tracks[(size_t) t].strip.r2 : s.tracks[(size_t) t].strip.r1);
            d = db;
        }, juce::String (k ? "R2" : "R1") + " send on " + (t < 0 ? juce::String ("the drum bus") : juce::String (S.tracks[(size_t) t].mic)) + ": "
           + (db <= -59.5 ? juce::String ("off") : juce::String (db, 1) + " dB") + ".");
        return;
    }
    if (a == "busfader")
    {
        const double db = std::round (std::clamp (v, -100.0, 6.0) * 10) / 10;
        change ([db] (hl::Session& s) { s.busFader = db; }, "Drum bus fader: " + (db <= -99 ? juce::String ("-inf") : juce::String (db, 1) + " dB") + ".");
        return;
    }
    if (a == "busmute")
    {
        change ([v] (hl::Session& s) { s.busMute = v > 0.5; }, v > 0.5 ? "Drum bus muted." : "Drum bus on.");
        return;
    }
    if (a.rfind ("plugband:", 0) == 0)
    {
        // Prism's bands: add ("plugband:add:<type>:<freq>") or delete ("plugband:del:<n>")
        if (plugT < -1) return;
        const int t = plugT, k = plugK;
        juce::StringArray parts; parts.addTokens (juce::String (a), ":", "");
        const bool add = parts[1] == "add";
        const juce::String type = parts[2]; const double f = parts[3].getDoubleValue(); const int del = parts[2].getIntValue();
        change ([t, k, add, type, f, del] (hl::Session& s)
        {
            juce::var& ii = t < 0 ? s.busIns : s.tracks[(size_t) t].strip.inserts;
            if (! ii.isArray() || k >= ii.getArray()->size()) return;
            auto* po = ii[k]["p"].getDynamicObject(); if (! po) return;
            juce::var bands = po->getProperty ("bands");
            if (! bands.isArray()) { bands = juce::var (juce::Array<juce::var>()); po->setProperty ("bands", bands); }
            auto* arr = bands.getArray();
            if (add)
            {
                if (arr->size() >= 6) return;
                auto* b = new juce::DynamicObject();
                b->setProperty ("type", type); b->setProperty ("f", f); b->setProperty ("g", 0.0); b->setProperty ("q", type == "bell" ? 1.0 : 0.71);
                b->setProperty ("on", true); if (type == "lowcut" || type == "highcut") b->setProperty ("slope", 24);
                arr->add (juce::var (b));
            }
            else if (del >= 0 && del < arr->size()) arr->remove (del);
        }, add ? "Prism: band added." : "Prism: band removed.");
        return;
    }
    if (a.rfind ("plug", 0) == 0)
    {
        // the plugin window: open / close / bypass, and setting one of its values by path ("decap.drive")
        if (a.rfind ("plug:open:", 0) == 0)
        {
            const auto rest = a.substr (10); const auto c = rest.find (':');
            plugT = whoOf (rest.substr (0, c)); plugK = std::stoi (rest.substr (c + 1));
            refreshPlugin(); return;
        }
        if (a == "plug:close") { plugT = -2; refreshPlugin(); return; }
        if (plugT < -1) return;
        const int t = plugT, k = plugK;
        if (a == "plug:byp") { onMixAction ("insbyp:" + (t < 0 ? std::string ("bus") : std::to_string (t)) + ":" + std::to_string (k), 0); return; }
        const bool isStr = a.rfind ("plugstr:", 0) == 0;
        std::string path, sval;
        if (isStr) { const auto rest = a.substr (8); const auto c = rest.rfind (':'); path = rest.substr (0, c); sval = rest.substr (c + 1); }
        else if (a.rfind ("plug:set:", 0) == 0) path = a.substr (9);
        else return;
        double val = v;
        // Skull Cutter's cut knobs park at "off" (0) at their ends
        if (path == "decap.lowcut" && val <= 20.5) val = 0;
        if (path == "decap.highcut" && val >= 19900) val = 0;
        change ([t, k, path, sval, isStr, val] (hl::Session& s)
        {
            juce::var& ii = t < 0 ? s.busIns : s.tracks[(size_t) t].strip.inserts;
            if (! ii.isArray() || k >= ii.getArray()->size()) return;
            juce::var node = ii[k]["p"];
            juce::StringArray parts; parts.addTokens (juce::String (path), ".", "");
            for (int i = 0; i + 1 < parts.size(); ++i)
            {
                if (node.isArray())
                {
                    const int idx = parts[i].getIntValue();
                    if (idx < 0 || idx >= node.getArray()->size()) return;
                    node = (*node.getArray())[idx];
                    continue;
                }
                juce::var nx = node[juce::Identifier (parts[i])];
                if (! nx.isObject()) { nx = juce::var (new juce::DynamicObject()); if (auto* o = node.getDynamicObject()) o->setProperty (parts[i], nx); }
                node = nx;
            }
            auto* o = node.getDynamicObject(); if (! o) return;
            const juce::Identifier leaf (parts[parts.size() - 1]);
            if (path == "keyT" && val < 0) { o->removeProperty (leaf); return; }
            if (isStr) o->setProperty (leaf, juce::String (sval));
            else if (o->getProperty (leaf).isBool()) o->setProperty (leaf, val > 0.5);
            else o->setProperty (leaf, val);
        }, [&]
        {
            // the notice in words: "Skull Cutter: Drive 6.5", "Destroy on", "Style III"
            static const std::map<std::string, std::string> names {
                { "decap.drive", "Drive" }, { "devil.crush", "Drive" }, { "saturn.drive", "Drive" }, { "fatso.input", "Drive" },
                { "decap.tone", "Tone" }, { "devil.darkness", "Darkness" }, { "saturn.tone", "Tone" }, { "fatso.warmth", "Warmth" },
                { "decap.lowcut", "Low cut" }, { "decap.highcut", "High cut" }, { "mix", "Mix" }, { "out", "Output" },
                { "decap.punish", "Destroy" }, { "decap.thump", "Boom" }, { "decap.steep", "Sploosh" }, { "auto", "Auto gain" },
                { "model", "Model" }, { "decap.style", "Style" }, { "saturn.style", "Style" }, { "fatso.comp", "Comp" },
                // The Vise
                { "dist.input", "Input" }, { "76.input", "Input" }, { "fair.input", "Input" }, { "160.thresh", "Threshold" }, { "chan.thresh", "Threshold" },
                { "dist.attack", "Attack" }, { "76.attack", "Attack" }, { "chan.attack", "Attack" }, { "fair.thresh", "Threshold" },
                { "dist.release", "Release" }, { "76.release", "Release" }, { "chan.release", "Release" }, { "fair.tc", "Time constant" },
                { "dist.ratio", "Ratio" }, { "76.ratio", "Ratio" }, { "160.ratio", "Ratio" }, { "chan.ratio", "Ratio" }, { "scHpf", "Side-chain HPF" },
                // Portcullis
                { "thr", "Threshold" }, { "range", "Range" }, { "ratio", "Ratio" }, { "hyst", "Hysteresis" }, { "atk", "Attack" }, { "hold", "Hold" },
                { "rel", "Release" }, { "look", "Look-ahead" }, { "hpf", "Key HPF" }, { "lpf", "Key LPF" }, { "listen", "Key listen" }, { "flip", "Flip" }, { "keyT", "Key" },
                { "phase", "Phase" }, { "out", "Output" },
                { "width", "Width" }, { "bassMono", "Bass mono" }, { "highWidth", "High width" }, { "highFrom", "Highs from" }, { "keep", "Keep loudness" },
                // Ember
                { "lf", "Low frequency" }, { "lfBoost", "Low boost" }, { "lfAtten", "Low atten" }, { "hf", "High frequency" }, { "bw", "Bandwidth" },
                { "hfBoost", "High boost" }, { "hfa", "Atten select" }, { "hfAtten", "High atten" }, { "drive", "Drive" }, { "eqp", "EQP section" },
                { "meq", "Mid EQ section" }, { "tube", "Tube" }, { "phase", "Phase" } };
            const auto& arr = t < 0 ? S.busIns : S.tracks[(size_t) t].strip.inserts;
            const juce::String plug (insertDisplayName (arr[k]["type"].toString()));
            std::string n = names.count (path) ? names.at (path) : path;
            if (path.rfind ("bands.", 0) == 0)
            {
                // "bands.2.dyn.thr" -> "Band 3 dynamic threshold"
                const auto d1 = path.find ('.', 6); const int bi = std::atoi (path.substr (6, d1 - 6).c_str());
                const auto w = path.substr (d1 + 1);
                static const std::map<std::string, std::string> bw { { "f", "frequency" }, { "g", "gain" }, { "q", "Q" }, { "on", "" }, { "type", "shape" },
                    { "dyn.on", "dynamic" }, { "dyn.thr", "dynamic threshold" }, { "dyn.range", "dynamic range" } };
                n = "Band " + std::to_string (bi + 1) + (bw.count (w) ? (bw.at (w).empty() ? "" : " " + bw.at (w)) : " " + w);
            }
            juce::String val2;
            if (isStr) val2 = juce::String (sval).toUpperCase();
            else if (path == "decap.punish" || path == "decap.thump" || path == "decap.steep" || path == "auto" || path == "listen" || path == "flip" || path == "eqp" || path == "meq" || path == "tube" || path == "keep"
                     || (path.rfind ("bands.", 0) == 0 && (path.size() > 3 && (path.substr (path.size() - 3) == ".on")))) val2 = val > 0.5 ? "on" : "off";
            else if (path == "keyT") val2 = val < 0 ? juce::String ("self") : juce::String (S.tracks[(size_t) (int) val].mic);
            else if ((path == "decap.lowcut" || path == "decap.highcut") && val == 0) val2 = "off";
            else if (path == "mix" || path == "width" || path == "highWidth") val2 = juce::String (juce::roundToInt (val * 100)) + " %";
            else if ((path == "bassMono" && val < 20) || (path == "highFrom" && val <= 0)) val2 = "off";
            else val2 = juce::String (val, 1);
            return plug + ": " + juce::String (n) + " " + val2 + ".";
        }());
        return;
    }
    if (a.rfind ("insbyp:", 0) == 0)
    {
        const auto rest = a.substr (7); const auto c = rest.find (':');
        const int t = whoOf (rest.substr (0, c)), k = std::stoi (rest.substr (c + 1));
        const juce::var& ins = t < 0 ? S.busIns : S.tracks[(size_t) t].strip.inserts;
        auto* arr = ins.getArray();
        if (! arr || k >= arr->size()) return;
        const bool on = ! (bool) (*arr)[k]["on"];
        change ([t, k, on] (hl::Session& s)
        {
            juce::var& ii = t < 0 ? s.busIns : s.tracks[(size_t) t].strip.inserts;
            if (auto* o = ii[k].getDynamicObject()) o->setProperty ("on", on);
        }, juce::String (insertDisplayName ((*arr)[k]["type"].toString())) + (on ? " on." : " bypassed."));
        return;
    }
    if (a == "ins")
    {
        const int k = (int) v;
        juce::var& ins = sel < 0 ? S.busIns : S.tracks[(size_t) sel].strip.inserts;
        auto* arr = ins.getArray();
        if (! arr || k >= arr->size()) { setNotice ("Adding inserts comes with the insert editors, next."); return; }
        const bool on = ! (bool) (*arr)[k]["on"];
        change ([sel, k, on] (hl::Session& s)
        {
            juce::var& ii = sel < 0 ? s.busIns : s.tracks[(size_t) sel].strip.inserts;
            if (auto* o = ii[k].getDynamicObject()) o->setProperty ("on", on);
        }, (*arr)[k]["type"].toString().toUpperCase() + (on ? " on." : " bypassed."));
        return;
    }
    if (a == "ref:load")
    {
        refChooser = std::make_unique<juce::FileChooser> ("Load a reference", juce::File(), "*.wav");
        refChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile() || ! engineState || ! engineState->mix) return;
            auto R = hl::loadReference (engineState->session, *engineState->mix, f);
            if (! R.error.empty() || R.curve.empty()) { setNotice ("Couldn't read that reference: " + juce::String (R.error)); return; }
            refAudio = R.audio; refName = R.name;
            auto curve = R.curve;
            if (audition) audition->stop();
            auto& s = engineState->session;
            s.tone.hasRef = true; s.tone.refCurve = curve; if (s.tone.refAmt == 0) s.tone.refAmt = 0.6;
            s.tone.hasRefEnv = R.hasEnv; s.tone.refEnv = R.env; if (R.hasEnv && s.tone.refEnvAmt == 0) s.tone.refEnvAmt = 0.5;
            refreshModel(); refreshMix(); updateHeader();
            setNotice ("Reference loaded: " + juce::String (refName) + ". The mix moves toward it at " + juce::String ((int) std::lround (s.tone.refAmt * 100)) + " %.");
            rerender();
        });
        return;
    }
    if (a == "ref:remove")
    {
        refAudio.clear(); refName.clear();
        change ([] (hl::Session& s) { s.tone.hasRef = false; s.tone.refCurve.clear(); s.tone.hasRefEnv = false; }, "Reference removed.");
        return;
    }
    if (a == "ref:play")
    {
        if (refAudio.empty()) { setNotice ("Load the reference again to hear it."); onMixAction ("ref:load", 0); return; }
        if (! audition || ! engineState->mix) return;
        if (edit) edit->getTransport().stop (false, false);
        audition->playReference (refAudio, std::max (0, studio.selectedHit));
        setNotice ("The reference, then this hit through the mix, at the same loudness.");
        updateHeader();
        return;
    }
}

void MainComponent::onMixMenu (const std::string& what, float x, float y, float w, float h)
{
    if (! engineState) return;
    const auto& S = engineState->session;
    const auto opts = juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (x, y, w, h).toNearestInt())).withMinimumWidth ((int) w);
    juce::PopupMenu m;
    if (what == "verb0" || what == "verb1")
    {
        const int u = what.back() - '0';
        static const char* types[4] = { "room", "plate", "hall", "chamber" };
        static const char* labels[4] = { "Room", "Plate", "Hall", "Chamber" };
        m.addSectionHeader (u ? "R2" : "R1");
        for (int i = 0; i < 4; ++i) m.addItem (i + 1, labels[i], true, S.verbs[(size_t) u].type == types[i]);
        m.showMenuAsync (opts, [this, u] (int r)
        {
            if (r <= 0 || ! engineState) return;
            if (audition) audition->stop();
            engineState->session.verbs[(size_t) u].type = types[r - 1];
            refreshModel(); refreshMix(); updateHeader();
            setNotice (juce::String (u ? "R2: " : "R1: ") + labels[r - 1] + ".");
            rerender();
        });
        return;
    }
    if (what == "plugkey")
    {
        // the gate's key: itself, or another mic
        if (plugT < -1) return;
        m.addItem (1, "Self", true);
        for (int t = 0; t < (int) S.tracks.size(); ++t) if (t != plugT) m.addItem (t + 2, juce::String (S.tracks[(size_t) t].mic));
        m.showMenuAsync (opts, [this] (int r)
        {
            if (r <= 0) return;
            onMixAction ("plug:set:keyT", r == 1 ? -1.0 : (double) (r - 2));
        });
        return;
    }
    if (what.rfind ("ins:", 0) == 0)
    {
        // an insert slot: the built-in plugins (and, filled, open / bypass / remove)
        const auto rest = what.substr (4); const auto c = rest.find (':');
        const int t = rest.substr (0, c) == "bus" ? -1 : std::stoi (rest.substr (0, c)), k = std::stoi (rest.substr (c + 1));
        const juce::var& ins = t < 0 ? S.busIns : S.tracks[(size_t) t].strip.inserts;
        const int filled = ins.getArray() ? ins.getArray()->size() : 0;
        const bool has = k < filled;
        static const char* types[6] = { "eq", "pultec", "comp", "sat", "gate", "width" };
        static const char* blurb[6] = { "Prism  (EQ)", "Ember  (Tube EQ)", "The Vise  (Compressor)", "Skull Cutter  (Saturation)", "Portcullis  (Gate)", "Horizon  (Stereo width)" };
        const bool stereo = t < 0 || (t < (int) S.tracks.size() && S.tracks[(size_t) t].ch.size() > 1);
        juce::PopupMenu add;
        for (int i = 0; i < 6; ++i)
            if (i != 5 || stereo) add.addItem (10 + i, blurb[i]);
        add.addSeparator();
        add.addItem (30, "Third Party Plugin...  (coming with plugin hosting)", false);
        if (has)
        {
            const auto cur = ins[k]["type"].toString();
            m.addSectionHeader (insertDisplayName (cur));
            m.addItem (1, "Open");
            m.addItem (2, (bool) ins[k]["on"] ? "Bypass" : "Turn on");
            m.addSubMenu ("Replace with", add);
            m.addSeparator();
            m.addItem (3, "Remove");
        }
        else
        {
            m.addSectionHeader ("Insert " + juce::String (std::min (k, filled) + 1));
            for (int i = 0; i < 6; ++i)
                if (i != 5 || stereo) m.addItem (10 + i, blurb[i]);
            m.addSeparator();
            m.addItem (30, "Third Party Plugin...  (coming with plugin hosting)", false);
        }
        m.showMenuAsync (opts, [this, t, k, has] (int r)
        {
            if (r <= 0 || ! engineState) return;
            auto& s0 = engineState->session;
            auto mutate = [&] (std::function<void (juce::Array<juce::var>&)> op, const juce::String& what2)
            {
                if (audition) audition->stop();
                const auto before = settingsSnapshot();
                juce::var& ii = t < 0 ? s0.busIns : s0.tracks[(size_t) t].strip.inserts;
                if (! ii.isArray()) ii = juce::var (juce::Array<juce::var>());
                op (*ii.getArray());
                recordChange (before, what2);
                refreshModel(); refreshMix(); updateHeader(); setNotice (what2);
                rerender();
            };
            const std::string who = t < 0 ? std::string ("bus") : std::to_string (t);
            if (r == 1) { onMixAction ("plug:open:" + who + ":" + std::to_string (k), 0); return; }
            if (r == 2) { onMixAction ("insbyp:" + (t < 0 ? std::string ("bus") : std::to_string (t)) + ":" + std::to_string (k), 0); return; }
            if (r == 3) { mutate ([k] (juce::Array<juce::var>& a) { if (k < a.size()) a.remove (k); }, "Insert removed."); return; }
            if (r >= 10 && r < 16)
            {
                const juce::String type (types[r - 10]);
                mutate ([k, has, type] (juce::Array<juce::var>& a)
                {
                    if (has && k < a.size()) a.set (k, insertDefaults (type));
                    else if (a.size() < 5) a.add (insertDefaults (type));
                }, juce::String (insertDisplayName (type)) + (has ? " replaces it." : " added."));
                const auto& ii = t < 0 ? engineState->session.busIns : engineState->session.tracks[(size_t) t].strip.inserts;
                const int n = ii.getArray() ? ii.getArray()->size() : 0;
                onMixAction ("plug:open:" + who + ":" + std::to_string (has ? k : n - 1), 0);
            }
        });
        return;
    }
    m.addItem (1, "Bus (every channel)", true, mixSel < 0);
    m.addSeparator();
    for (int t = 0; t < (int) S.tracks.size(); ++t) m.addItem (t + 2, juce::String (S.tracks[(size_t) t].mic), true, mixSel == t);
    m.showMenuAsync (opts, [this] (int r) { if (r <= 0) return; mixSel = r == 1 ? -1 : r - 2; refreshMix(); });
}
} // namespace ld

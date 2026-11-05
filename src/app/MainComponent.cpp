#include "MainComponent.h"
#include "BinaryData.h"

namespace ld
{
namespace te = tracktion;

//==============================================================================
void LookAndFeel::setPalette (const theme::Palette& p)
{
    pal = p;
    setColour (juce::TextButton::textColourOffId, p.ink);
    setColour (juce::TextButton::textColourOnId, p.ground);
    setColour (juce::ComboBox::backgroundColourId, p.raised);
    setColour (juce::ComboBox::textColourId, p.ink);
    setColour (juce::ComboBox::outlineColourId, p.line);
    setColour (juce::ComboBox::arrowColourId, p.inkQuiet);
    setColour (juce::PopupMenu::backgroundColourId, p.panel);
    setColour (juce::PopupMenu::textColourId, p.ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.select.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, p.ink);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? pal.signal : pal.raised;
    if (over) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, theme::radius::m - 2);
    g.setColour (on ? pal.signal.brighter (0.2f) : pal.line);
    g.drawRoundedRectangle (r, theme::radius::m - 2, 1.0f);
}

//==============================================================================
static std::unique_ptr<juce::Drawable> loadSvg (const char* data, int size)
{
    return juce::Drawable::createFromImageData (data, (size_t) size);
}

MainComponent::MainComponent (te::Engine& e) : engine (e)
{
    setLookAndFeel (&lnf);
    lockupNight = loadSvg (BinaryData::lockuphorizontalnight_svg, BinaryData::lockuphorizontalnight_svgSize);
    lockupDay = loadSvg (BinaryData::lockuphorizontalday_svg, BinaryData::lockuphorizontalday_svgSize);

    for (auto* b : { &importButton, &playButton, &startButton, &fitButton, &themeButton })
    {
        addAndMakeVisible (b);
        b->setWantsKeyboardFocus (false);
    }
    addChildComponent (takeBox);
    takeBox.setWantsKeyboardFocus (false);

    importButton.onClick = [this] { chooseFiles(); };
    playButton.onClick = [this] { togglePlay(); };
    startButton.onClick = [this] { if (edit) edit->getTransport().setPosition (te::TimePosition()); };
    fitButton.onClick = [this] { if (view) view->zoomToFit(); };
    themeButton.onClick = [this] { dayMode = ! dayMode; applyTheme(); };
    takeBox.onChange = [this] { const int i = takeBox.getSelectedItemIndex(); if (i != currentTake && i >= 0) openTake (i); };

    auto& dm = engine.getDeviceManager().deviceManager;
    if (auto* dev = dm.getCurrentAudioDevice())
        deviceText = dev->getName() + "  ·  " + juce::String (dev->getCurrentSampleRate() / 1000.0, 1) + " kHz";
    else
        deviceText = "No audio output";

    setWantsKeyboardFocus (true);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
#endif
    applyTheme();
    startTimerHz (20);
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    stopTimer();
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
#endif
    history = nullptr;
    view = nullptr;
    if (edit) edit->getTransport().stop (false, true);
    edit = nullptr;
    setLookAndFeel (nullptr);
}

void MainComponent::applyTheme()
{
    lnf.setPalette (palette());
    themeButton.setButtonText (dayMode ? "Night" : "Day");
    sendLookAndFeelChange();
    repaint();
    if (view) view->repaint();
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

void MainComponent::filesDropped (const juce::StringArray& paths, int, int)
{
    dragOver = false;
    juce::Array<juce::File> items;
    for (auto& p : paths) items.add (juce::File (p));
    importItems (items);
}

void MainComponent::importItems (const juce::Array<juce::File>& items)
{
    for (auto& f : items)
        if (f.hasFileExtension (project::extension))
            return openProject (f);

    auto planned = planImport (items);

    if (planned.empty())
    {
        notice = "No WAV or AIFF files found there.";
        repaint();
        return;
    }

    takes = std::move (planned);
    takeBox.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) takes.size(); ++i)
        takeBox.addItem (takes[(size_t) i].title + "  (" + juce::String (takes[(size_t) i].mics.size()) + ")", i + 1);
    takeBox.setVisible (takes.size() > 1);
    currentTake = -1;
    openTake (0);
}

void MainComponent::closeEdit()
{
    history = nullptr;
    view = nullptr;
    if (edit)
    {
        edit->getTransport().stop (false, true);
        project::autosaveFileFor (projectFile, sessionKey).deleteFile();
    }
    edit = nullptr;
}

void MainComponent::showEdit (std::unique_ptr<te::Edit> newEdit, std::vector<MicRole> roles)
{
    edit = std::move (newEdit);
    currentRoles = std::move (roles);
    project::storeRoles (*edit, currentRoles);
    edit->resetChangedStatus();
    edit->getUndoManager().clearUndoHistory();

    view = std::make_unique<SessionView> (*edit, currentRoles, [this] { return palette(); });
    addAndMakeVisible (*view);
    history = std::make_unique<HistoryPanel> (edit->getUndoManager(), [this] { return palette(); });
    addChildComponent (*history);
    history->setVisible (showHistory);
    resized();
    view->zoomToFit();
    grabKeyboardFocus();
    updateTitle();
    repaint();

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
    takeBox.setSelectedItemIndex (index, juce::dontSendNotification);

    if (newEdit == nullptr)
    {
        notice = "Couldn't open " + take.title + ".";
        repaint();
        return;
    }

    notice = take.warnings.joinIntoString ("  ");
    if (! take.references.empty())
        notice = (notice.isNotEmpty() ? notice + "  " : juce::String())
                 + juce::String ((int) take.references.size()) + " one-shot reference found (used later for reference matching).";

    showEdit (std::move (newEdit), take.roles);
}

void MainComponent::openProject (const juce::File& f)
{
    std::vector<MicRole> roles;
    juce::StringArray missing;
    auto loaded = project::open (engine, f, roles, missing);
    if (loaded == nullptr)
    {
        notice = "Couldn't open " + f.getFileName() + ".";
        repaint();
        return;
    }

    closeEdit();
    takes.clear();
    takeBox.setVisible (false);
    projectFile = f;
    notice = missing.isEmpty() ? juce::String()
                               : juce::String (missing.size()) + " audio file(s) are missing: " + missing.joinIntoString (", ");
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
            notice = "Saved " + f.getFileName();
        }
        else
        {
            notice = "Couldn't save to " + f.getFullPathName();
        }
        updateTitle();
        repaint();
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
    repaint();
    if (view) view->repaint();
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

    files.sort();
    auto newest = files.getLast();
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
            notice = "Restored unsaved work. Save to keep it.";
            updateTitle();
            repaint();
        }
        for (auto& f : files) if (f != newest || result != 1) f.deleteFile();
    });
}

//==============================================================================
enum MenuIDs { miNew = 1, miOpen, miImport, miSave, miSaveAs, miUndo, miRedo, miHistory, miTheme, miFit };

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    const bool hasEdit = edit != nullptr;
    if (index == 0)
    {
        m.addItem (miOpen, "Open Project\u2026\tCmd+O");
        m.addItem (miImport, "Import Tracks\u2026\tCmd+I");
        m.addSeparator();
        m.addItem (miSave, "Save\tCmd+S", hasEdit);
        m.addItem (miSaveAs, "Save As\u2026\tShift+Cmd+S", hasEdit);
    }
    else if (index == 1)
    {
        const bool canUndo = hasEdit && edit->getUndoManager().canUndo();
        const bool canRedo = hasEdit && edit->getUndoManager().canRedo();
        m.addItem (miUndo, canUndo ? "Undo " + edit->getUndoManager().getUndoDescription() + "\tCmd+Z" : juce::String ("Undo\tCmd+Z"), canUndo);
        m.addItem (miRedo, canRedo ? "Redo " + edit->getUndoManager().getRedoDescription() + "\tShift+Cmd+Z" : juce::String ("Redo\tShift+Cmd+Z"), canRedo);
    }
    else
    {
        m.addItem (miHistory, "Show History", true, showHistory);
        m.addItem (miTheme, dayMode ? "Night Mode" : "Day Mode");
        m.addItem (miFit, "Zoom to Fit\tF", hasEdit);
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
        case miHistory: showHistory = ! showHistory; if (history) history->setVisible (showHistory); resized(); break;
        case miTheme:   dayMode = ! dayMode; applyTheme(); break;
        case miFit:     if (view) view->zoomToFit(); break;
        default: break;
    }
}

void MainComponent::togglePlay()
{
    if (! edit) return;
    auto& t = edit->getTransport();
    if (t.isPlaying()) t.stop (false, false);
    else t.play (false);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey)        { togglePlay(); return true; }
    if (k == juce::KeyPress::returnKey && edit) { edit->getTransport().setPosition (te::TimePosition()); return true; }
    if (k.getTextCharacter() == '=' && view)   { view->zoomBy (1.5, edit->getTransport().getPosition().inSeconds()); return true; }
    if (k.getTextCharacter() == '-' && view)   { view->zoomBy (1.0 / 1.5, edit->getTransport().getPosition().inSeconds()); return true; }
    if (k.getTextCharacter() == 'f' && view)   { view->zoomToFit(); return true; }
    const auto mods = k.getModifiers();
    if (mods.isCommandDown())
    {
        const int c = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) k.getKeyCode());
        if (c == 'O') { chooseProject(); return true; }
        if (c == 'I') { chooseFiles(); return true; }
        if (c == 'S') { saveProject (mods.isShiftDown()); return true; }
        if (c == 'Z') { undo (mods.isShiftDown()); return true; }
    }
    return false;
}

void MainComponent::timerCallback()
{
    const bool playing = edit && edit->getTransport().isPlaying();
    playButton.setToggleState (playing, juce::dontSendNotification);
    playButton.setButtonText (playing ? "Stop" : "Play");
    if (edit) repaint (0, 0, getWidth(), 56);

    if (edit)
    {
        const bool dirty = edit->hasChangedSinceSaved();
        if (dirty != lastDirty) { lastDirty = dirty; updateTitle(); }
        if (++autosaveTicks >= 20 * 30)   // every 30 s
        {
            autosaveTicks = 0;
            project::autosave (*edit, project::autosaveFileFor (projectFile, sessionKey));
        }
    }
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    const auto p = palette();
    g.fillAll (p.ground);

    // Toolbar: a machined faceplate with screws, like a rack unit
    const bool day = dayMode;
    auto bar = getLocalBounds().removeFromTop (56).toFloat();
    g.setGradientFill (juce::ColourGradient (p.raised.brighter (day ? 0.08f : 0.1f), 0, 0, p.panel.darker (day ? 0.04f : 0.3f), 0, bar.getBottom(), false));
    g.fillRect (bar);
    // brushed texture: faint horizontal streaks
    juce::Random grain (7);
    for (int y = 2; y < (int) bar.getBottom() - 2; y += 2)
    {
        g.setColour ((grain.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (day ? 0.025f : 0.035f));
        g.fillRect (0.0f, (float) y, bar.getWidth(), 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (day ? 0.6f : 0.07f));
    g.fillRect (0.0f, 0.0f, bar.getWidth(), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (day ? 0.25f : 0.7f));
    g.fillRect (0.0f, bar.getBottom() - 1.0f, bar.getWidth(), 1.0f);

    auto screw = [&] (float cx, float cy)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9bec6), cx - 3, cy - 3, juce::Colour (0xff4a4e55), cx + 3, cy + 3, false));
        g.fillEllipse (cx - 4, cy - 4, 8, 8);
        g.setColour (juce::Colour (0xff22252a));
        g.drawLine (cx - 2.5f, cy - 2.5f, cx + 2.5f, cy + 2.5f, 1.2f);
    };
    screw (7, 9); screw (7, 47); screw (bar.getWidth() - 7, 9); screw (bar.getWidth() - 7, 47);

    if (edit)
    {
        // LED time display in a recessed glass window
        auto lcd = juce::Rectangle<float> (300, 9, 196, 38);
        g.setColour (theme::glass::deep);
        g.fillRoundedRectangle (lcd, 5.0f);
        g.setColour (theme::glass::rim);
        g.drawRoundedRectangle (lcd, 5.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0, lcd.getY(), juce::Colours::transparentBlack, 0, lcd.getY() + 8, false));
        g.fillRect (lcd.withHeight (8).reduced (1, 0));

        const double pos = edit->getTransport().getPosition().inSeconds();
        const int mins = (int) (pos / 60.0);
        const auto text = juce::String (mins) + ":" + juce::String (std::fmod (pos, 60.0), 3).paddedLeft ('0', 6);
        const auto led = edit->getTransport().isPlaying() ? theme::neon::lime : theme::neon::amber;
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold));
        g.setColour (led.withAlpha (0.10f));
        g.drawText ("8:88.888", lcd.reduced (14, 0), juce::Justification::centredRight);   // unlit segments
        for (auto [dx, dy, a] : { std::tuple { -1.0f, 0.0f, 0.18f }, { 1.0f, 0.0f, 0.18f }, { 0.0f, -1.0f, 0.18f }, { 0.0f, 1.0f, 0.18f } })
        {
            g.setColour (led.withAlpha (a));
            g.drawText (text, lcd.reduced (14, 0).translated (dx, dy), juce::Justification::centredRight);
        }
        g.setColour (led);
        g.drawText (text, lcd.reduced (14, 0), juce::Justification::centredRight);
    }

    if (lockupNight != nullptr && edit)
        (dayMode ? lockupDay : lockupNight)->drawWithin (g, juce::Rectangle<float> ((float) getWidth() - 380, 14, 150, 28),
                                                        juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid, 1.0f);

    // Footer
    auto footer = getLocalBounds().removeFromBottom (26);
    g.setColour (p.panel);
    g.fillRect (footer);
    g.setColour (p.inkQuiet);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ((notice.isNotEmpty() ? notice + "     " : juce::String()) + deviceText + "  ·  Limitless Drums " + LD_VERSION_STRING,
                footer.reduced (16, 0), juce::Justification::centredLeft, true);

    if (! edit)
    {
        // Empty state: the logo and a drop target
        auto area = getLocalBounds().withTrimmedTop (56).withTrimmedBottom (26).reduced (theme::space::xl);
        auto panel = area.withSizeKeepingCentre (std::min (area.getWidth(), 760), std::min (area.getHeight(), 420)).toFloat();
        g.setColour (dragOver ? p.select.withAlpha (0.12f) : p.panel);
        g.fillRoundedRectangle (panel, theme::radius::l);
        if (dragOver)
        {
            g.setColour (p.select);
            g.drawRoundedRectangle (panel.reduced (1.0f), theme::radius::l, 2.0f);
        }

        if (auto* lockup = dayMode ? lockupDay.get() : lockupNight.get())
            lockup->drawWithin (g, panel.withTrimmedBottom (150).reduced (110.0f, 50.0f), juce::RectanglePlacement::centred, 1.0f);

        g.setColour (p.ink);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText ("Drop a folder of drum tracks here", panel.withTrimmedTop (panel.getHeight() - 140).withHeight (28),
                    juce::Justification::centred);
        g.setColour (p.inkQuiet);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("WAV or AIFF from any DAW. Tracks sort into kick, snare, toms, overheads, then rooms, and get renamed for you.",
                    panel.withTrimmedTop (panel.getHeight() - 108).withHeight (22).reduced (24, 0), juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    auto bar = getLocalBounds().removeFromTop (56).reduced (16, 12);
    importButton.setBounds (bar.removeFromLeft (84));
    bar.removeFromLeft (16);
    startButton.setBounds (bar.removeFromLeft (40));
    bar.removeFromLeft (6);
    playButton.setBounds (bar.removeFromLeft (72));
    themeButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (8);
    fitButton.setBounds (bar.removeFromRight (52));
    bar.removeFromRight (8);
    takeBox.setBounds (bar.removeFromRight (220).withTrimmedLeft (0));

    startButton.setVisible (edit != nullptr);
    playButton.setVisible (edit != nullptr);
    fitButton.setVisible (edit != nullptr);

    auto content = getLocalBounds().withTrimmedTop (56).withTrimmedBottom (26);
    if (history && showHistory)
        history->setBounds (content.removeFromRight (240));
    if (view)
        view->setBounds (content);
}

} // namespace ld

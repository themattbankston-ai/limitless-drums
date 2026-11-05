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
    applyTheme();
    startTimerHz (20);
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    stopTimer();
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

void MainComponent::openTake (int index)
{
    if (index < 0 || index >= (int) takes.size())
        return;

    view = nullptr;
    if (edit) edit->getTransport().stop (false, true);
    edit = nullptr;

    auto& take = takes[(size_t) index];
    edit = createSessionEdit (engine, take.mics, 120.0, te::Edit::EditRole::forEditing);
    currentTake = index;
    takeBox.setSelectedItemIndex (index, juce::dontSendNotification);

    if (edit == nullptr)
    {
        notice = "Couldn't open " + take.title + ".";
        repaint();
        return;
    }

    notice = take.warnings.joinIntoString ("  ");
    if (! take.references.empty())
        notice = (notice.isNotEmpty() ? notice + "  " : juce::String())
                 + juce::String ((int) take.references.size()) + " one-shot reference found (used later for reference matching).";

    view = std::make_unique<SessionView> (*edit, take.roles, [this] { return palette(); });
    addAndMakeVisible (*view);
    resized();
    view->zoomToFit();
    grabKeyboardFocus();
    repaint();
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
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'O') { chooseFiles(); return true; }
    return false;
}

void MainComponent::timerCallback()
{
    const bool playing = edit && edit->getTransport().isPlaying();
    playButton.setToggleState (playing, juce::dontSendNotification);
    playButton.setButtonText (playing ? "Stop" : "Play");
    if (edit) repaint (0, 0, getWidth(), 56);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    const auto p = palette();
    g.fillAll (p.ground);

    // Toolbar
    auto bar = getLocalBounds().removeFromTop (56);
    g.setColour (p.panel);
    g.fillRect (bar);
    g.setColour (p.line);
    g.fillRect (bar.removeFromBottom (1));

    if (edit)
    {
        const double pos = edit->getTransport().getPosition().inSeconds();
        const int mins = (int) (pos / 60.0);
        const auto text = juce::String (mins) + ":" + juce::String (std::fmod (pos, 60.0), 3).paddedLeft ('0', 6);
        g.setColour (p.ink);
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.drawText (text, 330, 0, 160, 56, juce::Justification::centredLeft);
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

    if (view)
        view->setBounds (getLocalBounds().withTrimmedTop (56).withTrimmedBottom (26));
}

} // namespace ld

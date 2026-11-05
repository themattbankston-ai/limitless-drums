#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theme.h"
#include "SessionView.h"
#include "core/Import.h"

namespace ld
{
/** Buttons and menus drawn from the theme tokens. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setPalette (const theme::Palette&);
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (13.0f, juce::Font::bold); }

private:
    theme::Palette pal = theme::night();
};

/** The main window: an empty state you can drop a folder onto, then the session with its
    toolbar (import, transport, time, zoom, theme) and the multitrack view. */
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit MainComponent (tracktion::Engine&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    /** Opens files/folders as if dropped. */
    void importItems (const juce::Array<juce::File>&);

private:
    tracktion::Engine& engine;
    LookAndFeel lnf;
    bool dayMode = false, dragOver = false;
    std::unique_ptr<juce::Drawable> lockupNight, lockupDay;

    std::vector<ImportTake> takes;
    int currentTake = -1;
    std::unique_ptr<tracktion::Edit> edit;
    std::unique_ptr<SessionView> view;

    juce::TextButton importButton { "Import" }, playButton { "Play" }, startButton { "|<" }, fitButton { "Fit" },
                     themeButton { "Day" };
    juce::ComboBox takeBox;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String deviceText, notice;

    theme::Palette palette() const { return dayMode ? theme::day() : theme::night(); }
    void openTake (int index);
    void togglePlay();
    void chooseFiles();
    void applyTheme();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace ld

#include "MainComponent.h"
#include "BinaryData.h"

namespace ld
{

static std::unique_ptr<juce::Drawable> loadSvg (const char* data, int size)
{
    return juce::Drawable::createFromImageData (data, (size_t) size);
}

MainComponent::MainComponent (tracktion::Engine& e) : engine (e)
{
    lockupNight = loadSvg (BinaryData::lockuphorizontalnight_svg, BinaryData::lockuphorizontalnight_svgSize);
    lockupDay = loadSvg (BinaryData::lockuphorizontalday_svg, BinaryData::lockuphorizontalday_svgSize);

    themeButton.setButtonText ("Day mode");
    themeButton.onClick = [this]
    {
        dayMode = ! dayMode;
        themeButton.setButtonText (dayMode ? "Night mode" : "Day mode");
        repaint();
    };
    addAndMakeVisible (themeButton);

    auto& dm = engine.getDeviceManager().deviceManager;
    if (auto* dev = dm.getCurrentAudioDevice())
        deviceText = dev->getName() + "  ·  " + juce::String (dev->getCurrentSampleRate() / 1000.0, 1) + " kHz";
    else
        deviceText = "No audio device";

    setSize (1100, 680);
}

void MainComponent::paint (juce::Graphics& g)
{
    const auto p = palette();
    g.fillAll (p.ground);

    auto area = getLocalBounds().reduced (theme::space::xl);
    auto panel = area.withSizeKeepingCentre (std::min (area.getWidth(), 760), 360).toFloat();

    g.setColour (p.panel);
    g.fillRoundedRectangle (panel, theme::radius::l);

    if (auto* lockup = dayMode ? lockupDay.get() : lockupNight.get())
    {
        auto logoArea = panel.reduced (80.0f, 110.0f);
        lockup->drawWithin (g, logoArea, juce::RectanglePlacement::centred, 1.0f);
    }

    g.setColour (p.inkQuiet);
    g.setFont (juce::FontOptions (13.0f));
    auto footer = getLocalBounds().reduced (theme::space::l).removeFromBottom (20);
    g.drawText ("Limitless Drums " + juce::String (LD_VERSION_STRING) + "  ·  M0 foundations  ·  Tracktion Engine "
                    + tracktion::Engine::getVersion() + "  ·  " + deviceText,
                footer, juce::Justification::centredLeft);

    g.setColour (p.signal);
    g.fillEllipse (getWidth() - theme::space::l - 10.0f, footer.getCentreY() - 5.0f, 10.0f, 10.0f);
}

void MainComponent::resized()
{
    themeButton.setBounds (getWidth() - 140, theme::space::l, 116, 28);
}

} // namespace ld

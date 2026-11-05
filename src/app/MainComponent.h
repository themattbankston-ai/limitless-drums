#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "Theme.h"

namespace ld
{
/** M0 shell: the logo, the engine status, and the theme switch. Real screens arrive in M1. */
class MainComponent : public juce::Component
{
public:
    explicit MainComponent (tracktion::Engine&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    tracktion::Engine& engine;
    bool dayMode = false;
    std::unique_ptr<juce::Drawable> lockupNight, lockupDay;
    juce::TextButton themeButton;
    juce::String deviceText;

    theme::Palette palette() const { return dayMode ? theme::day() : theme::night(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
} // namespace ld

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <visage/app.h>

namespace ld::ui
{
/** Hosts the GPU (Visage) interface inside a JUCE component. JUCE keeps the window, menus,
    dialogs and the audio engine; Visage draws everything inside the window and receives
    mouse and keyboard input directly through its own native view.

    The Visage window is created once the component is on screen with a size, and is torn
    down (render loop first) before the frames it draws are freed. */
class VisageHost : public juce::Component, private juce::Timer
{
public:
    explicit VisageHost (visage::ApplicationWindow& root);
    ~VisageHost() override;

    /** Stops rendering and detaches from the native window. Safe to call twice. */
    void shutdown();

    void resized() override;
    void paint (juce::Graphics&) override;
    void parentHierarchyChanged() override { startTimer (10); }
    void visibilityChanged() override { startTimer (10); }

private:
    visage::ApplicationWindow& root;
    bool attached = false;

    void timerCallback() override;
    void attach();
    void pushSize();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisageHost)
};
} // namespace ld::ui

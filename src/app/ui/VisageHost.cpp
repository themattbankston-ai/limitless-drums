#include "VisageHost.h"

namespace ld::ui
{
VisageHost::VisageHost (visage::ApplicationWindow& r) : root (r)
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
    startTimer (10);
}

VisageHost::~VisageHost() { shutdown(); }

void VisageHost::shutdown()
{
    stopTimer();
    if (! attached)
        return;
   #if JUCE_LINUX
    if (auto* w = root.window())
        juce::LinuxEventLoop::unregisterFdCallback (w->posixFd());
   #endif
    root.close();
    attached = false;
}

void VisageHost::paint (juce::Graphics& g)
{
    // Only visible for the instant before the GPU view covers it.
    g.fillAll (juce::Colour (0xff0b0c10));
}

void VisageHost::timerCallback()
{
    if (! attached)
        attach();
    if (attached)
        stopTimer();
}

void VisageHost::attach()
{
    auto* peer = getPeer();
    if (peer == nullptr || ! isShowing() || getWidth() <= 0 || getHeight() <= 0)
        return;

    // The GPU view fills this component, which MainComponent keeps at the full window size.
    root.setWindowDimensions (visage::Dimension::logicalPixels ((float) getWidth()),
                              visage::Dimension::logicalPixels ((float) getHeight()));
    root.show (peer->getNativeHandle());
    attached = root.window() != nullptr;

   #if JUCE_LINUX
    if (auto* w = root.window())
        juce::LinuxEventLoop::registerFdCallback (w->posixFd(), [this] (int)
        {
            if (auto* win = root.window())
                win->processPluginFdEvents();
        });
   #endif
    pushSize();
}

void VisageHost::resized()
{
    if (attached)
        pushSize();
}

void VisageHost::pushSize()
{
   #if JUCE_MAC
    root.setWindowDimensionsFromHost (getWidth(), getHeight());
   #else
    const auto scale = (float) juce::Component::getApproximateScaleFactorForComponent (this);
    root.setNativeWindowDimensionsFromHost (juce::roundToInt (getWidth() * scale),
                                            juce::roundToInt (getHeight() * scale));
   #endif
}
} // namespace ld::ui

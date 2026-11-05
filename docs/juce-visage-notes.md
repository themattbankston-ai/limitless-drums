# JUCE + Visage notes (Limitless Drums)

## How it fits together
- `src/app/ui/VisageHost.*`: a JUCE component that shows a `visage::ApplicationWindow` inside the
  JUCE window's native view (`show (peer->getNativeHandle())`, plugin-style embedding).
  - macOS: Visage's Metal view is a subview and gets mouse and keys itself. Sizes go through
    `setWindowDimensionsFromHost` (logical pixels).
  - Linux: X11 child window. JUCE polls Visage's `posixFd()` with
    `LinuxEventLoop::registerFdCallback` and calls `processPluginFdEvents()`. Sizes go through
    `setNativeWindowDimensionsFromHost`.
  - The window is created on a timer once the component has a peer and a size, never in a
    constructor.
- `src/app/ui/Studio.*`: the whole interface as one Visage frame tree (header, strips, glass
  screen with an overlay for the playhead and transient pulses, console, rack, footer, history).
  It reads a plain `StudioModel` and reports actions through callbacks; it never touches the engine.
- `src/app/SessionModel.*`: builds the `StudioModel` from the edit (peaks, hits, velocity layers,
  levels, pitch, arrival).
- `MainComponent` owns the edit, menus and native dialogs (file choosers, alerts, the rename box).

## Rules
- Shutdown order: `VisageHost::shutdown()` (closes the window and render loop), then
  `root.removeChild (&studio)`, then frames are destroyed. `MainComponent::shutdownUi()` does this.
- Keys: when the GPU view has focus, keys arrive in `Studio::keyPress` and go to
  `MainComponent::handleKey`. `MainComponent::keyPressed` maps JUCE keys onto the same function.
- File drops land on the GPU view (`Studio::dropFiles`); they are forwarded to `importItems`
  on the next message loop turn so dialogs can open safely.
- Text with non-ASCII characters (·, –, ♯) must go through `visage::String::convertToUtf32`.
- Visage frames redraw only when asked: the screen redraws on view or model changes, the overlay,
  header and rack every tick (60 Hz timer in MainComponent).

## Build
- Visage is fetched at a pinned commit. freetype is declared first from its GitHub mirror,
  because gitlab.freedesktop.org is not reachable from every build machine.
- Linux needs `libxcb-present-dev`; for software rendering under Xvfb, `mesa-vulkan-drivers` and
  `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`.

## Not yet in the GPU layer
- Rename uses a native JUCE AlertWindow, and choosing a take uses a native PopupMenu.
- Song-mode header (edit tools, Shuffle/Slip/Grid, grid value, nudge) waits for Song mode.

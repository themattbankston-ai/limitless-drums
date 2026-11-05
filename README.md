# Limitless Drums

A standalone Mac app for editing multitrack drum recordings: import WAVs from any DAW, edit, export back to any DAW.
Built with JUCE and Tracktion Engine. Personal use.

- **Plan:** see the Limitless Drums Master Plan doc.
- **Build (Mac):** `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build`
- **Cloud builds:** every push builds the Apple Silicon app and runs the tests; the app is attached to each run as a download.

## Layout

| Folder | What's in it |
| --- | --- |
| `src/app` | The app window and screens |
| `src/core` | Editing engine: sessions, analysis, conform (no UI) |
| `tests` | Tests run on every build, using generated sessions and, later, your real recordings |
| `assets/logo` | Logo, app icon and badge files |

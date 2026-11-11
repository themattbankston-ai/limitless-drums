# Status against the 10 Nov "Current GUI and Processing" handoff

Source: branch `samples`, this commit; before it c29037c (Mac CI run 42886603501 passed, 88 cases).
Pins unchanged: Tracktion a5e4499, Visage 238d1b5, FreeType 2.14.1, Catch2 3.7.1. Framework untouched.

## Already there (with tests unless noted)
- SA01–SA12, SA14, CR01, CR03, CR04; export checks from Milestone 1 (unwritable destination, deterministic bytes,
  monitor isolation and restore, rapid edits, closing mid-render).
- Graphite/coral tokens, golden-ratio splits, primary button / segmented / switch / knob / focus ring primitives.
- No mic or drum artwork exists in this build (nothing to remove).

## Done in this commit (UI only, no DSP change)
- One track header used by Hits and Align: name, MONO/STEREO in words, solo, mute, a visible monitor fader with
  its gain in dB, pan with its value, meter. Drag from where you grab, Shift for fine, Option-click resets.
- The pop-up fader and pan are gone (the only copies are in the headers).
- Polarity is only on Align's header (Align owns it); Hits no longer has a second polarity button.
- Rows are never shorter than a full header; when they don't all fit, they scroll (wheel over the headers on
  Hits, anywhere on Align) with a thumb, and scrolled rows can't be clicked through the bars around them.
Evidence: screenshots and a scripted drag/click/scroll run at 1100 x 700 (docs/gui/2026-11-10-track-headers-*).
Monitor isolation is covered by the existing export regression. No unit test of the header's drag maths yet.

## UI-only work still to do
- Track headers on Tune, Tone, Clean and Level (those screens show one mic at a time today).
- Hit / Velocity layer / All layers views and a scope summary; expandable Tune/Align details panel.
- Knobs replacing the Tone/Clean sliders; typed numeric entry everywhere (fader gain and pan included).
- "Solo active" indicator with clear-solos; renaming tracks; audition modes in a compact menu.
- Keyboard and VoiceOver semantics (SA13) — waiting on Matt's go-ahead.

## Needs measured DSP work (not started; planned as its own versioned change)
Reference match, checked against the current `hl::loadReference` (src/hl/Prep.cpp:693):
| ID | On this branch | Status |
|---|---|---|
| RM01 | Reference channels are averaged before analysis | present (source-confirmed; not yet shown audible) |
| RM02 | Source: up to 6 hits of the hardest layer; reference: up to 12 detections | present |
| RM03 | Reference audition resampled by rounded index | present |
| RM04 | A band rejected for a weak reference (inside 60 Hz–15 kHz) gets `0 − mean`, i.e. a correction it was excluded from | present; the clearest of the six |
| RM05 | Source is the mic sum after clean-up and alignment, before tone | present; no revision key ties the profile to that stage |
| RM06 | Curve feeds the existing tone kernels; envelope kept separately | present |
Plan: fixtures first (identity, gain-only, inverse-polarity stereo, weak-band, known EQ), then fixes behind an
engine version so existing sessions keep their sound.

## Deferred
Export cancel, project save/recall, full-app device timing, removed-audition for Clean, Harrison, song mode,
plugin hosting.

## Old mockups this supersedes
The three concept images in the earlier comprehensive handoff: their mic/drum artwork, the detached
selected-mic monitor strip and the illustrative phase plot are not being built.

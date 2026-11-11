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

## Added next (UI only): the track column on Tune, Tone, Clean and Level
The same headers run down the left of those four steps with their live monitor controls; clicking a row picks the
mic the step looks at (on Tone that is also its visible "Applies to" choice). The step's panels keep their golden
split in the remaining width. Evidence: screenshots at 1100 x 700 and a scripted row-click and slider drag.
Screens: docs/gui/2026-11-10-track-column-1100x700.png.

## Added after that (UI only): knobs, typed values, clear solo
- Every processing amount (Tone, Envelope, reference match, Clean, Level, Fine tune) is a scalloped knob with its
  name above and value below; the old horizontal processing sliders are gone. Drag up or right (Shift: fine),
  Option-click resets, click the value to type one (Return keeps, Esc cancels; while typing, no shortcut fires).
- "Clear solo" appears at the left of the audition bar whenever any track is soloed.
Evidence: screenshots at 1100 x 700 and a scripted run (typed 25 into Tone -> +25; dragged Enhance; solo then
clear). No change to what any control does to the sound. Screens: docs/gui/2026-11-10-knob*.png.

## Added after that (UI only): the three views
- The audition row is now VIEW: Hit / Velocity layer / All layers. The view decides what's on show and what Play
  auditions; it never decides what an edit applies to, and the selected hit survives every switch.
- Hits zooms the lanes to the view; Align adds the view's other hits faintly (at most 8, each to its own peak,
  said in the legend); Tune and Level dim the hits outside the view; Tone and Clean get a strip of the view's hits
  to pick from ("8 of 40" when thinned).
- A scope note beside the Tone and Clean controls says what they apply to. On Tune, Tone, Clean and Level a track
  row now only changes the mic shown; Tone's "Applies to" is the only thing that changes its edit scope.
- The Groove, MIDI and Mono buttons are gone: they did nothing in this build.
- Found on the way: a run that was killed left about 1 GB a take in the temp folder (14 GB here after two days of
  scripted captures). Opening a take now clears cache folders over a day old.
Evidence: unit test of view membership and thinning (604 assertions with the Hits logic), screenshots of an
8 x 5 snare at 1100 x 700. Screens: docs/gui/2026-11-10-three-views-1100x700.png.

## Added after that: Align details, and the first engine addition (a phase EQ band set by hand)
- "Details" on Align opens a panel beside the lanes for the selected mic: Arrival knob, polarity and phase EQ
  switches, a knob pair (frequency, Q) for each band that really exists, add/remove a band, and the phase of the
  filters as they run, computed from the same coefficients the audio path uses (degrees, wrapped).
- ENGINE CHANGE (new capability, nothing existing changes): `hl::setAllPass` / `removeAllPass`. A band set by hand
  is kept through every nudge, flip and re-measure until Reset; mics never touched by hand resolve exactly as
  before. Regression test: the band reaches the plugin, survives a nudge and a flip, is clamped to a sane range,
  switches off and on, undoes, and Reset returns to the measured bands (30 assertions).
- Typed entry for each track's monitor gain (dB) and pan (-100..100); scope notes on Tune and Level.
Evidence: the regression test, the full suite, a scripted run at 1100 x 700 (opened Details, added a band,
typed +3.5 dB on a fader). Not yet listened to on real drums. Screen: docs/gui/2026-11-10-align-details.png.

## UI-only work still to do
- A details panel for Tune; "Assign layer / round robin" naming on Hits.
- Renaming tracks.
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

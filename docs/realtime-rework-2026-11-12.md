# Making it feel like Hitline again (12 Nov 2026)

Matt's test of the wrapped-up sample build (5fdc6f0): changes aren't heard in real time, the app hangs, it's less
intuitive than Hitline 4.8, the groove/sweep/layer auditions are missing, and he preferred Hitline's tuning view,
layers view and the fluid main track view. No new features until these are fixed.

## What was wrong (measured, GGD Snare Gretsch: 130 s, 6 mics, 40 hits, 2-core Linux box)
- A tone knob change re-ran the whole chain for the whole take, clean-up included: **5.5 s** until heard.
- Every change copied every mic's mono sum (Session::tracks[].mono) for undo and for the render: **80-100 ms on the
  message thread per change**, and undo history grew by the whole take's audio per step (up to 200 steps).
- When a render landed, every clip was removed and re-inserted and playback restarted.

## What changed
1. `renderMix (S, prev)`: the clean-up stage and the tone pass's scans are reused when their inputs match
   (`MixRender::cleanKey/scanKey`). 5.5 s -> 1.7 s for the whole render.
2. `hl::fxRenderRange` / `hl::HitPreviewer`: the tone and shape render for any range of hits alone; the samples are
   identical to the whole render's (test "Preview: one hit worked out on its own...").
3. `live::AlignPlugin::setHitAudio`: a hit's audio played from memory instead of the clip, 8 ms crossfade on change.
4. `SampleModel::startPreview`: on a mix or tune change the hits are worked out again from the playhead on and
   handed to the plugins. `renderSoon` holds the whole render until the knob has rested 250 ms. While something
   plays, landed renders don't touch the clips (`clipsStale`); `settleClips` lays them when playback stops.
   `warmPreview` makes the tone pass's listening ahead of the first knob touch.
5. `SharedF32` for the mono sums: change() went from ~100 ms to ~1 ms on the message thread.

First hit heard: ~110-150 ms after the change on the 2-core box (6 mics, two hits each); the Mac should be several
times faster. The whole render (levels, export) follows ~250 ms after the knob rests.

## Not previewed per hit (heard when the whole render lands, as before)
Taming (linked across hits and mics), click repair, clean-up amount (the clean-up stage itself), inserts, bus, room.
Level gains still update when the whole render lands.

## Still to do, in Matt's order
1. Confirm on the Mac that knob changes are heard at once; chase the monitor fader if it still lags.
2. Auditions as in Hitline 4.8 (`/home/claude/hitline4/src/app/40_audio.js`: MODES hit, layer, sweep, groove,
   rudiments, mono; `grooveEvents`, `playSweep`). Now: Hit / Velocity layer / All layers only. Sweep and groove need
   hits re-triggered off the timeline: the plugin's hit-audio table can carry them (needs overlapping voices).
3. Hitline's tuning view and layers view (`50_inspector.js`, `55_panels.js`, `30_editor.js`), in the current look.
4. The main track view's feel against Hitline's (`30_editor.js`): selecting, auditioning and listening.

## Later the same night: Hitline's layout, auditions and views
- **Layout** (Tune, Tone, Clean, Level): the tracks and the take stay in the middle; the step's controls sit in a
  404 px panel down the right (`HitsView::drawWorkspace`). The middle switches between Waveforms, Grid and the step's
  own picture (Pitches, Before / after, Tail, Levels). Hits, Align, Setup and Export are as they were.
- **Grid**: Hitline's velocity x round robin cards (`drawGrid`): shape of each hit, export level, click to hear.
- **Tune panel**: switch, tuner dial for the selected hit, "Sits at", target, fine, how, and the pitch map in cents.
- **Auditions**: As shown (the timeline, as before), Layer, Sweep, Groove, Rudiments. The last four are phrases
  (`HitsLogic.h` builds them as Hitline did; `SampleModel::playPhrase` plays them): hits held in memory, placed past
  the take's end on the timeline and summed by the Align plugin, so every mic goes through its own alignment, phase
  EQ and level. A knob moved while a phrase plays is heard in the phrase.
- Captures: `docs/gui/2026-11-12-*.jpg`.

## Still not instant (heard when the whole render lands)
Taming, click repair, the clean-up amount, and the Level gains that follow a tone change. Phrases play the processed
side only (A is silent there, so a phrase switches to B). Hitline's Mono and MIDI auditions and the velocity pads
aren't back yet. The audition bar is still at the top (Hitline's was at the bottom).

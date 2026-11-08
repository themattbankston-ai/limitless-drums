# Overnight build, 7–8 Nov 2026

Working notes for the morning summary (kept up to date through the night).

## Built
- Plugin windows in the app, all six, wired to the real processing:
  Skull Cutter, The Vise, Portcullis, Ember, Prism, Horizon.
- Engine: compressor gain-reduction curves, gate attenuation curves + Listen + Flip, width-by-frequency split,
  correlation curve / mono risk, insert EQ curves for the windows.
- Memory: malloc_trim after renders, stages moved instead of copied.
- Song mode: hits, tempo (Keep/Smooth/Flatten, half/double), Tightness, Swing, Keep Feel, grid 1/8, 1/16, 1/8T,
  problem finder, MIDI tempo map + performance, mixer applies to songs, drum bus + reverb return export, Clean (bleed).
- Undo/redo for mixer, plugin, Song, Clean and Export changes.
- Export step panel.

## Known gaps
- Third-party AU/VST3 hosting not started.
- Song mode: no AAF import, comping, reinforcement, consistency, click removal, sections, tap/draw tempo.
- Tone/Shape/Reference are off in Song mode (they work per sampled hit).
- A 4-minute song with a reverb send takes ~6 s to re-render on this 2-core machine.

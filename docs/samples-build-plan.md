# The sample build: decisions (8 Nov 2026)

The next Limitless Drums is a sample-prep tool: multitrack recordings of multi-velocity, multi-round-robin drum
samples, made ready for a sampler, a virtual instrument or drum replacement. Song mode, third-party plugin hosting
and the insert plugin windows wait for a later, separate build (the current app is kept as it is).

## What it does
- Steps, in this order: **Setup, Hits, Align, Tune, Tone, Clean, Level, Export.**
- No gate. Tone keeps the tone-shaping tools: transient (attack, sustain), reference matching, simple body/air shape.
- Processing has to be as good as or better than commercial tools, proven by measurement and level-matched listening:
  - Phase: Matt's own manual alignment is the reference; also Waves InPhase.
  - Tuning: Slate Trigger's pitch analysis and Matt's ears (not Melodyne: poor on drums).
  - Levelling: spread within each velocity layer and even steps between layers.

## How it's built
- JUCE with Tracktion Engine. Every change is heard in real time: processors run live in playback; export renders
  the same graph offline, so export matches what you heard.
- Analysis (hits, offsets, pitch, levels) runs once and becomes settings the live processors apply.

## How it looks and works
- Modelled on a standard DAW edit window: flat greys, track lanes, plain waveforms, familiar edit tools. Personality
  from colour only (vivid per-track waveforms, cool-to-warm velocity colours, one accent), not ornament.
- Stereo mics draw as stereo (left above right).
- Track faders: Mon and Pan readouts that pop up a vertical fader (Option-click: 0 dB). **Monitoring only: they never
  change exported files.** Export comes out exactly as the Level step sets it.
- Audition bar on every step: Hit, Layer, Sweep, Groove/Rudiments, MIDI, Mono; A (as recorded), B (processed), A→B;
  Loop, Follow, Level-match, Pads; phase meter and master meter. Velocity pads play a random round robin.
- Edit-window behaviour: select across tracks and play or loop it, Tab between hits, zoom keys, zoom presets,
  waveform height, track height, undo for everything. Key commands follow the standard DAW defaults.
- No commercial product names or borrowed labels anywhere in the app (Hitline's "Auto Prep" becomes **Enhance**).
- Design is approved screen by screen on the design canvas before it's built.

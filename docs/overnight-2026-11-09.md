# Overnight build, 8–9 Nov 2026 (the sample build, branch `samples`)

## Where it stands
All eight steps are on screen and running on Tracktion: Setup, Hits, Align, Tune, Tone, Clean, Level and Export.
Playback, A/B and the export all go through the same Tracktion edit and plugins, so what you hear is what gets written.

- **Running live (heard straight away, playback never stops):**
  - Align: shifts, polarity, phase EQ and nudges, with 10 ms crossfades.
  - Tune: varispeed per hit, using Tracktion's best sinc.
  - Level gains per hit.
  - Monitor faders and pans (these never reach the files).
  - A/B and Level match.
- **Rendered in the background (1–5 s on a long take; playback carries on and switches over when it's done):**
  - Tone moves.
  - Clean-up.
  - Reference match.
  - Level re-measure.
- **Export:**
  - Rendered by Tracktion per hit and per mic, with the reference tails and fades.
  - Settings: 16, 24 or 32-bit; same rate, 44.1k, 48k or 96k; three naming layouts.

## Tested on your recordings
The corpus is 12 takes built from your libraries, plus the 2 parity sessions:
- Panda snares ×2, kick, tom, closed and open hat, ride and crash.
- GGD multi-velocity snare (8×5), kick (8×5) and rack tom (6×5).
- JAD crash.

All of it is in `docs/analysis/corpus-2026-11-09.md`.

| Check | Result |
|---|---|
| Hits found | Every hit on all 14 takes, with V×RR right on each |
| A (as recorded) through Tracktion | Bit-exact to the files (error 0) |
| B against the reference export | Nulls of −42 to −150 dB per mic (most below −75) |
| Export, file by file against the reference | 1,461 files: same names, lengths and formats; nulls −33 to −83 dB |
| Line-up with the lead after Align | Every mic Align moved sums with the lead at least as well as before; the big gains were on overheads and bottoms (e.g. tom OH −0.41 → +0.84, snare OH +0.20 → +0.73) |

Where B and the reference differ, ours is the more accurate one. The reference's fractional delay is off by −52 dB at 20 kHz; ours is off by −104 dB. I measured both against an ideal FFT delay.

## Fixed tonight (found by the tests)
- Per-hit gains switched one plugin latency late, smearing each hit's start; nulls were −9 to −18 dB. Now −50 to −150.
- A hit right at the start of a file lost its first 51 ms (Tracktion doesn't pre-roll the look-ahead at time 0). The take now sits 0.25 s into the edit, which you never see, and a render or seek starts on the right settings.
- B was playing the raw recording, not the cleaned and toned audio, because the tone stage is freed after the render. On the ride this gave −28 dB nulls; it's now −75.
- Frame costs:
  - The Export step spent 29 ms a frame rebuilding the file list; it's now cached.
  - Setup scanned every sample for peaks each frame; that's now done once.
  - Nothing redraws while idle.
- Memory on a 130 s, six-mic take: 1.83 GB peak, settling at 1.10 GB (freed render memory is handed back now).
- Align's per-hit table reaches the audio thread through a try-lock, so the audio thread never waits.

## Measured
- **Align DSP:** about 1 % of one core per stereo track, even while always crossfading.
- **Frames on a 6-mic, 40-hit take** (stress run of clicks, drags and keys on every step, all while playing):
  - 3–7 ms a frame on average, up to 25 ms on Align.
  - The Hits step's first frame takes about 45 ms.
- **Auto Prep:** 0.6–4.7 s including the level measure.
- **Export:** 5–12 s per take.
- **Tests:** 68 test cases pass. The Mac build passes CI.

## Not done / honest gaps
- **Tone and Clean** aren't sample-by-sample live; they render in the background and then swap in. Making them true real-time plugins is the next big job.
- **Groove, MIDI and Mono audition** aren't built; their buttons are greyed out. Hit, Layer and Sweep work, and so do A, B and A→B (A→B switches on each loop pass).
- **Room polarity:** Auto Prep decides it the way Hitline does (band by band over 60 ms). On one Panda room it disagrees with the first arrival on its own, so it's worth a listen.
- **Lead change, drum type and V/RR** re-run Auto Prep rather than updating live.

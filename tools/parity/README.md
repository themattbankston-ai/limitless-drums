# Parity with Hitline 4.8

`hitline-4.8.html` is Hitline 4.8 exactly as built. The C++ port is tested against it.

1. `python3 make_harness.py`: makes `harness.html`, Hitline plus one line exposing its state.
2. Put `harness.html` and the session folders in one folder, serve it (`python3 -m http.server 8777`).
3. `PARITY_ROOT=<that folder> node run.js "<that folder>/SNARE 2 - BB/Center" ../../tests/parity/SNARE_2_-_BB_Center.json`
   runs Auto prep headless and writes Hitline's full analysis state (the answer key).
4. `LD_PARITY_SESSIONS=<folder with the sessions> LDTests "Parity*"` checks the port against the keys.

## Whole export with the Mix step on

`golden_export.js` runs Auto prep, switches on clean-up, tone, shape, tame, channel and bus inserts, the shared
room and tuning (config A), or the Panda reference, kit levels, gated tails and 48 kHz 24-bit (config B), then
runs Hitline's own export into a sink and records every file (bytes, CRC-32, energy, every 211th sample) with the
state it exported from:

    PARITY_ROOT=<folder> node golden_export.js "<folder>/SNARE 2 - BB/Center" A ../../tests/parity/export_A.json ../../tests/parity/SNARE_2_-_BB_Center.json
    PARITY_ROOT=<folder> node golden_export.js "<folder>/SNARE 1 - MH/Center" B ../../tests/parity/export_B.json

The optional last argument is an answer key whose alignment is used when Auto prep leaves the mics unaligned in
that run (Snare 2 does in the current headless setup). The tame pass splits its hits by core count, so the
runner fixes `navigator.hardwareConcurrency` at 4 (three stretches), and the C++ test uses the same split.
Sampler files (.sfz, .dspreset) and the QA report are not compared: Limitless Drums does not write them.

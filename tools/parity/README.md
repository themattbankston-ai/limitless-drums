# Parity with Hitline 4.8

`hitline-4.8.html` is Hitline 4.8 exactly as built. The C++ port is tested against it.

1. `python3 make_harness.py`: makes `harness.html`, Hitline plus one line exposing its state.
2. Put `harness.html` and the session folders in one folder, serve it (`python3 -m http.server 8777`).
3. `PARITY_ROOT=<that folder> node run.js "<that folder>/SNARE 2 - BB/Center" ../../tests/parity/SNARE_2_-_BB_Center.json`
   runs Auto prep headless and writes Hitline's full analysis state (the answer key).
4. `LD_PARITY_SESSIONS=<folder with the sessions> LDTests "Parity*"` checks the port against the keys.

# Drum data

Measured from Matt's material (Nov 2026), for the engine's mic detection and later for Song mode.

- `perf.py` — full multitrack drum performances: hits per drum (bleed rejected), how hard, when every mic
  hears the kick and snare and whether that holds all song. One JSON per session.
- `chroma.py` → `chroma.json` — the Panda chromatic set: per drum and tuning, strongest pitch vs the label,
  ring time, round-robin evenness; per mic, arrival after close mic 1, polarity, and what the sound-based
  detection calls it.

Findings that shaped the engine (src/hl/Rules.cpp):
- Rooms hear the hit 5–43 ms after the close mic, overheads 2–3 ms on snare/toms/cymbals but 4–8.5 ms on a
  kick, so a mic is only called a room by timing when its name doesn't say what it is.
- Sub kicks: only on kicks, at least as low as the kick in with under a third of its beater click. Dark
  second tom mics and some kick-in mics fooled a plain "mostly lows" test.
- Snare bottoms: 14 of 15 Panda snares by brightness against the top (the 15th is the snares-off "Open").
- Toms: the strongest partial sits a steady ~1150 cents (just under an octave) below the tuning label.
  Snares and kicks need a better pitch method than the strongest peak.
- Hat/ride/crash articulations separate cleanly by ring time (closed ~0.15 s, choke 0.3–0.6 s, open 1.2–2.2 s,
  ride/crash 2–3 s).
- Mic-to-mic timing holds within ±0.1–0.5 ms across whole songs.

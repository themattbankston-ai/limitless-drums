
## Ember (Tube EQ) — live behaviour to build
- Valve glow: the hot render layer is added over the cold one by `0.25 + 0.75 * drive`.
- Valve flicker: while audio is passing, the glow amount wobbles very slightly (±4%) with smoothed random
  noise (a few Hz), each valve on its own phase; it settles back to steady when the input goes quiet.
- Green screen: the EQ curve in bright phosphor, with a faint live spectrum of the input behind it
  (filled, phosphor-green, slow afterglow decay).

## Horizon (Width) — how it works
- Mid/side: M = (L+R)/2, S = (L-R)/2. Width scales S (0 mono, 1 as recorded, 2 double); M is untouched, so the
  mono sum never changes and nothing is delayed (no comb filtering, Align's phase work is kept).
- Bass mono: S is high-passed (minimum phase, 24 dB/oct, no pre-ring) at the set frequency.
- Width by frequency (new): S is split at "Highs from" (1–10 kHz, Linkwitz–Riley so the bands sum flat);
  the throttle sets the width below the split, "High width" the width above it.
- Keep loudness: M and S rescaled together so total loudness stays put.
- Mono safety (new): when the correlation over the take falls below +0.2 the correlation tube rings amber and
  reads MONO RISK.


## Ember (Tube EQ) — live behaviour to build
- Valve glow: the hot render layer is added over the cold one by `0.25 + 0.75 * drive`.
- Valve flicker: while audio is passing, the glow amount wobbles very slightly (±4%) with smoothed random
  noise (a few Hz), each valve on its own phase; it settles back to steady when the input goes quiet.
- Green screen: the EQ curve in bright phosphor, with a faint live spectrum of the input behind it
  (filled, phosphor-green, slow afterglow decay).

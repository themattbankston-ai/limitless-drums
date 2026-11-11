# Reference match: fixtures and what they measure (10 Nov 2026)

The first half of this file is the tests-only commit (c08feb6); the fixes are at the end. `LDTests "[refmatch]"` runs them.
Fixture: a synthetic one-mic snare take (4 hits, 48 kHz), noise clean-up off, so the source the match measures is
the recording itself; each reference is that same recording with one known difference. Synthetic, so these are
engineering measurements of `hl::loadReference` as it is, not a verdict on real drums.

| Case | Expected | Measured | Status |
|---|---|---|---|
| Identity: the recording as its own reference | no correction (within 0.5 dB) | up to **+2.2 dB** at 100-125 Hz, -0.8 dB at 3 kHz | fails (expected-fail) |
| Gain only: the same, 12 dB quieter | same curve as the loud one (0.1 dB) | identical | **passes** |
| Known EQ: +6 dB high shelf at 3 kHz | recovered within 2 dB, plateau at least 4 dB | within 2 dB; 5.0-5.3 dB at 6-10 kHz | **passes** |
| RM01: stereo reference, right side inverted | same curve as its mono self | **+8 dB (the cap) on every band from 50 Hz to 12.7 kHz** | fails (expected-fail) |
| RM04: reference with nothing above ~2 kHz | 0 dB above 6 kHz | -0.4 dB at 4-12.7 kHz (the removed mean leaking in), and -8 dB (the cap) at 1.25-2.5 kHz chasing the roll-off | fails (expected-fail) |
| RM03: 44.1 kHz reference auditioned at 48 kHz | a 5 kHz tone stays a tone (residue below -50 dB) | residue only **13.8 dB** below the tone | fails (expected-fail) |

Reading them:
- **RM01 is the serious one**: an out-of-phase stereo reference doesn't merely get ignored, it asks for the
  maximum boost everywhere. Averaging the channels leaves rounding noise, which the match then chases.
- **Identity** isn't neutral: the source and reference are windowed differently (source: 0.6 s from the session's
  onsets with pre-roll; reference: its own detection, to the next hit), which shows in the low bands.
- **RM03** only affects what you hear when auditioning the reference, not the match or the export.
- RM02 (which hits are compared), RM05 (analysis stage and stale profiles) and RM06 (kernel integration,
  cache/undo/session) have no fixture yet.

Expected-fail cases are tagged `[!shouldfail]`: the suite stays green now, and reports them the moment a fix makes
them pass, so the tag comes off with the fix.

Proposed order for the fixes (behind an engine version, so existing sessions keep their sound): RM01
(per-channel power, not an averaged waveform), identity (the same windowing for both sides), RM04 (keep the
rejected-band mask through centring and smoothing; taper instead of chasing a roll-off), RM03 (proper resampling
for audition).

## Engine 2 (later on 10 Nov): the fixes, measured on the same fixtures

`hl::loadReference (..., engine)`: engine 1 is Hitline's, untouched (the main app and parity use it, and a test pins
its numbers above). The sample build now asks for engine 2. A curve already stored in a session is never
recomputed, so nothing that was matched before changes; only loading a reference again uses the new engine.

| Case | Engine 1 | Engine 2 |
|---|---|---|
| Identity | up to 2.2 dB | **0.0 dB** on every band |
| Gain only | same curve | same curve |
| Known +6 dB shelf | within 2 dB | within **0.22 dB**; 5.2-5.5 dB at 6-10 kHz |
| RM01 inverse-polarity stereo | +8 dB on every band | **identical to its mono self** |
| RM04 nothing above ~2 kHz | -0.4 dB leaking above 4 kHz | **0.0 dB from 2 kHz up**; last band with evidence tapered (-8 then -4 dB at 1.26 and 1.59 kHz) |
| RM03 audition resample | residue 13.8 dB under the tone | residue **113.8 dB** under it |

What changed, and why the identity error went away: engine 1's detection kept only the loudest hit of the
reference (or the file's start), and compared it with the median of the take's hits; engine 2 measures every
reference hit within 30 dB of the loudest (4 of 4 here). The rest: per-channel spectra combined as power,
detection on the strongest channel; a band counts only if the reference has it within 60 dB of its total, and
bands without evidence stay at 0 through level removal and smoothing; windowed-sinc resampling for audition.

Still to do: RM02's source side (which of the take's hits are compared), RM05 (a revision key so a stale profile
is rebuilt), RM06 (cache/undo/session), real-drum references and listening, and the sample build's own reference
audition (the audio is prepared but the sample build doesn't play it yet). Synthetic fixtures only so far.

# Overnight build, 7–8 Nov 2026

Notes for the morning, kept up to date through the night.

## Built
- **Plugin windows**: all six are in the app and wired to the real processing: Skull Cutter, The Vise, Portcullis,
  Ember, Prism and Horizon.
  - The Vise ram moves with gain reduction.
  - Portcullis gate opens with attenuation.
  - Prism shows a live spectrum and the EQ curve.
  - Horizon shows correlation.
  - Escape closes any of them.
- **Engine**:
  - Compressor gain-reduction curves.
  - Gate attenuation curves, plus Listen and Flip.
  - Width by frequency, plus a correlation curve and mono-risk reading.
  - Insert EQ curves for the windows.
- **Third-party plugins (VST3, plus AU on the Mac)**:
  - "Find my plugins" scans for them; a plugin that crashes the scan is skipped next time.
  - They work as inserts, rendered offline with their latency removed.
  - Their own editor opens in a window, and changes to it are undoable.
- **Song mode**:
  - Edit & Tempo:
    - Hits and tempo detection.
    - Tempo modes Keep, Smooth and Flatten, plus Half time and Double time.
    - Tightness, Swing and Keep Feel.
    - Grid of 1/8, 1/16 or 1/8 triplets.
    - Problem finder.
    - Grid, hits and moved-from marks on the waveform.
  - Clean: Bleed control, and **Consistency**, which evens kick, snare and tom hit levels while leaving ghost notes
    and accents as played.
  - Export:
    - Every mic at full length.
    - Drum bus, with reverb returns as separate files or printed into the bus.
    - Tempo Map.mid and Drums (MIDI).mid.
  - The mixer applies to songs.
  - Playback runs from the playhead and includes the reverb returns.
- **Undo/redo**: covers mixer, plugin, Song, Clean and Export changes.

## QA done
- **Monkey tests** (random clicks, drags and keys for 15–30 minutes each) were run on the sample session, the
  60-second song, the 4-minute song and every plugin window. Two crashes were found and fixed:
  - Clicking a console insert before the mixer existed.
  - Running out of memory on the 4-minute song: heavy jobs now run one at a time.
- **Layout check**: run with `LD_LAYOUT_CHECK=1`. Every label in all six windows is checked for labels running
  into each other. All six were clean at their defaults and through random mode and value changes.
- **Escape**: it only closed plugin windows when JUCE had keyboard focus. That's fixed.
- **Saved projects lost the mix**: a project file held only the edit, so reopening it lost the mixer, inserts and
  Song/Clean/Export settings. These now go into the project and its autosave.
  - Pressing Auto Prep again also reset the whole mix. It now keeps it.
  - The record that marks a session as a Song (rather than samples) was lost when the tracks were re-ordered after
    loading, so a restored song came back as a "Kick" sample session. Fixed. I tested recovery end to end: kill the
    app, restore, then Auto Prep, and the session comes back as a Song with its Consistency setting.
- **Out of memory on the 4-minute song** (10 mics, monkey test): three causes, all fixed.
  - Every re-render copied all the session's audio (565 MB) first. Copies now share the recordings.
  - Auto Prep held the old mix while building the new one. The old one is now freed first.
  - On Linux, glibc kept freed buffers resident. Large buffers now go back to the system straight away; macOS
    already does this.

- **Stress results after the fixes**:
  - **4-minute song**: two 7-minute monkey runs survived about 1,750 actions each. Peak memory was 4.4 and 3.8 GB,
    down from 5.6 GB or more, when the OS was killing the app.
  - **Sample session**: a 20-minute soak survived 4,918 actions. Memory went 1.41 → 1.58 → 1.69 → 1.72 GB by
    quarter, levelling off: caches filling, not a steady leak.
  - **Third-party plugin on every mic** of the 4-minute song: a re-render takes about 1 s.

## Real-song tuning (after the audit export)
Checked against two real multitracks (a 12-mic song and a 10-mic double-pedal song):
- **Drum voices**: mics are grouped into separate drums from how often they hit together and whether their level
  offsets cancel. Three toms now come out as three toms rather than one.
- **Flams and doubles inside runs** (double-pedal streams, rolls) are no longer flagged.
- **Phantom kick hits**: a very quiet kick next to a loud one is the second kick mic, not a note, so it's no longer
  flagged.
- **Problems flagged on the double-pedal song**: 200 → 54. The 12-mic song: 27 → 19, with its isolated flams kept.

## Doubles that were really decay bumps
Of the 18 doubles flagged on the double-pedal song, 9 of the 10 kick ones weren't second strikes: a kick's low end
swells again about 50 ms after the hit, and the detector read that as a new hit (neither kick mic gets any louder).
- **New rule in hit detection:** a new hit has to get at least 3 dB louder than what was already sounding on that
  mic, looking up to 25 ms on (tom attacks can take 15 ms to build).
- Results: 247 kick "hits" on that song were decay bumps and are gone. Tom hits that fired early on bleed now land
  on the real attack. Of the louder toms no longer found, 7 of 8 checked on the waveform were snare bleed or
  nothing. One (188.44 s) is borderline between a tom-and-snare unison and a hard snare.
- Very quiet partners don't count as flams or doubles on toms either (ringing), as on kicks.
- Problems flagged on that song: 54 → 32 (doubles 18 → 3: a snare double stroke, a real kick double, and one kick
  to listen to at 113.66 s). The 12-mic song: 19 → 17, and the rush near 80 s is gone.

## Tried and not kept: arrival order
Idea: a drum reaches its own mic first and every other mic a fixed time later, so a hit that turns up on a mic before
the bleed delay must have been played near that mic. Measured on both multitracks:
- **Bleed delays are real and steady where the source is sharp**: snare into the toms and overheads holds within
  about 0.1 ms (1–3 ms delays). Kick is looser (0.4–0.9 ms) and tom-sourced pairs are noisy (1–2.7 ms).
- **As a rule it didn't help.** First version: it rescued tom-mic noise that happened to start a few ms before a snare
  ghost note (+142 "toms" on the double-pedal song). With a 2 ms window: kick bleed counted as snare, because the kick
  mic's onset is slow (low end), so the snare-bottom mic hears the beater click "first". Without the kick as a source:
  only 10 toms and a few very quiet hats and snares changed, and the waveforms didn't show them to be real hits.
  Tightening on the bleed delay removed nothing.
- Next candidate is the tone check instead: compare a transient's spectrum with that mic's own confirmed hits.

## Tried and not kept: tone checks
Measured on both multitracks with a diagnostic dump of every close-mic onset (`LD_SONG_DUMP=<file>`):
- **Tone template** (each mic's own hits vs its bleed, 12 bands over the first 30 ms): separates them 90–100% on kick
  and snare mics, only 73–84% on toms (few clean tom hits to learn from). As a rule it's wrong in a key case: a snare
  hit played with the kick sounds like kick bleed (it has the kick's low end), so real snare-and-kick unisons would be
  dropped. It was right about two tom-fill bleeds kept as snares on the 12-mic song.
- **Per-band bleed prediction** (learn how much of each band of the louder drum reaches this mic, then ask how much
  is left over): clean on the double-pedal song for kick and snare (real hits 20–45 dB above the prediction, bleed
  about 4–9 dB) and it keeps the snare-and-kick unisons. On tom mics it breaks down: they hear cymbals and ring all
  the time, which the one louder drum doesn't predict, so a 10 dB rule would add 100–200 noise "hits" per tom mic.
  Subtracting what was already ringing before the onset made it noisier, not cleaner. Too few tom hits on the
  12-mic song to learn the tom pairs at all.
- **What would need to change** for it to work on toms: predict bleed from every mic at once (cymbals included), not
  only the loudest neighbour, and have enough hits per drum to learn from (or ask the user to mark a few).

## Correction
- Last night I said the memory cleanup would halve memory by storing audio as 32-bit instead of 64-bit. The audio was
  already 32-bit, so that part was wrong.
- The real saving came from freeing memory after renders and moving stages instead of copying them. Steady memory
  on the sample session went from 2.2 GB to 1.15 GB.

## Rough or unapproved
- **Song mode screens**: Session, Edit & Tempo, Clean and Export were built without an approved mockup.
- **Reverb renders**: a 4-minute song with a reverb send takes about 6–9 s to re-render on this 2-core machine.
  Without one it takes about 1 s.
- **Session step of a Song**: the right-hand rack now shows a song overview instead of the sample-mode Level panel
  and pads. It has the tempo, bars, hits per drum, and the steps that follow. This is also without a mockup.
- **Smaller windows**: at 1280×800 everything scales cleanly. The status line now ends in an ellipsis when it's too
  long instead of being cut mid-word.
- **Audible playback**: this machine has no audio device, so I couldn't listen. The offline playback tests pass.

## Known gaps
- **Song mode**:
  - Not built: AAF import, comping, reinforcement from your own kit, click removal, sections, tap or draw tempo,
    dragging the tempo line.
  - Tone, Shape and Reference are off in Song mode (they work per sampled hit).
- **Inserts on the drum bus**: everything except the compressor runs on each mic separately, not on the summed bus.
  The export is one file per mic, so a non-linear plugin on the sum can't be split back out. The bus compressor is
  driven by the sum and applies one gain curve to every mic.
- **Not built**: versions/snapshots, performance analyser, batch processing, and mockups for the Hits and Session
  steps.

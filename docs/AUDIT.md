# Limitless Drums: audit package

Personal-use desktop app for editing multitrack drum recordings, built by Matt Bankston with Claude. Not for sale.
This guide covers how to build it, how it's tested, what each built-in processor is modelled on, and where to find the specs.

## Where things live
- **Source of record:** https://github.com/themattbankston-ai/limitless-drums (private), branch `main`. This archive is
  `git archive` of the commit named in `COMMIT.txt`.
- **Development checkout:** in Claude's cloud workspace at `/home/claude/limitless-drums` (Linux; used for
  development and the stress tests).
- **macOS app:** every push to `main` is built on GitHub Actions (`.github/workflows/build.yml`, macos-15, Apple
  Silicon). The test suite runs, the app is ad-hoc signed and zipped, and it's attached to the run as the artifact
  `Limitless-Drums-macOS` (kept 30 days).

## Build
Requirements: CMake 3.24 or later, Ninja, a C++20 compiler (Xcode 16 / Apple clang on macOS; GCC 13 tested on Linux).
macOS 13 or later is the deployment target.

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Options: `-DLD_BUILD_APP=OFF` builds the engine and tests only (no GPU interface); `-DLD_BUILD_TESTS=OFF` skips tests.

## Dependencies
All pinned and fetched by CMake (FetchContent). None are vendored in this archive.

| Dependency | Version | Use |
| --- | --- | --- |
| Tracktion Engine | commit `a5e44996f5f08dfa6af5f5d02d900fb3cfa37422` | Sessions, editing, playback |
| JUCE | 9.0.5, bundled with that Tracktion commit | Audio, files, plugin hosting (VST3/AU) |
| Visage | commit `238d1b542002e03ad42accd076f938304e67971d` | GPU interface (Metal on macOS, Vulkan on Linux) |
| FreeType | `VER-2-14-1` | Visage text |
| Catch2 | `v3.7.1` | Tests |

Bundled fonts and their licences are listed in `assets/fonts/LICENSES.md`.

## Tests
`tests/` is a Catch2 suite: 53 cases at the time of export, all passing. The suite covers:
- **Engine parity:** against Hitline 4.8's outputs.
- **Per-processor behaviour:** compressor gain reduction, gate, width split and correlation, EQ curves.
- **Song mode on synthetic songs with known answers:** hit and tempo accuracy, tightening, consistency, the problem
  finder (rolls, triplets, tempo changes), whole-session export.
- **Projects and undo:** project save and load, undo round-trips.
- **Playback and plugins:** offline playback, third-party plugin hosting (with a test VST3).

Hidden cases (run with a tag):
- `[.mem]`: memory probes.
- `[.perf]`: performance probes.

Tests that need local data (the synthetic songs in `/home/claude/songtest`, the test VST3) skip themselves when
that data is absent.

## Specifications and notes
- `docs/plugin-windows.md`: each plugin window's controls and live behaviour.
- `docs/juce-visage-notes.md`: how the interface and engine connect.
- `docs/overnight-2026-11-08.md`: the beta build log, the QA done, and the known gaps.
- `docs/analysis/`: analysis outputs from reference sessions.
- The teardown and research notes (Pro Audio Teardown Notes, Context Brief) are Claude documents, linked from the
  conversation rather than kept in the repo.

## Built-in processors and their intended references
Limitless Drums began as a port of **Hitline 4.8**'s processing (`src/hl/`). Its inserts are Hitline's models,
re-skinned with original artwork and names. The names below are the products each model was voiced after.
None of those products' code, impulse responses or artwork is used; the DSP is Hitline's model, ported. No
third-party code was decompiled.

| Window | Insert type | Modes, and what each is modelled on |
| --- | --- | --- |
| **Skull Cutter** (saturation) | `sat` | VALVE = Soundtoys Decapitator (styles A, E, N, T, P); CRUSH = Soundtoys Devil-Loc (Crush, Darkness); MULTI = FabFilter Saturn (Gentle, Tape, Tube, Transformer, Heavy); TAPE = Empirical Labs FATSO (Warmth, compression Off / Buss / GP / Spank) |
| **The Vise** (compressor) | `comp` | PANIC = Empirical Labs Distressor (ratios 1 to 20 and Nuke); FET = UREI/UA 1176 (ratios 4, 8, 12, 20, All); VARI-MU = Fairchild 660/670 (threshold, time constant); VCA = dbx 160 (ratios 1.5 to infinity); CLEAN = a transparent channel compressor (no specific reference) |
| **Portcullis** (gate) | `gate` | Generic gate/expander with key filters, Listen and Flip (ducking). No single reference. |
| **Ember** (tube EQ) | `pultec` | Pultec EQP-1A with MEQ-5 |
| **Prism** (EQ) | `eq` | Generic parametric/dynamic EQ (Hitline's EQ). No single reference. |
| **Horizon** (width) | `width` | Generic mid/side width with bass mono and a frequency split. No single reference. |

Reverbs use Hitline's measured programs (`assets/irlib.json`).

Third-party VST3/AU plugins can also be inserted (`src/app/ExtPlugins.*`). They are rendered offline with their
latency removed.

## Known gaps relevant to an audit
- **Audible playback:** not yet verified on a real audio device. The development machine has none; offline
  renders are tested.
- **Double-bass kits:** two kick drums on separate mics are currently treated as one kick. This was found on a real
  multitrack, and the fix is pending.
- **Drum-bus inserts:** everything except the compressor runs on each mic separately, not on the summed bus. This
  is by design, because the export is one file per mic.

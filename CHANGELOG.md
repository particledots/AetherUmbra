# Changelog

## 0.2.0: Milestone 1 (the engine runs; result to be filled in after the Logic test)
- The Oneiroi engine is compiled into the plugin: an AU Instrument (`aumu`) named `AetherUmbra` and a VST3. The two Milestone 0
  shells are gone. The engine runs at 48 kHz in blocks of 32 samples; a band-limited resampler and a block FIFO make it work at
  44.1, 48 and 96 kHz and any host block size (delay 32 samples at 48 kHz, 65 at 44.1 kHz, 132 at 96 kHz, reported to Logic).
- MIDI notes set the oscillator pitch (last note, held). Audio from the Side Chain menu goes through the looper, filter,
  resonator, echo and ambience.
- Window: the approved "module layout" (option C), resizable, with Filter Noise, Randomise and Undo / Redo; rough in looks.
- Parameters (55, names and order fixed from now on; new ones only at the end), all automatable, saved with the project.
- Firmware changes, marked and listed: Filter.h (Filter Noise control), DelayLine.h (read position kept inside the buffer;
  found by the address sanitizer).
- Tests: engine test (resampler, oscillators, wavetable, looper, every effect, random-settings run at 44.1/48/96 kHz, instances
  bit-identical) on Apple silicon, Intel (Rosetta) and, here, under the address and undefined-behaviour sanitizers and ARM64;
  plugin test (parameters, notes, side chain audio, looper, randomise and undo, state, window pictures).

## 0.1.0: Milestone 0 (two empty test shells)
- Two empty plugins built from the same code, to find out in Logic which Audio Unit type gets audio AND MIDI notes:
  `AetherUmbra Test Instrument` (type `aumu`) and `AetherUmbra Test Music Effect` (type `aumf`). Both: stereo out; audio in
  stereo, mono or none; MIDI notes in; the audio passes through unchanged; a test tone (sine, -20 dB, a parameter "Test tone",
  on by default) follows the last MIDI note and keeps its pitch after the key is released. Tail length 0.
- Placeholder window with the version, the plugin type, the "unofficial, not affiliated with Befaco" notice, and two live
  lines: audio input (channels and level) and last MIDI note.
- Oneiroi firmware 1.2.2 (commit e048098) imported unmodified into `third_party/oneiroi-firmware` (35 files). Not compiled
  yet. The workflow compares it byte for byte with Befaco's repository.
- Universal build (arm64 + x86_64), macOS 11.0 minimum, built on GitHub Actions (workflow from Iro-x). Install and
  uninstall scripts. The zip contains licences, `SOURCE.txt` and the change list.
- Automatic plugin test (`tools/plugin_test`), both shells: layouts, audio unchanged at 44.1, 48 and 96 kHz, tone frequency
  follows the notes, pitch stays after release, no-audio-input case, saving and loading, window text.
- Result: passed on Nov 11, 2026 in Logic 11.2 on the M4 Mac mini (macOS Tahoe), 44.1 kHz, all 9 checks. The Instrument shell gets notes and, through the Side Chain menu in its plug-in header, audio. The Music Effect shell gets audio but no notes (tested on an audio track and in the audio FX slot of a software instrument track). Decision: the Instrument type is kept; the Music Effect shell is dropped in Milestone 1. After the test: the install script installs only the Audio Unit (not the VST3); the uninstall script also removes leftover VST3s. Intel Mac not tested (not requested).

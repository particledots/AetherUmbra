# Changelog

## 1.1.1: the snap switches now move the knob itself
- Result of 1.1.0 on the M4 Mac mini (Bryn): the snaps could not be seen or felt when turning the knobs; he expected the knob itself to click from step to step with a read-out, as in the PhenomeEon plugin. In 1.1.0 the snapping was done only inside the sound engine, so the knob on screen stayed smooth and nothing showed it.
- Change: with a snap switch on, its knob now clicks from step to step while it is dragged, and the pop-up read-out names the step: Pitch "+3 st", Looper Speed "fwd +3 st (1.21x)" / "rev ..." / "stopped", Resonator Tune "+1 st", Filter Cutoff "D#4  311 Hz", Looper Start and Length "n x 1/16 note" ("x.xx s (no tempo)" and free when Logic gives no tempo). Turning a switch on moves its knob onto the nearest step at once. The engine still snaps the value too, so automation, Mod and saved projects stay on the grid. The grids are in one file, src/engine/AetherSnap.h, used by both the window and the engine. No firmware change.
- Tests added: plugin test (switch off leaves the knob free; switch on moves Pitch, Speed, Cutoff and Tune onto the step and the read-out names it; Start and Length stay free without a tempo).
- Result: waiting for the Logic test on the M4 Mac mini (Logic 11.2, 44.1 kHz).

## 1.1.0: fix round on 1.0.0 (Echo Sync and snap switches)
- Result: the snap switches could not be seen working (see 1.1.1). Echo Sync not reported yet. Automatic tests pass (see below).
- New parameters 56 to 62, at the end of the list (parameter order is unchanged; 1.0.0 had 55): Echo Sync (default off), then the snap switches
  Oscillator Pitch (default on), Looper Speed, Resonator Tune, Filter Cutoff, Looper Start, Looper Length (all off). The window has a small
  on/off button for each in the title bar of its panel (Looper: Speed, Start, Length; Oscillator: Pitch; Filter: Cutoff; Resonator: Tune; Echo: Echo Sync).
- Echo Sync: off = free echo (smooth Density, 10 ms to about 4 s) even when Logic gives a tempo; on = the nine tempo steps of 1.0.0 (1/64 to one beat).
  Modulation and Ambience auto-pan follow Logic's tempo either way. Longer sync steps are not added (not requested).
- Snap switches: Pitch = whole semitones; Looper Speed = ratios 2^(n/12) up and reversed, locked zones at 0x, 1x, -1x stay; Resonator Tune = whole semitones
  of the main pole; Filter Cutoff = notes (A = 440 Hz), fully open or closed unchanged; Looper Start / Length = 1/16-note divisions (a quarter beat) of Logic's tempo
  as exact sample positions, free when no tempo. Pitch, Speed, Tune and Cutoff snap the knob value (Mod is added after); Start and Length snap the knob plus Mod
  (otherwise a moving Mod would restart the loop fade in every block).
- Firmware changes (marked, listed in AETHERUMBRA-CHANGES.md): Commons.h (three new PatchCtrls fields, one PatchState field), Echo.h (Echo Sync), Looper.h (beat-division
  Start and Length). The other four snaps are in src/engine/AetherEngine.cpp.
- Bug found by the random-settings test and fixed (existed in 1.0.0): with Mod type Chaos, Mod speed at its fastest ratios and a Logic tempo of about 120 BPM or more,
  the chaos generator (LorenzAttractor.h) ran away to infinity and the plugin stayed silent until it was reloaded. Its step is now limited (chaos at those top
  speeds is about a quarter slower) and it restarts by itself if it ever goes invalid. New engine test section 5b.
- Tail: still 12 s. With Echo Sync off the echo ringing is longer than with the stepped echo (defaults 6.8 s, covered; Repeats 0.5 + Density full 24 s and
  Repeats 0.9 + Density 0.5 34 s, not covered). Rows added to the engine tail test.
- Engine defaults are the firmware's own (Echo Sync on, no snapping); the plugin's defaults are set in src/Parameters.h. Saved 1.0.0 projects open with Echo Sync off and Oscillator Pitch snap on.
- Tests added: engine test section 7 (Echo Sync: tempo follows only when on, smooth Density when off, about 3.9 s at full, modulation still follows tempo, switching while playing;
  Pitch, Speed, Tune, Cutoff snaps; Looper Length interval 0.250 s at 120 BPM and 0.300 s at 100 BPM; Looper Start; free without tempo; fuzz now switches all seven at random with and without tempo);
  plugin test (62 parameters in order, names, defaults, Echo Sync and Pitch snap through the plugin, window has the seven new buttons, pictures with the switches).
- Version 1.1.0.

## 1.0.0: Milestone 2 (the finished plugin)
- Result: PASSED on Nov 12, 2026: all checklist items on the M4 Mac mini (Logic 11.2, 44.1 kHz) and on the Intel Mac (macOS 12.7.8,
  Logic 10.7.8); automatic tests pass (engine test incl. tempo and tail sections; plugin test 97 checks). Notes: Supersaw works; Wavetable
  only sounds with a recording in the looper (same on the module). Open note: with Logic's tempo the Echo Density has only 9 steps (1/64 to one
  beat), so repeats are close together (slapback at full right, Karplus-Strong-like to the left). The module without a clock cable has a smooth
  Density up to about 4 s. A free-running Echo (Echo Sync parameter, at the end of the list) is the proposed fix.
- Engine test: new diagnostic `AETHER_TEST_ONLY=sweep` prints where the echo taps land for each Density position, with and without tempo.
- Logic's tempo: the host tempo (and song position) feeds the firmware's clock, one pulse per quarter note, 40 to 300 BPM; echo
  time and modulation / ambience auto-pan follow it, also when the tempo changes while playing. No host tempo: the firmware's
  own clock. The window shows the tempo.
- Firmware changes (marked, listed): Echo.h follows clock changes (it stayed on the first tempo), Looper.h does not restart on
  clock pulses (it would chop the loop on every beat).
- Tail: 12 s, measured with a new engine-test section (limits and settings that ring longer are listed in `src/engine/AetherTail.h`).
- Window: final look (section colours, glowing arcs, modulation bar, tempo line); same controls and layout as approved.
- Licence check: sndkit (MIT / Unlicense), OwlProgram LibSource (GPL-3, with fastlog GPL-2+ and fastpow BSD notices),
  LICENSE_VCV.md of the Oneiroi repo read; supersaw thesis PDF not readable from the build workspace (numbers only used).
  The zip now contains LICENSE_VCV-Oneiroi-firmware.md and LICENSE-OwlProgram.txt; the workflow checks the notices are present.
- Tests added: tempo (modulation Hz, echo time ratio, tempo changes, song position, looper unaffected, extremes), tail, and the plugin
  seeing the host tempo, identical sound at host block sizes 256 and 512.

## 0.2.1: Milestone 1 fix round (window and Ambience default)
- Result: PASSED on Nov 11, 2026 in Logic 11.2 on the M4 Mac mini, 44.1 kHz (labels readable, Audio In line shown, no wobble, clean Ambience at every fader position, Resonator and Echo fine together). Milestone 1 complete.
- Window: labels about twice as large, MOD knobs larger (56 to 74 grid units), panel titles and button text larger, default
  size 1260 x 900. Layout otherwise as approved. Audio In box: added "Muting the source track does not stop the side chain."
  (Logic's side chain is taken before the source track's fader and mute; measured in the Milestone 1 test.)
- Ambience (reverb): the default of the Spacetime control is now 0.6 (was 0.5). Cause of the slow "LFO-like" wobble heard in the
  Logic test: between about 0.45 and 0.65 the firmware blends a backwards copy of the sound into the direct sound (at 0.5 about
  73 percent), which swings the level of a held note up to 5 times, one slow cycle every 1.5 s. At 0.6 the swing is small and
  Bryn judged the tone the purest. Parameter order and range unchanged; saved projects keep their own value. Ruled out: the
  DelayLine.h change, the 44.1 kHz path, ARM64 versus Intel (see AetherUmbra-ambience-investigation.md).

## 0.2.0: Milestone 1 (the engine runs)
- Result: first Logic test Nov 11, 2026 (M4 Mac mini, Logic 11.2, 44.1 kHz): 14 of 16 checks passed; Ambience wobble and unreadable labels fixed in 0.2.1.
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

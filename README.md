# AetherUmbra

An unofficial Audio Unit (for Logic Pro) and VST3 plugin for macOS, based on the open-source firmware of the Befaco
Oneiroi (version 1.2.2). **Not made, approved or supported by Befaco.** Do not contact them about this plugin.

AetherUmbra is meant to be a synth voice with effects: it makes sound by itself (oscillators, looper) and also
processes the audio of a Logic track. Pitch comes from MIDI notes instead of the module's V/Oct jack. For personal use
on the author's own Macs.

## Status
Version 1.0.0, Milestone 2 (the finished plugin; tested and passed on both Macs, with one open note on the Echo, see below). AetherUmbra is an AU **Instrument** (plus a VST3
from the same code). It makes sound by itself (sine, supersaw or wavetable oscillator, looper), takes its pitch from MIDI notes
(the last note played, held like a V/Oct voltage), and processes the audio of another Logic track chosen in the plugin's
**Side Chain** menu. Looper, filter, resonator, echo, ambience, the internal modulation, Randomise and Undo/Redo all work.

New in 1.0.0:
- **Logic's tempo** drives the echo (Density is a note length) and the modulation / ambience auto-pan, like the module's clock
  input. Logic's bar and beat position starts the beat in the right place. The window shows the tempo. With no tempo from the host
  the firmware's own clock runs. Tempo is limited to 40 to 300 BPM. The **looper does not follow the tempo** (on the module an
  external clock restarts the loop on every beat; in a plugin that would chop the loop up), so it plays freely.
- **Tail length 12 s**, measured (see below), reported to Logic so Bounce in Place and export do not cut the reverb and echo.
- **Final window**: coloured sections, glowing knob arcs, a modulation level bar, the tempo line.
- Licence check finished (see Source and licence).

Tail: the reverb, echo and resonator are measured as the time until the sound is 80 dB down after a noise burst, at 40, 60
and 120 BPM. Normal settings die away in at most 10.5 s (echo Repeats 0.9, Density 0.5, 40 BPM), so 12 s is reported. A few
settings ring much longer or for ever by the firmware's design (reverb Decay at full, resonator Feedback 0.9 and up, echo
Repeats at full); Logic cuts those at 12 s when it bounces, so use a longer bounce tail or leave the plugin playing.

Things that differ from the module:
- With Logic's tempo the **Echo Density** knob works in the firmware's clocked way: steps from 1/64 of a beat to one beat (at 120 BPM: up to 0.5 s),
  so the repeats are close together. The module without a clock cable has a smooth, much longer Density (up to about 4 s, with pitch-shifting
  sweeps). A free (unsynced) Echo is planned as a fix (see CHANGELOG).
- The **Wavetable** oscillator only sounds when the looper holds a recording (the same on the module).
- The **Filter Noise** knob is new: the firmware has a noise level in its filter but no control for it. At the middle
  position the sound is exactly the firmware's (the noise is only heard when Resonance is above about 0.9, as on the module).
- The **Looper** fader starts at 0. The firmware fills a new looper with quiet noise (the wavetable oscillator uses it as
  its first tables); record something or press Clear, then raise the fader.
- The sine oscillator is on by default (as on the module): the plugin plays a steady tone until you turn Sine down.
- Randomise / Undo-Redo follow the firmware's rules (What = All, Osc, Looper, FX; Amount = Low, Mid, High) and can also be
  automated (the parameters "Randomise" and "Undo/Redo Random").

## Install
1. Download `AetherUmbra-macOS` from the latest run's Artifacts (GitHub, Actions tab) and unzip it.
2. Double-click `Install-AetherUmbra.command`. It asks for the Mac password once and installs the Audio Unit for all
   users, replacing an older copy. It does not install the VST3 (the zip still contains the VST3 files).
3. Open Logic. If the plugins are missing: Logic's Settings > Plug-in Manager > Reset & Rescan Selection.

`Uninstall-AetherUmbra.command` removes AetherUmbra again (Audio Unit and VST3), and the Milestone 0 test shells if they are still there. GitHub deletes a run's download after 90 days. The build is
universal (Apple silicon and Intel), needs macOS 11 or later, and is not signed with an Apple developer ID: on a Mac
that downloaded it from the internet, macOS may ask for approval the first time (`INSTALL.txt` in the zip says what to do).

## Plan
Three milestones: 0 (done: two empty shells, found the right plugin type), 1 (done: the engine runs in Logic, notes set the pitch),
2 (this version: Logic's tempo, tail, final window, licence check; tested on both Macs with the checklist in the CHANGELOG).

## Source and licence
- `third_party/oneiroi-firmware`: Befaco's Oneiroi source, GPL-3.0 (`SOURCE.txt` names the exact commit). Four small changes
  (Filter Noise, a memory-safety fix in the delay line, the echo following tempo changes, the looper not restarting on the clock); changes are marked "AetherUmbra change" and listed in `AETHERUMBRA-CHANGES.md`. The workflow checks this file by
  file on every build.
- `src`: AetherUmbra's own code (the plugin, the window, the engine wrapper in `src/engine`, the OWL stand-ins in `src/engine/shim`).
- `third_party/owlprogram`: the OpenWare / OwlProgram headers the firmware needs, unmodified (GPL).
- The Oneiroi source contains code adapted from Mutable Instruments (MIT), ports from pbat.ch sndkit and a supersaw adapted
  from a published thesis; see `THIRD-PARTY-LICENSES.txt` (all checked: nothing blocks passing the plugin on with its source and notices; the supersaw thesis PDF
  could not be opened from here, only its published numbers are used).
- AetherUmbra is released under GPL-3.0 (`LICENSE`). A build that is passed on must come with its source.

## Test history
- Milestone 0, Nov 11, 2026: automatic plugin test passes (56 checks, both shells: layouts, audio unchanged at 44.1, 48 and 96 kHz, tone follows notes and keeps the last note, no-audio-input case, saving and loading, window text), run natively on Apple silicon and through Rosetta on Intel. Test in Logic 11.2 on the M4 Mac mini (macOS Tahoe), 44.1 kHz: passed, 9 of 9 checks. Both shells install, are listed in the Plug-in Manager and load. Instrument: the test tone follows the notes you play and the pitch stays after release; the window shows the notes; the plug-in header has a Side Chain menu and, with an audio track chosen, the window shows the audio level. Music Effect: the audio of an audio track plays through unchanged, the window shows the level and the tone switch works; on a software instrument track it showed the audio level but received no notes (Bryn's reply, read as "no notes received"). Settings are kept after save and reopen; the uninstall script removes both. Install note from Bryn: the script also installed the VST3s; only the Audio Unit is wanted (fixed after the test, applies from the next build). Intel Mac not tested (not requested).
- Milestone 1, Nov 11, 2026: Logic 11.2 on the M4 Mac mini, 44.1 kHz. First test (commit 193b543): 14 of 16 checks passed; two problems: the Ambience wobbled (slow level swing) and the window labels were too small to read. Fix round (0.2.1, commit 45d638e): larger labels and MOD knobs, side chain note in the Audio In box, Spacetime default 0.6 (the firmware blends a reversed copy of the sound into the direct sound between about 0.45 and 0.65, which swung a held note up to 5 times in level, one cycle every 1.5 s; the DelayLine.h change, the 44.1 kHz path and ARM64 versus Intel were ruled out). Retest: PASSED, all four checks (labels readable, Audio In line shown, fresh instance steady with no wobble, clean tone at every Ambience fader position; Resonator and Echo fine together with it). Findings: Logic's side chain is taken before the source track's fader and mute. Intel Mac not tested (Milestone 2).
- Milestone 2, Nov 11-12, 2026: automatic tests: engine test (x86, ARM64 under qemu, address/undefined-behaviour sanitizers; tempo and tail sections), plugin test (97 checks) natively and through Rosetta on GitHub. Logic checklist (20 items) on the M4 Mac mini (Logic 11.2, 44.1 kHz) and on the Intel Mac (macOS 12.7.8, Logic 10.7.8): PASSED on both. Notes from Bryn: the Echo follows Logic's tempo, but with Density fully right the repeats are close together like a slapback echo, and turning left they blur into a Karplus-Strong-like tone (measured: with tempo, Density runs in 9 clock steps from 1/64 to 1 beat; without tempo it is smooth up to about 4 s); Supersaw works; Wavetable needs a recording in the looper (same on the module).

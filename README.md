# AetherUmbra

An unofficial Audio Unit (for Logic Pro) and VST3 plugin for macOS, based on the open-source firmware of the Befaco
Oneiroi (version 1.2.2). **Not made, approved or supported by Befaco.** Do not contact them about this plugin.

AetherUmbra is meant to be a synth voice with effects: it makes sound by itself (oscillators, looper) and also
processes the audio of a Logic track. Pitch comes from MIDI notes instead of the module's V/Oct jack. For personal use
on the author's own Macs.

## Status
Version 0.2.1, Milestone 1 (fix round): the Oneiroi engine runs as an AU **Instrument** (plus a VST3 from the same code) with a rough
but complete window (every control of the approved "module layout"). It makes sound by itself (sine, supersaw or
wavetable oscillator, looper), takes its pitch from MIDI notes (the last note played, held like a V/Oct voltage), and
processes the audio of another Logic track chosen in the plugin's **Side Chain** menu. Looper, filter, resonator, echo,
ambience, the internal modulation, Randomise and Undo/Redo are all working. Not in this version: Logic's tempo for the
echo and modulation (Milestone 2), a measured tail length (12 s is used, measured for the similar Iro-x effects), the final
window design.

Things that differ from the module:
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
Three milestones: 0 (done: two empty shells, found the right plugin type), 1 (this one: the engine runs in Logic with a rough
window, notes set the pitch), 2 (finished: Logic's tempo, tail, final window, licence check, both Macs).

## Source and licence
- `third_party/oneiroi-firmware`: Befaco's Oneiroi source, GPL-3.0 (`SOURCE.txt` names the exact commit). Two small changes
  (Filter Noise, one memory-safety fix in the delay line); changes are marked "AetherUmbra change" and listed in `AETHERUMBRA-CHANGES.md`. The workflow checks this file by
  file on every build.
- `src`: AetherUmbra's own code (the plugin, the window, the engine wrapper in `src/engine`, the OWL stand-ins in `src/engine/shim`).
- `third_party/owlprogram`: the OpenWare / OwlProgram headers the firmware needs, unmodified (GPL).
- The Oneiroi source contains code adapted from Mutable Instruments (MIT), ports from pbat.ch sndkit and a supersaw adapted
  from a published thesis; see `THIRD-PARTY-LICENSES.txt`. Licences still to check are named there.
- AetherUmbra is released under GPL-3.0 (`LICENSE`). A build that is passed on must come with its source.

## Test history
- Milestone 0, Nov 11, 2026: automatic plugin test passes (56 checks, both shells: layouts, audio unchanged at 44.1, 48 and 96 kHz, tone follows notes and keeps the last note, no-audio-input case, saving and loading, window text), run natively on Apple silicon and through Rosetta on Intel. Test in Logic 11.2 on the M4 Mac mini (macOS Tahoe), 44.1 kHz: passed, 9 of 9 checks. Both shells install, are listed in the Plug-in Manager and load. Instrument: the test tone follows the notes you play and the pitch stays after release; the window shows the notes; the plug-in header has a Side Chain menu and, with an audio track chosen, the window shows the audio level. Music Effect: the audio of an audio track plays through unchanged, the window shows the level and the tone switch works; on a software instrument track it showed the audio level but received no notes (Bryn's reply, read as "no notes received"). Settings are kept after save and reopen; the uninstall script removes both. Install note from Bryn: the script also installed the VST3s; only the Audio Unit is wanted (fixed after the test, applies from the next build). Intel Mac not tested (not requested).

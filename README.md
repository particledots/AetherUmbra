# AetherUmbra

An unofficial Audio Unit (for Logic Pro) and VST3 plugin for macOS, based on the open-source firmware of the Befaco
Oneiroi (version 1.2.2). **Not made, approved or supported by Befaco.** Do not contact them about this plugin.

AetherUmbra is meant to be a synth voice with effects: it makes sound by itself (oscillators, looper) and also
processes the audio of a Logic track. Pitch comes from MIDI notes instead of the module's V/Oct jack. For personal use
on the author's own Macs.

## Status
Version 0.1.0, Milestone 0 (two empty test shells). Nothing of the Oneiroi engine runs yet. The firmware source is
imported unmodified into `third_party/oneiroi-firmware` and is not compiled yet.

The two shells (an AU **Instrument**, type `aumu`, and an AU **Music Effect**, type `aumf`) exist to find out in Logic
which type can be given audio and MIDI notes at the same time. Each one passes the audio of its track through unchanged,
adds a simple test tone (a sine at -20 dB, switchable) that follows the last MIDI note, and shows in its window what it
receives: the number of audio input channels with their level, and the last note.

## Install
1. Download `AetherUmbra-macOS` from the latest run's Artifacts (GitHub, Actions tab) and unzip it.
2. Double-click `Install-AetherUmbra.command`. It asks for the Mac password once and installs the Audio Units and the
   VST3s for all users, replacing older copies.
3. Open Logic. If the plugins are missing: Logic's Settings > Plug-in Manager > Reset & Rescan Selection.

`Uninstall-AetherUmbra.command` removes them again. GitHub deletes a run's download after 90 days. The build is
universal (Apple silicon and Intel), needs macOS 11 or later, and is not signed with an Apple developer ID: on a Mac
that downloaded it from the internet, macOS may ask for approval the first time (`INSTALL.txt` in the zip says what to do).

## Plan
Three milestones: 0 (this one: two empty shells, find the right plugin type), 1 (the engine runs in Logic with a rough
window, notes set the pitch), 2 (finished: Logic's tempo, tail, randomiser, final window, licence check, both Macs).

## Source and licence
- `third_party/oneiroi-firmware`: Befaco's Oneiroi source, GPL-3.0 (`SOURCE.txt` names the exact commit). No changes yet;
  later changes are marked "AetherUmbra change" and listed in `AETHERUMBRA-CHANGES.md`. The workflow checks this file by
  file on every build.
- `src`: AetherUmbra's own code (the shells and their window).
- The Oneiroi source contains code adapted from Mutable Instruments (MIT), ports from pbat.ch sndkit and a supersaw adapted
  from a published thesis; see `THIRD-PARTY-LICENSES.txt`. Licences still to check are named there.
- AetherUmbra is released under GPL-3.0 (`LICENSE`). A build that is passed on must come with its source.

## Test history
- Milestone 0 (automatic plugin test, workspace, Linux): see CHANGELOG. Test in Logic: not done yet.

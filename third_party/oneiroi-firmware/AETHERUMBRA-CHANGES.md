# Changes to the Oneiroi firmware copy

Every firmware file is a byte-for-byte copy of Befaco/Oneiroi at the commit named in SOURCE.txt, except the files
listed below. Each change is marked in the file with the words "AetherUmbra change". The workflow checks this file by
file on every build and fails if a changed file is not marked and listed.

## Listed changes
- Filter.h: the Filter Noise control. The noise level now also depends on the plugin's Filter Noise setting
  (patchCtrls_->filterNoiseLevel, set to 0.5 = exactly the firmware's own level). The firmware declares this field but never uses it.
- DelayLine.h: the read position is kept inside the buffer (while-loop instead of a single wrap). The original could read
  outside its memory when a delay time plus modulation reached the buffer length (found by the address sanitizer in the
  Ambience section). Results in range are identical.
- Echo.h (Milestone 2): in the clock-synced mode the tap times are also set again when the clock period changes
  (new member `clockSamples_`). The original only recalculated them when the Density control moved to another step, so
  after a tempo change the echo stayed at the old tempo. Befaco's own Iroi source has this same fix. The engine test
  checks it (tempo 120 to 90 BPM while playing).
- Looper.h (Milestone 2): the loop is no longer restarted by clock pulses (`kLooperFollowsClock = false`). The module
  restarts the loop on every pulse on its sync input; in the plugin Logic's tempo is always present, so the loop would
  jump back every beat. Without a host tempo nothing changes (the internal clock case is untouched). Echo, Modulation
  and Ambience Auto-pan still follow Logic's tempo. The engine test checks that the looper sounds identical with and
  without a host tempo.
- Commons.h (version 1.1.0): new fields that the plugin's new controls need. In `PatchCtrls`: `echoSync`, `looperStartSnap`, `looperLengthSnap`
  (0 = off, 1 = on; the engine clears the structure, so the module's behaviour is unchanged when they are 0 apart from Echo Sync, see
  Echo.h). In `PatchState`: `hostBeatSamples`, the length of one beat of Logic's tempo in samples (0 = no host tempo). Nothing else in the
  file changed.
- Echo.h (version 1.1.0, Echo Sync): the clocked echo mode (Density in steps of the tempo) is now used only while a clock is present AND
  `echoSync` is on. With Echo Sync off the echo runs in the firmware's own free mode (smooth Density, pitch-shifting sweeps) even when Logic
  gives a tempo, like the module with nothing on its sync input. In that free mode, while a host tempo is present, the Density range is that
  of the module's own clock (8192 blocks x 23 = about 4 s) instead of the length of a host beat. (The module's own free mode takes this
  range from the looper's cycle, which changes with the looper length; the plugin keeps it fixed at 4 s when Logic gives a tempo.) Modulation
  and auto-pan are not touched by Echo Sync: they follow the clock either way.
- Looper.h (version 1.1.0, Looper Start / Length snap): when `looperStartSnap` / `looperLengthSnap` is on and `hostBeatSamples` is known,
  the finished Start (knob plus modulation) or Length value is moved to the nearest 1/16 note of the host tempo (a quarter of a beat in
  samples) and used as an exact number of samples (`SetStart` / `SetLength` take an optional exact value instead of the 128-step lookup
  table, which cannot hit beat divisions). Length never goes below one grid step or outside the buffer. With the switch off, or without a
  host tempo, the code path is the firmware's own. The other four snap switches (Oscillator Pitch, Looper Speed, Resonator Tune,
  Filter Cutoff) need no firmware change: they are done in `src/engine/AetherEngine.cpp` on the knob value before the firmware reads it.
- LorenzAttractor.h (version 1.1.0, found by the random-settings test): (1) the integration step is limited to 0.005 (the module's own
  limit at the top of the range was 0.00667). At 0.00667 with the "torus" setting used by the Mod chaos type, the simple integration runs
  away within about 130 steps: x becomes infinite and stays infinite, and every module that reads the modulation (oscillator offset, echo
  Density, and others) then gives invalid numbers until the plugin is reloaded. That setting is reached when Mod type is Chaos and Mod
  speed is at its fastest ratios with a host tempo of about 120 BPM or more (tempo x 32 or x 64). 0.005 was checked to stay bounded for
  10 minutes of audio; the chaos modulation at those top speeds is about a quarter slower. (2) If the state ever becomes invalid or larger
  than 1000, it starts again from its initial values instead of staying broken.

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

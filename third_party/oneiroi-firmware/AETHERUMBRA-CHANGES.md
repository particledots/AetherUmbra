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

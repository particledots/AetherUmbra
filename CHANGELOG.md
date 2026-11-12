# Changelog

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
- Result: not tested in Logic yet.

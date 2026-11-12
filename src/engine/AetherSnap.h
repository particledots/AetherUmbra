// The snap grids of the seven AetherUmbra snap switches (new in 1.1.0), in one place. The engine uses them on the knob value before
// the firmware reads it (so automation, saved projects and Mod are covered); the plugin window uses the same functions so that the
// knob itself clicks from step to step and shows what it is set to (1.1.1).
// Each function takes a knob value 0..1 and returns the knob value of the nearest step.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace aether
{
namespace snap
{
constexpr float kSpeedZero = 0.51f;               // where the Looper Speed knob is stopped (Commons.h CenterMap, speedZero)
constexpr float kSpeedTop = 0.99f;
constexpr float kLooperBufferSamples = 262144.f;  // kLooperChannelBufferLength
constexpr float kLooperLengthMinSamples = 367.f;  // kLooperLoopLengthMin
constexpr float kLn2200 = 7.696213f;              // ln (22000 / 10)

// The Pitch knob: 12 * (value - 0.5) semitones, so whole semitones are 13 positions from -6 to +6.
inline float pitchKnob (float v)
{
    return std::min (1.f, std::max (0.f, 0.5f + std::round ((v - 0.5f) * 12.f) / 12.f));
}
inline int pitchSemitones (float v) { return (int) std::lround (12.f * (v - 0.5f)); }

// The Looper Speed knob: the firmware's CenterMap turns it into -2 .. +2 (0 at kSpeedZero). Ratios 2^(n/12), up and reversed. The
// zones the firmware locks at 0x (under 0.1) and 1x / -1x (0.95 to 1.05) are left alone.
inline float speedValue (float knob, float zero = kSpeedZero)
{
    return knob < zero ? -2.f + 2.f * knob / zero : 2.f * (knob - zero) / (kSpeedTop - zero);
}
inline float speedKnob (float knob, float zero = kSpeedZero)
{
    const float v = speedValue (knob, zero);
    const float a = std::fabs (v);
    if (a <= 0.1f || (a >= 0.95f && a <= 1.05f)) return knob;
    const int n = std::min (12, (int) std::lround (12.f * std::log2 (a)));
    const float s = std::copysign (std::exp2 ((float) n / 12.f), v);
    return s < 0.f ? (s + 2.f) * zero / 2.f : zero + s * (kSpeedTop - zero) / 2.f;
}

// The Resonator Tune knob: the main pole moves from -24 semitones to +range (24 at Dissonance 0, 16 at full, an integer as in the
// firmware's Resonator.h). Whole semitones.
inline int tuneRange (float dissonance) { return (int) (24.f - 8.f * std::min (1.f, std::max (0.f, dissonance))); }
inline int tuneSemitones (float tune, float dissonance)
{
    const int range = tuneRange (dissonance);
    return std::min (range, std::max (-24, (int) std::lround (-24.f + tune * (float) (range + 24))));
}
inline float tuneKnob (float tune, float dissonance)
{
    const int range = tuneRange (dissonance);
    return (float) (tuneSemitones (tune, dissonance) + 24) / (float) (range + 24);
}

// The Filter Cutoff knob: 10 Hz * 2200^value (10 Hz to 22 kHz, Filter.h). Notes of the equal-tempered scale, A = 440 Hz. The two ends
// (fully closed and fully open) stay where they are.
inline float cutoffHz (float v) { return 10.f * std::exp (v * kLn2200); }
inline int cutoffNote (float v) { return (int) std::lround (12.f * std::log2 (cutoffHz (v) / 440.f)); }     // semitones from A4
inline float cutoffKnob (float v)
{
    if (v <= 0.005f || v >= 0.995f) return v;
    const float snapped = 440.f * std::exp2 ((float) cutoffNote (v) / 12.f);
    return std::min (1.f, std::max (0.f, std::log (snapped / 10.f) / kLn2200));
}

// Looper Start and Length: the grid is a 1/16 note of Logic's tempo (a quarter of a beat), as exact samples. gridSamples = 0 means
// no tempo known: free. These work on the samples the knob asks for, as Looper.h does; the knob value of the step is returned.
inline float startSamples (float v) { return std::min (1.f, std::max (0.f, v)) * (kLooperBufferSamples - 1.f); }
inline float startKnob (float v, float gridSamples)
{
    if (gridSamples < 1.f) return v;
    const float s = std::min (kLooperBufferSamples - 1.f, std::floor (startSamples (v) / gridSamples + 0.5f) * gridSamples);
    return s / (kLooperBufferSamples - 1.f);
}
inline float lengthSamples (float v)
{
    const float c = std::min (1.f, std::max (0.f, v));
    return kLooperLengthMinSamples + (kLooperBufferSamples - kLooperLengthMinSamples) * c * c * c;
}
inline float lengthKnob (float v, float gridSamples)
{
    if (gridSamples < 1.f) return v;
    const float s = std::min (kLooperBufferSamples, std::max (kLooperLengthMinSamples, std::max (1.f, std::floor (lengthSamples (v) / gridSamples + 0.5f)) * gridSamples));
    return std::cbrt ((s - kLooperLengthMinSamples) / (kLooperBufferSamples - kLooperLengthMinSamples));
}

// ---- read-outs (the text next to the knob while it is turned) ------------------------------------------------------------------
inline std::string pitchText (float v)
{
    char b[48]; const int n = pitchSemitones (v);
    std::snprintf (b, sizeof b, "%+d st", n); return b;
}
inline std::string speedText (float knob)
{
    const float v = speedValue (knob), a = std::fabs (v);
    char b[48];
    if (a <= 0.1f) return "stopped";
    if (a >= 0.95f && a <= 1.05f) { std::snprintf (b, sizeof b, "%s 1.00x", v < 0 ? "rev" : "fwd"); return b; }
    const int n = (int) std::lround (12.f * std::log2 (a));
    std::snprintf (b, sizeof b, "%s %+d st (%.2fx)", v < 0 ? "rev" : "fwd", n, (double) a); return b;
}
inline std::string tuneText (float tune, float dissonance)
{
    char b[48]; std::snprintf (b, sizeof b, "%+d st", tuneSemitones (tune, dissonance)); return b;
}
inline std::string cutoffText (float v)
{
    static const char* names[12] = { "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#" };
    const int n = cutoffNote (v);
    const int idx = ((n % 12) + 12) % 12;
    const int octave = 4 + (int) std::floor ((float) (n + 9) / 12.f);      // C4 is 3 semitones above A3: octave changes at C
    char b[64]; std::snprintf (b, sizeof b, "%s%d  %.0f Hz", names[idx], octave, (double) (440.f * std::exp2 ((float) n / 12.f))); return b;
}
inline std::string sixteenthsText (float samples, float gridSamples)
{
    char b[48];
    if (gridSamples < 1.f) { std::snprintf (b, sizeof b, "%.2f s (no tempo)", (double) (samples / 48000.f)); return b; }
    std::snprintf (b, sizeof b, "%d x 1/16 note", (int) std::lround (samples / gridSamples)); return b;
}
}
}

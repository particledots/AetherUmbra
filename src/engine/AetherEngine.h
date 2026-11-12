// AetherUmbra engine wrapper. No JUCE and no firmware headers here, so the plugin code stays clean and the engine can be
// tested on its own (tools/engine_test).
//
// The Oneiroi firmware was written for a module that runs at 48 kHz in blocks of 32 samples (1500 blocks per second).
// Many of its numbers are tied to that (delay line lengths, the modulation clock, the filter smoothing, the looper), so
// the engine always runs exactly like that. AetherStream (AetherStream.h) puts a resampler and a block FIFO around it so
// the plugin works at any host sample rate and block size.
#pragma once
#include <memory>

namespace aether
{
constexpr int kEngineBlockSize = 32;
constexpr double kEngineSampleRate = 48000.0;

// Every value is what a knob or fader of the module would report: 0 to 1 (unless a comment says otherwise).
// The names follow PatchCtrls in the firmware (Commons.h).
struct EngineParams
{
    // Faders
    float inputVolume = 0.7f;
    float looperVolume = 0.7f;
    float osc1Volume = 0.6f;             // sine
    float osc2Volume = 0.0f;             // supersaw or wavetable
    float filterLevel = 1.0f;            // the VCA after the filter
    float resonatorLevel = 0.0f;
    float echoLevel = 0.0f;
    float ambienceLevel = 0.0f;

    // Looper
    float looperSos = 0.0f;              // sound on sound (the module's Start knob, shifted)
    float looperFilter = 0.5f;           // 0.45 to 0.55 = off, below low pass, above high pass
    float looperSpeed = 0.75f;           // 0.75 = 1x forward, about 0.51 = stopped, 0 = 2x backwards, 1 = 2x forward
    float looperSpeedMod = 0.0f;
    float looperStart = 0.0f;
    float looperStartMod = 0.0f;
    float looperLength = 1.0f;
    float looperLengthMod = 0.0f;
    bool looperRecording = false;
    bool looperPost = false;             // the Pre/Post switch: false = record the input, true = record the output (resample)

    // Oscillators
    int oscOctave = 5;                   // 1..9 (the module's octave setting); 5 plays MIDI notes at their own pitch
    float oscUnison = 0.0f;              // -1..1 (the module's Unison knob, centre = off)
    float oscPitch = 0.5f;               // the Pitch knob, 0.5 = centre, +- 6 semitones
    float oscPitchMod = 0.0f;
    float oscDetune = 0.0f;
    float oscDetuneMod = 0.0f;
    bool oscUseWavetable = false;
    // The last MIDI note played, in semitones above or below middle C (note 60). Replaces the module's V/Oct jack.
    float noteOffset = 0.0f;

    // Filter + VCA
    float filterMode = 0.125f;           // 0..0.25 low pass, to 0.5 band pass, to 0.75 high pass, above comb
    float filterNoise = 0.5f;            // AetherUmbra addition: 0.5 = the firmware's own noise (see AETHERUMBRA-CHANGES.md)
    float filterCutoff = 1.0f;
    float filterCutoffMod = 0.5f;
    float filterResonance = 0.0f;
    float filterResonanceMod = 0.0f;
    float filterPosition = 0.125f;       // 4 positions in the chain: 0..0.25, 0.25..0.5, 0.5..0.75, 0.75..1

    // Resonator
    float resonatorTune = 0.5f;
    float resonatorTuneMod = 0.0f;
    float resonatorFeedback = 0.5f;
    float resonatorFeedbackMod = 0.0f;
    float resonatorDissonance = 0.0f;

    // Echo
    float echoRepeats = 0.5f;
    float echoRepeatsMod = 0.0f;
    float echoDensity = 0.5f;
    float echoDensityMod = 0.0f;
    float echoFilter = 0.5f;             // 0.45 to 0.55 = off, below low pass, above high pass

    // Ambience
    float ambienceDecay = 0.5f;
    float ambienceDecayMod = 0.0f;
    float ambienceSpacetime = 0.5f;
    float ambienceSpacetimeMod = 0.0f;
    float ambienceAutoPan = 0.0f;

    // Modulation
    float modLevel = 0.0f;
    float modSpeed = 0.5f;
    float modType = 0.0f;                // 0 Lorenz, 1/6 sine, 2/6 ramp, 3/6 inverted ramp, 4/6 square, 5/6 sample and hold, 1 envelope follower
};

// One Oneiroi engine. Not thread safe: one thread at a time per object. Every object has its own state.
class Engine
{
public:
    Engine();
    ~Engine();
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    void setParams (const EngineParams& p);
    // Empties the looper (the module's Shift + Record). Takes effect in the next block.
    void clearLooper();
    // Processes exactly kEngineBlockSize samples in place (48 kHz). The input goes in, the instrument's sound comes out.
    void processBlock (float* left, float* right);

    // For the window: the modulation value (about -1..1) and the oscillator pitch in Hz as the engine uses it.
    float modulationValue() const;
    float oscillatorHz() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}

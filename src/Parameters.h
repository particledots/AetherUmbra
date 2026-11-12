#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AetherEngine.h"
#include "AetherTail.h"
#include <cmath>
#include <vector>

// Parameter ids. They are FIXED from Milestone 1 on: saved Logic projects and automation refer to them.
// New parameters are only ever added at the END of the list (see createLayout).
namespace aether_params
{
// Tail length reported to Logic, in seconds: measured for AetherUmbra, see src/engine/AetherTail.h.
constexpr double kTailSeconds = aether::kTailSeconds;

struct FloatDef { const char* id; const char* name; float def; };

// Names for the choice parameters (what the window and Logic show).
inline juce::StringArray filterModeNames()   { return { "Low pass", "Band pass", "High pass", "Comb" }; }
inline juce::StringArray filterPositionNames() { return { "I", "II", "III", "IV" }; }
inline juce::StringArray modTypeNames()      { return { "Lorenz", "Sine", "Ramp", "Inverted ramp", "Square", "Sample and hold", "Envelope follower" }; }
inline juce::StringArray prePostNames()      { return { "Pre", "Post" }; }
inline juce::StringArray oscKindNames()      { return { "Supersaw", "Wavetable" }; }
inline juce::StringArray randomModeNames()   { return { "All", "Osc", "Looper", "FX" }; }
inline juce::StringArray randomAmountNames() { return { "Low", "Mid", "High" }; }

// The order of this function IS the order in Logic's parameter list. Do not reorder; add new ones at the end.
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f), def));
    };
    auto b = [&] (const char* id, const char* name, bool def)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto c = [&] (const char* id, const char* name, const StringArray& choices, int def)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };

    // Faders and the input
    f ("input_volume", "Input Volume", 0.7f);
    // Looper
    // Starts at 0: the firmware fills a new looper with quiet noise (the wavetable oscillator uses it as its first tables),
    // so an open Looper fader on a fresh plugin would hiss. Record something, or press Clear, and raise the fader.
    f ("looper_volume", "Looper Volume", 0.0f);
    f ("looper_sos", "Looper Sound on Sound", 0.0f);
    f ("looper_filter", "Looper Filter", 0.5f);
    f ("looper_speed", "Looper Speed", 0.75f);
    f ("looper_speed_mod", "Looper Speed Mod", 0.0f);
    f ("looper_start", "Looper Start", 0.0f);
    f ("looper_start_mod", "Looper Start Mod", 0.0f);
    f ("looper_length", "Looper Length", 1.0f);
    f ("looper_length_mod", "Looper Length Mod", 0.0f);
    b ("looper_recording", "Looper Record", false);
    c ("looper_resampling", "Looper Pre/Post", prePostNames(), 0);
    // Oscillators
    f ("osc1_volume", "Sine Volume", 0.6f);
    f ("osc2_volume", "Supersaw / Wavetable Volume", 0.0f);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "osc_octave", 1 }, "Oscillator Octave", 1, 9, 5));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "osc_unison", 1 }, "Oscillator Unison", NormalisableRange<float> (-1.f, 1.f), 0.f));
    f ("osc_pitch", "Oscillator Pitch", 0.5f);
    f ("osc_pitch_mod", "Oscillator Pitch Mod", 0.0f);
    f ("osc_detune", "Oscillator Detune", 0.0f);
    f ("osc_detune_mod", "Oscillator Detune Mod", 0.0f);
    c ("osc_kind", "Oscillator Supersaw/Wavetable", oscKindNames(), 0);
    // Filter
    f ("filter_level", "Filter Level", 1.0f);
    c ("filter_mode", "Filter Mode", filterModeNames(), 0);
    f ("filter_noise", "Filter Noise", 0.5f);
    f ("filter_cutoff", "Filter Cutoff", 1.0f);
    f ("filter_cutoff_mod", "Filter Cutoff Mod", 0.5f);
    f ("filter_resonance", "Filter Resonance", 0.0f);
    f ("filter_resonance_mod", "Filter Resonance Mod", 0.0f);
    c ("filter_position", "Filter Position", filterPositionNames(), 0);
    // Resonator
    f ("resonator_volume", "Resonator Volume", 0.0f);
    f ("resonator_tune", "Resonator Tune", 0.5f);
    f ("resonator_tune_mod", "Resonator Tune Mod", 0.0f);
    f ("resonator_feedback", "Resonator Feedback", 0.5f);
    f ("resonator_feedback_mod", "Resonator Feedback Mod", 0.0f);
    f ("resonator_dissonance", "Resonator Dissonance", 0.0f);
    // Echo
    f ("echo_volume", "Echo Volume", 0.0f);
    f ("echo_repeats", "Echo Repeats", 0.5f);
    f ("echo_repeats_mod", "Echo Repeats Mod", 0.0f);
    f ("echo_density", "Echo Density", 0.5f);
    f ("echo_density_mod", "Echo Density Mod", 0.0f);
    f ("echo_filter", "Echo Filter", 0.5f);
    // Ambience
    f ("ambience_volume", "Ambience Volume", 0.0f);
    f ("ambience_decay", "Ambience Decay", 0.5f);
    f ("ambience_decay_mod", "Ambience Decay Mod", 0.0f);
    f ("ambience_spacetime", "Ambience Spacetime", 0.6f);
    f ("ambience_spacetime_mod", "Ambience Spacetime Mod", 0.0f);
    f ("ambience_autopan", "Ambience Auto-pan", 0.0f);
    // Modulation
    f ("mod_level", "Modulation Level", 0.0f);
    f ("mod_speed", "Modulation Speed", 0.5f);
    c ("mod_type", "Modulation Type", modTypeNames(), 0);
    // Randomiser
    c ("random_mode", "Random What", randomModeNames(), 0);
    c ("random_amount", "Random Amount", randomAmountNames(), 1);
    // Output
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output_level", 1 }, "Output Level", NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    // Added at the end so Logic can automate the two buttons: going from off to on does it once; the plugin switches it off again.
    b ("randomise", "Randomise", false);
    b ("undo_redo", "Undo/Redo Random", false);
    // Added in 1.1.0 (parameters 56 to 62), at the end as always. Echo Sync: off = free echo with a smooth Density (like the module with
    // no clock cable); on = Density steps follow Logic's tempo. Snap switches: one per knob, see Engine (AetherEngine.cpp).
    b ("echo_sync", "Echo Sync", false);
    b ("snap_osc_pitch", "Oscillator Pitch Snap", true);
    b ("snap_looper_speed", "Looper Speed Snap", false);
    b ("snap_resonator_tune", "Resonator Tune Snap", false);
    b ("snap_filter_cutoff", "Filter Cutoff Snap", false);
    b ("snap_looper_start", "Looper Start Snap", false);
    b ("snap_looper_length", "Looper Length Snap", false);
    return l;
}

inline aether::EngineParams toEngine (const juce::AudioProcessorValueTreeState& s, float noteOffset)
{
    auto v = [&] (const char* id) { return s.getRawParameterValue (id)->load(); };
    aether::EngineParams p;
    p.inputVolume = v ("input_volume");
    p.looperVolume = v ("looper_volume"); p.looperSos = v ("looper_sos"); p.looperFilter = v ("looper_filter");
    p.looperSpeed = v ("looper_speed"); p.looperSpeedMod = v ("looper_speed_mod");
    p.looperStart = v ("looper_start"); p.looperStartMod = v ("looper_start_mod");
    p.looperLength = v ("looper_length"); p.looperLengthMod = v ("looper_length_mod");
    p.looperRecording = v ("looper_recording") > 0.5f;
    p.looperPost = v ("looper_resampling") > 0.5f;
    p.osc1Volume = v ("osc1_volume"); p.osc2Volume = v ("osc2_volume");
    p.oscOctave = (int) std::lround (v ("osc_octave"));
    p.oscUnison = v ("osc_unison");
    p.oscPitch = v ("osc_pitch"); p.oscPitchMod = v ("osc_pitch_mod");
    p.oscDetune = v ("osc_detune"); p.oscDetuneMod = v ("osc_detune_mod");
    p.oscUseWavetable = v ("osc_kind") > 0.5f;
    p.noteOffset = noteOffset;
    p.filterLevel = v ("filter_level");
    p.filterMode = (v ("filter_mode") + 0.5f) / 4.f;          // 4 choices -> the middle of each quarter
    p.filterNoise = v ("filter_noise");
    p.filterCutoff = v ("filter_cutoff"); p.filterCutoffMod = v ("filter_cutoff_mod");
    p.filterResonance = v ("filter_resonance"); p.filterResonanceMod = v ("filter_resonance_mod");
    p.filterPosition = (v ("filter_position") + 0.5f) / 4.f;
    p.resonatorLevel = v ("resonator_volume"); p.resonatorTune = v ("resonator_tune"); p.resonatorTuneMod = v ("resonator_tune_mod");
    p.resonatorFeedback = v ("resonator_feedback"); p.resonatorFeedbackMod = v ("resonator_feedback_mod");
    p.resonatorDissonance = v ("resonator_dissonance");
    p.echoLevel = v ("echo_volume"); p.echoRepeats = v ("echo_repeats"); p.echoRepeatsMod = v ("echo_repeats_mod");
    p.echoDensity = v ("echo_density"); p.echoDensityMod = v ("echo_density_mod"); p.echoFilter = v ("echo_filter");
    p.ambienceLevel = v ("ambience_volume"); p.ambienceDecay = v ("ambience_decay"); p.ambienceDecayMod = v ("ambience_decay_mod");
    p.ambienceSpacetime = v ("ambience_spacetime"); p.ambienceSpacetimeMod = v ("ambience_spacetime_mod");
    p.ambienceAutoPan = v ("ambience_autopan");
    p.modLevel = v ("mod_level"); p.modSpeed = v ("mod_speed");
    p.modType = v ("mod_type") / 6.f;                          // 7 shapes
    p.echoSync = v ("echo_sync") > 0.5f;
    p.snapOscPitch = v ("snap_osc_pitch") > 0.5f;
    p.snapLooperSpeed = v ("snap_looper_speed") > 0.5f;
    p.snapResonatorTune = v ("snap_resonator_tune") > 0.5f;
    p.snapFilterCutoff = v ("snap_filter_cutoff") > 0.5f;
    p.snapLooperStart = v ("snap_looper_start") > 0.5f;
    p.snapLooperLength = v ("snap_looper_length") > 0.5f;
    return p;
}

// ---- Randomiser and Undo/Redo (the module's Random button and its second function) --------------------------------------
// The same rules as the firmware (ParamController.h, LockableParam::Randomize, and Ui.h Randomize / UndoRedo):
//   What = All, Osc, Looper or FX decides which knobs move; Amount = Low / Mid / High.
//   Low:  move by up to 0.02 towards the centre of the knob (a bit past it is possible)
//   Mid:  move by up to 0.10 towards the centre
//   High: jump to a random value (1 in 10 times: to 0.9 or 0.1, away from the centre)
//   Pitch (the semitone knob): Low = 1 or 2 semitones, Mid = 2 to 4 semitones, High = anywhere; always whole semitones.
// Undo/Redo goes back to the values from before the last randomise (and forth again when pressed a second time).
enum class RandomWhat { All = 0, Osc, Looper, FX };
enum class RandomHow { Low = 0, Mid, High };

struct RandomTarget { const char* id; RandomWhat group; bool semitones; };
inline const RandomTarget* randomTargets (int& count)
{
    static const RandomTarget t[] = {
        { "osc_pitch", RandomWhat::Osc, true }, { "osc_detune", RandomWhat::Osc, false },
        { "looper_speed", RandomWhat::Looper, false }, { "looper_start", RandomWhat::Looper, false }, { "looper_length", RandomWhat::Looper, false },
        { "filter_cutoff", RandomWhat::FX, false }, { "filter_resonance", RandomWhat::FX, false },
        { "resonator_tune", RandomWhat::FX, false }, { "resonator_feedback", RandomWhat::FX, false },
        { "echo_repeats", RandomWhat::FX, false }, { "echo_density", RandomWhat::FX, false },
        { "ambience_decay", RandomWhat::FX, false }, { "ambience_spacetime", RandomWhat::FX, false } };
    count = (int) (sizeof (t) / sizeof (t[0]));
    return t;
}
inline bool inGroup (RandomWhat chosen, RandomWhat group) { return chosen == RandomWhat::All || chosen == group; }

// value: 0..1. rf: a random number 0..1 (the firmware uses one number for the size and the direction).
inline float randomisedValue (float value, RandomHow how, bool semitones, float rf)
{
    const float s1 = value < 0.5f ? 1.f : -1.f;      // towards the centre
    const float s2 = rf < 0.5f ? 1.f : -1.f;         // either way
    float v;
    if (how == RandomHow::High)
        v = rf < 0.1f ? (value < 0.5f ? 0.9f : 0.1f) : rf;
    else if (how == RandomHow::Mid)
        v = semitones ? value + 0.083f * (rf < 0.1f ? 4.f : rf < 0.5f ? 3.f : 2.f) * s2 : value + rf * 0.1f * s1;
    else
        v = semitones ? value + 0.083f * (rf < 0.1f ? 2.f : 1.f) * s2 : value + rf * 0.02f * s1;
    v = juce::jlimit (0.f, 1.f, v);
    if (semitones) v = std::rint (v * 12.f) / 12.f;
    return v;
}

class Randomiser
{
public:
    Randomiser() : rng_ (0x4165746855ull) { previous_.assign (13, -1.f); }

    // Message thread only. Each control moves inside a change gesture so Logic can record it as automation.
    void randomise (juce::AudioProcessorValueTreeState& s)
    {
        const auto what = (RandomWhat) juce::jlimit (0, 3, (int) std::lround (s.getRawParameterValue ("random_mode")->load()));
        const auto how  = (RandomHow)  juce::jlimit (0, 2, (int) std::lround (s.getRawParameterValue ("random_amount")->load()));
        int n = 0; const RandomTarget* t = randomTargets (n);
        for (int i = 0; i < n; ++i)
        {
            if (! inGroup (what, t[i].group)) continue;
            auto* p = s.getParameter (t[i].id);
            if (p == nullptr) continue;
            const float now = p->getValue();
            previous_[(size_t) i] = now;
            set (*p, randomisedValue (now, how, t[i].semitones, rng_.nextFloat()));
        }
    }

    // Swaps the current values of the controls in the chosen group with the stored ones.
    void undoRedo (juce::AudioProcessorValueTreeState& s)
    {
        const auto what = (RandomWhat) juce::jlimit (0, 3, (int) std::lround (s.getRawParameterValue ("random_mode")->load()));
        int n = 0; const RandomTarget* t = randomTargets (n);
        for (int i = 0; i < n; ++i)
        {
            if (! inGroup (what, t[i].group) || previous_[(size_t) i] < 0.f) continue;
            auto* p = s.getParameter (t[i].id);
            if (p == nullptr) continue;
            const float now = p->getValue();
            set (*p, previous_[(size_t) i]);
            previous_[(size_t) i] = now;
        }
    }

private:
    static void set (juce::RangedAudioParameter& p, float normalised)
    {
        p.beginChangeGesture();
        p.setValueNotifyingHost (normalised);
        p.endChangeGesture();
    }
    juce::Random rng_;
    std::vector<float> previous_;     // -1 = nothing stored
};
}

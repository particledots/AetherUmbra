// The only file that includes the Oneiroi firmware. The firmware's hardware layer (Ui.h, ParamController.h, Midi.h,
// Led.h) is NOT used: on the module it turns knob, fader and jack readings into the PatchCtrls values the engine reads.
// Here the plugin parameters are written into PatchCtrls directly, with the same arithmetic as Ui::Poll for the parts
// that matter to the sound: the fader curves, the fader snapping to full, the per-block smoothing of the knobs (the
// same coefficients as the KnobController / FaderController objects in Ui.h), and the oscillator pitch (octave +
// Pitch knob + modulation + note), where a MIDI note takes the place of the V/Oct jack.
// The CV inputs are not copied (scope item 3), so every CV amount is 0 and every CV value is 0.
// Sync: Logic's tempo is turned into pulses on the module's sync input (feedClock below), as in the Iro-x plugin.
#include "AetherEngine.h"
#include "owl_runtime.h"
#include "Oneiroi.h"
#include "Clock.h"
#include <algorithm>
#include <cmath>

namespace aether
{
namespace
{
// One-pole smoothing per block, as ONE_POLE in the firmware: out += coefficient * (in - out). The first block jumps.
struct Smooth
{
    float v = 0.f;
    float step (float target, float coeff, bool first)
    {
        if (first) v = target; else v += coeff * (target - v);
        return v;
    }
};

// ---- snap helpers: each moves a knob value (0..1) onto the grid of its control -----------------------------------------
// The Pitch knob: 12 * (value - 0.5) semitones, so whole semitones are 13 positions from -6 to +6.
float snapPitchKnob (float v)
{
    return std::min (1.f, std::max (0.f, 0.5f + std::round ((v - 0.5f) * 12.f) / 12.f));
}

// The Looper Speed knob: the firmware's CenterMap (Commons.h) turns it into -2 .. +2 (0 at speedZero, 1x at 0.75 for 0.51). Ratios
// 2^(n/12), up and reversed. The zones the firmware locks at 0x (under 0.1) and 1x / -1x (0.95 to 1.05) are left alone.
float snapSpeedKnob (float knob, float zero)
{
    constexpr float kTop = 0.99f;
    const float v = knob < zero ? -2.f + 2.f * knob / zero : 2.f * (knob - zero) / (kTop - zero);
    const float a = std::fabs (v);
    if (a <= 0.1f || (a >= 0.95f && a <= 1.05f)) return knob;
    const int n = std::min (12, (int) std::lround (12.f * std::log2 (a)));
    const float s = std::copysign (std::exp2 ((float) n / 12.f), v);
    return s < 0.f ? (s + 2.f) * zero / 2.f : zero + s * (kTop - zero) / 2.f;
}

// The Resonator Tune knob: the main pole moves from -24 semitones to +range (range = 24 at Dissonance 0, 16 at full, an integer as
// in the firmware's Resonator.h). Whole semitones. (The other two poles keep the offsets of their own.)
float snapResonatorTuneKnob (float tune, float dissonance)
{
    const int range = (int) (24.f - 8.f * std::min (1.f, std::max (0.f, dissonance)));
    const float span = (float) (range + 24);
    const int n = std::min (range, std::max (-24, (int) std::lround (-24.f + tune * span)));
    return (float) (n + 24) / span;
}

// The Filter Cutoff knob: 10 Hz * 2200^value (10 Hz to 22 kHz, Filter.h). Notes of the equal-tempered scale, A = 440 Hz. The two
// ends (fully closed and fully open) stay where they are.
float snapCutoffKnob (float v)
{
    if (v <= 0.005f || v >= 0.995f) return v;
    constexpr float kLn2200 = 7.696213f;       // ln (22000 / 10)
    const float hz = 10.f * std::exp (v * kLn2200);
    const int n = (int) std::lround (12.f * std::log2 (hz / 440.f));
    const float snapped = 440.f * std::exp2 ((float) n / 12.f);
    return std::min (1.f, std::max (0.f, std::log (snapped / 10.f) / kLn2200));
}
}

struct Engine::Impl
{
    PatchCtrls ctrls {};
    PatchCvs cvs {};
    PatchState state {};
    EngineParams params;
    Oneiroi* oneiroi = nullptr;
    Clock* clock = nullptr;
    AudioBuffer* buffer = nullptr;
    bool firstBlock = true;
    bool clearRequested = false;
    // The firmware draws random numbers while it runs (the sample-and-hold modulation). Every engine keeps its own
    // generator state and swaps it in only while it runs, so several instances never disturb each other.
    uint32_t rng = 33641u;
    struct RandomScope
    {
        uint32_t saved; uint32_t& mine;
        explicit RandomScope (uint32_t& m) : saved (aether_random_state()), mine (m) { aether_seed_random (mine); }
        ~RandomScope() { mine = aether_random_state(); aether_seed_random (saved); }
    };

    // smoothed controls
    Smooth inVol, looperVol, osc1Vol, osc2Vol, filterVol, resonatorVol, echoVol, ambienceVol;
    Smooth looperSpeed, looperStart, looperLength, pitchKnob, detune, cutoff, resonance, resTune, resFeedback,
           echoDensity, echoRepeats, ambSpacetime, ambDecay, modLevel, modSpeed;
    float notePot = 0.f, noteCv = 0.f;

    // Host tempo -> the firmware's sync input. The module has a sync jack: a pulse on it makes the firmware's clock
    // (Clock.h) follow the pulse rate, and the modulation speed, the echo times and the auto-pan are then tied to it.
    // Here the pulses are made from Logic's tempo: one per quarter note, each one block (32 samples) long.
    Transport transport;
    bool transportPending = false;
    bool wasPlaying = false;
    double beatPhase = 0.0;        // 0..1, position inside the current quarter note
    bool pulseHigh = false;
    static constexpr double kMinBpm = 40.0, kMaxBpm = 300.0;   // the firmware drops an external clock slower than 2 s per pulse

    Impl()
    {
        aether_init_owl_runtime();
        aether_ZeroedAllocations zeroed;      // the firmware expects cleared memory, like OpenWare's allocator gives
        RandomScope randomScope (rng);

        state.sampleRate = (float) kEngineSampleRate;
        state.blockSize = kEngineBlockSize;
        state.blockRate = (float) (kEngineSampleRate / kEngineBlockSize);   // 1500
        state.inputLevel = FloatArray::create (kEngineBlockSize);
        state.efModLevel = FloatArray::create (kEngineBlockSize);
        state.outLevel = 1.f;
        state.randomSlew = kRandomSlewSamples;
        state.randomHasSlew = false;
        state.softTakeover = false;
        state.modAttenuverters = false;
        state.cvAttenuverters = false;
        state.funcMode = FUNC_MODE_NONE;
        state.clockSource = CLOCK_SOURCE_INTERNAL;
        state.syncIn = false;
        state.startupPhase = STARTUP_DONE;
        // Calibration values of the module's own V/Oct and speed knob. Only the speed knob's centre matters here: the
        // looper speed knob is 0 at 0.51, 1x at 0.75 and 2x at 1.0 (the module's value comes from its own calibration).
        state.c2 = 1102.f / 4095.f;
        state.c5 = 2334.f / 4095.f;
        state.pitchZero = 2128.f / 4041.f;
        state.speedZero = 0.51f;

        // Same order as the firmware's patch: Oneiroi, then Clock (the Clock creates the tempo object).
        oneiroi = Oneiroi::create (&ctrls, &cvs, &state);
        clock = Clock::create (&ctrls, &state);
        buffer = AudioBuffer::create (2, kEngineBlockSize);
    }

    ~Impl()
    {
        AudioBuffer::destroy (buffer);
        Oneiroi::destroy (oneiroi);
        Clock::destroy (clock);
        TapTempo::destroy (state.tempo);
        FloatArray::destroy (state.inputLevel);
        FloatArray::destroy (state.efModLevel);
    }

    static float snap (float v) { return v >= kOne ? 1.f : v; }   // as in Ui::Poll

    void applyParams()
    {
        const EngineParams& p = params;
        const bool f = firstBlock;

        // Faders: smoothed (0.02 per block), curve value squared (MapExpo), the effect faders snap to full at the top.
        ctrls.inputVol = MapExpo (inVol.step (p.inputVolume, 0.02f, f));
        ctrls.looperVol = MapExpo (looperVol.step (p.looperVolume, 0.02f, f));
        ctrls.osc1Vol = MapExpo (osc1Vol.step (p.osc1Volume, 0.02f, f));
        ctrls.osc2Vol = MapExpo (osc2Vol.step (p.osc2Volume, 0.02f, f));
        ctrls.filterVol = snap (filterVol.step (p.filterLevel, 0.02f, f));
        ctrls.resonatorVol = snap (resonatorVol.step (p.resonatorLevel, 0.02f, f));
        ctrls.echoVol = snap (echoVol.step (p.echoLevel, 0.02f, f));
        ctrls.ambienceVol = snap (ambienceVol.step (p.ambienceLevel, 0.02f, f));

        // Looper
        ctrls.looperSos = p.looperSos;
        ctrls.looperFilter = p.looperFilter;
        {
            const float sp = looperSpeed.step (p.looperSpeed, 0.005f, f);
            ctrls.looperSpeed = p.snapLooperSpeed ? snapSpeedKnob (sp, state.speedZero) : sp;      // Looper Speed snap
        }
        ctrls.looperSpeedModAmount = p.looperSpeedMod;
        ctrls.looperStart = looperStart.step (p.looperStart, 0.005f, f);
        ctrls.looperStartSnap = p.snapLooperStart ? 1.f : 0.f;       // Start and Length snap happen inside the looper (Looper.h)
        ctrls.looperLengthSnap = p.snapLooperLength ? 1.f : 0.f;
        ctrls.looperStartModAmount = p.looperStartMod;
        ctrls.looperLength = looperLength.step (p.looperLength, 0.005f, f);
        ctrls.looperLengthModAmount = p.looperLengthMod;
        ctrls.looperRecording = p.looperRecording ? 1.f : 0.f;
        ctrls.looperResampling = p.looperPost ? 1.f : 0.f;

        // Oscillators
        ctrls.oscDetune = detune.step (p.oscDetune, 0.1f, f);
        ctrls.oscDetuneModAmount = p.oscDetuneMod;
        ctrls.oscPitchModAmount = p.oscPitchMod;
        ctrls.oscUseWavetable = p.oscUseWavetable ? 1.f : 0.f;
        ctrls.oscOctave = (float) p.oscOctave;

        // Pitch: as in Ui::Poll, with the MIDI note in the place of the V/Oct jack (the note is a semitone offset).
        {
            const float interval = p.noteOffset - noteCv;
            if (f || interval < -0.4f || interval > 0.4f) noteCv = p.noteOffset;
            else noteCv += 0.1f * interval;
            float tune = pitchKnob.step (p.oscPitch, 0.995f, f);
            if (p.snapOscPitch) tune = snapPitchKnob (tune);          // Oscillator Pitch snap: whole semitones
            float note = Modulate (tune - 0.5f, ctrls.oscPitchModAmount, state.modValue, 0, 0, -1.f, 1.f, state.modAttenuverters);
            note = 12.f * note + 12.f * ctrls.oscOctave;
            if (f) notePot = note; else notePot += 0.1f * (note - notePot);
            ctrls.oscPitch = M2F (notePot + noteCv);

            ctrls.oscUnison = Clamp (p.oscUnison, -1.f, 1.f);
            if (ctrls.oscUnison >= -0.03f && ctrls.oscUnison <= 0.03f) { ctrls.oscUnison = 0.f; state.oscUnisonCenterFlag = true; }
            else state.oscUnisonCenterFlag = false;
        }

        // Filter
        ctrls.filterMode = p.filterMode;
        ctrls.filterPosition = p.filterPosition;
        ctrls.filterNoiseLevel = p.filterNoise;          // used by the AetherUmbra change in Filter.h
        {
            const float c = cutoff.step (p.filterCutoff, 0.01f, f);
            ctrls.filterCutoff = p.snapFilterCutoff ? snapCutoffKnob (c) : c;        // Filter Cutoff snap: musical notes
        }
        ctrls.filterCutoffModAmount = p.filterCutoffMod;
        ctrls.filterResonance = resonance.step (p.filterResonance, 0.01f, f);
        ctrls.filterResonanceModAmount = p.filterResonanceMod;

        // Resonator
        {
            const float t = resTune.step (p.resonatorTune, 0.005f, f);
            ctrls.resonatorTune = p.snapResonatorTune ? snapResonatorTuneKnob (t, p.resonatorDissonance) : t;   // Resonator Tune snap
        }
        ctrls.resonatorTuneModAmount = p.resonatorTuneMod;
        ctrls.resonatorFeedback = resFeedback.step (p.resonatorFeedback, 0.01f, f);
        ctrls.resonatorFeedbackModAmount = p.resonatorFeedbackMod;
        ctrls.resonatorDissonance = p.resonatorDissonance;

        // Echo
        ctrls.echoDensity = echoDensity.step (p.echoDensity, 0.005f, f);
        ctrls.echoDensityModAmount = p.echoDensityMod;
        ctrls.echoRepeats = echoRepeats.step (p.echoRepeats, 0.01f, f);
        ctrls.echoRepeatsModAmount = p.echoRepeatsMod;
        ctrls.echoFilter = p.echoFilter;
        ctrls.echoSync = p.echoSync ? 1.f : 0.f;      // off = free echo even when Logic gives a tempo (Echo.h)

        // Ambience
        ctrls.ambienceSpacetime = ambSpacetime.step (p.ambienceSpacetime, 0.005f, f);
        ctrls.ambienceSpacetimeModAmount = p.ambienceSpacetimeMod;
        ctrls.ambienceDecay = ambDecay.step (p.ambienceDecay, 0.01f, f);
        ctrls.ambienceDecayModAmount = p.ambienceDecayMod;
        ctrls.ambienceAutoPan = p.ambienceAutoPan;

        // Modulation
        ctrls.modLevel = modLevel.step (p.modLevel, 0.01f, f);
        ctrls.modSpeed = modSpeed.step (p.modSpeed, 0.01f, f);
        ctrls.modType = p.modType;
        state.modActive = ctrls.modLevel > 0.1f;

        // CV amounts stay 0 (the CV jacks are replaced by host automation); CV values are 0 as well.
        ctrls.looperSpeedCvAmount = ctrls.looperStartCvAmount = ctrls.looperLengthCvAmount = 0.f;
        ctrls.oscPitchCvAmount = ctrls.oscDetuneCvAmount = 0.f;
        ctrls.filterCutoffCvAmount = ctrls.filterResonanceCvAmount = 0.f;
        ctrls.resonatorTuneCvAmount = ctrls.resonatorFeedbackCvAmount = 0.f;
        ctrls.echoRepeatsCvAmount = ctrls.echoDensityCvAmount = 0.f;
        ctrls.ambienceDecayCvAmount = ctrls.ambienceSpacetimeCvAmount = 0.f;

        if (clearRequested) { state.clearLooperFlag = true; clearRequested = false; }
    }

    // Called once per block of 32 samples, before the clock runs.
    void feedClock()
    {
        if (pulseHigh)                       // the pulse lasts one block
        {
            state.syncIn = false;
            state.tempo->trigger (false);
            pulseHigh = false;
        }
        if (transport.bpm <= 0.0) { beatPhase = 0.0; state.hostBeatSamples = 0.f; return; }   // nothing known: internal clock, snaps are free

        const double bpm = std::min (std::max (transport.bpm, kMinBpm), kMaxBpm);
        const double blocksPerBeat = 60.0 * state.blockRate / bpm;
        state.hostBeatSamples = (float) (60.0 * state.sampleRate / bpm);     // for the Looper Start / Length snap (exact, not in blocks)

        bool snapped = false;
        if (transportPending)
        {
            transportPending = false;
            if (transport.playing && transport.hasPpq)
            {
                const double target = transport.ppq - std::floor (transport.ppq);
                double diff = target - beatPhase; diff -= std::floor (diff + 0.5);       // -0.5 .. 0.5
                // Follow the song position when playback has just started or jumped; otherwise keep running smoothly.
                if (! wasPlaying || std::fabs (diff) > 0.03) { beatPhase = target; snapped = true; }
            }
            wasPlaying = transport.playing;
        }
        if (! snapped) beatPhase += 1.0 / blocksPerBeat;

        if (beatPhase >= 1.0)
        {
            beatPhase -= std::floor (beatPhase);
            state.syncIn = true;
            state.tempo->trigger (true);
            // the firmware measures the time between pulses; give it the exact value so the tempo does not jitter
            state.tempo->setPeriodInSamples ((size_t) std::lround (blocksPerBeat));
            pulseHigh = true;
        }
    }

    void process (float* left, float* right)
    {
        RandomScope randomScope (rng);
        feedClock();
        clock->Process();
        // The firmware's Clock only takes over a new tempo when it differs by more than 48 blocks (it expects a noisy
        // jack). The host tempo is exact, so follow it exactly.
        if (state.clockSource == CLOCK_SOURCE_EXTERNAL) state.clockSamples = state.tempo->getPeriodInSamples();
        applyParams();
        firstBlock = false;

        FloatArray l = buffer->getSamples (LEFT_CHANNEL);
        FloatArray r = buffer->getSamples (RIGHT_CHANNEL);
        for (int i = 0; i < kEngineBlockSize; ++i) { l[i] = left[i]; r[i] = right[i]; }
        oneiroi->Process (*buffer);
        for (int i = 0; i < kEngineBlockSize; ++i) { left[i] = l[i]; right[i] = r[i]; }
    }
};

Engine::Engine() : impl (new Impl) {}
Engine::~Engine() = default;
void Engine::setParams (const EngineParams& p) { impl->params = p; }
void Engine::clearLooper() { impl->clearRequested = true; }
void Engine::setTransport (const Transport& t) { impl->transport = t; impl->transportPending = true; }
double Engine::beatPhase() const { return impl->beatPhase; }
bool Engine::clockIsExternal() const { return impl->state.clockSource == CLOCK_SOURCE_EXTERNAL; }
void Engine::processBlock (float* left, float* right) { impl->process (left, right); }
float Engine::modulationValue() const { return impl->state.modValue; }
float Engine::oscillatorHz() const { return impl->ctrls.oscPitch; }
}

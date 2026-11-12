// AetherUmbra plugin test: runs the real plugin code (PluginProcessor, PluginEditor) without Logic or a display.
// Checks: it is an instrument that takes MIDI notes and audio, accepts the layouts Logic uses, makes sound by itself and
// processes audio at 44.1, 48 and 96 kHz, reports its delay, follows MIDI notes, records and clears the looper, keeps its
// settings when saved and loaded, copes with several instances and moving controls, randomises and undoes (button and
// parameter), reports its tail, and draws its window.
// With a folder as the first argument it also writes pictures of the window there (PNG).
// Not part of the download. Build with -DAETHER_BUILD_PLUGIN_TEST=ON (see CMakeLists.txt).
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("%-100s %s\n", what.toRawUTF8(), ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}

// Counts how often a control was grabbed and let go (the randomiser must do both for every control it moves).
struct GestureCounter : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
};

juce::AudioProcessor::BusesLayout layout (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
{
    juce::AudioProcessor::BusesLayout l;
    if (! in.isDisabled()) l.inputBuses.add (in); else l.inputBuses.add (juce::AudioChannelSet::disabled());
    l.outputBuses.add (out);
    return l;
}

void collect (juce::Component& c, std::vector<juce::Component*>& out)
{
    out.push_back (&c);
    for (auto* ch : c.getChildren()) collect (*ch, out);
}

void writePng (const juce::Image& img, const juce::File& f)
{
    juce::FileOutputStream out (f);
    if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (img, out); }
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    AetherProcessor proc;
    using aether_params::randomTargets;

    // ---- what kind of plugin it is ----
    check (proc.getName() == "AetherUmbra", "name is AetherUmbra");
    check (proc.acceptsMidi() && ! proc.producesMidi() && ! proc.isMidiEffect(), "takes MIDI notes, sends none (an instrument)");
    check (proc.getTotalNumInputChannels() == 2 && proc.getTotalNumOutputChannels() == 2, "starts with stereo audio in (side chain), stereo out");
    check (juce::String (AETHER_VERSION_STRING).isNotEmpty() && AetherProcessor::versionString() == AETHER_VERSION_STRING, "version string present");

    // ---- layouts ----
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto none = juce::AudioChannelSet::disabled();
    check (proc.checkBusesLayoutSupported (layout (stereo, stereo)), "layout stereo in, stereo out accepted");
    check (proc.checkBusesLayoutSupported (layout (none, stereo)),   "layout no audio in, stereo out accepted");
    check (proc.checkBusesLayoutSupported (layout (mono, stereo)),   "layout mono in, stereo out accepted");
    check (! proc.checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())), "layout 5.1 refused");

    // ---- helpers ----
    auto fillSignal = [] (juce::AudioBuffer<float>& b, int offset, float amp = 1.f)
    {
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float t = (float) (i + offset);
            b.setSample (0, i, amp * (std::sin (0.05f * t) * 0.5f + 0.2f * std::sin (0.9f * t)));
            if (b.getNumChannels() > 1) b.setSample (1, i, amp * std::sin (0.031f * t + 1.0f) * 0.4f);
        }
    };
    auto setParam = [] (AetherProcessor& p, const char* id, float normalised)
    {
        if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (normalised);
    };
    auto val = [] (AetherProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto prepare = [] (AetherProcessor& p, double rate, int block, int ins = 2)
    {
        p.setPlayConfigDetails (ins, 2, rate, block);
        p.prepareToPlay (rate, block);
    };
    auto rms = [] (const std::vector<float>& v, size_t a, size_t b)
    {
        double s = 0; b = std::min (b, v.size());
        for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
        return b > a ? std::sqrt (s / (double) (b - a)) : 0.0;
    };
    // runs 'blocks' blocks; input = the test signal (amp 0 = silence); returns the left output
    auto run = [&] (AetherProcessor& p, int blocks, int blockSize, float inAmp, std::vector<float>& outL, bool& finite, const juce::MidiBuffer* firstMidi = nullptr, int offset = 0)
    {
        finite = true;
        juce::MidiBuffer empty;
        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            fillSignal (buf, (offset + b) * blockSize, inAmp);
            juce::MidiBuffer m = (b == 0 && firstMidi != nullptr) ? *firstMidi : empty;
            p.processBlock (buf, m);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i) if (! std::isfinite (buf.getSample (ch, i))) finite = false;
            for (int i = 0; i < blockSize; ++i) outL.push_back (buf.getSample (0, i));
        }
    };

    // ---- parameters ----
    {
        int n = proc.getParameters().size();
        check (n == 55, "parameter count is " + juce::String (n));
        check (proc.apvts.getParameter ("input_volume") == proc.getParameters().getFirst(), "first parameter is Input Volume (order is fixed from Milestone 1)");
        check (proc.getParameters()[n - 2] == proc.apvts.getParameter ("randomise") && proc.getParameters().getLast() == proc.apvts.getParameter ("undo_redo"),
               "Randomise and Undo/Redo are the last two parameters (new ones are only ever added at the end)");
        check (proc.apvts.getParameter ("filter_noise") != nullptr, "the Filter Noise parameter exists");
        bool allThere = true, allAutomatable = true;
        for (auto* p : proc.getParameters())
        {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);
            if (withId == nullptr || proc.apvts.getParameter (withId->paramID) == nullptr) allThere = false;
            if (! p->isAutomatable()) allAutomatable = false;
        }
        check (allThere, "every parameter has an id");
        check (allAutomatable, "every parameter can be automated");
        auto* oct = proc.apvts.getParameter ("osc_octave");
        check (oct != nullptr && oct->convertFrom0to1 (0.f) == 1.f && oct->convertFrom0to1 (1.f) == 9.f && val (proc, "osc_octave") == 5.f, "octave goes 1 to 9 and starts at 5");
        auto* uni = proc.apvts.getParameter ("osc_unison");
        check (uni != nullptr && uni->convertFrom0to1 (0.f) == -1.f && uni->convertFrom0to1 (1.f) == 1.f, "unison goes -1 to +1");
        auto* mt = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mod_type"));
        check (mt != nullptr && mt->choices.size() == 7, "modulation type has the firmware's 7 shapes");
        auto* ol = proc.apvts.getParameter ("output_level");
        check (ol != nullptr && std::abs (ol->convertFrom0to1 (ol->getDefaultValue())) < 0.001f && std::abs (ol->convertFrom0to1 (0.f) + 24.f) < 0.01f && std::abs (ol->convertFrom0to1 (1.f) - 12.f) < 0.01f, "output level is -24 to +12 dB, starts at 0 dB");
    }

    // ---- it makes sound by itself, and with audio in, at the rates Logic uses ----
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const juce::String r = juce::String ((int) rate) + " Hz";
        AetherProcessor p; prepare (p, rate, 512);
        std::vector<float> o; bool fin = true;
        run (p, 30, 512, 0.f, o, fin);
        check (fin && rms (o, 8000, o.size()) > 0.02 && rms (o, 8000, o.size()) < 2.0, "no audio in: the sine oscillator sounds by itself at " + r + " (rms " + juce::String (rms (o, 8000, o.size()), 3) + ")");
        check (p.underruns() == 0, "no buffer shortage at " + r);
        AetherProcessor q; prepare (q, rate, 512);
        setParam (q, "osc1_volume", 0.f);
        std::vector<float> a, b; run (q, 30, 512, 1.f, a, fin);
        check (fin && rms (a, 8000, a.size()) > 0.02, "audio in (side chain) is heard, oscillators off, at " + r);
        AetherProcessor q2; prepare (q2, rate, 512);
        setParam (q2, "osc1_volume", 0.f); setParam (q2, "input_volume", 0.f);
        run (q2, 30, 512, 1.f, b, fin);
        check (fin && rms (b, 8000, b.size()) < 1e-3, "input volume 0: the side chain audio is not heard, at " + r);
        AetherProcessor w; prepare (w, rate, 512);
        setParam (w, "resonator_volume", 1.f); setParam (w, "resonator_feedback", 0.9f); setParam (w, "echo_volume", 1.f); setParam (w, "ambience_volume", 1.f);
        std::vector<float> e; run (w, 30, 512, 1.f, e, fin);
        double diff = 0; for (size_t i = 0; i < std::min (e.size(), o.size()); ++i) diff += std::abs (e[i] - o[i]);
        check (fin && diff / (double) o.size() > 1e-3 && *std::max_element (e.begin(), e.end()) < 16.f, "all effects on: finite, sensible level, different sound at " + r);
    }

    // ---- no audio input at all (an instrument without a side chain) ----
    {
        AetherProcessor p; prepare (p, 48000.0, 256, 0);
        std::vector<float> o; bool fin = true; juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b) { juce::AudioBuffer<float> buf (2, 256); buf.clear(); p.processBlock (buf, midi); for (int i = 0; i < 256; ++i) { o.push_back (buf.getSample (0, i)); fin = fin && std::isfinite (buf.getSample (1, i)); } }
        check (fin && rms (o, 4000, o.size()) > 0.02, "no audio input connected: still sounds (oscillators), stereo out");
    }

    // ---- reported delay (latency) ----
    {
        AetherProcessor p; prepare (p, 48000.0, 512);
        check (p.getLatencySamples() == 32, "reported delay at 48 kHz is 32 samples (" + juce::String (p.getLatencySamples()) + ")");
        prepare (p, 44100.0, 512);
        check (p.getLatencySamples() > 32 && p.getLatencySamples() < 100, "reported delay at 44.1 kHz is " + juce::String (p.getLatencySamples()) + " samples");
        prepare (p, 96000.0, 512);
        check (p.getLatencySamples() > 64 && p.getLatencySamples() < 200, "reported delay at 96 kHz is " + juce::String (p.getLatencySamples()) + " samples");
    }

    // ---- MIDI notes set the pitch ----
    {
        AetherProcessor p; prepare (p, 44100.0, 256);
        std::vector<float> o; bool fin = true;
        auto hzAfterNote = [&] (int note)
        {
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 3);
            run (p, 1, 256, 0.f, o, fin, &m);
            run (p, 150, 256, 0.f, o, fin);
            return p.stream().oscillatorHz();
        };
        const float c4 = hzAfterNote (60), c5 = hzAfterNote (72), a4 = hzAfterNote (69);
        check (std::abs (c4 / 261.63f - 1.f) < 0.01f, "note 60 (C4) plays " + juce::String (c4, 2) + " Hz");
        check (std::abs (c5 / c4 - 2.f) < 0.01f, "note 72 is one octave higher (" + juce::String (c5, 2) + " Hz)");
        check (std::abs (a4 / 440.f - 1.f) < 0.01f, "note 69 (A4) plays " + juce::String (a4, 2) + " Hz");
        check (p.lastNote() == 69, "the window knows the last note (69)");
        // note off does not change the pitch (like a held CV)
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 69), 0);
        run (p, 1, 256, 0.f, o, fin, &off); run (p, 50, 256, 0.f, o, fin);
        check (std::abs (p.stream().oscillatorHz() / 440.f - 1.f) < 0.01f, "after the note is released the pitch stays");
        // the octave knob moves it
        setParam (p, "osc_octave", 0.75f);     // 7 of 1..9
        run (p, 150, 256, 0.f, o, fin);
        check (std::abs (p.stream().oscillatorHz() / 1760.f - 1.f) < 0.02f, "octave 7 plays two octaves higher (" + juce::String (p.stream().oscillatorHz(), 1) + " Hz)");
    }

    // ---- the looper through the plugin: record, play, clear ----
    {
        AetherProcessor p; prepare (p, 44100.0, 512);
        setParam (p, "osc1_volume", 0.f); setParam (p, "input_volume", 0.f); setParam (p, "looper_volume", 1.f);
        std::vector<float> o; bool fin = true;
        run (p, 3, 512, 0.f, o, fin);
        p.requestClearLooper();
        run (p, 140, 512, 0.f, o, fin);               // about 1.6 s: the clear runs through the memory
        // record 2 s with the side chain audio, input volume 0 so only the looper is heard
        setParam (p, "looper_recording", 1.f);
        run (p, 180, 512, 1.f, o, fin, nullptr, 200);
        setParam (p, "looper_recording", 0.f);
        std::vector<float> after;
        run (p, 150, 512, 0.f, after, fin);
        check (fin && rms (after, 20000, after.size()) > 0.005, "looper: what was recorded plays back (rms " + juce::String (rms (after, 20000, after.size()), 3) + ")");
        p.requestClearLooper();
        std::vector<float> cleared;
        run (p, 250, 512, 0.f, cleared, fin);
        check (fin && rms (cleared, 90000, cleared.size()) < 1e-3, "looper: Clear empties it (silent afterwards)");
        // Pre / Post parameter
        AetherProcessor q; prepare (q, 48000.0, 512);
        setParam (q, "looper_resampling", 1.f);
        check (val (q, "looper_resampling") == 1.f, "Pre/Post switch can be set to Post");
    }

    // ---- several instances do not disturb each other ----
    {
        AetherProcessor a, b, c;
        setParam (b, "echo_volume", 1.0f); setParam (b, "echo_repeats", 0.9f);
        setParam (c, "ambience_volume", 1.0f); setParam (c, "osc2_volume", 0.8f);
        std::vector<float> alone, together; bool f = true;
        { AetherProcessor solo; prepare (solo, 48000.0, 256); run (solo, 60, 256, 1.f, alone, f); }
        prepare (a, 48000.0, 256); prepare (b, 48000.0, 256); prepare (c, 48000.0, 256);
        juce::MidiBuffer midi;
        for (int blk = 0; blk < 60; ++blk)
            for (auto* p : { &a, &b, &c })
            {
                juce::AudioBuffer<float> buf (2, 256);
                fillSignal (buf, blk * 256);
                p->processBlock (buf, midi);
                if (p == &a) for (int i = 0; i < 256; ++i) together.push_back (buf.getSample (0, i));
            }
        check (alone == together, "three instances running at once do not disturb each other (bit-identical)");
    }

    // ---- tail ----
    check (proc.getTailLengthSeconds() == aether_params::kTailSeconds && proc.getTailLengthSeconds() >= 10.0, "tail length reported to the host: " + juce::String (proc.getTailLengthSeconds(), 1) + " s");

    // ---- the randomiser ----
    {
        bool inRange = true, down = false, up = false, wholeSemi = true; juce::Random r (5);
        for (auto how : { aether_params::RandomHow::Low, aether_params::RandomHow::Mid, aether_params::RandomHow::High })
            for (int i = 0; i < 100000; ++i)
            {
                const float v0 = (i % 7 == 0) ? 0.f : (i % 11 == 0) ? 1.f : r.nextFloat();
                const float v = aether_params::randomisedValue (v0, how, false, r.nextFloat());
                if (! (v >= 0.f && v <= 1.f)) inRange = false;
                if (v < v0 - 1e-6f) down = true;
                if (v > v0 + 1e-6f) up = true;
                const float s = aether_params::randomisedValue (v0, how, true, r.nextFloat());
                if (std::abs (s * 12.f - std::rint (s * 12.f)) > 1e-4f || ! (s >= 0.f && s <= 1.f)) wholeSemi = false;
            }
        check (inRange && down && up, "randomiser arithmetic: always 0 to 1, moves up and down");
        check (wholeSemi, "pitch randomiser: always whole semitones");
        // Low and Mid move towards the centre by at most 0.02 / 0.10 (a bit past it is possible)
        check (std::abs (aether_params::randomisedValue (0.2f, aether_params::RandomHow::Low, false, 0.9f) - (0.2f + 0.9f * 0.02f)) < 1e-6f
               && std::abs (aether_params::randomisedValue (0.8f, aether_params::RandomHow::Mid, false, 0.5f) - (0.8f - 0.05f)) < 1e-6f,
               "Low and Mid move by at most 0.02 / 0.10 towards the centre, as in the firmware");
        check (aether_params::randomisedValue (0.3f, aether_params::RandomHow::High, false, 0.05f) == 0.9f && aether_params::randomisedValue (0.3f, aether_params::RandomHow::High, false, 0.6f) == 0.6f,
               "High jumps to a random value (1 in 10: to 0.9 or 0.1)");

        AetherProcessor p;
        GestureCounter gc;
        for (auto* prm : p.getParameters()) prm->addListener (&gc);
        int n = 0; const auto* t = randomTargets (n);
        check (n == 13, "the randomiser knows 13 controls (2 oscillator, 3 looper, 8 effects)");
        auto snapshot = [&] { std::vector<float> v; for (int i = 0; i < n; ++i) v.push_back (val (p, t[i].id)); return v; };
        auto reset = [&] { for (int i = 0; i < n; ++i) setParam (p, t[i].id, 0.5f); };
        // What = All, Osc, Looper, FX; amount High so every control moves
        setParam (p, "random_amount", 1.0f);       // High (third of three)
        struct Case { float what; const char* name; aether_params::RandomWhat group; };
        for (auto cs : { Case { 0.f, "All", aether_params::RandomWhat::All }, Case { 1.f / 3.f, "Osc", aether_params::RandomWhat::Osc },
                         Case { 2.f / 3.f, "Looper", aether_params::RandomWhat::Looper }, Case { 1.f, "FX", aether_params::RandomWhat::FX } })
        {
            setParam (p, "random_mode", cs.what);
            reset(); const auto before = snapshot();
            // other controls
            setParam (p, "filter_level", 0.9f); setParam (p, "echo_volume", 0.3f); setParam (p, "filter_cutoff_mod", 0.4f); setParam (p, "output_level", 0.7f);
            const float lv = val (p, "filter_level"), ev = val (p, "echo_volume"), md = val (p, "filter_cutoff_mod"), ol = val (p, "output_level");
            gc.begins = gc.ends = 0;
            int moved = 0, expected = 0; bool others = true;
            for (int attempt = 0; attempt < 3 && moved == 0; ++attempt) { reset(); p.randomise(); }
            const auto after = snapshot();
            for (int i = 0; i < n; ++i)
            {
                const bool inGrp = aether_params::inGroup (cs.group, t[i].group);
                if (inGrp) ++expected;
                if (std::abs (after[(size_t) i] - 0.5f) > 1e-4f) { ++moved; if (! inGrp) others = false; }
            }
            check (others && moved > 0 && (int) moved <= expected, juce::String ("Random What = ") + cs.name + ": only its controls move (" + juce::String (moved) + " of " + juce::String (expected) + " moved)");
            check (val (p, "filter_level") == lv && val (p, "echo_volume") == ev && val (p, "filter_cutoff_mod") == md && val (p, "output_level") == ol, juce::String ("Random What = ") + cs.name + ": faders, mod and out knobs stay");
            (void) before;
        }
        // gestures: every moved control is grabbed and let go
        setParam (p, "random_mode", 0.f); reset(); gc.begins = gc.ends = 0; p.randomise();
        check (gc.begins == n && gc.ends == n, "every randomised control is grabbed and let go (Logic can record it): " + juce::String (gc.begins) + " / " + juce::String (gc.ends));
        for (auto* prm : p.getParameters()) prm->removeListener (&gc);

        // Undo / Redo
        reset(); setParam (p, "random_mode", 0.f); setParam (p, "random_amount", 1.0f);
        const auto s0 = snapshot();
        p.randomise();
        const auto s1 = snapshot();
        check (s1 != s0, "Randomise (All, High) changes the controls");
        p.undoRedo();
        check (snapshot() == s0, "Undo goes back to the values from before");
        p.undoRedo();
        check (snapshot() == s1, "pressing it again redoes the random values");
        // Undo only touches the chosen group
        setParam (p, "random_mode", 1.f);          // FX only
        p.randomise();
        setParam (p, "random_mode", 1.f / 3.f);    // Osc only
        const float pitchNow = val (p, "osc_pitch"), cutoffNow = val (p, "filter_cutoff");
        p.undoRedo();
        check (val (p, "filter_cutoff") == cutoffNow, "Undo with What = Osc leaves effect controls alone");
        (void) pitchNow;

        // The two trigger parameters: off to on once, then they switch themselves off
        AetherProcessor q; prepare (q, 48000.0, 256);
        setParam (q, "random_mode", 0.f); setParam (q, "random_amount", 1.0f);
        juce::MidiBuffer midi; juce::AudioBuffer<float> buf (2, 256);
        auto qsnap = [&] { std::vector<float> v; for (int i = 0; i < n; ++i) v.push_back (val (q, t[i].id)); return v; };
        const auto v0 = qsnap();
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRequests();
        check (qsnap() == v0, "nothing happens while Randomise stays off");
        setParam (q, "randomise", 1.0f);
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRequests();
        const auto v1 = qsnap();
        check (v1 != v0, "Randomise parameter off to on: the controls move");
        check (val (q, "randomise") < 0.5f, "the Randomise parameter switches itself off again");
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRequests();
        check (qsnap() == v1, "and it does not randomise again on its own");
        setParam (q, "undo_redo", 1.0f);
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRequests();
        check (qsnap() == v0 && val (q, "undo_redo") < 0.5f, "Undo/Redo parameter off to on: goes back, and switches itself off");
        // a project saved with a trigger on must not fire when opened
        juce::MemoryBlock blk;
        { AetherProcessor src; setParam (src, "randomise", 1.0f); setParam (src, "undo_redo", 1.0f); src.getStateInformation (blk); }
        AetherProcessor dst; prepare (dst, 48000.0, 256);
        dst.setStateInformation (blk.getData(), (int) blk.getSize());
        std::vector<float> d0; for (int i = 0; i < n; ++i) d0.push_back (val (dst, t[i].id));
        fillSignal (buf, 0); dst.processBlock (buf, midi); dst.runPendingRequests();
        std::vector<float> d1; for (int i = 0; i < n; ++i) d1.push_back (val (dst, t[i].id));
        check (d0 == d1 && val (dst, "randomise") < 0.5f && val (dst, "undo_redo") < 0.5f, "opening a project does not randomise or undo");
    }

    // ---- saving and loading: settings come back ----
    {
        AetherProcessor src;
        setParam (src, "filter_cutoff", 0.37f); setParam (src, "echo_volume", 0.8f); setParam (src, "filter_mode", 2.0f / 3.0f);
        setParam (src, "output_level", 0.75f); setParam (src, "osc_octave", 0.25f); setParam (src, "osc_unison", 0.9f);
        setParam (src, "mod_type", 5.0f / 6.0f); setParam (src, "osc_kind", 1.f); setParam (src, "looper_resampling", 1.f);
        setParam (src, "filter_noise", 0.2f); setParam (src, "looper_recording", 1.f);
        juce::MemoryBlock block;
        src.getStateInformation (block);
        check (block.getSize() > 0, "state can be saved");
        AetherProcessor dst;
        dst.setStateInformation (block.getData(), (int) block.getSize());
        bool same = true;
        for (auto* prm : src.getParameters())
            if (auto* id = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm))
                if (std::abs (val (src, id->paramID.toRawUTF8()) - val (dst, id->paramID.toRawUTF8())) > 1e-4f && ! id->paramID.startsWith ("randomise") && ! id->paramID.startsWith ("undo")) same = false;
        check (same, "every saved setting comes back after loading (all parameters compared)");
        check ((int) val (dst, "filter_mode") == 2 && (int) val (dst, "osc_octave") == 3 && (int) val (dst, "mod_type") == 5 && val (dst, "looper_recording") > 0.5f, "choices, octave and the record switch come back");
        dst.setStateInformation ("garbage", 7);
        check (true, "garbage state is ignored without a crash");
        dst.setStateInformation (nullptr, 0);
        check (true, "empty state is ignored without a crash");
        prepare (dst, 48000.0, 256);
        std::vector<float> o; bool f; run (dst, 10, 256, 1.f, o, f);
        check (f, "plugin still works after loading bad state");
    }

    // ---- automation while playing (every parameter changes every block), notes too ----
    {
        AetherProcessor p; prepare (p, 44100.0, 300);
        bool fin = true; juce::Random rnd (7); float peak = 0.f;
        for (int b = 0; b < 300; ++b)
        {
            for (auto* prm : p.getParameters()) { prm->setValueNotifyingHost (rnd.nextFloat()); }
            p.runPendingRequests();
            juce::AudioBuffer<float> buf (2, 300);
            fillSignal (buf, b * 300);
            juce::MidiBuffer m; if (b % 5 == 0) m.addEvent (juce::MidiMessage::noteOn (1, 30 + rnd.nextInt (70), (juce::uint8) 90), rnd.nextInt (300));
            p.processBlock (buf, m);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 300; ++i) { fin = fin && std::isfinite (buf.getSample (ch, i)); peak = std::max (peak, std::abs (buf.getSample (ch, i))); }
        }
        check (fin, "300 blocks with every control and notes moving: no bad numbers (peak " + juce::String (peak, 1) + ")");
        check (p.underruns() == 0, "no buffer shortage while everything moves");
    }

    // ---- window ----
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        check (ed != nullptr, "window can be created");
        if (ed != nullptr)
        {
            check (ed->getWidth() > 600 && ed->getHeight() > 400 && std::abs ((double) ed->getWidth() / ed->getHeight() - 1.4) < 0.02, "window has a size, shape 1.4 : 1 (" + juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()) + ")");
            check (ed->isResizable(), "window can be resized");
            std::vector<juce::Component*> all; collect (*ed, all);
            int sliders = 0, rotary = 0, linear = 0, buttons = 0;
            for (auto* c : all)
                if (auto* s = dynamic_cast<juce::Slider*> (c)) { ++sliders; (s->isRotary() ? rotary : linear)++; }
                else if (dynamic_cast<juce::Button*> (c) != nullptr) ++buttons;
            check (rotary == 38 && linear == 8, "window has 38 knobs and 8 faders (" + juce::String (rotary) + " / " + juce::String (linear) + ")");
            // 11 segmented groups: osc kind 2, mode 4, position 4, pre/post 2, random what 4, amount 3 = 19 buttons, + record, clear, randomise, undo
            check (buttons == 19 + 4, "window has the choice buttons, Record, Clear, Randomise and Undo/Redo (" + juce::String (buttons) + " buttons)");
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            check (img.isValid() && img.getWidth() == ed->getWidth(), "window can be drawn");
            for (int w : { 700, 1400, 2100 })
            {
                ed->setSize (w, (int) std::lround (w / 1.4));
                auto im = ed->createComponentSnapshot (ed->getLocalBounds());
                check (im.isValid() && im.getWidth() == w, "window can be drawn at width " + juce::String (w));
            }
            ed->setSize (1400, 1000);
            if (argc > 1)
            {
                juce::File dir (juce::String::fromUTF8 (argv[1]));
                dir.createDirectory();
                // a settled state for the picture: notes, a few controls moved, audio level shown
                setParam (proc, "osc2_volume", 0.5f); setParam (proc, "filter_cutoff", 0.6f); setParam (proc, "filter_resonance", 0.4f);
                setParam (proc, "echo_volume", 0.55f); setParam (proc, "ambience_volume", 0.4f); setParam (proc, "looper_recording", 1.f);
                setParam (proc, "mod_type", 2.f / 6.f);
                proc.setPlayConfigDetails (2, 2, 48000.0, 256); proc.prepareToPlay (48000.0, 256);
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
                juce::AudioBuffer<float> buf (2, 256); fillSignal (buf, 0, 0.5f); proc.processBlock (buf, m);
                std::unique_ptr<juce::AudioProcessorEditor> shot (proc.createEditor());     // made now, so it shows the note and the level
                shot->setSize (1400, 1000);
                writePng (shot->createComponentSnapshot (shot->getLocalBounds()), dir.getChildFile ("window.png"));
                shot->setSize (700, 500);
                writePng (shot->createComponentSnapshot (shot->getLocalBounds()), dir.getChildFile ("window_small.png"));
                check (dir.getChildFile ("window.png").getSize() > 0, "pictures of the window written");
            }
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "RESULT - all plugin checks passed" : "RESULT - SOME PLUGIN CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}

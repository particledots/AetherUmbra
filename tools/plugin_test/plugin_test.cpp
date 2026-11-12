// AetherUmbra plugin test (Milestone 0): runs the real plugin code (ShellProcessor, ShellEditor) without Logic or a
// display, for both shells (Instrument and Music Effect).
// Checks: MIDI in and no MIDI out, the bus layouts Logic may ask for, audio passes through unchanged (tone off) at
// 44.1, 48 and 96 kHz, the test tone follows the last note and keeps it after the key is released, no audio input
// (instrument case), saving and loading, the window text, and a picture of the window (PNG) if a folder is given.
// Not part of the download. Build with -DAU_BUILD_PLUGIN_TEST=ON (see CMakeLists.txt).
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>
#include "ShellProcessor.h"
#include "ShellEditor.h"

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("%-80s %s\n", what.toRawUTF8(), ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
juce::AudioProcessor::BusesLayout layout (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (in);
    l.outputBuses.add (out);
    return l;
}
void setTone (ShellProcessor& p, bool on) { p.apvts.getParameter ("tone")->setValueNotifyingHost (on ? 1.f : 0.f); }

// Runs `blocks` blocks of 256 samples with a test signal on the inputs; returns the whole output of channel 0.
std::vector<float> run (ShellProcessor& p, int blocks, bool withInput, juce::MidiBuffer firstMidi = {}, std::vector<float>* inSig = nullptr)
{
    std::vector<float> out;
    const int n = 256, ch = std::max (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buf (ch, n);
        buf.clear();
        if (withInput)
            for (int c = 0; c < p.getTotalNumInputChannels(); ++c)
                for (int i = 0; i < n; ++i)
                    buf.setSample (c, i, 0.3f * std::sin (0.05f * (float) (b * n + i) * (float) (c + 1)));
        if (inSig) for (int i = 0; i < n; ++i) inSig->push_back (buf.getSample (0, i));
        juce::MidiBuffer m;
        if (b == 0) m = firstMidi;
        p.processBlock (buf, m);
        for (int i = 0; i < n; ++i) out.push_back (buf.getSample (0, i));
    }
    return out;
}
// Frequency from rising zero crossings of a (nearly) pure sine.
double measureHz (const std::vector<float>& s, double rate)
{
    int crossings = 0; long first = -1, last = -1;
    for (size_t i = 1; i < s.size(); ++i)
        if (s[i - 1] < 0.f && s[i] >= 0.f) { if (first < 0) first = (long) i; last = (long) i; ++crossings; }
    if (crossings < 3) return 0.0;
    return (crossings - 1) * rate / (double) (last - first);
}
juce::MidiBuffer noteOn (int note) { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0); return m; }
juce::MidiBuffer noteOff (int note) { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOff (1, note), 0); return m; }

void testShell (ShellKind kind, const juce::String& picturesDir)
{
    const bool inst = kind == ShellKind::Instrument;
    const juce::String tag = inst ? "[Instrument] " : "[Music Effect] ";
    ShellProcessor proc (kind);
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    check (proc.getName() == (inst ? "AetherUmbra Test Instrument" : "AetherUmbra Test Music Effect"), tag + "name");
    check (proc.acceptsMidi() && ! proc.producesMidi() && ! proc.isMidiEffect(), tag + "takes MIDI in, no MIDI out, not a MIDI effect");
    check (juce::String (AETHER_VERSION_STRING).isNotEmpty() && ShellProcessor::versionString() == AETHER_VERSION_STRING, tag + "version string present");
    check (proc.getTailLengthSeconds() == 0.0, tag + "tail length 0");
    check (proc.getTotalNumInputChannels() == 2 && proc.getTotalNumOutputChannels() == 2, tag + "starts as stereo in, stereo out");

    check (proc.checkBusesLayoutSupported (layout (stereo, stereo)), tag + "layout stereo in, stereo out accepted");
    check (proc.checkBusesLayoutSupported (layout (mono, stereo)),   tag + "layout mono in, stereo out accepted");
    check (proc.checkBusesLayoutSupported (layout (mono, mono)),     tag + "layout mono in, mono out accepted");
    check (proc.checkBusesLayoutSupported (layout (juce::AudioChannelSet::disabled(), stereo)), tag + "layout no audio in, stereo out accepted");
    check (! proc.checkBusesLayoutSupported (layout (stereo, mono)), tag + "layout stereo in, mono out refused");
    check (! proc.checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())), tag + "layout 5.1 refused");

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        proc.setPlayConfigDetails (2, 2, rate, 512);
        proc.prepareToPlay (rate, 512);
        setTone (proc, false);
        std::vector<float> in;
        auto out = run (proc, 20, true, {}, &in);
        double maxDiff = 0; for (size_t i = 0; i < out.size(); ++i) maxDiff = std::max (maxDiff, (double) std::fabs (out[i] - in[i]));
        check (maxDiff == 0.0, tag + "audio passes through unchanged (tone off) at " + juce::String (rate, 0) + " Hz");
    }

    // tone follows the last note, stays after the key is released
    const double rate = 44100.0;
    proc.setPlayConfigDetails (2, 2, rate, 512);
    proc.prepareToPlay (rate, 512);
    setTone (proc, true);
    auto silentOut = run (proc, 40, false, noteOn (69));
    {
        std::vector<float> tail (silentOut.begin() + 256, silentOut.end());
        const double hz = measureHz (tail, rate);
        check (std::fabs (hz - 440.0) < 2.0, tag + "note 69 gives a tone of 440 Hz (measured " + juce::String (hz, 1) + ")");
        float pk = 0; for (float v : tail) pk = std::max (pk, std::fabs (v));
        check (std::fabs (pk - 0.1f) < 0.005f, tag + "test tone level is -20 dB (peak " + juce::String (pk, 3) + ")");
    }
    auto o2 = run (proc, 40, false, noteOn (81));
    { std::vector<float> t (o2.begin() + 256, o2.end()); const double hz = measureHz (t, rate);
      check (std::fabs (hz - 880.0) < 4.0, tag + "next note 81 gives 880 Hz (measured " + juce::String (hz, 1) + ")"); }
    auto o3 = run (proc, 40, false, noteOff (81));
    { std::vector<float> t (o3.begin() + 256, o3.end()); const double hz = measureHz (t, rate);
      check (std::fabs (hz - 880.0) < 4.0, tag + "pitch stays after the key is released (measured " + juce::String (hz, 1) + ")"); }
    check (proc.lastNote() == 81 && proc.noteCount() == 2, tag + "last note 81, 2 notes counted");

    // audio and notes together: the audio is still there under the tone
    {
        std::vector<float> in;
        auto out = run (proc, 20, true, noteOn (60), &in);
        double dev = 0; for (size_t i = 0; i < out.size(); ++i) dev = std::max (dev, (double) std::fabs (out[i] - in[i]));
        check (dev > 0.05 && dev <= 0.1001, tag + "audio and tone together: output = audio + tone (-20 dB)");
    }

    // no audio input bus
    {
        ShellProcessor p2 (kind);
        p2.setBusesLayout (layout (juce::AudioChannelSet::disabled(), stereo));
        p2.setPlayConfigDetails (0, 2, rate, 512);
        p2.prepareToPlay (rate, 512);
        auto o = run (p2, 40, false, noteOn (69));
        std::vector<float> t (o.begin() + 256, o.end());
        check (p2.getTotalNumInputChannels() == 0 && std::fabs (measureHz (t, rate) - 440.0) < 2.0, tag + "no audio input: tone alone, 440 Hz for note 69");
    }
    // mono in, stereo out: both outputs carry the audio
    {
        ShellProcessor p3 (kind);
        p3.setBusesLayout (layout (mono, stereo));
        p3.setPlayConfigDetails (1, 2, rate, 512);
        p3.prepareToPlay (rate, 512);
        setTone (p3, false);
        juce::AudioBuffer<float> b (2, 128); b.clear();
        for (int i = 0; i < 128; ++i) b.setSample (0, i, 0.25f);
        juce::MidiBuffer m; p3.processBlock (b, m);
        check (b.getSample (0, 50) == 0.25f && b.getSample (1, 50) == 0.25f, tag + "mono in is copied to both outputs");
    }

    // saving and loading
    {
        setTone (proc, false);
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        ShellProcessor p4 (kind);
        check (p4.apvts.getRawParameterValue ("tone")->load() > 0.5f, tag + "test tone is on by default");
        p4.setStateInformation (mb.getData(), (int) mb.getSize());
        check (p4.apvts.getRawParameterValue ("tone")->load() < 0.5f, tag + "setting 'tone off' survives save and load");
    }

    // window
    {
        setTone (proc, true);
        proc.setPlayConfigDetails (2, 2, rate, 512);
        proc.prepareToPlay (rate, 512);
        run (proc, 4, true, noteOn (69));
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* ed = dynamic_cast<ShellEditor*> (base.get());
        check (ed != nullptr && ed->getWidth() > 300 && ed->getHeight() > 200, tag + "window is created");
        if (ed)
        {
            run (proc, 4, true);      // audio arrives while the window is open (the window reads the level it saw since its last refresh)
            ed->refresh();
            check (ed->audioLine().contains ("2 channel(s)") && ed->audioLine().contains ("dB"), tag + "window shows the audio input (" + ed->audioLine() + ")");
            check (ed->notesLine().contains ("last note 69") && ed->notesLine().contains ("A4"), tag + "window shows the last note (" + ed->notesLine() + ")");
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            check (img.isValid() && img.getWidth() > 300, tag + "window picture is drawn");
            if (picturesDir.isNotEmpty())
            {
                juce::File f = juce::File (picturesDir).getChildFile (inst ? "window_instrument.png" : "window_music_effect.png");
                juce::FileOutputStream out (f);
                if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (img, out); }
            }
        }
    }
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String dir = argc > 1 ? juce::String (argv[1]) : juce::String();
    if (dir.isNotEmpty()) juce::File (dir).createDirectory();
    testShell (ShellKind::Instrument, dir);
    testShell (ShellKind::MusicEffect, dir);
    std::printf ("\n%s\n", failures == 0 ? "RESULT - all plugin checks passed" : "RESULT - SOME PLUGIN CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}

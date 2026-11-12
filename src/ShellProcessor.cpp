#include "ShellProcessor.h"
#include "ShellEditor.h"
#include <cmath>

namespace
{
constexpr int kDefaultNote = 57;      // A3, 220 Hz, until the first MIDI note arrives
constexpr float kToneLevel = 0.1f;    // -20 dB

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tone", 1 }, "Test tone", true));
    return l;
}
}

ShellProcessor::ShellProcessor (ShellKind kind)
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AetherUmbraState", createLayout()),
      kind_ (kind)
{
}

double ShellProcessor::toneHz() const
{
    const int n = lastNote_.load() < 0 ? kDefaultNote : lastNote_.load();
    return 440.0 * std::pow (2.0, (n - 69) / 12.0);
}

void ShellProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRate_ = sampleRate;
    phase_ = 0.0;
}

// Stereo out. Audio in: stereo, mono, or none (an instrument may have no audio input).
bool ShellProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in.isDisabled()) return true;
    if (in == juce::AudioChannelSet::stereo()) return out == juce::AudioChannelSet::stereo();
    if (in == juce::AudioChannelSet::mono()) return true;
    return false;
}

void ShellProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    // Notes: the last note played counts and stays after the key is released (like a held CV).
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { lastNote_.store (m.getNoteNumber()); noteCount_.fetch_add (1); }
    }

    // Audio in: measure it, pass it through unchanged. Outputs without an input start silent.
    inputChannels_.store (numIn);
    float peak = 0.f;
    for (int ch = 0; ch < numIn; ++ch) peak = std::max (peak, buffer.getMagnitude (ch, 0, n));
    if (peak > inputPeak_.load()) inputPeak_.store (peak);
    for (int ch = numIn; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (numIn == 1 && numOut >= 2) buffer.copyFrom (1, 0, buffer, 0, 0, n);   // mono in, stereo out

    // Test tone (a drone: no envelope), following the last note.
    if (apvts.getRawParameterValue ("tone")->load() > 0.5f)
    {
        const double inc = juce::MathConstants<double>::twoPi * toneHz() / sampleRate_;
        for (int i = 0; i < n; ++i)
        {
            const float s = kToneLevel * (float) std::sin (phase_);
            phase_ += inc;
            if (phase_ >= juce::MathConstants<double>::twoPi) phase_ -= juce::MathConstants<double>::twoPi;
            for (int ch = 0; ch < numOut; ++ch) buffer.getWritePointer (ch)[i] += s;
        }
    }
}

juce::AudioProcessorEditor* ShellProcessor::createEditor() { return new ShellEditor (*this); }

void ShellProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("version", versionString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ShellProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

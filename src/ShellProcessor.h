#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

// AetherUmbra, Milestone 0: an empty shell. The audio of the track passes through unchanged, a simple test tone
// (a sine) follows the last MIDI note, and the window reports what Logic delivers (audio in, notes in).
// The same code is built twice: as an AU Instrument (aumu) and as an AU Music Effect (aumf).
enum class ShellKind { Instrument, MusicEffect };

class ShellProcessor : public juce::AudioProcessor
{
public:
    explicit ShellProcessor (ShellKind kind);

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return kind_ == ShellKind::Instrument ? "AetherUmbra Test Instrument" : "AetherUmbra Test Music Effect"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::String versionString() { return AETHER_VERSION_STRING; }
    ShellKind kind() const { return kind_; }
    juce::String kindText() const { return kind_ == ShellKind::Instrument ? "AU Instrument (aumu)" : "AU Music Effect (aumf)"; }

    // What the window shows (written on the audio thread, read on the message thread).
    int lastNote() const { return lastNote_.load(); }              // -1 = no note yet
    int noteCount() const { return noteCount_.load(); }
    int inputChannels() const { return inputChannels_.load(); }    // channels of the audio input bus in the last block
    float takeInputPeak() { return inputPeak_.exchange (0.f); }    // largest input sample since the last call
    double toneHz() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    ShellKind kind_;
    double sampleRate_ = 44100.0, phase_ = 0.0;
    std::atomic<int> lastNote_ { -1 }, noteCount_ { 0 }, inputChannels_ { 0 };
    std::atomic<float> inputPeak_ { 0.f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShellProcessor)
};

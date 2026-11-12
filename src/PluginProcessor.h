#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "AetherStream.h"

// AetherUmbra: the Oneiroi sound engine as an AU Instrument. It makes sound by itself (oscillators, looper), takes its pitch
// from MIDI notes, and processes audio that arrives on the Side Chain input (Logic's Side Chain menu).
class AetherProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater
{
public:
    AetherProcessor();
    ~AetherProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AetherUmbra"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return aether_params::kTailSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::String versionString() { return AETHER_VERSION_STRING; }

    juce::AudioProcessorValueTreeState apvts;
    aether::Stream& stream() { return stream_; }

    // Window buttons (message thread). Clear is picked up by the audio thread at the start of the next block.
    void requestClearLooper() { clearRequested_.store (true); }
    void randomise() { randomiser_.randomise (apvts); }
    void undoRedo() { randomiser_.undoRedo (apvts); }
    // For the plugin test: run a randomise / undo request that is waiting (normally the message loop does it).
    void runPendingRequests() { handleUpdateNowIfNeeded(); }

    // What the window shows (written on the audio thread, read on the message thread).
    int lastNote() const { return lastNote_.load(); }              // -1 = no note yet
    int inputChannels() const { return inputChannels_.load(); }    // channels of the audio input bus in the last block
    float takeInputPeak() { return inputPeak_.exchange (0.f); }    // largest input sample since the last call
    float modulationValue() const { return modValue_.load(); }
    double hostBpm() const { return lastBpm_.load(); }             // Logic's tempo in the last block; 0 = none known
    int underruns() const { return stream_.underruns(); }

private:
    void handleAsyncUpdate() override;
    aether::Stream stream_;
    aether_params::Randomiser randomiser_;
    std::atomic<bool> clearRequested_ { false };
    bool lastRandomise_ = false, lastUndo_ = false;
    std::atomic<int> lastNote_ { -1 }, inputChannels_ { 0 };
    std::atomic<float> inputPeak_ { 0.f }, modValue_ { 0.f };
    std::atomic<double> lastBpm_ { 0.0 };
    float noteOffset_ = 0.f;
    std::vector<float> scratch_[4];
    int maxBlock_ = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherProcessor)
};

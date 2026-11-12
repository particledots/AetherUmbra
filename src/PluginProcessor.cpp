#include "PluginProcessor.h"
#include "PluginEditor.h"

AetherProcessor::AetherProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AetherUmbraState", aether_params::createLayout())
{
}

void AetherProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock_ = std::max (samplesPerBlock, 512);
    for (auto& s : scratch_) s.assign ((size_t) maxBlock_, 0.f);
    stream_.prepare (sampleRate, maxBlock_);
    stream_.setParams (aether_params::toEngine (apvts, noteOffset_), 0.f, apvts.getRawParameterValue ("output_level")->load());
    setLatencySamples (stream_.latencySamples());
    lastRandomise_ = apvts.getRawParameterValue ("randomise")->load() > 0.5f;
    lastUndo_ = apvts.getRawParameterValue ("undo_redo")->load() > 0.5f;
}

// One of the two trigger parameters went from off to on (automation, or a host's own parameter list): do it once on the
// message thread (changing parameters is not done on the audio thread), then switch the parameter off again.
void AetherProcessor::handleAsyncUpdate()
{
    auto off = [this] (const char* id)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (0.f); p->endChangeGesture(); }
    };
    if (apvts.getRawParameterValue ("randomise")->load() > 0.5f) { randomise(); off ("randomise"); }
    if (apvts.getRawParameterValue ("undo_redo")->load() > 0.5f) { undoRedo(); off ("undo_redo"); }
}

// Stereo out. Audio in (the Side Chain): stereo, mono, or none.
bool AetherProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in.isDisabled()) return true;
    if (in == juce::AudioChannelSet::stereo()) return out == juce::AudioChannelSet::stereo();
    if (in == juce::AudioChannelSet::mono()) return true;
    return false;
}

void AetherProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int total = buffer.getNumSamples();
    if (maxBlock_ == 0) return;     // not prepared

    // Notes: the last note played sets the pitch and stays after the key is released (like a held CV on the module).
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { lastNote_.store (m.getNoteNumber()); noteOffset_ = (float) (m.getNoteNumber() - 60); }
    }

    stream_.setParams (aether_params::toEngine (apvts, noteOffset_), 0.f, apvts.getRawParameterValue ("output_level")->load());
    if (clearRequested_.exchange (false)) stream_.clearLooper();

    const bool rnd = apvts.getRawParameterValue ("randomise")->load() > 0.5f;
    const bool und = apvts.getRawParameterValue ("undo_redo")->load() > 0.5f;
    if ((rnd && ! lastRandomise_) || (und && ! lastUndo_)) triggerAsyncUpdate();
    lastRandomise_ = rnd; lastUndo_ = und;

    inputChannels_.store (numIn);
    float peak = 0.f;
    for (int ch = 0; ch < numIn; ++ch) peak = std::max (peak, buffer.getMagnitude (ch, 0, total));
    if (peak > inputPeak_.load()) inputPeak_.store (peak);

    for (int pos = 0; pos < total; pos += maxBlock_)
    {
        const int n = std::min (maxBlock_, total - pos);
        // copy the input first: the output goes into the same buffer. No input at all = silence.
        if (numIn >= 1) std::copy (buffer.getReadPointer (0) + pos, buffer.getReadPointer (0) + pos + n, scratch_[0].begin());
        else std::fill (scratch_[0].begin(), scratch_[0].begin() + n, 0.f);
        if (numIn >= 2) std::copy (buffer.getReadPointer (1) + pos, buffer.getReadPointer (1) + pos + n, scratch_[1].begin());
        else std::copy (scratch_[0].begin(), scratch_[0].begin() + n, scratch_[1].begin());
        stream_.process (scratch_[0].data(), scratch_[1].data(), scratch_[2].data(), scratch_[3].data(), n);
        if (numOut >= 2)
        {
            std::copy (scratch_[2].begin(), scratch_[2].begin() + n, buffer.getWritePointer (0) + pos);
            std::copy (scratch_[3].begin(), scratch_[3].begin() + n, buffer.getWritePointer (1) + pos);
        }
        else
        {
            float* o = buffer.getWritePointer (0) + pos;
            for (int i = 0; i < n; ++i) o[i] = 0.5f * (scratch_[2][(size_t) i] + scratch_[3][(size_t) i]);
        }
    }
    modValue_.store (stream_.modulationValue());
}

juce::AudioProcessorEditor* AetherProcessor::createEditor() { return new AetherEditor (*this); }

void AetherProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("version", versionString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AetherProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // a project saved with a trigger on must not fire when it is opened
            for (const char* id : { "randomise", "undo_redo" })
                if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (0.f);
            lastRandomise_ = lastUndo_ = false;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AetherProcessor(); }

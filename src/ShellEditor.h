#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ShellProcessor.h"

// Placeholder window for Milestone 0: name, version, the "not affiliated" notice, and live lines that show what
// Logic delivers to this plugin (audio in, notes in) so the test in Logic can be read from the screen.
class ShellEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShellEditor (ShellProcessor&);
    ~ShellEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::String audioLine() const { return audioText_; }
    juce::String notesLine() const { return notesText_; }
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;
    ShellProcessor& proc_;
    juce::Label title_, kind_, version_, audio_, notes_, tone_, notice_;
    juce::ToggleButton toneButton_ { "Test tone on" };
    juce::String audioText_, notesText_;
    // declared last so it is destroyed first, before the control it is attached to
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneAtt_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShellEditor)
};

#include "ShellEditor.h"
#include <cmath>

namespace
{
const juce::Colour kBg { 0xff111317 }, kInk { 0xffe8eaee }, kDim { 0xff9aa0aa }, kAccent { 0xff5ec8f2 };
juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}
void style (juce::Label& l, const juce::String& text, float size, juce::Colour c, bool bold = false)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (juce::Justification::centredLeft);
}
}

ShellEditor::ShellEditor (ShellProcessor& p) : AudioProcessorEditor (&p), proc_ (p)
{
    setSize (620, 400);
    style (title_, "AetherUmbra", 28.f, kInk, true);
    style (kind_, "Test shell: " + p.kindText(), 15.f, kAccent, true);
    style (version_, "Version " + ShellProcessor::versionString() + "  (Milestone 0: empty test shell)", 13.f, kDim);
    style (audio_, "", 15.f, kInk);
    style (notes_, "", 15.f, kInk);
    style (tone_, "", 13.f, kDim);
    style (notice_, "Unofficial. Based on the open-source Befaco Oneiroi firmware 1.2.2 (GPL-3.0). Not made, approved or "
                    "supported by Befaco. Do not contact them about this plugin.", 12.f, kDim);
    notice_.setMinimumHorizontalScale (1.0f);
    for (auto* l : { &title_, &kind_, &version_, &audio_, &notes_, &tone_, &notice_ }) addAndMakeVisible (*l);
    toneButton_.setColour (juce::ToggleButton::textColourId, kInk);
    toneButton_.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (toneButton_);
    toneAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "tone", toneButton_);
    timerCallback();
    startTimerHz (10);
}

ShellEditor::~ShellEditor() { stopTimer(); }

void ShellEditor::paint (juce::Graphics& g) { g.fillAll (kBg); }

void ShellEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    title_.setBounds (r.removeFromTop (40));
    kind_.setBounds (r.removeFromTop (24));
    version_.setBounds (r.removeFromTop (22));
    r.removeFromTop (18);
    audio_.setBounds (r.removeFromTop (28));
    notes_.setBounds (r.removeFromTop (28));
    tone_.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    toneButton_.setBounds (r.removeFromTop (28).removeFromLeft (200));
    notice_.setBounds (r.removeFromBottom (60));
}

void ShellEditor::timerCallback()
{
    const int ch = proc_.inputChannels();
    const float peak = proc_.takeInputPeak();
    const juce::String level = peak > 1.0e-5f ? juce::String (20.f * std::log10 (peak), 1) + " dB" : "silent";
    audioText_ = "Audio in: " + (ch == 0 ? juce::String ("no audio input bus (0 channels)")
                                          : juce::String (ch) + " channel(s), level " + level);
    const int n = proc_.lastNote();
    notesText_ = "Notes in: " + (n < 0 ? juce::String ("no note received yet")
                                        : "last note " + juce::String (n) + " (" + noteName (n) + "), "
                                          + juce::String (proc_.noteCount()) + " note(s) received");
    audio_.setText (audioText_, juce::dontSendNotification);
    notes_.setText (notesText_, juce::dontSendNotification);
    tone_.setText ("Test tone follows the last note: " + juce::String (proc_.toneHz(), 1) + " Hz", juce::dontSendNotification);
}

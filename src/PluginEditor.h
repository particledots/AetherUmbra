#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <memory>
#include <vector>

// The window: "module layout" (option C of the approved mock-up). It is drawn on a 1400 x 1000 grid and scaled to the window.
class AetherEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AetherEditor (AetherProcessor&);
    ~AetherEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // For the plugin test: draw the window into an image.
    juce::Image renderToImage();

private:
    struct Look;
    struct Knob;
    struct Fader;
    struct Segments;
    void timerCallback() override;
    void place (juce::Component&, float x, float y, float w, float h);
    Knob& addKnob (const char* paramId, const juce::String& name, float x, float y, float size, bool big = false);
    Fader& addFader (const char* paramId, const juce::String& name, float x, float y, float h);
    Segments& addSegments (const char* paramId, const juce::String& caption, const juce::StringArray& buttonNames, juce::Colour c, float x, float y, float w, float h, bool vertical = false);
    void panel (const juce::String& title, float x, float y, float w, float h, juce::Colour c);

    AetherProcessor& proc_;
    std::unique_ptr<Look> look_;
    struct Placed { juce::Component* c; float x, y, w, h; };
    std::vector<Placed> placed_;
    struct Panel { juce::String title; float x, y, w, h; juce::Colour colour; };
    std::vector<Panel> panels_;
    juce::OwnedArray<juce::Component> owned_;
    juce::Label title_, notice_, noteInfo_, audioInfo_;
    juce::TextButton randomiseButton_ { "Randomise" }, undoButton_ { "Undo / Redo" }, clearButton_ { "Clear" };
    juce::ToggleButton recordButton_ { "Record" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordAtt_;
    juce::ComponentBoundsConstrainer constrainer_;
    float scale_ = 1.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherEditor)
};

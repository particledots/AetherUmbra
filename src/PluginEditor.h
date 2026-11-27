#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Style.h"
#include <memory>
#include <vector>

// The window, in the "gold sections" look (1.1.2): black panel, gold hairline frames with their titles set into the top line,
// cream knob caps in gold range arcs, blue lamps for on / chosen, flow lines for how the sound and the modulation travel.
// It is laid out on a 1400 x 1000 grid (the module layout of the approved mock-up) and scaled to the window.
//
// Drawing is split in two so it stays light: the fixed artwork (frames, titles, labels, arcs, flow lines) is a child component
// drawn once into a stored picture (again only when the window size changes); 30 times a second only the live areas are redrawn
// (the modulation bar, the tempo, last-note and audio-in texts).
class AetherEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AetherEditor (AetherProcessor&);
    ~AetherEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // For the plugin test: draw the window into an image; the fixed artwork layer; the areas redrawn 30 times a second and the
    // frames (all in panel units); the width a control's own text needs.
    juce::Image renderToImage();
    juce::Component& getBackdrop();
    static juce::Array<juce::Rectangle<int>> liveAreas();
    static std::vector<juce::Rectangle<float>> frameRects();
    static float needWidth (const juce::Button&);

private:
    struct Look;
    struct Knob;
    struct Fader;
    struct Segments;
    struct Panel;
    struct Backdrop;
    enum class Kind { Pill, List };

    void timerCallback() override;
    void paintBackdrop (juce::Graphics&);
    void paintLive (juce::Graphics&);
    Knob& addKnob (const char* paramId, const juce::String& name, float cx, float cy, float size, bool bipolar = false,
                   const juce::StringArray& labelLines = {});
    void addFader (const char* paramId, const juce::String& name, float cx, float top, float bottom);
    Segments& addSegments (const char* paramId, const juce::StringArray& buttonNames, Kind kind, float x, float y, float w, float h);
    juce::TextButton& addSwitch (const char* paramId, const juce::String& text, float cx, float cy, float minLeft = 0.0f);
    void caption (const juce::String& s, float x, float y, float spacing = 1.5f);

    AetherProcessor& proc_;
    std::unique_ptr<Look> look_;
    std::unique_ptr<Panel> panel_;
    std::unique_ptr<Backdrop> backdrop_;
    struct KnobArt { float cx, cy, size; bool bipolar; juce::StringArray lines; };
    struct FaderArt { float cx, bottom; juce::String name; };
    struct Caption { juce::String s; float x, y, spacing; };
    std::vector<KnobArt> knobArt_;
    std::vector<FaderArt> faderArt_;
    std::vector<Caption> captions_;
    juce::OwnedArray<juce::Component> owned_;
    // declared after owned_ so the attachments are destroyed before the buttons they point to
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> switchAtts_;
    juce::TextButton randomiseButton_ { "Randomise" }, undoButton_ { "Undo / Redo" }, clearButton_ { "Clear" };
    juce::ToggleButton recordButton_ { "Record" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordAtt_;
    juce::ComponentBoundsConstrainer constrainer_;
    float modShown_ = 0.f;                       // the modulation value drawn in the Modulation frame
    juce::String tempoText_, noteText_;
    juce::StringArray audioLines_;
    int audioNumLines_ = 0;
    int tick_ = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AetherEditor)
};

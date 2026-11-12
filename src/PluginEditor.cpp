#include "PluginEditor.h"
#include <cmath>

namespace
{
const juce::Colour kBg { 0xff0f1115 }, kPanel { 0xff191c22 }, kInk { 0xffe8eaee }, kDim { 0xff8e949e }, kTrack { 0xff2b3038 };
const juce::Colour kLooper { 0xfff0b23c }, kOsc { 0xff5ec8f2 }, kMod { 0xffb58cff }, kFilter { 0xff6fdc8c },
                   kReso { 0xfff07a9c }, kEcho { 0xffff9a5c }, kAmb { 0xff7aa2ff }, kRandom { 0xffe85d5d }, kMix { 0xffc8ccd4 };

juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}
juce::Font fontOf (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
}

// ---- look -----------------------------------------------------------------------------------------------------------
struct AetherEditor::Look : juce::LookAndFeel_V4
{
    Look()
    {
        setColour (juce::TextButton::buttonColourId, kTrack);
        setColour (juce::TextButton::textColourOffId, kInk);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Label::textColourId, kInk);
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (juce::jmin (w, h) * 0.08f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
        const float thick = juce::jmax (2.f, r * 0.14f);
        juce::Path track, arc;
        track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.f, a0, a1, true);
        g.setColour (kTrack); g.strokePath (track, juce::PathStrokeType (thick));
        const float ang = a0 + pos * (a1 - a0);
        arc.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.f, a0, ang, true);
        g.setColour (colour); g.strokePath (arc, juce::PathStrokeType (thick));
        g.setColour (juce::Colour (0xff23272f)); g.fillEllipse (c.x - r * 0.68f, c.y - r * 0.68f, r * 1.36f, r * 1.36f);
        juce::Path p; p.addRoundedRectangle (-r * 0.04f, -r * 0.62f, r * 0.08f, r * 0.34f, r * 0.03f);
        g.setColour (kInk); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float cx = x + w * 0.5f, trackW = juce::jmax (4.f, w * 0.12f), top = y + h * 0.03f, bot = y + h * 0.97f;
        g.setColour (kTrack); g.fillRoundedRectangle (cx - trackW * 0.5f, top, trackW, bot - top, trackW * 0.5f);
        g.setColour (s.findColour (juce::Slider::trackColourId)); g.fillRoundedRectangle (cx - trackW * 0.5f, pos, trackW, bot - pos, trackW * 0.5f);
        const float hw = w * 0.62f, hh = juce::jmax (8.f, h * 0.07f);
        g.setColour (juce::Colour (0xff2e333c)); g.fillRoundedRectangle (cx - hw * 0.5f, pos - hh * 0.5f, hw, hh, hh * 0.25f);
        g.setColour (kInk); g.fillRect (cx - hw * 0.5f + 2.f, pos - 1.f, hw - 4.f, 2.f);
    }
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        g.setColour (on ? kRandom : kTrack); g.fillRoundedRectangle (r, r.getHeight() * 0.18f);
        g.setColour (on ? juce::Colours::white : kInk);
        g.setFont (fontOf (r.getHeight() * 0.3f, true));
        g.drawText (on ? "RECORDING" : b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
    }
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override { return fontOf (juce::jmin ((float) h * 0.5f, (float) b.getWidth() * 0.3f), true); }
};

// ---- controls -------------------------------------------------------------------------------------------------------
struct AetherEditor::Knob : juce::Component
{
    Knob (AetherProcessor& p, const char* id, const juce::String& name, juce::Colour c, bool big)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, big ? kInk : kDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
        // double click = back to the default value of the parameter
        if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        setHelpText (name);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int lh = juce::jmax (10, (int) (getWidth() * 0.2f));
        label.setFont (fontOf ((float) lh * 0.8f));
        label.setBounds (r.removeFromBottom (lh));
        slider.setBounds (r);
    }
    juce::Slider slider; juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

struct AetherEditor::Fader : juce::Component
{
    Fader (AetherProcessor& p, const char* id, const juce::String& name, juce::Colour c)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::trackColourId, c);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (slider);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, kInk);
        addAndMakeVisible (label);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
        if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int lh = juce::jmax (10, (int) (getWidth() * 0.26f));
        label.setFont (fontOf ((float) lh * 0.78f, true));
        label.setBounds (r.removeFromBottom (lh));
        slider.setBounds (r);
    }
    juce::Slider slider; juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// A row (or column) of buttons of which exactly one is on: for choice parameters and on/off switches.
struct AetherEditor::Segments : juce::Component
{
    Segments (AetherProcessor& p, const char* id, const juce::String& cap, const juce::StringArray& names, juce::Colour c, bool vert)
        : caption (cap), vertical (vert)
    {
        static int nextGroup = 4000;
        const int group = nextGroup++;
        auto* prm = p.apvts.getParameter (id);
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (group);
            b->setColour (juce::TextButton::buttonOnColourId, c);
            b->onClick = [this, i] { if (att) att->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            const int idx = juce::jlimit (0, buttons.size() - 1, (int) std::lround (v));
            buttons[idx]->setToggleState (true, juce::dontSendNotification);
        });
        att->sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override
    {
        if (caption.isEmpty()) return;
        g.setColour (kDim); g.setFont (fontOf (captionH() * 0.82f));
        g.drawText (caption, getLocalBounds().removeFromTop ((int) captionH()), juce::Justification::centredLeft);
    }
    float captionH() const { return caption.isEmpty() ? 0.f : juce::jmax (10.f, (float) getHeight() * (vertical ? 0.14f : 0.36f)); }
    void resized() override
    {
        auto r = getLocalBounds(); r.removeFromTop ((int) captionH());
        const int n = buttons.size();
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds (vertical ? r.removeFromTop (r.getHeight() / (n - i)).reduced (0, 1) : r.removeFromLeft (r.getWidth() / (n - i)).reduced (1, 0));
    }
    juce::String caption; bool vertical;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
};

// ---- the window -----------------------------------------------------------------------------------------------------
AetherEditor::Knob& AetherEditor::addKnob (const char* id, const juce::String& name, float x, float y, float size, bool big)
{
    juce::Colour c = kOsc;
    const juce::String s (id);
    if (s.startsWith ("looper")) c = kLooper; else if (s.startsWith ("osc")) c = kOsc; else if (s.startsWith ("mod")) c = kMod;
    else if (s.startsWith ("filter")) c = kFilter; else if (s.startsWith ("resonator")) c = kReso; else if (s.startsWith ("echo")) c = kEcho;
    else if (s.startsWith ("ambience")) c = kAmb; else if (s.startsWith ("output")) c = kMix;
    auto* k = new Knob (proc_, id, name, c, big);
    owned_.add (k); addAndMakeVisible (k);
    place (*k, x, y, size, size + size * (big ? 0.2f : 0.34f));
    return *k;
}
AetherEditor::Fader& AetherEditor::addFader (const char* id, const juce::String& name, float x, float y, float h)
{
    auto* f = new Fader (proc_, id, name, kMix);
    owned_.add (f); addAndMakeVisible (f);
    place (*f, x, y, 66.f, h);
    return *f;
}
AetherEditor::Segments& AetherEditor::addSegments (const char* id, const juce::String& caption, const juce::StringArray& names, juce::Colour c, float x, float y, float w, float h, bool vertical)
{
    auto* sg = new Segments (proc_, id, caption, names, c, vertical);
    owned_.add (sg); addAndMakeVisible (sg);
    place (*sg, x, y, w, h);
    return *sg;
}

void AetherEditor::place (juce::Component& c, float x, float y, float w, float h) { placed_.push_back ({ &c, x, y, w, h }); }
void AetherEditor::panel (const juce::String& t, float x, float y, float w, float h, juce::Colour c) { panels_.push_back ({ t, x, y, w, h, c }); }

AetherEditor::AetherEditor (AetherProcessor& p) : AudioProcessorEditor (&p), proc_ (p), look_ (new Look)
{
    setLookAndFeel (look_.get());
    constrainer_.setFixedAspectRatio (1.4);
    constrainer_.setSizeLimits (700, 500, 2100, 1500);
    setConstrainer (&constrainer_);
    setResizable (true, true);

    // Header
    title_.setText ("AetherUmbra", juce::dontSendNotification);
    title_.setColour (juce::Label::textColourId, kInk);
    notice_.setText ("Unofficial. Based on the open-source Befaco Oneiroi firmware 1.2.2 (GPL-3.0). Not made, approved or supported by Befaco.  Version "
                         + AetherProcessor::versionString(), juce::dontSendNotification);
    notice_.setColour (juce::Label::textColourId, kDim);
    noteInfo_.setColour (juce::Label::textColourId, kInk);
    noteInfo_.setJustificationType (juce::Justification::centredRight);
    audioInfo_.setColour (juce::Label::textColourId, kDim);
    audioInfo_.setJustificationType (juce::Justification::topLeft);
    for (auto* l : { &title_, &notice_, &noteInfo_, &audioInfo_ }) { addAndMakeVisible (*l); l->setMinimumHorizontalScale (1.0f); }
    place (title_, 20, 6, 330, 40); place (notice_, 20, 46, 1100, 22); place (noteInfo_, 820, 10, 440, 30);
    addKnob ("output_level", "Out", 1292, 4, 56);

    // Looper
    panel ("LOOPER", 20, 80, 450, 280, kLooper);
    addKnob ("looper_speed", "Speed", 40, 110, 130, true);
    addKnob ("looper_speed_mod", "MOD", 77, 270, 56);
    addKnob ("looper_start", "Start", 200, 112, 100, true);
    addKnob ("looper_start_mod", "MOD", 192, 246, 56);
    addKnob ("looper_sos", "Sound on sound", 256, 246, 56);
    addKnob ("looper_length", "Length", 340, 112, 100, true);
    addKnob ("looper_length_mod", "MOD", 332, 246, 56);
    addKnob ("looper_filter", "Filter", 396, 246, 56);

    // Oscillator
    panel ("OSCILLATOR", 20, 370, 450, 290, kOsc);
    addKnob ("osc_pitch", "Pitch", 40, 398, 130, true);
    addKnob ("osc_pitch_mod", "MOD", 50, 560, 56);
    addKnob ("osc_octave", "Octave", 112, 560, 56);
    addKnob ("osc_detune", "Detune", 200, 400, 100, true);
    addKnob ("osc_detune_mod", "MOD", 192, 534, 56);
    addKnob ("osc_unison", "Unison", 256, 534, 56);
    addSegments ("osc_kind", "Second oscillator", { "Supersaw", "Wavetable" }, kOsc, 336, 420, 120, 96, true);

    // Modulation
    panel ("MODULATION", 485, 80, 135, 580, kMod);
    addKnob ("mod_level", "Level", 500, 118, 105, true);
    addKnob ("mod_speed", "Speed", 500, 300, 105, true);
    addKnob ("mod_type", "Type", 500, 482, 105, true);

    // Filter, Resonator, Echo, Ambience
    panel ("FILTER", 635, 80, 365, 285, kFilter);
    addKnob ("filter_cutoff", "Cutoff", 648, 110, 82, true);
    addKnob ("filter_cutoff_mod", "MOD", 661, 236, 56);
    addKnob ("filter_resonance", "Resonance", 750, 110, 82, true);
    addKnob ("filter_resonance_mod", "MOD", 763, 236, 56);
    addKnob ("filter_noise", "Noise", 856, 118, 62, true);
    addSegments ("filter_mode", "Mode", { "LP", "BP", "HP", "CF" }, kFilter, 648, 318, 190, 40);
    addSegments ("filter_position", "Position", { "I", "II", "III", "IV" }, kFilter, 852, 318, 138, 40);

    panel ("RESONATOR", 1015, 80, 365, 285, kReso);
    addKnob ("resonator_tune", "Tune", 1028, 110, 82, true);
    addKnob ("resonator_tune_mod", "MOD", 1041, 236, 56);
    addKnob ("resonator_feedback", "Feedback", 1130, 110, 82, true);
    addKnob ("resonator_feedback_mod", "MOD", 1143, 236, 56);
    addKnob ("resonator_dissonance", "Dissonance", 1240, 118, 70, true);

    panel ("ECHO", 635, 375, 365, 285, kEcho);
    addKnob ("echo_density", "Density", 648, 405, 82, true);
    addKnob ("echo_density_mod", "MOD", 661, 531, 56);
    addKnob ("echo_repeats", "Repeats", 750, 405, 82, true);
    addKnob ("echo_repeats_mod", "MOD", 763, 531, 56);
    addKnob ("echo_filter", "Filter", 856, 413, 70, true);

    panel ("AMBIENCE", 1015, 375, 365, 285, kAmb);
    addKnob ("ambience_spacetime", "Spacetime", 1028, 405, 82, true);
    addKnob ("ambience_spacetime_mod", "MOD", 1041, 531, 56);
    addKnob ("ambience_decay", "Decay", 1130, 405, 82, true);
    addKnob ("ambience_decay_mod", "MOD", 1143, 531, 56);
    addKnob ("ambience_autopan", "Auto-pan", 1240, 413, 70, true);

    // Bottom row
    panel ("LOOPER RECORD", 20, 675, 205, 320, kLooper);
    addSegments ("looper_resampling", "Pre = input, Post = output", { "Pre", "Post" }, kLooper, 35, 712, 175, 54);
    addAndMakeVisible (recordButton_); place (recordButton_, 35, 784, 175, 84);
    recordAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "looper_recording", recordButton_);
    addAndMakeVisible (clearButton_); place (clearButton_, 35, 884, 175, 56);
    clearButton_.onClick = [this] { proc_.requestClearLooper(); };

    panel ("LEVELS", 235, 675, 385, 320, kMix);
    const char* f1[][2] = { { "looper_volume", "Looper" }, { "osc1_volume", "Sine" }, { "osc2_volume", "SS / WT" }, { "input_volume", "Input" } };
    for (int i = 0; i < 4; ++i) addFader (f1[i][0], f1[i][1], 252.f + 90.f * (float) i, 718, 262);

    panel ("RANDOM", 630, 675, 215, 320, kRandom);
    addSegments ("random_mode", "What", { "All", "Osc", "Loop", "FX" }, kRandom, 645, 712, 185, 50);
    addSegments ("random_amount", "Amount", { "Low", "Mid", "High" }, kRandom, 645, 770, 185, 50);
    addAndMakeVisible (randomiseButton_); place (randomiseButton_, 645, 832, 185, 66);
    randomiseButton_.setColour (juce::TextButton::buttonColourId, kRandom.darker (0.3f));
    randomiseButton_.onClick = [this] { proc_.randomise(); };
    addAndMakeVisible (undoButton_); place (undoButton_, 645, 908, 185, 50);
    undoButton_.onClick = [this] { proc_.undoRedo(); };

    panel ("LEVELS", 855, 675, 385, 320, kMix);
    const char* f2[][2] = { { "filter_level", "Filter" }, { "resonator_volume", "Resonator" }, { "echo_volume", "Echo" }, { "ambience_volume", "Ambience" } };
    for (int i = 0; i < 4; ++i) addFader (f2[i][0], f2[i][1], 872.f + 90.f * (float) i, 718, 262);

    panel ("AUDIO IN", 1250, 675, 130, 320, kMix);
    addAndMakeVisible (audioInfo_); place (audioInfo_, 1260, 712, 112, 270);

    timerCallback();
    startTimerHz (10);
    setSize (1120, 800);
}

AetherEditor::~AetherEditor() { stopTimer(); setLookAndFeel (nullptr); }

void AetherEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    for (const auto& p : panels_)
    {
        const auto r = juce::Rectangle<float> (p.x, p.y, p.w, p.h) * scale_;
        g.setColour (kPanel); g.fillRoundedRectangle (r, 8.f * scale_);
        g.setColour (p.colour.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (0.5f), 8.f * scale_, 1.f);
        g.setColour (p.colour); g.setFont (fontOf (13.f * scale_, true));
        g.drawText (p.title, (int) (r.getX() + 12.f * scale_), (int) (r.getY() + 6.f * scale_), (int) (r.getWidth() - 24.f * scale_), (int) (18.f * scale_), juce::Justification::centredLeft);
    }
}

void AetherEditor::resized()
{
    scale_ = (float) getWidth() / 1400.f;
    for (auto& p : placed_)
        p.c->setBounds (juce::Rectangle<float> (p.x, p.y, p.w, p.h).transformedBy (juce::AffineTransform::scale (scale_)).toNearestInt());
    title_.setFont (fontOf (30.f * scale_, true));
    notice_.setFont (fontOf (12.5f * scale_));
    noteInfo_.setFont (fontOf (16.f * scale_));
    audioInfo_.setFont (fontOf (12.f * scale_));
}

void AetherEditor::timerCallback()
{
    const int n = proc_.lastNote();
    noteInfo_.setText (n < 0 ? juce::String ("Pitch from MIDI notes: no note played yet (plays C4)")
                             : "Pitch from MIDI notes: last note " + noteName (n) + " (" + juce::String (n) + ")", juce::dontSendNotification);
    const int ch = proc_.inputChannels();
    const float peak = proc_.takeInputPeak();
    audioInfo_.setText ("In Logic, choose the audio track you want to process in this plugin's Side Chain menu.\n\n"
                        + (ch == 0 ? juce::String ("No audio input connected.")
                                   : juce::String (ch) + " channel(s) in, level "
                                         + (peak > 1.0e-5f ? juce::String (20.f * std::log10 (peak), 1) + " dB" : juce::String ("silent"))),
                        juce::dontSendNotification);
}

juce::Image AetherEditor::renderToImage() { return createComponentSnapshot (getLocalBounds(), true, 1.0f); }

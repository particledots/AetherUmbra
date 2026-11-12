#include "PluginEditor.h"
#include <cmath>

namespace
{
const juce::Colour kBg { 0xff0c0e12 }, kPanel { 0xff1a1d24 }, kInk { 0xffeceef2 }, kDim { 0xff8e949e }, kTrack { 0xff2a2f38 };
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
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);   // centre = off: the coloured arc starts at the top
        const float ang = a0 + pos * (a1 - a0);
        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        juce::Path track, arc;
        track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.f, a0, a1, true);
        g.setColour (kTrack); g.strokePath (track, juce::PathStrokeType (thick));
        if (std::abs (ang - from) > 0.01f)
        {
            arc.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.f, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (colour.withAlpha (0.28f)); g.strokePath (arc, juce::PathStrokeType (thick * 1.7f));    // soft glow under the arc
            g.setColour (colour); g.strokePath (arc, juce::PathStrokeType (thick));
        }
        if (bipolar)   // small tick at the centre position
        {
            const float mid = (a0 + a1) * 0.5f, r0 = r - thick * 1.05f, r1 = r + thick * 0.05f;
            g.setColour (kDim); g.drawLine (c.x + std::sin (mid) * r0, c.y - std::cos (mid) * r0, c.x + std::sin (mid) * r1, c.y - std::cos (mid) * r1, juce::jmax (1.f, thick * 0.22f));
        }
        const float capR = r * 0.68f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343a45), c.x, c.y - capR, juce::Colour (0xff1b1f26), c.x, c.y + capR, false));
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f);
        g.setColour (juce::Colour (0x22ffffff)); g.drawEllipse (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f, juce::jmax (1.f, r * 0.02f));
        juce::Path p; p.addRoundedRectangle (-r * 0.045f, -r * 0.62f, r * 0.09f, r * 0.34f, r * 0.03f);
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
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override { return fontOf (juce::jmin ((float) h * 0.5f, (float) b.getWidth() * (b.getButtonText().length() > 3 ? 0.34f : 0.46f)), true); }
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
        // controls whose middle position means "off" or "no change" get an arc that starts at the middle
        const juce::String pid (id);
        if (pid == "osc_pitch" || pid == "osc_unison" || pid == "looper_filter" || pid == "echo_filter") slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (slider);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, big ? kInk : juce::Colour (0xffc3c8d0));
        label.setInterceptsMouseClicks (false, false);
        label.setMinimumHorizontalScale (0.6f);
        label.setPaintingIsUnclipped (true);
        setPaintingIsUnclipped (true);
        addAndMakeVisible (label);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
        // double click = back to the default value of the parameter
        if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        setHelpText (name);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int lh = juce::jmax (10, getHeight() - getWidth());     // the knob is square, the label gets what is below it
        label.setFont (fontOf ((float) lh * 0.78f, true));
        label.setBounds (r.removeFromBottom (lh).expanded (getWidth() < 80 ? (int) (getWidth() * 0.3f) : getWidth() / 8, 0));
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
        label.setMinimumHorizontalScale (0.6f);
        label.setPaintingIsUnclipped (true);
        setPaintingIsUnclipped (true);
        addAndMakeVisible (label);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
        if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int lh = juce::jmax (10, (int) (getWidth() * 0.34f));
        label.setFont (fontOf ((float) lh * 0.7f, true));
        label.setBounds (r.removeFromBottom (lh).expanded (juce::jmax (6, getWidth() / 6), 0));
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
        g.setColour (juce::Colour (0xffc3c8d0)); g.setFont (fontOf (captionH() * 0.84f));
        g.drawFittedText (caption, getLocalBounds().removeFromTop ((int) captionH()), juce::Justification::centredLeft, 1, 0.7f);
    }
    float captionH() const { return caption.isEmpty() ? 0.f : juce::jmax (10.f, (float) getHeight() * (vertical ? 0.16f : 0.34f)); }
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
    place (*k, x, y, size, size + (big ? 28.f : 25.f));
    return *k;
}
AetherEditor::Fader& AetherEditor::addFader (const char* id, const juce::String& name, float x, float y, float h, juce::Colour c)
{
    auto* f = new Fader (proc_, id, name, c);
    owned_.add (f); addAndMakeVisible (f);
    place (*f, x, y, 70.f, h);
    return *f;
}
AetherEditor::Segments& AetherEditor::addSegments (const char* id, const juce::String& caption, const juce::StringArray& names, juce::Colour c, float x, float y, float w, float h, bool vertical)
{
    auto* sg = new Segments (proc_, id, caption, names, c, vertical);
    owned_.add (sg); addAndMakeVisible (sg);
    place (*sg, x, y, w, h);
    return *sg;
}

juce::TextButton& AetherEditor::addSwitch (const char* id, const juce::String& text, float x, float y, float w, juce::Colour c)
{
    auto* b = new juce::TextButton (text);
    b->setClickingTogglesState (true);
    b->setColour (juce::TextButton::buttonColourId, kTrack);
    b->setColour (juce::TextButton::buttonOnColourId, c);
    b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc3c8d0));
    b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    b->setHelpText (text);
    owned_.add (b); addAndMakeVisible (b);
    place (*b, x, y, w, 28.f);
    switchAtts_.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc_.apvts, id, *b));
    return *b;
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
    tempoInfo_.setColour (juce::Label::textColourId, kMod);
    tempoInfo_.setJustificationType (juce::Justification::centredLeft);
    for (auto* l : { &title_, &notice_, &noteInfo_, &audioInfo_, &tempoInfo_ }) { addAndMakeVisible (*l); l->setMinimumHorizontalScale (1.0f); }
    place (title_, 20, 6, 290, 40); place (tempoInfo_, 322, 10, 430, 32); place (notice_, 20, 48, 1270, 24); place (noteInfo_, 760, 10, 530, 32);
    addKnob ("output_level", "Out", 1318, 0, 56);

    // Looper (three columns centred at x 110, 270, 430; the small MOD knobs are 74 wide)
    panel ("LOOPER", 20, 80, 500, 306, kLooper);
    addSwitch ("snap_looper_speed", "Speed snap", 132, 84, 112, kLooper);
    addSwitch ("snap_looper_start", "Start snap", 250, 84, 106, kLooper);
    addSwitch ("snap_looper_length", "Length snap", 362, 84, 120, kLooper);
    addKnob ("looper_speed", "Speed", 52, 114, 116, true);
    addKnob ("looper_speed_mod", "MOD", 73, 266, 74);
    addKnob ("looper_start", "Start", 218, 118, 104, true);
    addKnob ("looper_start_mod", "MOD", 184, 266, 74);
    addKnob ("looper_sos", "Sound on sound", 272, 266, 74);
    addKnob ("looper_length", "Length", 378, 118, 104, true);
    addKnob ("looper_length_mod", "MOD", 358, 266, 74);
    addKnob ("looper_filter", "Filter", 438, 266, 74);

    // Oscillator
    panel ("OSCILLATOR", 20, 396, 500, 306, kOsc);
    addSwitch ("snap_osc_pitch", "Pitch snap", 170, 400, 112, kOsc);
    addKnob ("osc_pitch", "Pitch", 52, 430, 116, true);
    addKnob ("osc_pitch_mod", "MOD", 32, 582, 74);
    addKnob ("osc_octave", "Octave", 114, 582, 74);
    addKnob ("osc_detune", "Detune", 218, 434, 104, true);
    addKnob ("osc_detune_mod", "MOD", 192, 582, 74);
    addKnob ("osc_unison", "Unison", 274, 582, 74);
    addSegments ("osc_kind", "Second oscillator", { "Supersaw", "Wavetable" }, kOsc, 362, 440, 144, 120, true);

    // Modulation
    panel ("MODULATION", 530, 80, 125, 622, kMod);
    addKnob ("mod_level", "Level", 545, 122, 95, true);
    addKnob ("mod_speed", "Speed", 545, 322, 95, true);
    addKnob ("mod_type", "Type", 545, 522, 95, true);

    // Filter, Resonator, Echo, Ambience (columns centred 62, 181, 300 inside each 365-wide panel)
    panel ("FILTER", 665, 80, 355, 306, kFilter);
    addSwitch ("snap_filter_cutoff", "Cutoff snap", 775, 84, 124, kFilter);
    addKnob ("filter_cutoff", "Cutoff", 679, 108, 88, true);
    addKnob ("filter_cutoff_mod", "MOD", 686, 226, 74);
    addKnob ("filter_resonance", "Resonance", 798, 108, 88, true);
    addKnob ("filter_resonance_mod", "MOD", 805, 226, 74);
    addKnob ("filter_noise", "Noise", 923, 114, 76, true);
    addSegments ("filter_mode", "Mode", { "LP", "BP", "HP", "CF" }, kFilter, 678, 328, 190, 54);
    addSegments ("filter_position", "Position", { "I", "II", "III", "IV" }, kFilter, 876, 328, 134, 54);

    panel ("RESONATOR", 1030, 80, 355, 306, kReso);
    addSwitch ("snap_resonator_tune", "Tune snap", 1180, 84, 112, kReso);
    addKnob ("resonator_tune", "Tune", 1044, 108, 88, true);
    addKnob ("resonator_tune_mod", "MOD", 1051, 226, 74);
    addKnob ("resonator_feedback", "Feedback", 1163, 108, 88, true);
    addKnob ("resonator_feedback_mod", "MOD", 1170, 226, 74);
    addKnob ("resonator_dissonance", "Dissonance", 1288, 114, 76, true);

    panel ("ECHO", 665, 396, 355, 306, kEcho);
    addSwitch ("echo_sync", "Echo Sync", 775, 400, 124, kEcho);
    addKnob ("echo_density", "Density", 679, 428, 88, true);
    addKnob ("echo_density_mod", "MOD", 686, 548, 74);
    addKnob ("echo_repeats", "Repeats", 798, 428, 88, true);
    addKnob ("echo_repeats_mod", "MOD", 805, 548, 74);
    addKnob ("echo_filter", "Filter", 923, 434, 76, true);

    panel ("AMBIENCE", 1030, 396, 355, 306, kAmb);
    addKnob ("ambience_spacetime", "Spacetime", 1044, 428, 88, true);
    addKnob ("ambience_spacetime_mod", "MOD", 1051, 548, 74);
    addKnob ("ambience_decay", "Decay", 1163, 428, 88, true);
    addKnob ("ambience_decay_mod", "MOD", 1170, 548, 74);
    addKnob ("ambience_autopan", "Auto-pan", 1288, 434, 76, true);

    // Bottom row
    panel ("LOOPER RECORD", 20, 712, 215, 283, kLooper);
    addSegments ("looper_resampling", "Pre = input, Post = output", { "Pre", "Post" }, kLooper, 32, 744, 191, 58);
    addAndMakeVisible (recordButton_); place (recordButton_, 32, 810, 191, 84);
    recordAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "looper_recording", recordButton_);
    addAndMakeVisible (clearButton_); place (clearButton_, 32, 904, 191, 54);
    clearButton_.onClick = [this] { proc_.requestClearLooper(); };

    panel ("LEVELS", 245, 712, 350, 283, kMix);
    const char* f1[][2] = { { "looper_volume", "Looper" }, { "osc1_volume", "Sine" }, { "osc2_volume", "SS / WT" }, { "input_volume", "Input" } };
    const juce::Colour c1[] = { kLooper, kOsc, kOsc, kMix };
    for (int i = 0; i < 4; ++i) addFader (f1[i][0], f1[i][1], 258.f + 84.f * (float) i, 748, 236, c1[i]);

    panel ("RANDOM", 605, 712, 210, 283, kRandom);
    addSegments ("random_mode", "What", { "All", "Osc", "Loop", "FX" }, kRandom, 618, 744, 184, 56);
    addSegments ("random_amount", "Amount", { "Low", "Mid", "High" }, kRandom, 618, 806, 184, 56);
    addAndMakeVisible (randomiseButton_); place (randomiseButton_, 618, 870, 184, 56);
    randomiseButton_.setColour (juce::TextButton::buttonColourId, kRandom.darker (0.3f));
    randomiseButton_.onClick = [this] { proc_.randomise(); };
    addAndMakeVisible (undoButton_); place (undoButton_, 618, 934, 184, 48);
    undoButton_.onClick = [this] { proc_.undoRedo(); };

    panel ("LEVELS", 825, 712, 350, 283, kMix);
    const char* f2[][2] = { { "filter_level", "Filter" }, { "resonator_volume", "Resonator" }, { "echo_volume", "Echo" }, { "ambience_volume", "Ambience" } };
    const juce::Colour c2[] = { kFilter, kReso, kEcho, kAmb };
    for (int i = 0; i < 4; ++i) addFader (f2[i][0], f2[i][1], 838.f + 84.f * (float) i, 748, 236, c2[i]);

    panel ("AUDIO IN", 1185, 712, 205, 283, kMix);
    addAndMakeVisible (audioInfo_); place (audioInfo_, 1195, 744, 185, 244);

    timerCallback();
    startTimerHz (30);
    setSize (1260, 900);
}

AetherEditor::~AetherEditor() { stopTimer(); setLookAndFeel (nullptr); }

void AetherEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff12151b), 0.f, 0.f, kBg, 0.f, (float) getHeight(), false));
    g.fillAll();
    for (const auto& p : panels_)
    {
        const auto r = juce::Rectangle<float> (p.x, p.y, p.w, p.h) * scale_;
        g.setGradientFill (juce::ColourGradient (kPanel.brighter (0.10f), 0.f, r.getY(), kPanel.darker (0.12f), 0.f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.f * scale_);
        g.setColour (p.colour.withAlpha (0.45f)); g.drawRoundedRectangle (r.reduced (0.5f), 8.f * scale_, 1.f);
        g.setColour (juce::Colour (0x10ffffff)); g.drawRoundedRectangle (r.reduced (1.5f), 7.f * scale_, 1.f);    // faint inner highlight
        // coloured tab in front of the title, then the title
        g.setColour (p.colour); g.fillRoundedRectangle (r.getX() + 9.f * scale_, r.getY() + 10.f * scale_, 5.f * scale_, 17.f * scale_, 2.f * scale_);
        g.setFont (fontOf (18.f * scale_, true));
        g.drawFittedText (p.title, juce::Rectangle<int> ((int) (r.getX() + 21.f * scale_), (int) (r.getY() + 6.f * scale_), (int) (r.getWidth() - 26.f * scale_), (int) (24.f * scale_)), juce::Justification::centredLeft, 1, 0.8f);
    }

    // The modulation source: a bar that swings left and right with the current modulation value (-1 .. 1).
    {
        const auto r = juce::Rectangle<float> (545.f, 662.f, 95.f, 22.f) * scale_;
        g.setColour (kTrack); g.fillRoundedRectangle (r, 5.f * scale_);
        const float mid = r.getCentreX(), half = r.getWidth() * 0.5f - 2.f * scale_, v = juce::jlimit (-1.f, 1.f, modShown_);
        g.setColour (kMod);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, mid + v * half), r.getY() + 3.f * scale_, std::abs (v * half) + 1.f, r.getHeight() - 6.f * scale_), 3.f * scale_);
        g.setColour (kInk.withAlpha (0.55f)); g.drawLine (mid, r.getY() + 2.f * scale_, mid, r.getBottom() - 2.f * scale_, 1.f);
        g.setColour (kDim); g.setFont (fontOf (14.f * scale_));
        g.drawText ("MOD OUT", r.translated (0.f, 22.f * scale_).toNearestInt(), juce::Justification::centred);
    }
}

void AetherEditor::resized()
{
    scale_ = (float) getWidth() / 1400.f;
    for (auto& p : placed_)
        p.c->setBounds (juce::Rectangle<float> (p.x, p.y, p.w, p.h).transformedBy (juce::AffineTransform::scale (scale_)).toNearestInt());
    title_.setFont (fontOf (32.f * scale_, true));
    notice_.setFont (fontOf (15.f * scale_));
    noteInfo_.setFont (fontOf (19.f * scale_));
    tempoInfo_.setFont (fontOf (19.f * scale_, true));
    audioInfo_.setFont (fontOf (16.f * scale_));
}

void AetherEditor::timerCallback()
{
    // the modulation bar moves smoothly (30 times a second); the texts are refreshed 10 times a second
    modShown_ = proc_.modulationValue();
    repaint (juce::Rectangle<float> (545.f, 655.f, 95.f, 50.f).transformedBy (juce::AffineTransform::scale (scale_)).toNearestInt());
    if (tick_++ % 3 != 0) return;

    const int n = proc_.lastNote();
    noteInfo_.setText (n < 0 ? juce::String ("Pitch from MIDI notes: no note played yet (plays C4)")
                             : "Pitch from MIDI notes: last note " + noteName (n) + " (" + juce::String (n) + ")", juce::dontSendNotification);
    const double bpm = proc_.hostBpm();
    if (bpm <= 0.0) tempoInfo_.setText ("Tempo: none from Logic, own clock", juce::dontSendNotification);
    else
    {
        juce::String t = "Tempo: " + juce::String (bpm, 1) + " BPM from Logic";
        if (bpm < 40.0 || bpm > 300.0) t << " (used as " << juce::String (juce::jlimit (40.0, 300.0, bpm), 0) << ")";
        tempoInfo_.setText (t, juce::dontSendNotification);
    }
    const int ch = proc_.inputChannels();
    const float peak = proc_.takeInputPeak();
    audioInfo_.setText ("In Logic, choose the audio track you want to process in this plugin's Side Chain menu.\n\nMuting the source track does not stop the side chain.\n\n"
                        + (ch == 0 ? juce::String ("No audio input connected.")
                                   : juce::String (ch) + " channel(s) in, level "
                                         + (peak > 1.0e-5f ? juce::String (20.f * std::log10 (peak), 1) + " dB" : juce::String ("silent"))),
                        juce::dontSendNotification);
}

juce::Image AetherEditor::renderToImage() { return createComponentSnapshot (getLocalBounds(), true, 1.0f); }

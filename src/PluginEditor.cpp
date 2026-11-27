#include "PluginEditor.h"
#include "engine/AetherSnap.h"
#include <AetherFontData.h>
#include <cmath>

using namespace AetherStyle;

// ------------------------------------------------------------------------------------------------ fonts
namespace
{
    juce::Typeface::Ptr load (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
    struct Faces
    {
        juce::Typeface::Ptr regular = load (AetherFontData::SpaceGroteskRegular_ttf, AetherFontData::SpaceGroteskRegular_ttfSize);
        juce::Typeface::Ptr medium  = load (AetherFontData::SpaceGroteskMedium_ttf, AetherFontData::SpaceGroteskMedium_ttfSize);
        juce::Typeface::Ptr title   = load (AetherFontData::MarcellusRegular_ttf, AetherFontData::MarcellusRegular_ttfSize);
    };
    Faces& faces() { static Faces f; return f; }

    juce::Font make (juce::Typeface::Ptr face, float size, float spacing)
    {
        // if a font could not be read for any reason, fall back to the system font rather than draw nothing
        juce::FontOptions o = face != nullptr ? juce::FontOptions (face) : juce::FontOptions();
        juce::Font f (o.withPointHeight (size));
        if (spacing > 0.0f) f = f.withExtraKerningFactor (spacing / f.getHeight());
        return f;
    }
}
juce::Font AetherStyle::font (float size, bool medium, float spacing) { return make (medium ? faces().medium : faces().regular, size, spacing); }
juce::Font AetherStyle::titleFont() { return make (faces().title, titleSize, titleSpacing); }

// ------------------------------------------------------------------------------------------------ geometry (panel units)
namespace
{
    constexpr float kRow1 = 96.f, kRow2 = 430.f, kRowH = 300.f;
    constexpr float kLoopX = 20.f, kLoopW = 472.f, kModX = 516.f, kModW = 150.f;
    constexpr float kFiltX = 690.f, kResX = 1047.f, kSecW = 333.f;
    constexpr float kBotY = 750.f, kBotH = 222.f;
    constexpr float kBotX[5] = { 20.f, 228.f, 548.f, 768.f, 1148.f }, kBotW[5] = { 184.f, 296.f, 196.f, 356.f, 232.f };
    constexpr float kBus = kRow1 + kRowH + 11.f, kLane = kRow1 + kRowH + 23.f;       // the two lanes in the gap between the rows
    constexpr float kCol[3] = { kLoopX + 86.f, kLoopX + 236.f, kLoopX + 386.f };      // the big-knob columns of Looper / Oscillator

    struct FrameDef { float x, y, w, h; const char* title; bool centred; };
    const FrameDef kFrames[] = {
        { kLoopX, kRow1, kLoopW, kRowH, "LOOPER", false },        { kLoopX, kRow2, kLoopW, kRowH, "OSCILLATOR", false },
        { kModX, kRow1, kModW, kRow2 + kRowH - kRow1, "MODULATION", true },
        { kFiltX, kRow1, kSecW, kRowH, "FILTER", false },         { kResX, kRow1, kSecW, kRowH, "RESONATOR", false },
        { kFiltX, kRow2, kSecW, kRowH, "ECHO", false },           { kResX, kRow2, kSecW, kRowH, "AMBIENCE", false },
        { kBotX[0], kBotY, kBotW[0], kBotH, "LOOPER RECORD", false }, { kBotX[1], kBotY, kBotW[1], kBotH, "SOURCE LEVELS", false },
        { kBotX[2], kBotY, kBotW[2], kBotH, "RANDOM", false },    { kBotX[3], kBotY, kBotW[3], kBotH, "EFFECT LEVELS", false },
        { kBotX[4], kBotY, kBotW[4], kBotH, "AUDIO IN", false } };

    const juce::Font& capsFont() { static const juce::Font f = font (15.0f, false, 2.2f); return f; }

    juce::Path run (std::initializer_list<juce::Point<float>> pts)
    {
        juce::Path p;
        std::vector<juce::Point<float>> v (pts);
        p.startNewSubPath (v[0]);
        for (size_t i = 1; i < v.size(); ++i) p.lineTo (v[i]);
        return p;
    }
    void stroke (juce::Graphics& g, const juce::Path& p, bool dotted)
    {
        g.setColour (gold());
        if (! dotted) { g.strokePath (p, juce::PathStrokeType (1.2f)); return; }
        juce::Path d;
        const float dash[2] = { 2.2f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (d, p, dash, 2);
        g.fillPath (d);
    }
    void node (juce::Graphics& g, float x, float y)
    {
        g.setColour (panel());
        g.fillEllipse (x - 2.8f, y - 2.8f, 5.6f, 5.6f);
        g.setColour (gold());
        g.drawEllipse (x - 2.8f, y - 2.8f, 5.6f, 5.6f, 1.1f);
    }

    juce::String noteName (int n)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
    }

    juce::StringArray wrapText (const juce::String& s, float size, float spacing, float room)
    {
        juce::StringArray words, lines;
        words.addTokens (s, " ", "");
        juce::String line;
        for (const auto& w : words)
        {
            const juce::String t = line.isEmpty() ? w : line + " " + w;
            if (line.isNotEmpty() && textWidth (t, size, false, spacing) > room) { lines.add (line); line = w; }
            else line = t;
        }
        if (line.isNotEmpty()) lines.add (line);
        return lines;
    }

    // live areas (panel units)
    const juce::Rectangle<int> kTempoArea { 0, 48, 330, 26 }, kNoteArea { 860, 48, 440, 26 }, kBarArea { 530, 644, 122, 34 }, kAudioArea { 1160, 776, 212, 186 };
    constexpr float kAudioRoom = 196.f;
}

// ------------------------------------------------------------------------------------------------ controls
// A rotary slider whose value can be moved onto a grid while it is dragged (the snap switches, 1.1.1).
struct SnapSlider : juce::Slider
{
    std::function<double (double)> snapper;
    double snapValue (double attempted, DragMode) override { return snapper ? snapper (attempted) : attempted; }
};

float AetherEditor::needWidth (const juce::Button& b)
{
    const juce::String style = b.getProperties()["style"].toString();
    const juce::String t = b.getButtonText().toUpperCase();
    if (style == "option") return 17.0f + textWidth (t, optionSize, false, optionSpacing) + 4.0f;
    if (style == "pill")   return textWidth (t, cellSize, false, cellSpacing) + 13.0f + 2.0f;
    if (style == "list")   return 20.0f + textWidth (t, cellSize, false, cellSpacing);
    if (style == "round")  return 20.0f + roundR + roundGap + textWidth (t, optionSize, false, roundSpacing) + 4.0f;
    if (style == "action") return textWidth (t, optionSize, false, actionSpacing) + 16.0f;
    return 0.0f;
}

struct AetherEditor::Look : juce::LookAndFeel_V4
{
    Look()
    {
        setColour (juce::BubbleComponent::backgroundColourId, menu());
        setColour (juce::BubbleComponent::outlineColourId, gold());
        setColour (juce::TooltipWindow::textColourId, text());
        setColour (juce::Label::textColourId, text());
    }
    juce::Font getSliderPopupFont (juce::Slider&) override { return font (14.0f); }
    int getSliderThumbRadius (juce::Slider&) override { return 8; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override
    {
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float u = juce::jmin (r.getWidth(), r.getHeight()) / 62.0f;     // one unit of the knob
        const auto c = r.getCentre();
        auto disc = [&] (float radius, juce::Colour fill, juce::Colour line, float lw)
        {
            g.setColour (fill);  g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (line);  g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, lw);
        };
        disc (22.0f * u, skirt(), skirtLine(), 1.2f * u);
        disc (16.0f * u, cap(), capLine(), 0.8f * u);
        const float a = start + pos * (end - start);
        const float sx = std::sin (a), sy = -std::cos (a);
        juce::Path p;
        p.startNewSubPath (c.x + 5.0f * u * sx, c.y + 5.0f * u * sy);
        p.lineTo (c.x + 15.0f * u * sx, c.y + 15.0f * u * sy);
        g.setColour (pointer());
        g.strokePath (p, juce::PathStrokeType (2.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // a fader: a gold hairline with end ticks and a cream cap
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cx = (float) x + (float) w * 0.5f, top = (float) y, bot = (float) (y + h);   // (LookAndFeel_V4 already leaves the thumb radius at both ends)
        g.setColour (gold());
        g.fillRect (cx - 0.65f, top, 1.3f, bot - top);
        g.fillRect (cx - 7.0f, top - 0.65f, 14.0f, 1.3f);
        g.fillRect (cx - 7.0f, bot - 0.65f, 14.0f, 1.3f);
        g.setColour (skirt());      g.fillRoundedRectangle (cx - 15.0f, pos - 8.0f, 30.0f, 16.0f, 4.0f);
        g.setColour (skirtLine());  g.drawRoundedRectangle (cx - 15.0f, pos - 8.0f, 30.0f, 16.0f, 4.0f, 1.2f);
        g.setColour (cap());        g.fillRoundedRectangle (cx - 12.0f, pos - 5.0f, 24.0f, 10.0f, 3.0f);
        g.setColour (capLine());    g.drawRoundedRectangle (cx - 12.0f, pos - 5.0f, 24.0f, 10.0f, 3.0f, 0.9f);
        g.setColour (pointer());    g.fillRoundedRectangle (cx - 0.9f, pos - 4.0f, 1.8f, 8.0f, 0.9f);
    }

    static void roundButton (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, bool lit)
    {
        const float cx = r.getX() + 20.0f, cy = r.getCentreY();
        if (lit)
        {
            g.setColour (lamp().withAlpha (0.18f));  g.fillEllipse (cx - 21.0f, cy - 21.0f, 42.0f, 42.0f);
            g.setColour (lampFill());                g.fillEllipse (cx - roundR, cy - roundR, roundR * 2.0f, roundR * 2.0f);
            g.setColour (lamp());                    g.drawEllipse (cx - roundR, cy - roundR, roundR * 2.0f, roundR * 2.0f, 1.5f);
        }
        else
        {
            g.setColour (skirt());  g.fillEllipse (cx - roundR, cy - roundR, roundR * 2.0f, roundR * 2.0f);
            g.setColour (gold());   g.drawEllipse (cx - roundR, cy - roundR, roundR * 2.0f, roundR * 2.0f, 1.2f);
        }
        textIn (g, label, font (optionSize, false, roundSpacing), cx + roundR + roundGap, cy + 5.0f, text(), juce::Justification::centredLeft);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        LogPause pause;
        roundButton (g, b.getLocalBounds().toFloat(), b.getButtonText().toUpperCase(), b.getToggleState());
    }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override {}
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        LogPause pause;
        const juce::String style = b.getProperties()["style"].toString();
        const juce::String t = b.getButtonText().toUpperCase();
        const auto r = b.getLocalBounds().toFloat();
        const float cy = r.getCentreY();
        const bool on = b.getToggleState();
        if (style == "option")
        {
            drawLamp (g, r.getX() + 8.0f, cy, on, 3.3f);
            textIn (g, t, font (optionSize, false, optionSpacing), r.getX() + 18.0f, cy + 5.0f, on ? text() : soft(), juce::Justification::centredLeft);
        }
        else if (style == "list")
        {
            drawLamp (g, r.getX() + 8.0f, cy, on, 3.3f);
            textIn (g, t, font (cellSize, false, cellSpacing), r.getX() + 20.0f, cy + 5.0f, on ? text() : muted(), juce::Justification::centredLeft);
        }
        else if (style == "pill")      // one cell of a pill: the chosen one has a lamp in front of its text
        {
            const auto f = font (cellSize, false, cellSpacing);
            const float cx = r.getCentreX();
            if (on)
            {
                const float gx = cx - (widthOf (t, f) + 13.0f) * 0.5f;
                drawLamp (g, gx + 3.0f, cy, true, 2.6f);
                textIn (g, t, f, gx + 13.0f, cy + 5.0f, text(), juce::Justification::centredLeft);
            }
            else
                textIn (g, t, f, cx, cy + 5.0f, muted(), juce::Justification::centred);
        }
        else if (style == "round")
            roundButton (g, r, t, down);
        else if (style == "action")
        {
            const auto rr = r.reduced (0.5f);
            if (down) { g.setColour (lampFill()); g.fillRoundedRectangle (rr, rr.getHeight() * 0.5f); }
            g.setColour (down ? lamp() : gold());
            g.drawRoundedRectangle (rr, rr.getHeight() * 0.5f, 1.1f);
            textIn (g, t, font (optionSize, false, actionSpacing), r.getCentreX(), cy + 5.0f, text(), juce::Justification::centred);
        }
    }
};

struct AetherEditor::Knob : juce::Component
{
    Knob (AetherProcessor& p, const char* id, const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
        // double click = back to the default value of the parameter
        if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        setHelpText (name);
    }
    void resized() override { slider.setBounds (getLocalBounds()); }
    SnapSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

struct AetherEditor::Fader : juce::Component
{
    Fader (AetherProcessor& p, const char* id)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
        if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// A pill (one row of choices in a gold outline) or a list (choices stacked, a lamp each) bound to a choice parameter.
struct AetherEditor::Segments : juce::Component
{
    Segments (AetherProcessor& p, const char* id, const juce::StringArray& names, Kind k) : kind (k)
    {
        static int nextGroup = 4000;
        const int group = nextGroup++;
        auto* prm = p.apvts.getParameter (id);
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (group);
            b->getProperties().set ("style", k == Kind::Pill ? "pill" : "list");
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
        if (kind != Kind::Pill) return;
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (gold());
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.1f);
        const float cw = r.getWidth() / (float) buttons.size();
        for (int i = 1; i < buttons.size(); ++i) g.fillRect (r.getX() + cw * (float) i - 0.5f, r.getY() + 1.0f, 1.0f, r.getHeight() - 2.0f);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int n = buttons.size();
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds (kind == Kind::List ? r.removeFromTop (r.getHeight() / (n - i)) : r.removeFromLeft (r.getWidth() / (n - i)));
    }
    Kind kind;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
};

struct AetherEditor::Panel : juce::Component
{
    explicit Panel (AetherEditor& o) : owner (o) {}
    void paintOverChildren (juce::Graphics& g) override { owner.paintLive (g); }
    AetherEditor& owner;
};
struct AetherEditor::Backdrop : juce::Component
{
    explicit Backdrop (AetherEditor& o) : owner (o) {}
    void paint (juce::Graphics& g) override { owner.paintBackdrop (g); }
    AetherEditor& owner;
};

// ------------------------------------------------------------------------------------------------ building the window
AetherEditor::Knob& AetherEditor::addKnob (const char* id, const juce::String& name, float cx, float cy, float size, bool bipolar, const juce::StringArray& lines)
{
    auto* k = new Knob (proc_, id, name);
    owned_.add (k); panel_->addAndMakeVisible (k);
    k->setBounds (juce::Rectangle<float> (cx - size * 0.5f, cy - size * 0.5f, size, size).toNearestInt());
    knobArt_.push_back ({ cx, cy, size, bipolar, lines.isEmpty() ? juce::StringArray (name.toUpperCase()) : lines });
    return *k;
}
void AetherEditor::addFader (const char* id, const juce::String& name, float cx, float top, float bottom)
{
    auto* f = new Fader (proc_, id);
    owned_.add (f); panel_->addAndMakeVisible (f);
    f->setBounds (juce::Rectangle<float> (cx - 20.0f, top - 8.0f, 40.0f, bottom - top + 16.0f).toNearestInt());
    faderArt_.push_back ({ cx, bottom, name });
}
AetherEditor::Segments& AetherEditor::addSegments (const char* id, const juce::StringArray& names, Kind kind, float x, float y, float w, float h)
{
    auto* s = new Segments (proc_, id, names, kind);
    owned_.add (s); panel_->addAndMakeVisible (s);
    s->setBounds (juce::Rectangle<float> (x, y, w, h).toNearestInt());
    return *s;
}
juce::TextButton& AetherEditor::addSwitch (const char* id, const juce::String& text, float cx, float cy, float minLeft)
{
    auto* b = new juce::TextButton (text);
    b->setClickingTogglesState (true);
    b->getProperties().set ("style", "option");
    b->setHelpText (text);
    owned_.add (b); panel_->addAndMakeVisible (b);
    const float w = std::ceil (needWidth (*b)) + 2.0f;
    cx = std::max (cx, minLeft + w * 0.5f);      // keeps a wide label off the frame's left edge
    b->setBounds (juce::Rectangle<float> (cx - w * 0.5f, cy - 11.0f, w, 22.0f).toNearestInt());
    switchAtts_.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc_.apvts, id, *b));
    return *b;
}
void AetherEditor::caption (const juce::String& s, float x, float y, float spacing) { captions_.push_back ({ s, x, y, spacing }); }

// Ties a snap switch to its knob: while the switch is on the knob clicks from step to step and its read-out names the step; turning the
// switch on moves the knob onto the nearest step at once.
static void bindSnap (juce::Button& sw, SnapSlider& sl, std::function<float (float)> snapTo, std::function<juce::String (float)> text)
{
    auto* swp = &sw; auto* slp = &sl;
    sl.snapper = [swp, snapTo] (double v) { return swp->getToggleState() ? (double) snapTo ((float) v) : v; };
    sl.textFromValueFunction = [swp, text] (double v)
    {
        if (swp->getToggleState()) return text ((float) v);
        return juce::String (v, 2);
    };
    sw.onClick = [swp, slp, snapTo] { if (swp->getToggleState()) slp->setValue ((double) snapTo ((float) slp->getValue()), juce::sendNotificationSync); };
}
static float gridSamples (AetherProcessor& p)
{
    const double bpm = p.hostBpm();
    return bpm > 0.0 ? (float) (60.0 * 48000.0 / std::min (300.0, std::max (40.0, bpm)) / 4.0) : 0.f;
}

AetherEditor::AetherEditor (AetherProcessor& p) : AudioProcessorEditor (&p), proc_ (p), look_ (new Look)
{
    setLookAndFeel (look_.get());
    panel_.reset (new Panel (*this));
    backdrop_.reset (new Backdrop (*this));
    panel_->setLookAndFeel (look_.get());
    addAndMakeVisible (*panel_);
    panel_->setBounds (0, 0, panelW, panelH);
    backdrop_->setBounds (0, 0, panelW, panelH);
    backdrop_->setInterceptsMouseClicks (false, false);
    backdrop_->setBufferedToImage (true);
    panel_->addAndMakeVisible (*backdrop_);

    constrainer_.setFixedAspectRatio (1.4);
    constrainer_.setSizeLimits (700, 500, 2100, 1500);
    setConstrainer (&constrainer_);
    setResizable (true, true);

    addKnob ("output_level", "Out", 1344, 38, 48);

    // Looper
    auto& swSpeed = addSwitch ("snap_looper_speed", "Speed snap", kCol[0], kRow1 + 26);
    auto& swStart = addSwitch ("snap_looper_start", "Start snap", kCol[1], kRow1 + 26);
    auto& swLength = addSwitch ("snap_looper_length", "Length snap", kCol[2], kRow1 + 26);
    const float lbig = kRow1 + 96, lsmall = kRow1 + 214;
    auto& knSpeed = addKnob ("looper_speed", "Speed", kCol[0], lbig, 104);
    auto& knStart = addKnob ("looper_start", "Start", kCol[1], lbig, 104);
    auto& knLength = addKnob ("looper_length", "Length", kCol[2], lbig, 104);
    addKnob ("looper_speed_mod", "MOD", kCol[0], lsmall, 66);
    addKnob ("looper_sos", "Sound on sound", kCol[0] + 75, lsmall, 66, false, { "SOUND ON", "SOUND" });
    addKnob ("looper_start_mod", "MOD", kCol[1], lsmall, 66);
    addKnob ("looper_filter", "Filter", kCol[1] + 75, lsmall, 66, true);
    addKnob ("looper_length_mod", "MOD", kCol[2], lsmall, 66);

    // Oscillator
    auto& swPitch = addSwitch ("snap_osc_pitch", "Pitch snap", kCol[0], kRow2 + 26);
    auto& knPitch = addKnob ("osc_pitch", "Pitch", kCol[0], kRow2 + 96, 104, true);
    addKnob ("osc_detune", "Detune", kCol[1], kRow2 + 96, 104);
    addKnob ("osc_pitch_mod", "MOD", kCol[0], kRow2 + 214, 66);
    addKnob ("osc_octave", "Octave", kCol[0] + 75, kRow2 + 214, 66);
    addKnob ("osc_detune_mod", "MOD", kCol[1], kRow2 + 214, 66);
    addKnob ("osc_unison", "Unison", kCol[1] + 75, kRow2 + 214, 66, true);
    caption ("SECOND", kLoopX + 332, kRow2 + 74);
    caption ("OSCILLATOR", kLoopX + 332, kRow2 + 91);
    addSegments ("osc_kind", { "Supersaw", "Wavetable" }, Kind::List, kLoopX + 332, kRow2 + 106, 134, 52);

    // Modulation
    const float mcx = kModX + kModW * 0.5f;
    addKnob ("mod_level", "Level", mcx, kRow1 + 100, 84);
    addKnob ("mod_speed", "Speed", mcx, kRow1 + 250, 84);
    addKnob ("mod_type", "Type", mcx, kRow1 + 400, 84);

    // Filter, Resonator, Echo, Ambience (three columns in each frame)
    auto cols = [] (float x) { return std::array<float, 3> { x + 60.f, x + 166.f, x + 272.f }; };
    const auto cF = cols (kFiltX), cR = cols (kResX);
    auto& swCutoff = addSwitch ("snap_filter_cutoff", "Cutoff snap", cF[0], kRow1 + 26, kFiltX + 18.0f);
    auto& knCutoff = addKnob ("filter_cutoff", "Cutoff", cF[0], kRow1 + 80, 80);
    addKnob ("filter_resonance", "Resonance", cF[1], kRow1 + 80, 80);
    addKnob ("filter_noise", "Noise", cF[2], kRow1 + 80, 80);
    addKnob ("filter_cutoff_mod", "MOD", cF[0], kRow1 + 184, 70);
    addKnob ("filter_resonance_mod", "MOD", cF[1], kRow1 + 184, 70);
    caption ("MODE", kFiltX + 22, kRow1 + 260);
    caption ("POSITION", kFiltX + 182, kRow1 + 260);
    addSegments ("filter_mode", { "LP", "BP", "HP", "CF" }, Kind::Pill, kFiltX + 20, kRow1 + 266, 148, 24);
    addSegments ("filter_position", { "I", "II", "III", "IV" }, Kind::Pill, kFiltX + 180, kRow1 + 266, 133, 24);

    auto& swTune = addSwitch ("snap_resonator_tune", "Tune snap", cR[0], kRow1 + 26);
    auto& knTune = addKnob ("resonator_tune", "Tune", cR[0], kRow1 + 80, 80);
    addKnob ("resonator_feedback", "Feedback", cR[1], kRow1 + 80, 80);
    addKnob ("resonator_dissonance", "Dissonance", cR[2], kRow1 + 80, 80);
    addKnob ("resonator_tune_mod", "MOD", cR[0], kRow1 + 184, 70);
    addKnob ("resonator_feedback_mod", "MOD", cR[1], kRow1 + 184, 70);

    addSwitch ("echo_sync", "Echo Sync", cF[0], kRow2 + 26, kFiltX + 18.0f);
    addKnob ("echo_density", "Density", cF[0], kRow2 + 80, 80);
    addKnob ("echo_repeats", "Repeats", cF[1], kRow2 + 80, 80);
    addKnob ("echo_filter", "Filter", cF[2], kRow2 + 80, 80, true);
    addKnob ("echo_density_mod", "MOD", cF[0], kRow2 + 184, 70);
    addKnob ("echo_repeats_mod", "MOD", cF[1], kRow2 + 184, 70);

    addKnob ("ambience_spacetime", "Spacetime", cR[0], kRow2 + 80, 80);
    addKnob ("ambience_decay", "Decay", cR[1], kRow2 + 80, 80);
    addKnob ("ambience_autopan", "Auto-pan", cR[2], kRow2 + 80, 80);
    addKnob ("ambience_spacetime_mod", "MOD", cR[0], kRow2 + 184, 70);
    addKnob ("ambience_decay_mod", "MOD", cR[1], kRow2 + 184, 70);

    // The snap switches make their knobs click from step to step (1.1.1); the engine snaps the value as well (automation, Mod, saved projects).
    bindSnap (swPitch, knPitch.slider, [] (float v) { return aether::snap::pitchKnob (v); }, [] (float v) { return juce::String (aether::snap::pitchText (v)); });
    bindSnap (swSpeed, knSpeed.slider, [] (float v) { return aether::snap::speedKnob (v); }, [] (float v) { return juce::String (aether::snap::speedText (v)); });
    bindSnap (swCutoff, knCutoff.slider, [] (float v) { return aether::snap::cutoffKnob (v); }, [] (float v) { return juce::String (aether::snap::cutoffText (v)); });
    auto dissonance = [this] { return proc_.apvts.getRawParameterValue ("resonator_dissonance")->load(); };
    bindSnap (swTune, knTune.slider, [dissonance] (float v) { return aether::snap::tuneKnob (v, dissonance()); }, [dissonance] (float v) { return juce::String (aether::snap::tuneText (v, dissonance())); });
    bindSnap (swStart, knStart.slider, [this] (float v) { return aether::snap::startKnob (v, gridSamples (proc_)); },
              [this] (float v) { return juce::String (aether::snap::sixteenthsText (aether::snap::startSamples (aether::snap::startKnob (v, gridSamples (proc_))), gridSamples (proc_))); });
    bindSnap (swLength, knLength.slider, [this] (float v) { return aether::snap::lengthKnob (v, gridSamples (proc_)); },
              [this] (float v) { return juce::String (aether::snap::sixteenthsText (aether::snap::lengthSamples (aether::snap::lengthKnob (v, gridSamples (proc_))), gridSamples (proc_))); });

    // Bottom row
    caption ("PRE = INPUT,", kBotX[0] + 18, kBotY + 40, 1.3f);
    caption ("POST = OUTPUT", kBotX[0] + 18, kBotY + 57, 1.3f);
    addSegments ("looper_resampling", { "Pre", "Post" }, Kind::Pill, kBotX[0] + 18, kBotY + 66, kBotW[0] - 36, 24);
    recordButton_.getProperties().set ("style", "round");
    panel_->addAndMakeVisible (recordButton_);
    recordButton_.setBounds (kBotX[0] + 16, kBotY + 117, 140, 34);
    recordAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "looper_recording", recordButton_);
    clearButton_.getProperties().set ("style", "round");
    panel_->addAndMakeVisible (clearButton_);
    clearButton_.setBounds (kBotX[0] + 16, kBotY + 169, 140, 34);
    clearButton_.onClick = [this] { proc_.requestClearLooper(); };

    const char* f1[][2] = { { "looper_volume", "Looper" }, { "osc1_volume", "Sine" }, { "osc2_volume", "SS / WT" }, { "input_volume", "Input" } };
    for (int i = 0; i < 4; ++i) addFader (f1[i][0], f1[i][1], kBotX[1] + 37.f + 74.f * (float) i, kBotY + 40, kBotY + 170);

    caption ("WHAT", kBotX[2] + 20, kBotY + 40);
    caption ("AMOUNT", kBotX[2] + 106, kBotY + 40);
    addSegments ("random_mode", { "All", "Osc", "Loop", "FX" }, Kind::List, kBotX[2] + 20, kBotY + 50, 80, 96);
    addSegments ("random_amount", { "Low", "Mid", "High" }, Kind::List, kBotX[2] + 106, kBotY + 50, 80, 72);
    for (auto* b : { &randomiseButton_, &undoButton_ }) { b->getProperties().set ("style", "action"); panel_->addAndMakeVisible (*b); }
    randomiseButton_.setBounds (juce::Rectangle<float> (kBotX[2] + 18, kBotY + 156, kBotW[2] - 36, 24).toNearestInt());
    undoButton_.setBounds (juce::Rectangle<float> (kBotX[2] + 18, kBotY + 188, kBotW[2] - 36, 24).toNearestInt());
    randomiseButton_.onClick = [this] { proc_.randomise(); };
    undoButton_.onClick = [this] { proc_.undoRedo(); };

    const char* f2[][2] = { { "filter_level", "Filter" }, { "resonator_volume", "Resonator" }, { "echo_volume", "Echo" }, { "ambience_volume", "Ambience" } };
    for (int i = 0; i < 4; ++i) addFader (f2[i][0], f2[i][1], kBotX[3] + 46.f + 88.f * (float) i, kBotY + 40, kBotY + 170);

    timerCallback();
    startTimerHz (30);
    setSize (1260, 900);
}

AetherEditor::~AetherEditor()
{
    stopTimer();
    panel_->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::Component& AetherEditor::getBackdrop() { return *backdrop_; }
juce::Array<juce::Rectangle<int>> AetherEditor::liveAreas() { return { kTempoArea, kNoteArea, kBarArea, kAudioArea }; }
std::vector<juce::Rectangle<float>> AetherEditor::frameRects()
{
    std::vector<juce::Rectangle<float>> v;
    for (const auto& f : kFrames) v.push_back ({ f.x, f.y, f.w, f.h });
    return v;
}

// ------------------------------------------------------------------------------------------------ fixed artwork
void AetherEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (panel());

    // Header: the title between two gold ornament lines
    textIn (g, "AETHERUMBRA", titleFont(), 700.0f + titleSpacing * 0.5f, titleBaseline, goldText(), juce::Justification::centred);
    g.setColour (gold());
    g.fillRect (272.0f, 41.0f, ornamentLeftEnd - 272.0f, 1.3f);
    g.fillRect (ornamentRightStart, 41.0f, 1128.0f - ornamentRightStart, 1.3f);
    g.fillEllipse (268.4f, 39.0f, 5.2f, 5.2f);
    g.fillEllipse (1126.4f, 39.0f, 5.2f, 5.2f);
    textAt (g, "PARTICLEDOTS", 22.0f, 34.0f, muted(), 14.0f, juce::Justification::centredLeft, false, 2.5f);

    // Section frames, each with its title set into the top line
    for (const auto& f : kFrames)
    {
        g.setColour (gold());
        g.drawRoundedRectangle (f.x + 0.5f, f.y + 0.5f, f.w, f.h, 12.0f, 1.3f);
        const juce::String t (f.title);
        const float tw = widthOf (t, capsFont());
        const float tx = f.centred ? f.x + f.w * 0.5f - tw * 0.5f : f.x + 20.0f;
        g.setColour (panel());
        g.fillRect (tx - 7.0f, f.y - 6.0f, tw + 14.0f, 12.0f);
        textIn (g, t, capsFont(), tx, f.y + 5.0f, goldText(), juce::Justification::centredLeft);
    }

    // Knob range arcs (7:30 round to 4:30, a dot at each end, one at the top for knobs whose middle means "off") and labels
    for (const auto& k : knobArt_)
    {
        const float R = k.size * 0.5f;
        juce::Path arc;
        arc.addCentredArc (k.cx, k.cy, R, R, 0.0f, juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        g.setColour (gold());
        g.strokePath (arc, juce::PathStrokeType (1.3f));
        const float d = R * 0.70710678f;
        g.fillEllipse (k.cx - d - 2.1f, k.cy + d - 2.1f, 4.2f, 4.2f);
        g.fillEllipse (k.cx + d - 2.1f, k.cy + d - 2.1f, 4.2f, 4.2f);
        if (k.bipolar) g.fillEllipse (k.cx - 2.1f, k.cy - R - 2.1f, 4.2f, 4.2f);
        for (int i = 0; i < k.lines.size(); ++i)
            textAt (g, k.lines[i], k.cx, k.cy + R + 22.0f + 16.0f * (float) i, text(), 15.0f, juce::Justification::centred, false, k.lines.size() > 1 ? 1.3f : 1.6f);
    }
    for (const auto& f : faderArt_)
        textAt (g, f.name.toUpperCase(), f.cx, f.bottom + 30.0f, text(), 14.0f, juce::Justification::centred, false, 1.4f);
    for (const auto& c : captions_)
        textAt (g, c.s, c.x, c.y, goldText(), 14.0f, juce::Justification::centredLeft, false, c.spacing);
    textAt (g, "MOD OUT", kModX + kModW * 0.5f, kRow2 + 262.0f, muted(), 14.0f, juce::Justification::centred, false, 1.5f);
    textAt (g, "Unofficial. Based on the open-source Befaco Oneiroi firmware 1.2.2 (GPL-3.0). Not made, approved or supported by Befaco.  Version "
                   + AetherProcessor::versionString(), 700.0f, 992.0f, muted(), 13.0f, juce::Justification::centred, false, 0.2f);

    // Flow lines. Dotted: Modulation feeds the MOD knobs of six sections. Solid: the sound goes Resonator, Echo, Ambience
    // (the firmware's order; the Filter has no line because its place depends on its Position switch). It hops over the dotted line.
    const float ls = kCol[0] + 75.0f, bottomEdge = kRow1 + kRowH;
    stroke (g, run ({ { ls, kBus }, { kModX, kBus } }), true);
    stroke (g, run ({ { kModX + kModW, kBus }, { 1300.0f, kBus } }), true);
    node (g, kModX, kBus); node (g, kModX + kModW, kBus);
    const float stems[6][2] = { { ls, bottomEdge }, { ls, kRow2 }, { 844.0f, bottomEdge }, { 780.0f, kRow2 }, { 1160.0f, bottomEdge }, { 1300.0f, kRow2 } };
    for (const auto& s : stems) { stroke (g, run ({ { s[0], kBus }, { s[0], s[1] } }), true); node (g, s[0], s[1]); }
    const float axOut = 1100.0f, axIn = 820.0f;
    g.setColour (panel());
    g.fillRect (axOut - 5.0f, kBus - 2.0f, 10.0f, 4.0f);                                  // the hop clears the dotted line under it
    stroke (g, run ({ { axOut, bottomEdge }, { axOut, kBus - 9.0f } }), false);
    juce::Path hop;
    hop.startNewSubPath (axOut, kBus - 9.0f);
    hop.quadraticTo (axOut + 7.0f, kBus, axOut, kBus + 9.0f);
    stroke (g, hop, false);
    stroke (g, run ({ { axOut, kBus + 9.0f }, { axOut, kLane }, { axIn, kLane }, { axIn, kRow2 } }), false);
    node (g, axOut, bottomEdge); node (g, axIn, kRow2);
    stroke (g, run ({ { kFiltX + kSecW, kRow2 + 150.0f }, { kResX, kRow2 + 150.0f } }), false);
    node (g, kFiltX + kSecW, kRow2 + 150.0f); node (g, kResX, kRow2 + 150.0f);
}

// ------------------------------------------------------------------------------------------------ live drawing
void AetherEditor::paintLive (juce::Graphics& g)
{
    // Only what lies in the area being redrawn is drawn (laying out text is the expensive part on a Mac).
    const auto clip = g.getClipBounds();
    if (clip.intersects (kTempoArea))
        textAt (g, tempoText_, 22.0f, 68.0f, muted(), 15.0f, juce::Justification::centredLeft, false, 0.3f);
    if (clip.intersects (kNoteArea))
        textAt (g, noteText_, 1290.0f, 68.0f, text(), 15.0f, juce::Justification::centredRight, false, 0.3f);

    // The modulation source: a bar that swings left and right with the current modulation value (-1 .. 1).
    if (clip.intersects (kBarArea))
    {
        const auto r = juce::Rectangle<float> (kModX + 20.0f, kRow2 + 220.0f, kModW - 40.0f, 22.0f);
        g.setColour (skirt());  g.fillRoundedRectangle (r, 6.0f);
        g.setColour (gold());   g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.1f);
        const float mid = r.getCentreX(), half = r.getWidth() * 0.5f - 3.0f, v = juce::jlimit (-1.f, 1.f, modShown_);
        g.setColour (cap().withAlpha (0.8f));
        g.fillRoundedRectangle (juce::jmin (mid, mid + v * half), r.getY() + 4.0f, std::abs (v * half) + 1.0f, r.getHeight() - 8.0f, 3.0f);
        g.setColour (gold());   g.fillRect (mid - 0.6f, r.getY() + 3.0f, 1.2f, r.getHeight() - 6.0f);
    }

    for (int i = 0; i < audioLines_.size() && clip.intersects (kAudioArea); ++i)
    {
        const juce::String l = audioLines_[i];
        if (l.isEmpty()) continue;
        textAt (g, l, kBotX[4] + 18.0f, kBotY + 42.0f + 17.0f * (float) i, i < audioNumLines_ ? muted() : text(), 13.0f, juce::Justification::centredLeft, false, 0.2f);
    }
}

void AetherEditor::timerCallback()
{
    // the modulation bar moves smoothly (30 times a second); the texts are refreshed 10 times a second and redrawn only when they change
    modShown_ = proc_.modulationValue();
    if (panel_ != nullptr) panel_->repaint (kBarArea);
    if (tick_++ % 3 != 0) return;

    const int n = proc_.lastNote();
    const juce::String note = n < 0 ? juce::String ("Pitch from MIDI notes: no note played yet (plays C4)")
                                    : "Pitch from MIDI notes: last note " + noteName (n) + " (" + juce::String (n) + ")";
    const double bpm = proc_.hostBpm();
    juce::String tempo;
    if (bpm <= 0.0) tempo = "Tempo: none from Logic, own clock";
    else
    {
        tempo = "Tempo: " + juce::String (bpm, 1) + " BPM from Logic";
        if (bpm < 40.0 || bpm > 300.0) tempo << " (used as " << juce::String (juce::jlimit (40.0, 300.0, bpm), 0) << ")";
    }
    const int ch = proc_.inputChannels();
    const float peak = proc_.takeInputPeak();
    const juce::String level = ch == 0 ? juce::String ("No audio input connected.")
                                       : juce::String (ch) + " channel(s) in, level " + (peak > 1.0e-5f ? juce::String (20.f * std::log10 (peak), 1) + " dB" : juce::String ("silent"));
    juce::StringArray lines = wrapText ("In Logic, choose the audio track you want to process in this plugin's Side Chain menu.", 13.0f, 0.2f, kAudioRoom);
    lines.add (juce::String());
    lines.addArray (wrapText ("Muting the source track does not stop the side chain.", 13.0f, 0.2f, kAudioRoom));
    lines.add (juce::String());
    const int firstTwo = lines.size();
    lines.addArray (wrapText (level, 13.0f, 0.2f, kAudioRoom));

    if (tempo != tempoText_) { tempoText_ = tempo; panel_->repaint (kTempoArea); }
    if (note != noteText_)   { noteText_ = note;   panel_->repaint (kNoteArea); }
    if (lines != audioLines_) { audioLines_ = lines; audioNumLines_ = firstTwo; panel_->repaint (kAudioArea); }
}

void AetherEditor::paint (juce::Graphics& g) { g.fillAll (panel()); }

void AetherEditor::resized()
{
    panel_->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) panelW));
}

juce::Image AetherEditor::renderToImage() { return createComponentSnapshot (getLocalBounds(), true, 1.0f); }

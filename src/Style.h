// Colours, fonts and text helpers of the AetherUmbra window ("gold sections" look, taken from the Anamnesis 0.3.1 panel):
// black panel, gold hairline frames and flow lines, cream knob caps, blue lamps as the only colour (on / chosen).
// The window is designed at 1400 x 1000 units and scaled to its size.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace AetherStyle
{
    inline juce::Colour panel()     { return juce::Colour (0xFF0B0B0C); }
    inline juce::Colour gold()      { return juce::Colour (0xFFB08D57); }   // frames, lines, outlines
    inline juce::Colour goldText()  { return juce::Colour (0xFFD4B27A); }   // title, section and row titles
    inline juce::Colour text()      { return juce::Colour (0xFFE8E1D0); }   // labels, chosen settings
    inline juce::Colour soft()      { return juce::Colour (0xFFB5AC98); }   // labels of switches that are off
    inline juce::Colour muted()     { return juce::Colour (0xFFA39886); }   // values, choices not chosen
    inline juce::Colour lamp()      { return juce::Colour (0xFF4C9BFF); }   // lit lamps
    inline juce::Colour lampFill()  { return juce::Colour (0xFF1B3A66); }   // a lit round button
    inline juce::Colour skirt()     { return juce::Colour (0xFF121212); }   // knob skirts, round buttons
    inline juce::Colour skirtLine() { return juce::Colour (0xFF2E2A22); }
    inline juce::Colour cap()       { return juce::Colour (0xFFE9E2D2); }   // knob caps, fader caps
    inline juce::Colour capLine()   { return juce::Colour (0xFFBFB6A2); }
    inline juce::Colour pointer()   { return juce::Colour (0xFF161514); }
    inline juce::Colour menu()      { return juce::Colour (0xFF16161A); }   // pop-ups

    inline constexpr int panelW = 1400, panelH = 1000;

    // The title is centred at x = 700 and must stay between these x positions at every window size (inside the two gold
    // ornament lines; checked in the drawn pixels by the plugin test).
    inline constexpr float titleLeft = 450.0f, titleRight = 950.0f, titleSize = 40.0f, titleSpacing = 13.0f, titleBaseline = 56.0f;
    inline constexpr float ornamentLeftEnd = 438.0f, ornamentRightStart = 962.0f;

    // Space Grotesk and Marcellus are built in, so every Mac and the test measure the same.
    juce::Font font (float size, bool medium = false, float spacing = 0.0f);
    juce::Font titleFont();

    inline float widthOf (const juce::String& s, const juce::Font& f)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }
    inline float textWidth (const juce::String& s, float size, bool medium = false, float spacing = 0.0f)
    {
        return widthOf (s, font (size, medium, spacing));
    }

    // For the plugin test: every text drawn by the panel itself (not by a button) is noted here (panel units).
    struct LoggedText { juce::String s; juce::Rectangle<float> r; };
    struct LogPause    // buttons draw in their own coordinates: their texts are not noted
    {
        bool was;
        LogPause();
        ~LogPause();
    };
    inline bool& logOn() { static bool on = false; return on; }
    inline std::vector<LoggedText>& textLog() { static std::vector<LoggedText> v; return v; }
    inline LogPause::LogPause() : was (logOn()) { logOn() = false; }
    inline LogPause::~LogPause() { logOn() = was; }

    // y is the baseline. Left / centred / right of x.
    inline void textIn (juce::Graphics& g, const juce::String& s, const juce::Font& f, float x, float y, juce::Colour c,
                        juce::Justification just)
    {
        g.setColour (c);
        g.setFont (f);
        const float asc = f.getAscent(), h = f.getHeight();
        const float w = juce::jmax (600.0f, widthOf (s, f) + 40.0f);
        juce::Rectangle<float> r;
        if (just == juce::Justification::centredLeft)       r = { x, y - asc, w, h };
        else if (just == juce::Justification::centredRight) r = { x - w, y - asc, w, h };
        else                                                 r = { x - w * 0.5f, y - asc, w, h };
        g.drawText (s, r, just, false);
        if (logOn())
        {
            const float tw = widthOf (s, f), size = asc * 0.75f;
            float x0 = just == juce::Justification::centredLeft ? x : (just == juce::Justification::centredRight ? x - tw : x - tw * 0.5f);
            textLog().push_back ({ s, { x0, y - size, tw, size } });
        }
    }
    inline void textAt (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size = 14.0f,
                        juce::Justification just = juce::Justification::centred, bool medium = false, float spacing = 0.0f)
    {
        textIn (g, s, font (size, medium, spacing), x, y, c, just);
    }

    inline void drawLamp (juce::Graphics& g, float cx, float cy, bool lit, float r = 3.3f)
    {
        if (lit)
        {
            g.setColour (lamp().withAlpha (0.22f));
            g.fillEllipse (cx - r * 2.2f, cy - r * 2.2f, r * 4.4f, r * 4.4f);
            g.setColour (lamp());
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            g.setColour (muted());
            g.drawEllipse (cx - r + 0.3f, cy - r + 0.3f, r * 2.0f - 0.6f, r * 2.0f - 0.6f, 1.0f);
        }
    }

    // Sizes of the texts on controls (also used by the plugin test to check that every text fits its control)
    inline constexpr float optionSize = 14.0f, optionSpacing = 1.3f;
    inline constexpr float cellSize = 14.0f, cellSpacing = 1.2f;
    inline constexpr float roundR = 14.0f, roundGap = 10.0f, roundSpacing = 1.6f;
    inline constexpr float actionSpacing = 1.4f;
}

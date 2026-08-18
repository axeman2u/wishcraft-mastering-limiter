#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel/StudioConsolePainter.h"
#include "LookAndFeel/StudioConsoleTheme.h"

// Vertical meter matching the JSFX's draw_vmeter(): a live-fill bar plus a thin held-
// peak marker line. invert=false fills from the BOTTOM up (Output/Peak's convention --
// its "0" end IS the loud/extreme end, at the top, filling up toward it). invert=true
// fills from the TOP DOWN (GR/Selective Clip Activity's convention -- their "0" is the
// neutral/best-case reading, sitting at top as a reference the fill "hangs down" from).
// Fill brightens toward whichever end is the loud/extreme end, for the same "lit"
// quality the rest of the Studio Console finish uses.
class WishcraftVMeter : public juce::Component
{
public:
    WishcraftVMeter (juce::Colour fillColourIn, bool invertIn)
        : fillColour (fillColourIn), invert (invertIn) {}

    void setFractions (float liveFracIn, float holdFracIn)
    {
        const float newLive = juce::jlimit (0.0f, 1.0f, liveFracIn);
        const float newHold = juce::jlimit (0.0f, 1.0f, holdFracIn);
        if (! juce::exactlyEqual (newLive, liveFrac) || ! juce::exactlyEqual (newHold, holdFrac))
        {
            liveFrac = newLive;
            holdFrac = newHold;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (StudioConsoleTheme::meterBackground);
        g.fillRect (bounds);

        const float fillH = liveFrac * bounds.getHeight();
        float fillY, holdY;
        if (invert)
        {
            fillY = bounds.getY();
            holdY = bounds.getY() + holdFrac * bounds.getHeight();
        }
        else
        {
            fillY = bounds.getBottom() - fillH;
            holdY = bounds.getBottom() - holdFrac * bounds.getHeight();
        }

        // Brightness increases toward the fill's LEADING edge -- the end furthest from
        // its resting/zero position, which is what's actually interesting to see lit
        // up. For a bottom-up bar (invert=false) that's its top; for a top-down bar
        // hanging from a zero reference (invert=true) that's its bottom.
        StudioConsolePainter::paintMeterFill (g, { bounds.getX(), fillY, bounds.getWidth(), fillH }, fillColour, ! invert);

        g.setColour (StudioConsoleTheme::meterBorder);
        g.drawRect (bounds, 1.0f);

        g.setColour (StudioConsoleTheme::meterHoldMarker);
        g.fillRect (juce::Rectangle<float> (bounds.getX(), holdY - 1.0f, bounds.getWidth(), 2.0f));
    }

private:
    juce::Colour fillColour;
    bool invert;
    float liveFrac = 0.0f, holdFrac = 0.0f;
};

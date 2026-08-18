#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel/StudioConsolePainter.h"
#include "LookAndFeel/StudioConsoleTheme.h"

// Dynamic Range span indicator, matching the JSFX's draw_range_bar(): a single narrow
// bar showing the min-to-max span, no live/current marker -- this widget's whole
// purpose is the SPAN, not an instantaneous reading.
class WishcraftRangeBar : public juce::Component
{
public:
    void setFractions (float loFracIn, float hiFracIn)
    {
        const float newLo = juce::jlimit (0.0f, 1.0f, loFracIn);
        const float newHi = juce::jlimit (0.0f, 1.0f, hiFracIn);
        if (! juce::exactlyEqual (newLo, loFrac) || ! juce::exactlyEqual (newHi, hiFrac))
        {
            loFrac = newLo;
            hiFrac = newHi;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (StudioConsoleTheme::meterBackground);
        g.fillRect (bounds);

        const float yHi = bounds.getBottom() - hiFrac * bounds.getHeight();
        const float yLo = bounds.getBottom() - loFrac * bounds.getHeight();
        if (yLo > yHi)
            StudioConsolePainter::paintMeterFill (g, { bounds.getX(), yHi, bounds.getWidth(), yLo - yHi }, StudioConsoleTheme::rangeBar, true);

        g.setColour (StudioConsoleTheme::meterBorder);
        g.drawRect (bounds, 1.0f);
    }

private:
    float loFrac = 0.0f, hiFrac = 0.0f;
};

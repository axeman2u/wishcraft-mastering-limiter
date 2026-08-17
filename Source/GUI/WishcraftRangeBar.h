#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

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
        g.setColour (WishcraftColours::meterBackground);
        g.fillRect (bounds);
        g.setColour (WishcraftColours::meterBorder);
        g.drawRect (bounds, 1.0f);

        const float yHi = bounds.getBottom() - hiFrac * bounds.getHeight();
        const float yLo = bounds.getBottom() - loFrac * bounds.getHeight();
        if (yLo > yHi)
        {
            g.setColour (WishcraftColours::rangeBar);
            g.fillRect (juce::Rectangle<float> (bounds.getX(), yHi, bounds.getWidth(), yLo - yHi));
        }
    }

private:
    float loFrac = 0.0f, hiFrac = 0.0f;
};

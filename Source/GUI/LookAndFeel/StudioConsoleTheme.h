#pragma once

#include <juce_graphics/juce_graphics.h>

// Portable colour/gradient tokens for the "Studio Console" finish (soft gradients,
// layered shadow/highlight depth, glow on active elements -- approved design direction
// for the Wishcraft Mastering Limiter GUI restyle). Deliberately generic: no plugin-
// specific naming, no dependency on anything else in this project, so this whole
// LookAndFeel/ folder can be copied wholesale into a future JUCE project.
//
// Two kinds of entries here:
//  - Flat colours, named after what they're for (same naming convention the old
//    per-plugin WishcraftColours.h used).
//  - Gradient FACTORY functions (not fixed gradients), since a JUCE ColourGradient is
//    defined in terms of concrete start/end points -- it has to be built against the
//    actual bounds of whatever it's being applied to, not stored as a constant.
namespace StudioConsoleTheme
{
    inline juce::Colour fromHex (juce::uint32 rgb, float alpha = 1.0f)
    {
        return juce::Colour ((juce::uint8) ((rgb >> 16) & 0xff), (juce::uint8) ((rgb >> 8) & 0xff),
                              (juce::uint8) (rgb & 0xff)).withAlpha (alpha);
    }

    //==============================================================================
    // Flat tokens
    static const juce::Colour background        = fromHex (0x0d1117);
    static const juce::Colour backgroundLight    = fromHex (0x141a22);
    static const juce::Colour titleText          = fromHex (0xffffff);

    static const juce::Colour groupBackgroundTop = fromHex (0x171e28);
    static const juce::Colour groupBackgroundBot = fromHex (0x121821);
    static const juce::Colour groupBorder        = fromHex (0x26374a);
    static const juce::Colour groupLabel         = fromHex (0x6f9dc4);

    static const juce::Colour controlLabel       = fromHex (0x9fb0c2);
    static const juce::Colour controlValueText   = fromHex (0xdce6ee);
    static const juce::Colour controlEndLabel    = fromHex (0x8393a3);

    static const juce::Colour knobHighlight      = fromHex (0x869bb1); // bright hotspot, in-family with knobShadowEdge
    static const juce::Colour knobMid            = fromHex (0x3c4d60);
    static const juce::Colour knobShadowEdge     = fromHex (0x12161d);
    static const juce::Colour knobBorder         = fromHex (0x0a0d12);
    static const juce::Colour knobNeedle         = fromHex (0x3fa9ff);

    static const juce::Colour trackBackground    = fromHex (0x10151c);
    static const juce::Colour trackBorder        = fromHex (0x232c37);
    static const juce::Colour sliderCenterTick   = fromHex (0x2a323d);
    static const juce::Colour thumbTop           = fromHex (0x57b8ff);
    static const juce::Colour thumbBottom        = fromHex (0x2f8fdb);

    static const juce::Colour buttonOff          = fromHex (0x232c37);
    static const juce::Colour buttonOffText      = fromHex (0x8393a3);
    static const juce::Colour buttonBorder       = fromHex (0x0a0d12);

    // Single "on" accent per button family -- StudioConsolePainter::paintToggleChrome
    // derives the gradient's light/dark ends and the on-state text colour from
    // whichever one of these is passed in, so call sites only ever specify one colour.
    static const juce::Colour accentAmber        = fromHex (0xf0ad1f); // Gain Match / Bypass / Delta
    static const juce::Colour accentBlue         = fromHex (0x2f8fdb); // OS factor radio group

    static const juce::Colour meterBackground    = fromHex (0x10151c);
    static const juce::Colour meterBorder        = fromHex (0x232c37);
    static const juce::Colour meterHoldMarker    = fromHex (0xffffff);

    static const juce::Colour charMeterL         = fromHex (0xb34ce6);
    static const juce::Colour charMeterR         = fromHex (0xe6992e);
    static const juce::Colour grMeterL           = fromHex (0x3399ff);
    static const juce::Colour grMeterR           = fromHex (0xff7333);
    static const juce::Colour peakMeter          = fromHex (0x33cc66);

    static const juce::Colour rangeBar           = fromHex (0x4ce666);

    static const juce::Colour readoutLabel       = fromHex (0x6f9dc4);
    static const juce::Colour scaleLabel         = fromHex (0x8393a3);
    static const juce::Colour dynRangeLabel      = fromHex (0x4fd8a1);
    static const juce::Colour lufsLabel          = fromHex (0xe8c15a);

    static const juce::Colour overRed            = fromHex (0xff4d4d);
    static const juce::Colour overYellow         = fromHex (0xffcc33);
    static const juce::Colour overNeutral        = fromHex (0x2a323d);

    static const juce::Colour editBoxBackground  = fromHex (0x0a0d12);
    static const juce::Colour editBoxBorder      = accentAmber;
    static const juce::Colour editBoxText        = fromHex (0xffffff);

    //==============================================================================
    // Gradient factories -- built against real bounds, not stored as constants.

    // Subtle vertical wash for the overall window background.
    inline juce::ColourGradient windowBackground (juce::Rectangle<float> bounds)
    {
        return juce::ColourGradient (backgroundLight, bounds.getX(), bounds.getY(),
                                      background, bounds.getX(), bounds.getBottom(), false);
    }

    // Group/meter panel fill -- light-to-dark top-to-bottom.
    inline juce::ColourGradient panelFill (juce::Rectangle<float> bounds)
    {
        return juce::ColourGradient (groupBackgroundTop, bounds.getX(), bounds.getY(),
                                      groupBackgroundBot, bounds.getX(), bounds.getBottom(), false);
    }

    // Knob face -- off-centre bright hotspot (matches a light source at upper-left)
    // falling off through a mid tone to a near-black edge. point1/point2 define a
    // radial gradient's centre and radius in JUCE's model; the mid stop is added at
    // its proportional distance from centre.
    inline juce::ColourGradient knobFace (juce::Rectangle<float> circleBounds)
    {
        const auto hotspot = circleBounds.getRelativePoint (0.32f, 0.28f);
        const float radius = circleBounds.getWidth() * 0.5f;
        juce::ColourGradient grad (knobHighlight, hotspot,
                                    knobShadowEdge, hotspot.translated (radius, 0.0f), true);
        grad.addColour (0.55, knobMid);
        return grad;
    }

    // Slider thumb / knob needle base -- vertical light-to-dark.
    inline juce::ColourGradient thumbFill (juce::Rectangle<float> bounds)
    {
        return juce::ColourGradient (thumbTop, bounds.getX(), bounds.getY(),
                                      thumbBottom, bounds.getX(), bounds.getBottom(), false);
    }

    // Toggle-on / radio-on fill, parameterised by accent so both the amber
    // (Gain Match/Bypass/Delta) and blue (OS factor) button families share one factory.
    inline juce::ColourGradient buttonOnFill (juce::Rectangle<float> bounds, juce::Colour top, juce::Colour bottom)
    {
        return juce::ColourGradient (top, bounds.getX(), bounds.getY(),
                                      bottom, bounds.getX(), bounds.getBottom(), false);
    }

    // Vertical meter fill -- dim at the quiet end, bright at the loud end. `bright` is
    // whichever edge of `bounds` the meter fills toward (bottom-up meters brighten
    // upward; top-down/inverted meters brighten downward), so this takes explicit y
    // coordinates rather than assuming a direction.
    inline juce::ColourGradient meterFill (float x, float yDim, float yBright, juce::Colour dim, juce::Colour bright)
    {
        return juce::ColourGradient (dim, x, yDim, bright, x, yBright, false);
    }
}

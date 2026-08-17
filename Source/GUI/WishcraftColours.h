#pragma once

#include <juce_graphics/juce_graphics.h>

// Shared colour palette, matching the JSFX @gfx section's gfx_set() calls as closely as
// practical. Named after what each colour is used FOR (matching the JSFX's draw_*
// function comments), not just its RGB value, so a future layout pass can find things.
namespace WishcraftColours
{
    inline juce::Colour fromFloatRGB (float r, float g, float b, float a = 1.0f)
    {
        return juce::Colour::fromFloatRGBA (r, g, b, a);
    }

    static const juce::Colour background        = fromFloatRGB (0.0f, 0.0f, 0.0f);
    static const juce::Colour titleText          = fromFloatRGB (1.0f, 1.0f, 1.0f);

    static const juce::Colour groupBackground    = fromFloatRGB (0.10f, 0.15f, 0.19f, 0.8f);
    static const juce::Colour groupBorder        = fromFloatRGB (0.16f, 0.29f, 0.42f);
    static const juce::Colour groupLabel         = fromFloatRGB (0.44f, 0.66f, 0.81f);

    static const juce::Colour controlLabel       = fromFloatRGB (0.75f, 0.75f, 0.75f);
    static const juce::Colour controlValueText   = fromFloatRGB (0.85f, 0.85f, 0.85f);
    static const juce::Colour controlEndLabel    = fromFloatRGB (0.6f, 0.6f, 0.6f);

    static const juce::Colour knobTrack          = fromFloatRGB (0.08f, 0.09f, 0.11f);
    static const juce::Colour knobBorder         = fromFloatRGB (0.33f, 0.33f, 0.33f);
    static const juce::Colour knobNeedle         = fromFloatRGB (0.3f, 0.75f, 1.0f);

    static const juce::Colour sliderTrack        = fromFloatRGB (0.08f, 0.09f, 0.11f);
    static const juce::Colour sliderBorder       = fromFloatRGB (0.33f, 0.33f, 0.33f);
    static const juce::Colour sliderCenterTick   = fromFloatRGB (0.27f, 0.27f, 0.27f);
    static const juce::Colour sliderThumb        = fromFloatRGB (0.18f, 0.61f, 0.86f);

    static const juce::Colour buttonOff          = fromFloatRGB (0.35f, 0.35f, 0.35f);
    static const juce::Colour buttonBorder       = fromFloatRGB (0.1f, 0.1f, 0.1f);
    static const juce::Colour buttonLabelOnBg    = fromFloatRGB (0.05f, 0.05f, 0.05f);
    static const juce::Colour buttonLabelOffBg   = fromFloatRGB (0.85f, 0.85f, 0.85f);
    static const juce::Colour buttonYellow       = fromFloatRGB (1.0f, 0.82f, 0.15f); // Auto Gain / Bypass / Delta
    static const juce::Colour buttonBlue         = fromFloatRGB (0.2f, 0.6f, 1.0f);   // OS Factor radio pair

    static const juce::Colour meterBackground    = fromFloatRGB (0.08f, 0.09f, 0.11f);
    static const juce::Colour meterBorder        = fromFloatRGB (0.35f, 0.35f, 0.35f);
    static const juce::Colour meterHoldMarker    = fromFloatRGB (1.0f, 1.0f, 1.0f);

    static const juce::Colour charMeterL         = fromFloatRGB (0.7f, 0.3f, 0.9f);
    static const juce::Colour charMeterR         = fromFloatRGB (0.9f, 0.6f, 0.2f);
    static const juce::Colour grMeterL           = fromFloatRGB (0.2f, 0.6f, 1.0f);
    static const juce::Colour grMeterR           = fromFloatRGB (1.0f, 0.45f, 0.2f);
    static const juce::Colour peakMeter          = fromFloatRGB (0.2f, 0.8f, 0.4f);

    static const juce::Colour rangeBar           = fromFloatRGB (0.3f, 0.9f, 0.4f, 0.9f);

    static const juce::Colour readoutLabel       = fromFloatRGB (0.44f, 0.66f, 0.81f);
    static const juce::Colour scaleLabel         = fromFloatRGB (0.6f, 0.6f, 0.6f);
    static const juce::Colour dynRangeLabel      = fromFloatRGB (0.3f, 0.9f, 0.4f);
    static const juce::Colour lufsLabel          = fromFloatRGB (0.9f, 0.75f, 0.2f);

    static const juce::Colour overRed            = fromFloatRGB (1.0f, 0.2f, 0.2f);
    static const juce::Colour overYellow         = fromFloatRGB (1.0f, 0.8f, 0.1f);
    static const juce::Colour overNeutral        = fromFloatRGB (0.3f, 0.3f, 0.3f);

    static const juce::Colour editBoxBackground  = fromFloatRGB (0.05f, 0.05f, 0.05f);
    static const juce::Colour editBoxBorder      = buttonYellow;
    static const juce::Colour editBoxText        = fromFloatRGB (1.0f, 1.0f, 1.0f);
}

#pragma once

#include <juce_graphics/juce_graphics.h>

#include "StudioConsoleTheme.h"

// Shared drawing routines for the "Studio Console" finish. Free functions rather than a
// class -- they're called from two different places that don't share a base class: this
// project's own custom-painted Components (WishcraftKnob, WishcraftHSlider, ...), and
// StudioConsoleLookAndFeel's overrides for plugins built with stock JUCE widgets. Kept
// here, not duplicated in both, so the two can never drift apart.
namespace StudioConsolePainter
{
    // Group/meter panel background: gradient fill, border, a faint top highlight line
    // (the "light catching the top edge" cue that makes a flat panel read as raised),
    // soft drop shadow, and the bold top-left label every panel in this plugin has.
    inline void paintPanelChrome (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& label,
                                   float cornerRadius = 6.0f)
    {
        auto boundsF = bounds.toFloat();

        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.5f), 10, { 0, 4 });
        juce::Path panelPath;
        panelPath.addRoundedRectangle (boundsF, cornerRadius);
        shadow.drawForPath (g, panelPath);

        g.setGradientFill (StudioConsoleTheme::panelFill (boundsF));
        g.fillRoundedRectangle (boundsF, cornerRadius);

        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawLine (boundsF.getX() + cornerRadius, boundsF.getY() + 0.5f,
                     boundsF.getRight() - cornerRadius, boundsF.getY() + 0.5f, 1.0f);

        g.setColour (StudioConsoleTheme::groupBorder);
        g.drawRoundedRectangle (boundsF, cornerRadius, 1.0f);

        g.setColour (StudioConsoleTheme::groupLabel);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (label, bounds.reduced (8, 4).withHeight (20), juce::Justification::topLeft, false);
    }

    // Knob/rotary face only -- radial gradient with an off-centre hotspot, a thin lit
    // rim catching the same upper-left light, and a soft drop shadow for elevation.
    // Needle is drawn separately (paintKnobNeedle) since callers need the angle.
    inline void paintKnobFace (juce::Graphics& g, juce::Rectangle<float> circleBounds)
    {
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.55f), 5, { 0, 2 });
        juce::Path circlePath;
        circlePath.addEllipse (circleBounds);
        shadow.drawForPath (g, circlePath);

        g.setGradientFill (StudioConsoleTheme::knobFace (circleBounds));
        g.fillEllipse (circleBounds);

        // Thin lit rim, upper-left quadrant only, approximating a specular highlight.
        juce::Path rim;
        rim.addCentredArc (circleBounds.getCentreX(), circleBounds.getCentreY(),
                            circleBounds.getWidth() * 0.5f - 0.75f, circleBounds.getHeight() * 0.5f - 0.75f,
                            0.0f, juce::degreesToRadians (200.0f), juce::degreesToRadians (330.0f), true);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.strokePath (rim, juce::PathStrokeType (1.2f));

        g.setColour (StudioConsoleTheme::knobBorder);
        g.drawEllipse (circleBounds, 1.0f);
    }

    // Needle with a soft glow -- a wider, low-alpha stroke underneath the crisp line,
    // rather than a true blur (JUCE has no cheap realtime Gaussian blur for a single
    // line), reads convincingly at this size.
    inline void paintKnobNeedle (juce::Graphics& g, juce::Point<float> centre, float radius, float angleRadians,
                                  juce::Colour needleColour, float widthPx = 2.5f)
    {
        const float needleLen = juce::jmax (0.0f, radius - 7.0f);
        const juce::Point<float> tip (centre.x + std::sin (angleRadians) * needleLen,
                                       centre.y - std::cos (angleRadians) * needleLen);

        g.setColour (needleColour.withAlpha (0.35f));
        g.drawLine ({ centre, tip }, widthPx + 3.0f);
        g.setColour (needleColour);
        g.drawLine ({ centre, tip }, widthPx);
    }

    // Toggle/radio-segment chrome: gradient fill + glow when on, flat when off. Takes a
    // single accent colour (matching how callers already specify one "on" colour per
    // button, e.g. the amber Auto Gain/Bypass/Delta family or the blue OS-factor
    // family) and derives the gradient's light/dark ends plus a near-black, hue-tinted
    // on-state text colour from it, rather than requiring three colours per call site.
    inline void paintToggleChrome (juce::Graphics& g, juce::Rectangle<float> bounds, bool on,
                                    juce::Colour accent, const juce::String& text, float cornerRadius = 4.0f)
    {
        const auto onTop = accent.brighter (0.35f);
        const auto onBottom = accent.darker (0.1f);
        const auto onTextColour = accent.darker (0.85f);

        if (on)
        {
            juce::DropShadow glow (onBottom.withAlpha (0.45f), 8, { 0, 0 });
            juce::Path p;
            p.addRoundedRectangle (bounds, cornerRadius);
            glow.drawForPath (g, p);

            g.setGradientFill (StudioConsoleTheme::buttonOnFill (bounds, onTop, onBottom));
            g.fillRoundedRectangle (bounds, cornerRadius);
        }
        else
        {
            g.setColour (StudioConsoleTheme::buttonOff);
            g.fillRoundedRectangle (bounds, cornerRadius);
        }

        g.setColour (StudioConsoleTheme::buttonBorder);
        g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

        g.setColour (on ? onTextColour : StudioConsoleTheme::buttonOffText);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (text, bounds.toNearestInt(), juce::Justification::centred, false);
    }

    // Recessed track (for h-sliders and, in a future plugin, LookAndFeel linear
    // sliders) -- flat fill plus a hairline dark line along the top edge standing in
    // for an inset shadow.
    inline void paintTrack (juce::Graphics& g, juce::Rectangle<float> trackBounds)
    {
        g.setColour (StudioConsoleTheme::trackBackground);
        g.fillRoundedRectangle (trackBounds, trackBounds.getHeight() * 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (trackBounds.getX() + 1.0f, trackBounds.getY() + 0.75f,
                    trackBounds.getRight() - 1.0f, trackBounds.getY() + 0.75f, 1.0f);
        g.setColour (StudioConsoleTheme::trackBorder);
        g.drawRoundedRectangle (trackBounds, trackBounds.getHeight() * 0.5f, 1.0f);
    }

    // Slider thumb -- gradient fill plus a soft coloured glow.
    inline void paintThumb (juce::Graphics& g, juce::Rectangle<float> thumbBounds, juce::Colour glowColour)
    {
        juce::DropShadow glow (glowColour.withAlpha (0.4f), 5, { 0, 0 });
        juce::Path p;
        p.addRoundedRectangle (thumbBounds, 2.5f);
        glow.drawForPath (g, p);

        g.setGradientFill (StudioConsoleTheme::thumbFill (thumbBounds));
        g.fillRoundedRectangle (thumbBounds, 2.5f);
        g.setColour (StudioConsoleTheme::buttonBorder);
        g.drawRoundedRectangle (thumbBounds, 2.5f, 1.0f);
    }

    // Meter fill bar -- gradient brightening toward `brightEdgeIsTop`'s edge, plus a
    // faint glow so the loud end reads as "lit" rather than just a solid colour block.
    inline void paintMeterFill (juce::Graphics& g, juce::Rectangle<float> fillBounds, juce::Colour baseColour, bool brightEdgeIsTop)
    {
        if (fillBounds.getHeight() <= 0.0f)
            return;

        const auto dim = baseColour.darker (0.6f);
        const auto bright = baseColour.brighter (0.3f);
        const float yDim = brightEdgeIsTop ? fillBounds.getBottom() : fillBounds.getY();
        const float yBright = brightEdgeIsTop ? fillBounds.getY() : fillBounds.getBottom();

        g.setGradientFill (StudioConsoleTheme::meterFill (fillBounds.getX(), yDim, yBright, dim, bright));
        g.fillRect (fillBounds);
    }
}

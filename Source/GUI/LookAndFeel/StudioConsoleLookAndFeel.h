#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "StudioConsolePainter.h"
#include "StudioConsoleTheme.h"

// Drop-in juce::LookAndFeel_V4 for the "Studio Console" finish, for plugins built with
// STOCK JUCE widgets (juce::Slider, juce::Button, juce::GroupComponent, ...) rather than
// this project's fully custom-painted Components. A future project just needs:
//
//     StudioConsoleLookAndFeel laf;
//     setLookAndFeel (&laf);   // and setLookAndFeel(nullptr) before the laf is destroyed
//
// and every standard Slider/Button/GroupComponent picks up this look automatically --
// no custom Component subclassing required. Built on the same StudioConsolePainter
// routines this project's own custom controls use, so both stay visually identical.
//
// This whole GUI/LookAndFeel/ folder has no dependency on anything else in this
// project -- copy it wholesale into a new JUCE project to reuse it there.
class StudioConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StudioConsoleLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, StudioConsoleTheme::background);
        setColour (juce::Slider::textBoxTextColourId, StudioConsoleTheme::controlValueText);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, StudioConsoleTheme::controlLabel);
        setColour (juce::GroupComponent::textColourId, StudioConsoleTheme::groupLabel);
        setColour (juce::GroupComponent::outlineColourId, StudioConsoleTheme::groupBorder);
    }

    // Accent used for anything drawn "on"/active (toggle buttons, radio segments) when
    // the widget doesn't otherwise specify a colour -- defaults to the amber family
    // this plugin uses for Auto Gain/Bypass/Delta. Call setAccent() for the blue family
    // (this plugin's OS-factor radio group) on a per-instance basis if a future project
    // wants both accents available without two LookAndFeel instances.
    void setAccent (juce::Colour newAccent) { accent = newAccent; }

    //==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto circleBounds = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());

        StudioConsolePainter::paintKnobFace (g, circleBounds);

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        StudioConsolePainter::paintKnobNeedle (g, circleBounds.getCentre(), diameter * 0.5f, angle, StudioConsoleTheme::knobNeedle);
    }

    //==============================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        juce::ignoreUnused (minSliderPos, maxSliderPos);

        if (style == juce::Slider::SliderStyle::LinearHorizontal)
        {
            auto trackBounds = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - 3.0f, (float) width, 6.0f);
            StudioConsolePainter::paintTrack (g, trackBounds);

            auto thumbBounds = juce::Rectangle<float> (10.0f, (float) height * 0.7f).withCentre ({ sliderPos, trackBounds.getCentreY() });
            StudioConsolePainter::paintThumb (g, thumbBounds, StudioConsoleTheme::knobNeedle);
        }
        else if (style == juce::Slider::SliderStyle::LinearVertical)
        {
            auto trackBounds = juce::Rectangle<float> ((float) x + (float) width * 0.5f - 3.0f, (float) y, 6.0f, (float) height);
            StudioConsolePainter::paintTrack (g, trackBounds);

            auto thumbBounds = juce::Rectangle<float> ((float) width * 0.7f, 10.0f).withCentre ({ trackBounds.getCentreX(), sliderPos });
            StudioConsolePainter::paintThumb (g, thumbBounds, StudioConsoleTheme::knobNeedle);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    //==============================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                bool, bool) override
    {
        StudioConsolePainter::paintToggleChrome (g, button.getLocalBounds().toFloat(), button.getToggleState(),
                                                  accent, {}, 4.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour (button.getToggleState() ? accent.darker (0.85f) : StudioConsoleTheme::buttonOffText);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }

    //==============================================================================
    void drawGroupComponentOutline (juce::Graphics& g, int width, int height, const juce::String& text,
                                     const juce::Justification&, juce::GroupComponent&) override
    {
        StudioConsolePainter::paintPanelChrome (g, { width, height }, text);
    }

private:
    juce::Colour accent = StudioConsoleTheme::accentAmber;
};

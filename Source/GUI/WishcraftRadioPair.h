#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Mutually-exclusive two-option radio pair, matching the JSFX's two radio_button_interact()
// calls sharing the same os_choice slider: clicking either half sets the parameter to that
// option's value outright -- unlike WishcraftButton, clicking the ALREADY-active option
// never toggles it off. One shared parameter (0 = low option, 1 = high option), drawn as
// two adjacent rectangles.
class WishcraftRadioPair : public juce::Component
{
public:
    WishcraftRadioPair (juce::RangedAudioParameter& parameterIn,
                         juce::String lowLabelText, juce::String highLabelText, juce::Colour onColourIn)
        : param (parameterIn),
          lowLabel (std::move (lowLabelText)),
          highLabel (std::move (highLabelText)),
          onColour (onColourIn),
          attachment (parameterIn, [this] (float v) { currentValue = v; repaint(); })
    {
        currentValue = param.convertFrom0to1 (param.getValue());
        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int gap = 6;
        const int half = (b.getWidth() - gap) / 2;
        lowBounds = b.removeFromLeft (half);
        b.removeFromLeft (gap);
        highBounds = b;
    }

    void paint (juce::Graphics& g) override
    {
        const bool lowOn = param.convertTo0to1 (currentValue) < 0.5f;
        paintOption (g, lowBounds, lowOn, lowLabel);
        paintOption (g, highBounds, ! lowOn, highLabel);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool clickedHigh = e.getPosition().x >= highBounds.getX();
        const float newNorm = clickedHigh ? 1.0f : 0.0f;
        const float newReal = param.convertFrom0to1 (newNorm);
        if (! juce::exactlyEqual (param.convertTo0to1 (currentValue), newNorm))
        {
            currentValue = newReal;
            attachment.setValueAsCompleteGesture (newReal);
            repaint();
        }
    }

private:
    void paintOption (juce::Graphics& g, juce::Rectangle<int> r, bool on, const juce::String& text)
    {
        auto rf = r.toFloat();
        g.setColour (on ? onColour : WishcraftColours::buttonOff);
        g.fillRect (rf);
        g.setColour (WishcraftColours::buttonBorder);
        g.drawRect (rf, 1.0f);
        g.setColour (on ? WishcraftColours::buttonLabelOnBg : WishcraftColours::buttonLabelOffBg);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (text, r, juce::Justification::centred, false);
    }

    juce::RangedAudioParameter& param;
    juce::String lowLabel, highLabel;
    juce::Colour onColour;
    float currentValue = 0.0f;
    juce::Rectangle<int> lowBounds, highBounds;
    juce::ParameterAttachment attachment;
};

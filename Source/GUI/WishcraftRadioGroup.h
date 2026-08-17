#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Mutually-exclusive N-option radio group, matching the JSFX's radio_button_interact()
// pattern: clicking any option sets the parameter to that option's value outright --
// unlike WishcraftButton, clicking the ALREADY-active option never toggles it off. One
// shared discrete parameter (index 0 .. numOptions-1, evenly spaced 0..1), drawn as
// numOptions adjacent rectangles. Originally a fixed 2-option "WishcraftRadioPair";
// generalized to N options for the 2x/4x/8x oversampling selector -- the only call site
// (osChoiceRadio in PluginEditor.cpp) was updated alongside this generalization.
class WishcraftRadioGroup : public juce::Component
{
public:
    WishcraftRadioGroup (juce::RangedAudioParameter& parameterIn,
                          juce::StringArray optionLabelsIn, juce::Colour onColourIn)
        : param (parameterIn),
          optionLabels (std::move (optionLabelsIn)),
          onColour (onColourIn),
          attachment (parameterIn, [this] (float v) { currentValue = v; repaint(); })
    {
        jassert (optionLabels.size() >= 2);
        optionBounds.resize ((size_t) optionLabels.size());
        currentValue = param.convertFrom0to1 (param.getValue());
        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int gap = 6;
        const int numOptions = optionLabels.size();
        const int totalGap = gap * (numOptions - 1);
        const int optionW = (b.getWidth() - totalGap) / numOptions;

        for (int i = 0; i < numOptions; ++i)
        {
            optionBounds[(size_t) i] = (i == numOptions - 1) ? b : b.removeFromLeft (optionW);
            if (i < numOptions - 1)
                b.removeFromLeft (gap);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const int activeIndex = indexForNorm (param.convertTo0to1 (currentValue));
        for (int i = 0; i < optionLabels.size(); ++i)
            paintOption (g, optionBounds[(size_t) i], i == activeIndex, optionLabels[i]);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int numOptions = optionLabels.size();
        int clickedIndex = numOptions - 1;
        for (int i = 0; i < numOptions; ++i)
        {
            if (e.getPosition().x < optionBounds[(size_t) i].getRight())
            {
                clickedIndex = i;
                break;
            }
        }

        const float newNorm = (float) clickedIndex / (float) (numOptions - 1);
        const float newReal = param.convertFrom0to1 (newNorm);
        if (indexForNorm (param.convertTo0to1 (currentValue)) != clickedIndex)
        {
            currentValue = newReal;
            attachment.setValueAsCompleteGesture (newReal);
            repaint();
        }
    }

private:
    int indexForNorm (float norm) const
    {
        return (int) std::round (norm * (float) (optionLabels.size() - 1));
    }

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
    juce::StringArray optionLabels;
    juce::Colour onColour;
    float currentValue = 0.0f;
    std::vector<juce::Rectangle<int>> optionBounds;
    juce::ParameterAttachment attachment;
};

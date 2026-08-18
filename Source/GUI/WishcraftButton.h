#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel/StudioConsolePainter.h"

// Toggle button matching the JSFX's draw_button() + button_interact(): a gradient-lit
// rectangle when on, flat grey when off, a caller-supplied accent colour, label inside
// (centered, per this session's explicit label-centering deviation from the JSFX's
// left-aligned original). Plain click-toggle only -- no double-click or Ctrl+click text
// entry, same as button_interact() itself (a two-state toggle has neither).
class WishcraftButton : public juce::Button
{
public:
    WishcraftButton (juce::RangedAudioParameter& parameterIn, juce::String labelText, juce::Colour onColourIn)
        : juce::Button (labelText), label (std::move (labelText)), onColour (onColourIn),
          attachment (parameterIn, *this)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        StudioConsolePainter::paintToggleChrome (g, getLocalBounds().toFloat(), getToggleState(), onColour, label, 4.0f);
    }

private:
    juce::String label;
    juce::Colour onColour;
    juce::ButtonParameterAttachment attachment;
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Toggle button matching the JSFX's draw_button() + button_interact(): a solid
// rectangle, grey when off, a caller-supplied colour when on, label inside (centered,
// per this session's explicit label-centering deviation from the JSFX's left-aligned
// original). Plain click-toggle only -- no double-click or Ctrl+click text entry, same
// as button_interact() itself (a two-state toggle has neither).
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
        auto bounds = getLocalBounds().toFloat();
        const bool on = getToggleState();

        g.setColour (on ? onColour : WishcraftColours::buttonOff);
        g.fillRect (bounds);
        g.setColour (WishcraftColours::buttonBorder);
        g.drawRect (bounds, 1.0f);

        g.setColour (on ? WishcraftColours::buttonLabelOnBg : WishcraftColours::buttonLabelOffBg);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (label, getLocalBounds(), juce::Justification::centred, false);
    }

private:
    juce::String label;
    juce::Colour onColour;
    juce::ButtonParameterAttachment attachment;
};

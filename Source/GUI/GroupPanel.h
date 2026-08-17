#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Matches the JSFX's draw_group(): a translucent panel background, a border, and a
// label in the top-left corner. Purely decorative -- meant to sit BEHIND the real
// controls in z-order (add it to the editor first, or call toBack()).
class GroupPanel : public juce::Component
{
public:
    explicit GroupPanel (const juce::String& labelText) : label (labelText) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (WishcraftColours::groupBackground);
        g.fillRect (bounds);
        g.setColour (WishcraftColours::groupBorder);
        g.drawRect (bounds, 1.0f);

        g.setColour (WishcraftColours::groupLabel);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (label, getLocalBounds().reduced (8, 4).withHeight (20),
                    juce::Justification::topLeft, false);
    }

private:
    juce::String label;
};

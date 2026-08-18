#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel/StudioConsolePainter.h"

// Matches the JSFX's draw_group(): a gradient panel background, a border, and a label
// in the top-left corner. Purely decorative -- meant to sit BEHIND the real controls in
// z-order (add it to the editor first, or call toBack()).
class GroupPanel : public juce::Component
{
public:
    explicit GroupPanel (const juce::String& labelText) : label (labelText) {}

    void paint (juce::Graphics& g) override
    {
        StudioConsolePainter::paintPanelChrome (g, getLocalBounds(), label);
    }

private:
    juce::String label;
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "WishcraftColours.h"

// Stage 11: Quick Start help. Net-new -- the JSFX has no equivalent -- so this is pure
// UX addition, not a port. Kept deliberately lightweight: one scrollable panel, plain
// styled text, no images or multi-step wizard.

// Renders the Quick Start copy as a single AttributedString (headers bold/accent,
// body regular, tips indented) and reports the height it needs for a given width, so
// the owning Viewport can size it correctly.
class HelpContentComponent : public juce::Component
{
public:
    HelpContentComponent() { buildText(); }

    int computeHeightForWidth (int width)
    {
        contentWidth = width;
        layout.createLayout (attributedText, (float) width);
        return (int) std::ceil (layout.getHeight()) + bottomPadding;
    }

    void paint (juce::Graphics& g) override
    {
        layout.draw (g, juce::Rectangle<float> (0.0f, 0.0f, (float) contentWidth, (float) getHeight()));
    }

private:
    void addHeader (const juce::String& text)
    {
        attributedText.append (text + "\n", juce::FontOptions (15.0f, juce::Font::bold), WishcraftColours::groupLabel);
    }

    void addBody (const juce::String& text)
    {
        attributedText.append (text + "\n\n", juce::FontOptions (13.5f), WishcraftColours::controlValueText);
    }

    void addTip (const juce::String& label, const juce::String& text)
    {
        attributedText.append (label + " ", juce::FontOptions (13.5f, juce::Font::bold), WishcraftColours::lufsLabel);
        attributedText.append (text + "\n\n", juce::FontOptions (13.5f), WishcraftColours::controlValueText);
    }

    void buildText()
    {
        attributedText.setLineSpacing (3.0f);

        addHeader ("WHAT THIS IS");
        addBody ("Wishcraft Mastering Limiter eliminates the need for a separate clipper in front of your "
                 "limiter. Its Selective Clipper can be pushed harder than a standard clipper without the "
                 "usual audible cost, taking real load off the limiter before it ever sees the signal.");

        addHeader ("WORKFLOW");
        addBody ("1. Selective Clipper -- set Threshold and Selectivity first.\n"
                 "2. Gain -- use Input Gain to drive the already-clipped signal into the Limiter.\n"
                 "3. Limiter -- dial in Ceiling, Release, Link, and Auto Gain last.\n\n"
                 "This is the opposite order from most limiter plugins, where you'd normally start with "
                 "the limiter itself.");

        addHeader ("WHY IT'S DIFFERENT");
        addBody ("The Selective Clipper only touches genuinely brief peaks -- anything under about 5ms. "
                 "Sustained loud passages are left alone entirely and handled downstream by the Limiter's "
                 "own gain reduction instead. That's why it can run more aggressively than a standard "
                 "clipper without sounding like distortion: it's shaving isolated spikes, not flattening "
                 "whole passages.");

        addHeader ("WHAT TO EXPECT");
        addBody ("Pushed to a similar loudness, the Selective Clipper typically preserves noticeably more "
                 "dynamic range (LRA) than a standard clipper. The denser and more sustained the source "
                 "material, the more effective it is -- on already-loud, densely-produced material the gap "
                 "can be substantial; on more dynamic, transient-heavy material it narrows, since there's "
                 "less sustained content for the duration gate to exempt in the first place.");

        addHeader ("CONTROLS");
        addTip ("Auto Gain --", "lets you A/B different Limiter settings without the \"louder sounds "
                "better\" bias, since it keeps output level roughly matched regardless of how much gain "
                "reduction is happening. Turn it off before your final render/bounce -- the reduction it "
                "applies is dynamic and isn't meant to set your actual output loudness.");
        addTip ("Delta --", "solos what the Selective Clipper is removing, gained up so it's actually "
                "audible. Use it to dial in Threshold and Selectivity by ear before trusting them blind -- "
                "remember you can push Threshold more aggressively than you'd expect from a standard "
                "clipper without it sounding like distortion.");
        addTip ("Bypass --", "full latency-compensated bypass -- compares the true, unprocessed input "
                "against your processed output.");

        addHeader ("METERS");
        addBody ("SELECTIVE CLIP shows how much the clipper is removing, per channel. GAIN REDUCTION shows "
                 "how hard the Limiter is working. OUTPUT shows final peak level plus Dynamic Range and "
                 "short-term LUFS -- the dot beside each channel turns yellow past your Peak Ceiling, red "
                 "past true 0 dBFS.");
    }

    juce::AttributedString attributedText;
    juce::TextLayout layout;
    int contentWidth = 1;
    static constexpr int bottomPadding = 16;
};

// The modal itself: a scrim over the whole design canvas with a centered panel
// (title bar + close button + scrollable HelpContentComponent). Clicking the scrim
// outside the panel dismisses it, same as clicking the close button.
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay()
    {
        setInterceptsMouseClicks (true, true);

        titleLabel.setText ("QUICK START", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, WishcraftColours::titleText);
        addAndMakeVisible (titleLabel);

        closeButton.setButtonText ("X");
        closeButton.setColour (juce::TextButton::buttonColourId, WishcraftColours::buttonOff);
        closeButton.setColour (juce::TextButton::textColourOffId, WishcraftColours::controlValueText);
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        panelBounds = bounds.withSizeKeepingCentre (juce::jmin (640, bounds.getWidth() - 40),
                                                     juce::jmin (500, bounds.getHeight() - 40));

        auto panel = panelBounds.reduced (16);
        auto header = panel.removeFromTop (28);
        closeButton.setBounds (header.removeFromRight (24).withSizeKeepingCentre (24, 24));
        titleLabel.setBounds (header);
        panel.removeFromTop (8);

        viewport.setBounds (panel);
        const int contentWidth = panel.getWidth() - viewport.getScrollBarThickness() - 4;
        content.setSize (contentWidth, content.computeHeightForWidth (contentWidth));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (WishcraftColours::background.withAlpha (0.75f));
        g.fillRect (getLocalBounds());

        g.setColour (WishcraftColours::groupBackground.withAlpha (1.0f));
        g.fillRect (panelBounds);
        g.setColour (WishcraftColours::groupBorder);
        g.drawRect (panelBounds, 1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panelBounds.contains (e.getPosition()))
            setVisible (false);
    }

private:
    juce::Label titleLabel;
    juce::TextButton closeButton;
    juce::Viewport viewport;
    HelpContentComponent content;
    juce::Rectangle<int> panelBounds;
};

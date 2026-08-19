#pragma once

// Trial-build-only, matching TrialLicense.h's WISHCRAFT_TRIAL_BUILD gate -- doesn't
// exist at all in the normal Release build.
#if WISHCRAFT_TRIAL_BUILD

#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel/StudioConsoleTheme.h"

// Covers the whole design canvas once the trial period has ended, blocking interaction
// with everything behind it -- matches HelpOverlay's scrim+panel look for visual
// consistency. Deliberately has no close button: audio is already silenced
// independently in PluginProcessor::processBlock (this overlay is just the visible
// explanation of why), but staying non-dismissible keeps the message in front of the
// user rather than letting them work around a blank GUI and wonder if it's broken.
class TrialExpiredOverlay : public juce::Component
{
public:
    TrialExpiredOverlay()
    {
        setInterceptsMouseClicks (true, true);

        titleLabel.setText ("TRIAL PERIOD ENDED", juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, StudioConsoleTheme::titleText);
        titleLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (titleLabel);

        bodyLabel.setText (
            "Thanks for testing Wishcraft Mastering Limiter. This trial copy has reached "
            "the end of its evaluation period and will no longer process audio.\n\n"
            "Enjoyed it? Get in touch and let me know what you thought -- I'll send you "
            "a free full version.",
            juce::dontSendNotification);
        bodyLabel.setFont (juce::FontOptions (14.0f));
        bodyLabel.setColour (juce::Label::textColourId, StudioConsoleTheme::controlValueText);
        bodyLabel.setJustificationType (juce::Justification::centredTop);
        addAndMakeVisible (bodyLabel);

        emailButton.setButtonText ("Email wishcraftmusicstudio@gmail.com");
        emailButton.setColour (juce::TextButton::buttonColourId, StudioConsoleTheme::accentAmber);
        emailButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        emailButton.onClick = []
        {
            juce::URL ("mailto:wishcraftmusicstudio@gmail.com?subject=Wishcraft%20Mastering%20Limiter%20trial")
                .launchInDefaultBrowser();
        };
        addAndMakeVisible (emailButton);
    }

    void resized() override
    {
        panelBounds = getLocalBounds().withSizeKeepingCentre (
            juce::jmin (460, getWidth() - 40), juce::jmin (280, getHeight() - 40));

        auto panel = panelBounds.reduced (24);
        titleLabel.setBounds (panel.removeFromTop (30));
        panel.removeFromTop (12);
        emailButton.setBounds (panel.removeFromBottom (32));
        panel.removeFromBottom (16);
        bodyLabel.setBounds (panel);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (StudioConsoleTheme::background.withAlpha (0.88f));
        g.fillRect (getLocalBounds());

        constexpr float cornerRadius = 6.0f;
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.6f), 20, {});
        juce::Path panelPath;
        panelPath.addRoundedRectangle (panelBounds.toFloat(), cornerRadius);
        shadow.drawForPath (g, panelPath);

        g.setGradientFill (StudioConsoleTheme::panelFill (panelBounds.toFloat()));
        g.fillRoundedRectangle (panelBounds.toFloat(), cornerRadius);
        g.setColour (StudioConsoleTheme::groupBorder);
        g.drawRoundedRectangle (panelBounds.toFloat(), cornerRadius, 1.0f);
    }

private:
    juce::Label titleLabel, bodyLabel;
    juce::TextButton emailButton;
    juce::Rectangle<int> panelBounds;
};

#endif // WISHCRAFT_TRIAL_BUILD

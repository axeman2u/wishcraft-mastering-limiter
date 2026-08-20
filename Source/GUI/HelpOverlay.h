#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel/StudioConsoleTheme.h"

// Stage 11: Quick Start help. Net-new -- the JSFX has no equivalent -- so this is pure
// UX addition, not a port. Kept deliberately lightweight: one scrollable panel, plain
// styled text, no images or multi-step wizard.

// The manual PDF ships installed alongside the plugin binaries themselves (see
// Packaging/macOS/build_installer.sh and Packaging/Windows/installer.iss) rather than
// in a separate, easy-to-forget location -- this searches the fixed system-wide
// locations our own installers use, plus the equivalent per-user folders some users
// prefer on macOS, so the "Manual (PDF)" button works regardless of which format
// (VST3/AU) actually loaded.
static inline juce::File findManualFile()
{
    const juce::String filename = "Wishcraft_Mastering_Limiter_Manual.pdf";

   #if JUCE_MAC
    const juce::StringArray systemDirs {
        "/Library/Audio/Plug-Ins/VST3/",
        "/Library/Audio/Plug-Ins/Components/",
    };
    for (auto& dir : systemDirs)
    {
        juce::File f (dir + filename);
        if (f.existsAsFile())
            return f;
    }
    auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    for (auto* sub : { "Library/Audio/Plug-Ins/VST3/", "Library/Audio/Plug-Ins/Components/" })
    {
        auto f = home.getChildFile (juce::String (sub) + filename);
        if (f.existsAsFile())
            return f;
    }
   #elif JUCE_WINDOWS
    const auto programFiles = juce::SystemStats::getEnvironmentVariable ("ProgramFiles", "C:\\Program Files");
    juce::File f (programFiles + "\\Common Files\\VST3\\" + filename);
    if (f.existsAsFile())
        return f;
   #endif

    return {};
}

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
        attributedText.append (text + "\n", juce::FontOptions (15.0f, juce::Font::bold), StudioConsoleTheme::groupLabel);
    }

    void addBody (const juce::String& text)
    {
        attributedText.append (text + "\n\n", juce::FontOptions (13.5f), StudioConsoleTheme::controlValueText);
    }

    void addTip (const juce::String& label, const juce::String& text)
    {
        attributedText.append (label + " ", juce::FontOptions (13.5f, juce::Font::bold), StudioConsoleTheme::lufsLabel);
        attributedText.append (text + "\n\n", juce::FontOptions (13.5f), StudioConsoleTheme::controlValueText);
    }

    // Fine print -- smaller and dimmer than the rest of the content, so the credit is
    // present without competing with the actual how-to-use-it material above it.
    void addCopyright (const juce::String& text)
    {
        attributedText.append (text, juce::FontOptions (11.0f), StudioConsoleTheme::controlEndLabel);
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
                 "3. Limiter -- dial in Threshold, Release, Link, and Gain Match last.\n\n"
                 "This is the opposite order from most limiter plugins, where you'd normally start with "
                 "the limiter itself.");

        addHeader ("ADJUSTING VALUES");
        addBody ("Click-drag any knob or slider to change it -- knobs respond to vertical movement, "
                 "sliders track the mouse directly. Hold Shift while dragging for finer resolution. "
                 "Ctrl/Cmd+click any control to type an exact value. Double-click a control to reset it "
                 "to its default.");

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
        addTip ("Gain Match --", "lets you A/B different Limiter settings without the \"louder sounds "
                "better\" bias, since it keeps output level roughly matched regardless of how much gain "
                "reduction is happening. Turn it off before your final render/bounce -- the reduction it "
                "applies is dynamic and isn't meant to set your actual output loudness.");
        addTip ("Delta --", "solos what the Selective Clipper is removing, gained up so it's actually "
                "audible. Use it to dial in Threshold and Selectivity by ear before trusting them blind -- "
                "remember you can push Threshold more aggressively than you'd expect from a standard "
                "clipper without it sounding like distortion.");
        addTip ("Bypass --", "full latency-compensated bypass -- compares the true, unprocessed input "
                "against your processed output.");
        addTip ("Shaping EQ --", "shapes what triggers the Selective Clipper and the Limiter, not just "
                "what they detect -- unlike a compressor's sidechain EQ, the Limiter's copy of this filter "
                "audibly colors your output too, so treat it as a tone control, not something silent "
                "happening behind the scenes.");

        addHeader ("METERS");
        addBody ("SELECTIVE CLIP shows how much the clipper is removing, per channel. GAIN REDUCTION shows "
                 "how hard the Limiter is working. OUTPUT shows final peak level plus Dynamic Range and "
                 "short-term LUFS -- the dot beside each channel turns yellow past your TP Limit, red "
                 "past true 0 dBFS.");

        // juce::String's plain const char* constructor assumes ASCII/Latin-1, not
        // UTF-8 (see its own doc comment in juce_String.cpp) -- the raw UTF-8 bytes for
        // "\xc2\xa9" (c) would get decoded as two separate Latin-1 characters ("Â" then
        // "©") instead of one, so this needs the explicit CharPointer_UTF8 wrapper.
        addCopyright (juce::String (juce::CharPointer_UTF8 (
            "Wishcraft Mastering Limiter -- concept, design, and specification by Glenn Burgos.\n"
            "\xc2\xa9 2026 Glenn Burgos.")));
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
        titleLabel.setColour (juce::Label::textColourId, StudioConsoleTheme::titleText);
        addAndMakeVisible (titleLabel);

        closeButton.setButtonText ("X");
        closeButton.setColour (juce::TextButton::buttonColourId, StudioConsoleTheme::buttonOff);
        closeButton.setColour (juce::TextButton::textColourOffId, StudioConsoleTheme::controlValueText);
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);

        manualButton.setButtonText ("Manual (PDF)");
        manualButton.setColour (juce::TextButton::buttonColourId, StudioConsoleTheme::buttonOff);
        manualButton.setColour (juce::TextButton::textColourOffId, StudioConsoleTheme::controlValueText);
        manualFile = findManualFile();
        if (manualFile.existsAsFile())
        {
            manualButton.onClick = [this] { manualFile.startAsProcess(); };
        }
        else
        {
            manualButton.setEnabled (false);
            manualButton.setTooltip ("Manual not found -- expected installed alongside the plugin.");
        }
        addAndMakeVisible (manualButton);

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
        header.removeFromRight (8);
        manualButton.setBounds (header.removeFromRight (110).reduced (0, 2));
        titleLabel.setBounds (header);
        panel.removeFromTop (8);

        viewport.setBounds (panel);
        const int contentWidth = panel.getWidth() - viewport.getScrollBarThickness() - 4;
        content.setSize (contentWidth, content.computeHeightForWidth (contentWidth));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (StudioConsoleTheme::background.withAlpha (0.75f));
        g.fillRect (getLocalBounds());

        constexpr float cornerRadius = 6.0f;
        juce::DropShadow shadow (juce::Colours::black.withAlpha (0.5f), 18, {});
        juce::Path panelPath;
        panelPath.addRoundedRectangle (panelBounds.toFloat(), cornerRadius);
        shadow.drawForPath (g, panelPath);

        g.setGradientFill (StudioConsoleTheme::panelFill (panelBounds.toFloat()));
        g.fillRoundedRectangle (panelBounds.toFloat(), cornerRadius);
        g.setColour (StudioConsoleTheme::groupBorder);
        g.drawRoundedRectangle (panelBounds.toFloat(), cornerRadius, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panelBounds.contains (e.getPosition()))
            setVisible (false);
    }

private:
    juce::Label titleLabel;
    juce::TextButton closeButton;
    juce::TextButton manualButton;
    juce::File manualFile;
    juce::Viewport viewport;
    HelpContentComponent content;
    juce::Rectangle<int> panelBounds;
};

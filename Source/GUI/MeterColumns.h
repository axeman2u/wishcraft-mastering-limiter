#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../DSP/Metering.h"
#include "LookAndFeel/StudioConsolePainter.h"
#include "LookAndFeel/StudioConsoleTheme.h"
#include "WishcraftRangeBar.h"
#include "WishcraftVMeter.h"

// The three meter columns from the JSFX's @gfx section (COLUMNS 3-5), each polled once
// per editor timer tick via updateFromMetering() -- pure display, no interaction. Group
// panel chrome is just StudioConsolePainter::paintPanelChrome, same as GroupPanel.h --
// kept as a thin wrapper here so column-specific text and generic panel chrome still
// share one paint() call.

namespace WishcraftMeterColumnDetail
{
    inline void paintGroupChrome (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& label)
    {
        StudioConsolePainter::paintPanelChrome (g, bounds, label);
    }
}

//==============================================================================
class SelectiveClipColumn : public juce::Component
{
public:
    static constexpr float barMaxDb = 12.0f;

    SelectiveClipColumn()
    {
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
    }

    void updateFromMetering (const Metering& m)
    {
        heldL = m.getCharHeldL();
        heldR = m.getCharHeldR();
        meterL.setFractions (m.getCharLiveL() / barMaxDb, m.getCharHeldL() / barMaxDb);
        meterR.setFractions (m.getCharLiveR() / barMaxDb, m.getCharHeldR() / barMaxDb);
        repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (30); // group label
        readoutBounds = b.removeFromTop (52);
        scaleTopBounds = b.removeFromTop (18);
        scaleBottomBounds = b.removeFromBottom (18);
        auto bars = b.reduced (10, 4);
        meterL.setBounds (bars.removeFromLeft (bars.getWidth() / 2).reduced (6, 0));
        meterR.setBounds (bars.reduced (6, 0));
    }

    void paint (juce::Graphics& g) override
    {
        WishcraftMeterColumnDetail::paintGroupChrome (g, getLocalBounds(), "SELECTIVE CLIP");

        g.setFont (juce::FontOptions (13.0f));
        auto readouts = readoutBounds; // local copy -- paint() runs every repaint, don't consume the member
        auto lRow = readouts.removeFromTop (20);
        auto rRow = readouts.removeFromTop (20);
        g.setColour (StudioConsoleTheme::readoutLabel);
        g.drawText ("L " + juce::String (heldL, 2) + " dB", lRow.reduced (8, 0), juce::Justification::centredLeft, false);
        g.drawText ("R " + juce::String (heldR, 2) + " dB", rRow.reduced (8, 0), juce::Justification::centredLeft, false);

        g.setColour (StudioConsoleTheme::scaleLabel);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("0dB", scaleTopBounds.reduced (8, 0), juce::Justification::centredLeft, false);
        g.drawText ("-12dB", scaleBottomBounds.reduced (8, 0), juce::Justification::centredLeft, false);
    }

private:
    WishcraftVMeter meterL { StudioConsoleTheme::charMeterL, true };
    WishcraftVMeter meterR { StudioConsoleTheme::charMeterR, true };
    float heldL = 0.0f, heldR = 0.0f;
    juce::Rectangle<int> readoutBounds, scaleTopBounds, scaleBottomBounds;
};

//==============================================================================
class GainReductionColumn : public juce::Component
{
public:
    static constexpr float barMaxDb = 24.0f;

    GainReductionColumn()
    {
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
    }

    void updateFromMetering (const Metering& m)
    {
        heldL = m.getGrHeldL();
        heldR = m.getGrHeldR();
        meterL.setFractions (-m.getGrLiveL() / barMaxDb, -m.getGrHeldL() / barMaxDb);
        meterR.setFractions (-m.getGrLiveR() / barMaxDb, -m.getGrHeldR() / barMaxDb);
        repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (30);
        readoutBounds = b.removeFromTop (52);
        scaleTopBounds = b.removeFromTop (18);
        scaleBottomBounds = b.removeFromBottom (18);
        auto bars = b.reduced (10, 4);
        meterL.setBounds (bars.removeFromLeft (bars.getWidth() / 2).reduced (6, 0));
        meterR.setBounds (bars.reduced (6, 0));
    }

    void paint (juce::Graphics& g) override
    {
        WishcraftMeterColumnDetail::paintGroupChrome (g, getLocalBounds(), "GAIN REDUCTION");

        g.setFont (juce::FontOptions (13.0f));
        auto readouts = readoutBounds; // local copy -- paint() runs every repaint, don't consume the member
        auto lRow = readouts.removeFromTop (20);
        auto rRow = readouts.removeFromTop (20);
        g.setColour (StudioConsoleTheme::readoutLabel);
        g.drawText ("L " + juce::String (heldL, 2) + " dB", lRow.reduced (8, 0), juce::Justification::centredLeft, false);
        g.drawText ("R " + juce::String (heldR, 2) + " dB", rRow.reduced (8, 0), juce::Justification::centredLeft, false);

        g.setColour (StudioConsoleTheme::scaleLabel);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("0dB", scaleTopBounds.reduced (8, 0), juce::Justification::centredLeft, false);
        g.drawText ("-24dB", scaleBottomBounds.reduced (8, 0), juce::Justification::centredLeft, false);
    }

private:
    WishcraftVMeter meterL { StudioConsoleTheme::grMeterL, true };
    WishcraftVMeter meterR { StudioConsoleTheme::grMeterR, true };
    float heldL = 0.0f, heldR = 0.0f;
    juce::Rectangle<int> readoutBounds, scaleTopBounds, scaleBottomBounds;
};

//==============================================================================
class OutputColumn : public juce::Component
{
public:
    static constexpr float barRangeDb = 18.0f;

    OutputColumn()
    {
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
        addAndMakeVisible (rangeBar);
    }

    void updateFromMetering (const Metering& m)
    {
        heldL = m.getPeakHeldL();
        heldR = m.getPeakHeldR();
        yellowL = m.getOverYellowL(); redL = m.getOverRedL();
        yellowR = m.getOverYellowR(); redR = m.getOverRedR();
        dynRangeDb = m.getDynamicRangeDb();
        lufs = m.getLufsShortTermDb();

        meterL.setFractions ((m.getPeakLiveL() + barRangeDb) / barRangeDb, (m.getPeakHeldL() + barRangeDb) / barRangeDb);
        meterR.setFractions ((m.getPeakLiveR() + barRangeDb) / barRangeDb, (m.getPeakHeldR() + barRangeDb) / barRangeDb);
        rangeBar.setFractions ((m.getDynamicRangeMinDb() + barRangeDb) / barRangeDb,
                                (m.getDynamicRangeMaxDb() + barRangeDb) / barRangeDb);
        repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (30);
        readoutBounds = b.removeFromTop (36);
        extraReadoutBounds = b.removeFromTop (36);
        scaleBottomBounds = b.removeFromBottom (18);
        auto bars = b.reduced (10, 4);
        rangeBar.setBounds (bars.removeFromLeft (14).reduced (2, 0));
        bars.removeFromLeft (4);
        meterL.setBounds (bars.removeFromLeft (bars.getWidth() / 2).reduced (6, 0));
        meterR.setBounds (bars.reduced (6, 0));
    }

    void paint (juce::Graphics& g) override
    {
        WishcraftMeterColumnDetail::paintGroupChrome (g, getLocalBounds(), "OUTPUT");

        g.setFont (juce::FontOptions (13.0f));
        auto readouts = readoutBounds; // local copy -- paint() runs every repaint, don't consume the member
        auto lRow = readouts.removeFromTop (18);
        auto rRow = readouts.removeFromTop (18);
        g.setColour (StudioConsoleTheme::readoutLabel);
        g.drawText ("L " + juce::String (heldL, 2) + " dBTP", lRow.reduced (8, 0), juce::Justification::centredLeft, false);
        g.drawText ("R " + juce::String (heldR, 2) + " dBTP", rRow.reduced (8, 0), juce::Justification::centredLeft, false);

        const int dotR = 5;
        auto dotColour = [] (bool red, bool yellow)
        {
            return red ? StudioConsoleTheme::overRed : (yellow ? StudioConsoleTheme::overYellow : StudioConsoleTheme::overNeutral);
        };
        g.setColour (dotColour (redL, yellowL));
        g.fillEllipse (juce::Rectangle<float> ((float) (2 * dotR), (float) (2 * dotR))
                            .withCentre ({ (float) (lRow.getRight() - 14), (float) lRow.getCentreY() }));
        g.setColour (dotColour (redR, yellowR));
        g.fillEllipse (juce::Rectangle<float> ((float) (2 * dotR), (float) (2 * dotR))
                            .withCentre ({ (float) (rRow.getRight() - 14), (float) rRow.getCentreY() }));

        auto extraReadouts = extraReadoutBounds; // local copy -- same reasoning as readouts above
        auto dynRow = extraReadouts.removeFromTop (18);
        auto lufsRow = extraReadouts.removeFromTop (18);
        g.setColour (StudioConsoleTheme::dynRangeLabel);
        g.drawText ("Dyn " + juce::String (dynRangeDb, 1) + " dB", dynRow.reduced (8, 0), juce::Justification::centredLeft, false);
        g.setColour (StudioConsoleTheme::lufsLabel);
        g.drawText ("ST " + juce::String (lufs, 1) + " LUFS", lufsRow.reduced (8, 0), juce::Justification::centredLeft, false);

        g.setColour (StudioConsoleTheme::scaleLabel);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("-18dBTP", scaleBottomBounds.reduced (8, 0), juce::Justification::centredLeft, false);
    }

private:
    WishcraftVMeter meterL { StudioConsoleTheme::peakMeter, false };
    WishcraftVMeter meterR { StudioConsoleTheme::peakMeter, false };
    WishcraftRangeBar rangeBar;
    float heldL = -60.0f, heldR = -60.0f;
    bool yellowL = false, redL = false, yellowR = false, redR = false;
    float dynRangeDb = 0.0f, lufs = -70.0f;
    juce::Rectangle<int> readoutBounds, extraReadoutBounds, scaleBottomBounds;
};

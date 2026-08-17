#include "PluginEditor.h"

WishcraftMasteringLimiterAudioProcessorEditor::WishcraftMasteringLimiterAudioProcessorEditor (
    WishcraftMasteringLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (gainGroup);
    addAndMakeVisible (clipperGroup);
    addAndMakeVisible (sidechainGroup);
    addAndMakeVisible (limiterGroup);
    addAndMakeVisible (utilityGroup);

    // ---- Column 1: GAIN / SELECTIVE CLIPPER / SIDECHAIN EQ ----
    inputGainKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("input_gain_db"), "Input Gain",
        [] (float v) { return juce::String (v, 1) + " dB"; });
    addAndMakeVisible (*inputGainKnob);

    thresholdSlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("threshold_db"), "Threshold",
        [] (float v) { return juce::String (v, 1) + " dB"; }, "-24", "0");
    addAndMakeVisible (*thresholdSlider);

    selectivitySlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("selectivity"), "Selectivity",
        [] (float v) { return juce::String (v, 0) + "%"; }, "Transparent", "Aggressive");
    addAndMakeVisible (*selectivitySlider);

    deltaButton = std::make_unique<WishcraftButton> (
        rangedParam ("listen_mode"), "Delta", WishcraftColours::buttonYellow);
    addAndMakeVisible (*deltaButton);

    scLowSlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("sc_low_shelf_db"), "Low",
        [] (float v) { return juce::String (v, 1) + " dB"; }, "-6", "+6");
    addAndMakeVisible (*scLowSlider);

    scHighSlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("sc_high_shelf_db"), "High",
        [] (float v) { return juce::String (v, 1) + " dB"; }, "-6", "+6");
    addAndMakeVisible (*scHighSlider);

    // ---- Column 2: LIMITER / UTILITY ----
    ceilingKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("ceiling_db"), "Ceiling",
        [] (float v) { return juce::String (v, 1) + " dB"; });
    addAndMakeVisible (*ceilingKnob);

    lookaheadKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("lookahead_ms"), "Lookahead",
        [] (float v) { return juce::String (v, 1) + " ms"; });
    addAndMakeVisible (*lookaheadKnob);

    releaseKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("release_pct"), "Release",
        [] (float v) { return juce::String (v, 0) + "%"; });
    addAndMakeVisible (*releaseKnob);

    linkKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("link_pct"), "Link",
        [] (float v) { return juce::String (v, 0) + "%"; });
    addAndMakeVisible (*linkKnob);

    outputCeilingKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("output_ceiling_db"), "Out Ceiling",
        [] (float v) { return juce::String (v, 2) + " dB"; });
    addAndMakeVisible (*outputCeilingKnob);

    autoGainButton = std::make_unique<WishcraftButton> (
        rangedParam ("limiter_auto_gain"), "Auto Gain", WishcraftColours::buttonYellow);
    addAndMakeVisible (*autoGainButton);

    osChoiceRadio = std::make_unique<WishcraftRadioPair> (
        rangedParam ("os_choice"), "2x", "4x", WishcraftColours::buttonBlue);
    addAndMakeVisible (*osChoiceRadio);

    bypassButton = std::make_unique<WishcraftButton> (
        rangedParam ("bypass"), "Bypass", WishcraftColours::buttonYellow);
    addAndMakeVisible (*bypassButton);

    // ---- Columns 3-5: meters ----
    addAndMakeVisible (selectiveClipColumn);
    addAndMakeVisible (gainReductionColumn);
    addAndMakeVisible (outputColumn);

    setSize (1000, 640);
    startTimerHz (30);
}

WishcraftMasteringLimiterAudioProcessorEditor::~WishcraftMasteringLimiterAudioProcessorEditor()
{
    stopTimer();
}

juce::RangedAudioParameter& WishcraftMasteringLimiterAudioProcessorEditor::rangedParam (const juce::String& id)
{
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (processorRef.getAPVTS().getParameter (id));
    jassert (param != nullptr);
    return *param;
}

void WishcraftMasteringLimiterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (WishcraftColours::background);

    g.setColour (WishcraftColours::titleText);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("WISHCRAFT MASTERING LIMITER", getLocalBounds().removeFromTop (36),
                juce::Justification::centred, false);
}

void WishcraftMasteringLimiterAudioProcessorEditor::resized()
{
    constexpr int colTop = 44;
    constexpr int gap = 20;
    constexpr int headerH = 26; // room GroupPanel's own label needs before content can start

    // Control box sizes: knobs/sliders both reserve a taller label row than their text
    // strictly needs (20px, not ~13px) so descenders in letters like "g"/"y" never sit
    // on top of the track/circle below -- found by inspection, not just measurement.
    constexpr int knobW = 100, knobH = 92;
    constexpr int hsliderH = 70;

    // ---- Column 1 ----
    constexpr int c1X = 10, c1W = 220;
    constexpr int inH = 126, chH = 196, scH = 172;
    constexpr int colH = inH + chH + scH + gap * 2;
    const int inY = colTop;
    const int chY = inY + inH + gap;
    const int scY = chY + chH + gap;

    gainGroup.setBounds (c1X, inY, c1W, inH);
    {
        auto interior = gainGroup.getBounds().withTrimmedTop (headerH);
        inputGainKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre (interior.getCentre()));
    }

    clipperGroup.setBounds (c1X, chY, c1W, chH);
    {
        auto interior = clipperGroup.getBounds().withTrimmedTop (headerH).reduced (4, 2);
        thresholdSlider->setBounds (interior.removeFromTop (hsliderH));
        selectivitySlider->setBounds (interior.removeFromTop (hsliderH));
        interior.removeFromTop (4);
        auto row = interior.removeFromTop (22);
        deltaButton->setBounds (juce::Rectangle<int> (70, 22).withCentre (row.getCentre()));
    }

    sidechainGroup.setBounds (c1X, scY, c1W, scH);
    {
        auto interior = sidechainGroup.getBounds().withTrimmedTop (headerH).reduced (4, 2);
        scLowSlider->setBounds (interior.removeFromTop (hsliderH));
        scHighSlider->setBounds (interior.removeFromTop (hsliderH));
    }

    // ---- Column 2 ----
    constexpr int c2X = c1X + c1W + gap, c2W = 220;
    constexpr int limH = 370;
    constexpr int utlH = colH - limH - gap;
    const int limY = colTop;
    const int utlY = limY + limH + gap;

    limiterGroup.setBounds (c2X, limY, c2W, limH);
    {
        auto interior = limiterGroup.getBounds().withTrimmedTop (headerH).reduced (4, 2);
        auto row1 = interior.removeFromTop (knobH);
        ceilingKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre ({ row1.getX() + row1.getWidth() / 4, row1.getCentreY() }));
        lookaheadKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre ({ row1.getX() + row1.getWidth() * 3 / 4, row1.getCentreY() }));
        auto row2 = interior.removeFromTop (knobH);
        releaseKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre ({ row2.getX() + row2.getWidth() / 4, row2.getCentreY() }));
        linkKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre ({ row2.getX() + row2.getWidth() * 3 / 4, row2.getCentreY() }));
        interior.removeFromTop (6);
        auto row3 = interior.removeFromTop (knobH);
        outputCeilingKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre (row3.getCentre()));
        interior.removeFromTop (6);
        auto row4 = interior.removeFromTop (22);
        autoGainButton->setBounds (juce::Rectangle<int> (90, 22).withCentre (row4.getCentre()));
    }

    utilityGroup.setBounds (c2X, utlY, c2W, utlH);
    {
        auto interior = utilityGroup.getBounds().withTrimmedTop (headerH).reduced (4, 2);
        auto row1 = interior.removeFromTop (22);
        osChoiceRadio->setBounds (juce::Rectangle<int> (146, 22).withCentre (row1.getCentre()));
        interior.removeFromTop (8);
        auto row2 = interior.removeFromTop (22);
        bypassButton->setBounds (juce::Rectangle<int> (90, 22).withCentre (row2.getCentre()));
    }

    // ---- Columns 3-5: meters ----
    constexpr int mColW = 145;
    const int mCol1X = c2X + c2W + gap;
    const int mCol2X = mCol1X + mColW + gap;
    const int mCol3X = mCol2X + mColW + gap;

    selectiveClipColumn.setBounds (mCol1X, colTop, mColW, colH);
    gainReductionColumn.setBounds (mCol2X, colTop, mColW, colH);
    outputColumn.setBounds (mCol3X, colTop, mColW, colH);
}

void WishcraftMasteringLimiterAudioProcessorEditor::timerCallback()
{
    const auto& metering = processorRef.getMetering();
    selectiveClipColumn.updateFromMetering (metering);
    gainReductionColumn.updateFromMetering (metering);
    outputColumn.updateFromMetering (metering);
}

#include "PluginEditor.h"

WishcraftMasteringLimiterAudioProcessorEditor::WishcraftMasteringLimiterAudioProcessorEditor (
    WishcraftMasteringLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (contentComponent);

    titleLabel.setText ("WISHCRAFT MASTERING LIMITER", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, WishcraftColours::titleText);
    titleLabel.setJustificationType (juce::Justification::centred);
    contentComponent.addAndMakeVisible (titleLabel);

    helpButton.setButtonText ("?");
    helpButton.setColour (juce::TextButton::buttonColourId, WishcraftColours::buttonOff);
    helpButton.setColour (juce::TextButton::textColourOffId, WishcraftColours::controlValueText);
    helpButton.onClick = [this] { helpOverlay.setVisible (true); helpOverlay.toFront (true); };
    contentComponent.addAndMakeVisible (helpButton);

    // Covers the whole design canvas, starts hidden; brought to front and shown only
    // when helpButton is clicked (see above) or Options-menu equivalent later.
    // addChildComponent() (not addAndMakeVisible()) so it actually starts hidden --
    // addAndMakeVisible() unconditionally forces visible=true on add, which would
    // undo a setVisible(false) called beforehand.
    helpOverlay.setBounds (0, 0, designW, designH);
    contentComponent.addChildComponent (helpOverlay);

    contentComponent.addAndMakeVisible (gainGroup);
    contentComponent.addAndMakeVisible (clipperGroup);
    contentComponent.addAndMakeVisible (sidechainGroup);
    contentComponent.addAndMakeVisible (limiterGroup);
    contentComponent.addAndMakeVisible (utilityGroup);

    // ---- Column 1: GAIN / SELECTIVE CLIPPER / SIDECHAIN EQ ----
    inputGainKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("input_gain_db"), "Input Gain",
        [] (float v) { return juce::String (v, 1) + " dB"; });
    contentComponent.addAndMakeVisible (*inputGainKnob);

    thresholdSlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("threshold_db"), "Threshold",
        [] (float v) { return juce::String (v, 1) + " dB"; }, "-24", "0");
    contentComponent.addAndMakeVisible (*thresholdSlider);

    selectivitySlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("selectivity"), "Selectivity",
        [] (float v) { return juce::String (v, 0) + "%"; }, "Transparent", "Aggressive");
    contentComponent.addAndMakeVisible (*selectivitySlider);

    deltaButton = std::make_unique<WishcraftButton> (
        rangedParam ("listen_mode"), "Delta", WishcraftColours::buttonYellow);
    contentComponent.addAndMakeVisible (*deltaButton);

    scLowSlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("sc_low_shelf_db"), "Low",
        [] (float v) { return juce::String (v, 1) + " dB"; }, "-6", "+6");
    contentComponent.addAndMakeVisible (*scLowSlider);

    scHighSlider = std::make_unique<WishcraftHSlider> (
        rangedParam ("sc_high_shelf_db"), "High",
        [] (float v) { return juce::String (v, 1) + " dB"; }, "-6", "+6");
    contentComponent.addAndMakeVisible (*scHighSlider);

    // ---- Column 2: LIMITER / UTILITY ----
    ceilingKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("ceiling_db"), "Ceiling",
        [] (float v) { return juce::String (v, 1) + " dB"; });
    contentComponent.addAndMakeVisible (*ceilingKnob);

    lookaheadKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("lookahead_ms"), "Lookahead",
        [] (float v) { return juce::String (v, 1) + " ms"; });
    contentComponent.addAndMakeVisible (*lookaheadKnob);

    releaseKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("release_pct"), "Release",
        [] (float v) { return juce::String (v, 0) + "%"; });
    contentComponent.addAndMakeVisible (*releaseKnob);

    linkKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("link_pct"), "Link",
        [] (float v) { return juce::String (v, 0) + "%"; });
    contentComponent.addAndMakeVisible (*linkKnob);

    outputCeilingKnob = std::make_unique<WishcraftKnob> (
        rangedParam ("output_ceiling_db"), "Out Ceiling",
        [] (float v) { return juce::String (v, 2) + " dB"; });
    contentComponent.addAndMakeVisible (*outputCeilingKnob);

    autoGainButton = std::make_unique<WishcraftButton> (
        rangedParam ("limiter_auto_gain"), "Auto Gain", WishcraftColours::buttonYellow);
    contentComponent.addAndMakeVisible (*autoGainButton);

    osChoiceRadio = std::make_unique<WishcraftRadioPair> (
        rangedParam ("os_choice"), "2x", "4x", WishcraftColours::buttonBlue);
    contentComponent.addAndMakeVisible (*osChoiceRadio);

    bypassButton = std::make_unique<WishcraftButton> (
        rangedParam ("bypass"), "Bypass", WishcraftColours::buttonYellow);
    contentComponent.addAndMakeVisible (*bypassButton);

    // ---- Columns 3-5: meters ----
    contentComponent.addAndMakeVisible (selectiveClipColumn);
    contentComponent.addAndMakeVisible (gainReductionColumn);
    contentComponent.addAndMakeVisible (outputColumn);

    // Content is laid out once, at a fixed design resolution -- resized() never
    // touches these bounds again, only contentComponent's scale transform.
    contentComponent.setBounds (0, 0, designW, designH);
    layoutContent();

    setResizable (true, true);
    setResizeLimits ((int) (designW * minScale), (int) (designH * minScale),
                      (int) (designW * maxScale), (int) (designH * maxScale));
    setSize (designW, designH);
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
    // Covers the WHOLE window, including any letterbox/pillarbox margin outside
    // contentComponent's scaled area -- matches the JSFX filling gfx_w x gfx_h (the
    // actual window), not just its DESIGN_W x DESIGN_H canvas.
    g.fillAll (WishcraftColours::background);
}

void WishcraftMasteringLimiterAudioProcessorEditor::resized()
{
    // contentComponent's own bounds/children never change (see layoutContent()) --
    // resizing the window only ever recomputes this transform, matching the JSFX's
    // SCALE = min(gfx_w/DESIGN_W, gfx_h/DESIGN_H), clamped to [MIN_SCALE, MAX_SCALE],
    // with OFF_X/OFF_Y centering the result. Content always scales uniformly and
    // never stretches to fill a mismatched aspect ratio -- any leftover space becomes
    // a plain background margin on one axis, same as the JSFX.
    const float scaleX = (float) getWidth() / (float) designW;
    const float scaleY = (float) getHeight() / (float) designH;
    const float scale = juce::jlimit (minScale, maxScale, juce::jmin (scaleX, scaleY));

    const float offX = ((float) getWidth() - (float) designW * scale) * 0.5f;
    const float offY = ((float) getHeight() - (float) designH * scale) * 0.5f;

    contentComponent.setTransform (juce::AffineTransform::scale (scale).translated (offX, offY));
}

void WishcraftMasteringLimiterAudioProcessorEditor::layoutContent()
{
    constexpr int colTop = 44;
    constexpr int gap = 20;
    constexpr int headerH = 26; // room GroupPanel's own label needs before content can start

    // Control box sizes: knobs/sliders both reserve a taller label row than their text
    // strictly needs (20px, not ~13px) so descenders in letters like "g"/"y" never sit
    // on top of the track/circle below -- found by inspection, not just measurement.
    constexpr int knobW = 100, knobH = 92;
    constexpr int hsliderH = 70;

    titleLabel.setBounds (0, 0, designW, 36);
    helpButton.setBounds (designW - 34, 6, 24, 24);
    helpOverlay.setBounds (0, 0, designW, designH);

    // ---- Column 1 ----
    // SELECTIVE CLIPPER now sits above GAIN, matching both the actual signal path
    // (Selective Clipper -> Input Gain -> Limiter) and the real workflow: dial in the
    // clipper first, then use Input Gain to drive the limiter -- not the other way
    // around. A pure layout swap; the underlying processing order never changes.
    constexpr int c1X = 10, c1W = 220;
    constexpr int inH = 126, chH = 196, scH = 172;
    constexpr int colH = inH + chH + scH + gap * 2;
    const int chY = colTop;
    const int inY = chY + chH + gap;
    const int scY = inY + inH + gap;

    clipperGroup.setBounds (c1X, chY, c1W, chH);
    {
        auto interior = clipperGroup.getBounds().withTrimmedTop (headerH).reduced (4, 2);
        thresholdSlider->setBounds (interior.removeFromTop (hsliderH));
        selectivitySlider->setBounds (interior.removeFromTop (hsliderH));
        interior.removeFromTop (4);
        auto row = interior.removeFromTop (22);
        deltaButton->setBounds (juce::Rectangle<int> (70, 22).withCentre (row.getCentre()));
    }

    gainGroup.setBounds (c1X, inY, c1W, inH);
    {
        auto interior = gainGroup.getBounds().withTrimmedTop (headerH);
        inputGainKnob->setBounds (juce::Rectangle<int> (knobW, knobH).withCentre (interior.getCentre()));
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

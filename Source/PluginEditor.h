#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/GroupPanel.h"
#include "GUI/MeterColumns.h"
#include "GUI/WishcraftButton.h"
#include "GUI/WishcraftColours.h"
#include "GUI/WishcraftHSlider.h"
#include "GUI/WishcraftKnob.h"
#include "GUI/WishcraftRadioPair.h"
#include "PluginProcessor.h"

// Rebuilds the JSFX's @gfx custom layout in real JUCE components: the same broad
// three-macro-column arrangement (GAIN/SELECTIVE CLIPPER/SIDECHAIN EQ stacked on the
// left, LIMITER/UTILITY stacked in the middle, three meter columns on the right), the
// same group/column labels, and the same four mouse-interaction behaviors
// (click-drag, Shift+drag fine adjustment, Ctrl/Cmd+click text entry, double-click
// reset) via WishcraftKnob/WishcraftHSlider. Pixel-exact spacing/sizing was
// deprioritized this session per explicit instruction -- controls-present-and-
// functionally-correct came first; a follow-up session can refine the geometry. One
// deliberate departure from the JSFX throughout: every control's label is centered
// horizontally over its control, not left-aligned with a fixed offset.
class WishcraftMasteringLimiterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                        private juce::Timer
{
public:
    explicit WishcraftMasteringLimiterAudioProcessorEditor (WishcraftMasteringLimiterAudioProcessor&);
    ~WishcraftMasteringLimiterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::RangedAudioParameter& rangedParam (const juce::String& id);

    WishcraftMasteringLimiterAudioProcessor& processorRef;

    // Group panel backgrounds (painted first / sent to back).
    GroupPanel gainGroup { "GAIN" };
    GroupPanel clipperGroup { "SELECTIVE CLIPPER" };
    GroupPanel sidechainGroup { "SIDECHAIN EQ" };
    GroupPanel limiterGroup { "LIMITER" };
    GroupPanel utilityGroup { "UTILITY" };

    // Column 1: GAIN / SELECTIVE CLIPPER / SIDECHAIN EQ.
    std::unique_ptr<WishcraftKnob> inputGainKnob;
    std::unique_ptr<WishcraftHSlider> thresholdSlider;
    std::unique_ptr<WishcraftHSlider> selectivitySlider;
    std::unique_ptr<WishcraftButton> deltaButton;
    std::unique_ptr<WishcraftHSlider> scLowSlider;
    std::unique_ptr<WishcraftHSlider> scHighSlider;

    // Column 2: LIMITER / UTILITY.
    std::unique_ptr<WishcraftKnob> ceilingKnob;
    std::unique_ptr<WishcraftKnob> lookaheadKnob;
    std::unique_ptr<WishcraftKnob> releaseKnob;
    std::unique_ptr<WishcraftKnob> linkKnob;
    std::unique_ptr<WishcraftKnob> outputCeilingKnob;
    std::unique_ptr<WishcraftButton> autoGainButton;
    std::unique_ptr<WishcraftRadioPair> osChoiceRadio;
    std::unique_ptr<WishcraftButton> bypassButton;

    // Columns 3-5: meters.
    SelectiveClipColumn selectiveClipColumn;
    GainReductionColumn gainReductionColumn;
    OutputColumn outputColumn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WishcraftMasteringLimiterAudioProcessorEditor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/GroupPanel.h"
#include "GUI/HelpOverlay.h"
#include "GUI/LookAndFeel/StudioConsoleLookAndFeel.h" // not applied here (see its own doc comment) -- included so it stays compiled/verified as part of the normal build
#include "GUI/LookAndFeel/StudioConsoleTheme.h"
#include "GUI/MeterColumns.h"
#include "GUI/TrialOverlay.h" // no-op include in normal builds -- see its own WISHCRAFT_TRIAL_BUILD guard
#include "GUI/WishcraftButton.h"
#include "GUI/WishcraftHSlider.h"
#include "GUI/WishcraftKnob.h"
#include "GUI/WishcraftRadioGroup.h"
#include "PluginProcessor.h"

// Rebuilds the JSFX's @gfx custom layout in real JUCE components: the same broad
// three-macro-column arrangement (GAIN/SELECTIVE CLIPPER/SHAPING EQ stacked on the
// left, LIMITER/UTILITY stacked in the middle, three meter columns on the right), the
// same group/column labels, and the same four mouse-interaction behaviors
// (click-drag, Shift+drag fine adjustment, Ctrl/Cmd+click text entry, double-click
// reset) via WishcraftKnob/WishcraftHSlider. Pixel-exact spacing/sizing was
// deprioritized this session per explicit instruction -- controls-present-and-
// functionally-correct came first; a follow-up session can refine the geometry. One
// deliberate departure from the JSFX throughout: every control's label is centered
// horizontally over its control, not left-aligned with a fixed offset.
//
// Resizing matches the JSFX's own SCALE/OFF_X/OFF_Y scheme: all real content is laid
// out ONCE at a fixed "design" resolution inside contentComponent, which the window
// can be resized to any size/aspect ratio freely -- resized() just recomputes a single
// uniform scale factor (the smaller of width-ratio/height-ratio, clamped to
// [minScale, maxScale], same bounds as the JSFX's MIN_SCALE/MAX_SCALE) and applies it
// as an AffineTransform on contentComponent, centered with letterbox/pillarbox margins
// exactly like the JSFX's OFF_X/OFF_Y -- content always scales uniformly, never
// stretches non-uniformly to fill a mismatched aspect ratio.
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
    void layoutContent();

    juce::RangedAudioParameter& rangedParam (const juce::String& id);

    WishcraftMasteringLimiterAudioProcessor& processorRef;

    static constexpr int designW = 1000, designH = 640;
    static constexpr float minScale = 0.5f, maxScale = 3.0f; // matches JSFX MIN_SCALE/MAX_SCALE

    // Holds every real child component, laid out once at (designW, designH) and never
    // resized itself -- resized() only ever changes its AffineTransform, never its
    // bounds. Everything below that used to be added directly to the editor now
    // belongs to this instead.
    juce::Component contentComponent;
    juce::Label titleLabel;
    juce::TextButton helpButton;
    HelpOverlay helpOverlay;

    // Group panel backgrounds (painted first / sent to back).
    GroupPanel gainGroup { "GAIN" };
    GroupPanel clipperGroup { "SELECTIVE CLIPPER" };
    GroupPanel sidechainGroup { "SHAPING EQ" };
    GroupPanel limiterGroup { "LIMITER" };
    GroupPanel utilityGroup { "UTILITY" };

    // Column 1: GAIN / SELECTIVE CLIPPER / SHAPING EQ. Renamed from "SIDECHAIN EQ"
    // (the sidechainGroup/sc* identifiers below still say "sidechain" -- that's still
    // the accurate DSP term) because "sidechain" specifically implies
    // detector-only/inaudible, compressor-style, and that's only true for the
    // Selective Clipper's copy of this filter -- the Limiter's copy is NOT
    // detector-only (see Limiter.h), so the JSFX-matching behavior really does color
    // the audible output when these sliders move. "Sidechain EQ" over-promised
    // silence that half of what it controls doesn't actually deliver.
    std::unique_ptr<WishcraftKnob> inputGainKnob;
    std::unique_ptr<WishcraftHSlider> thresholdSlider;
    std::unique_ptr<WishcraftHSlider> selectivitySlider;
    std::unique_ptr<WishcraftButton> deltaButton;
    std::unique_ptr<WishcraftKnob> deltaTrimKnob;
    std::unique_ptr<WishcraftHSlider> scLowSlider;
    std::unique_ptr<WishcraftHSlider> scHighSlider;

    // Column 2: LIMITER / UTILITY.
    std::unique_ptr<WishcraftKnob> ceilingKnob;
    std::unique_ptr<WishcraftKnob> lookaheadKnob;
    std::unique_ptr<WishcraftKnob> releaseKnob;
    std::unique_ptr<WishcraftKnob> linkKnob;
    std::unique_ptr<WishcraftKnob> outputCeilingKnob;
    std::unique_ptr<WishcraftButton> gainMatchButton;
    // Unlike WishcraftKnob/WishcraftHSlider, WishcraftRadioGroup has no built-in name
    // label of its own (its segments just show "2x"/"4x"/"8x") -- this stands in for
    // one, since nothing else in UTILITY said what those numbers actually control.
    juce::Label oversamplingLabel;
    std::unique_ptr<WishcraftRadioGroup> osChoiceRadio;
    std::unique_ptr<WishcraftButton> bypassButton;

    // Columns 3-5: meters.
    SelectiveClipColumn selectiveClipColumn;
    GainReductionColumn gainReductionColumn;
    OutputColumn outputColumn;

   #if WISHCRAFT_TRIAL_BUILD
    juce::Label trialStatusLabel;
    TrialExpiredOverlay trialOverlay;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WishcraftMasteringLimiterAudioProcessorEditor)
};

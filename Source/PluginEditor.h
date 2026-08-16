#pragma once

#include "PluginProcessor.h"

//==============================================================================
class WishcraftMasteringLimiterAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit WishcraftMasteringLimiterAudioProcessorEditor (WishcraftMasteringLimiterAudioProcessor&);
    ~WishcraftMasteringLimiterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Reference to the processor that owns this editor.
    WishcraftMasteringLimiterAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WishcraftMasteringLimiterAudioProcessorEditor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/PolyphaseOversampler.h"
#include "DSP/SelectiveClipper.h"

//==============================================================================
class WishcraftMasteringLimiterAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    WishcraftMasteringLimiterAudioProcessor();
    ~WishcraftMasteringLimiterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    void reconfigureEngine (int osChoiceIndex);

    // JSFX slider1: os_choice (0 = 2x, 1 = 4x).
    juce::AudioParameterChoice* osChoiceParam = nullptr;
    // JSFX slider2: threshold_db -- Selective Clip Threshold.
    juce::AudioParameterFloat* thresholdParam = nullptr;
    // JSFX slider3: character -- Selectivity (0 = Transparent .. 100 = Aggressive).
    juce::AudioParameterFloat* selectivityParam = nullptr;

    PolyphaseOversampler oversamplerL, oversamplerR;
    SelectiveClipper selectiveClipper;

    int currentOsChoiceIndex = -1; // forces a reconfigure on the first block
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WishcraftMasteringLimiterAudioProcessor)
};

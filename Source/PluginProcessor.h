#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/Limiter.h"
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
    // JSFX slider5: ceiling_db -- Limiter Ceiling.
    juce::AudioParameterFloat* ceilingParam = nullptr;
    // JSFX slider7: release_pct -- Release.
    juce::AudioParameterFloat* releaseParam = nullptr;
    // JSFX slider8: input_gain_db -- Input Gain (Drive), after Selective Clipper, before Limiter.
    juce::AudioParameterFloat* inputGainParam = nullptr;
    // JSFX slider9: output_ceiling_db -- True Peak Output Ceiling (only used this stage to
    // cap Limiter Auto Gain; the safety clip itself isn't ported yet).
    juce::AudioParameterFloat* outputCeilingParam = nullptr;
    // JSFX slider10: link_pct -- Stereo Link.
    juce::AudioParameterFloat* linkParam = nullptr;
    // JSFX slider14: limiter_auto_gain -- Limiter Auto Gain (user-toggleable, unlike the
    // Selective Clipper's permanently-on Auto Makeup Gain).
    juce::AudioParameterBool* limiterAutoGainParam = nullptr;
    // JSFX slider11/12: sc_low_shelf_db/sc_high_shelf_db -- Sidechain EQ. Feeds two
    // separate filter instances (Selective Clipper's detector, Limiter's detector/audio
    // path); see Limiter.h for why the Limiter's isn't strictly detector-only.
    juce::AudioParameterFloat* scLowShelfParam = nullptr;
    juce::AudioParameterFloat* scHighShelfParam = nullptr;

    // TEMPORARY (Stage 4 only): read-only gain-reduction readout so GR can be verified by
    // eye before any real metering/GUI exists. Updated once per block, not per sample.
    juce::AudioParameterFloat* grMeterLParam = nullptr;
    juce::AudioParameterFloat* grMeterRParam = nullptr;

    PolyphaseOversampler oversamplerL, oversamplerR;
    SelectiveClipper selectiveClipper;
    Limiter limiter;

    int currentOsChoiceIndex = -1; // forces a reconfigure on the first block
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WishcraftMasteringLimiterAudioProcessor)
};

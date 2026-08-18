#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/Limiter.h"
#include "DSP/Metering.h"
#include "DSP/PolyphaseOversampler.h"
#include "DSP/SafetyClip.h"
#include "DSP/SelectiveClipper.h"
#include "DSP/TruePeakLimiter.h"

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

    //==============================================================================
    // For the editor: APVTS access to build ParameterAttachments for custom controls,
    // and read-only access to Metering's atomics for the meter displays.
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const Metering& getMetering() const noexcept { return metering; }

private:
    //==============================================================================
    void reconfigureEngine (int osChoiceIndex, double lookaheadMs);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Single source of truth for every processor parameter -- the 14 JSFX sliders plus
    // the TEMPORARY debug meter readouts (see Metering.h), all registered through one
    // ParameterLayout (createParameterLayout()) rather than the old mixed addParameter()
    // calls, so host automation and getStateInformation/setStateInformation both work
    // correctly for the whole parameter set, not just some of it.
    juce::AudioProcessorValueTreeState apvts;

    // Cached typed pointers into apvts's own parameter objects, looked up once in the
    // constructor body (apvts.getParameter(id)) -- every existing call site
    // (thresholdParam->get(), osChoiceParam->getIndex(), etc.) stays unchanged from
    // before the APVTS migration, while now going through APVTS for automation/save-load.
    //
    // JSFX slider1: os_choice (0 = 2x, 1 = 4x).
    juce::AudioParameterChoice* osChoiceParam = nullptr;
    // JSFX slider2: threshold_db -- Selective Clip Threshold.
    juce::AudioParameterFloat* thresholdParam = nullptr;
    // JSFX slider3: character -- labeled "Selectivity" in the UI (the underlying JSFX
    // slider variable is "character"; the GUI section itself is called "Selective
    // Clipper", but the control it holds is the selectivity/aggressiveness setting).
    juce::AudioParameterFloat* selectivityParam = nullptr;
    // JSFX slider5: ceiling_db -- displayed as "Threshold" (param ID stays ceiling_db for
    // host automation/state compatibility): it's a smoothed, program-dependent-release
    // target the signal can transiently overshoot, not a hard-enforced ceiling -- true
    // peak safety is TruePeakLimiter/SafetyClip's job now, not this control's.
    juce::AudioParameterFloat* ceilingParam = nullptr;
    // JSFX slider7: release_pct -- Release.
    juce::AudioParameterFloat* releaseParam = nullptr;
    // JSFX slider8: input_gain_db -- Input Gain (Drive), after Selective Clipper, before Limiter.
    juce::AudioParameterFloat* inputGainParam = nullptr;
    // JSFX slider9: output_ceiling_db -- displayed as "TP Limit". Drives the True Peak
    // Limiter's target ceiling (Source/DSP/TruePeakLimiter.h), which does the real
    // true-peak-safety work via smooth gain reduction. No longer caps Limiter Auto Gain
    // (fixed at 0.0 dB now, strictly cut-only) or feeds Safety Clip (a fixed 0.0 dBFS
    // backstop now, fully decoupled from this parameter).
    juce::AudioParameterFloat* outputCeilingParam = nullptr;
    // JSFX slider10: link_pct -- Stereo Link.
    juce::AudioParameterFloat* linkParam = nullptr;
    // JSFX slider14: limiter_auto_gain -- Limiter Auto Gain (user-toggleable, unlike
    // the Selective Clipper's permanently-on Auto Makeup Gain).
    juce::AudioParameterBool* limiterAutoGainParam = nullptr;
    // JSFX slider11/12: sc_low_shelf_db/sc_high_shelf_db -- Sidechain EQ. Feeds two
    // separate filter instances (Selective Clipper's detector, Limiter's detector/audio
    // path); see Limiter.h for why the Limiter's isn't strictly detector-only.
    juce::AudioParameterFloat* scLowShelfParam = nullptr;
    juce::AudioParameterFloat* scHighShelfParam = nullptr;
    // JSFX slider13: bypass -- latency-compensated Bypass. Priority order is
    // Bypass > Delta > Normal.
    juce::AudioParameterBool* bypassParam = nullptr;
    // JSFX slider4: listen_mode -- Delta Listen Mode. See PluginProcessor.cpp's
    // processBlock for the actual signal-path implementation.
    juce::AudioParameterChoice* listenModeParam = nullptr;
    // NOT a JSFX slider -- added after the port, per explicit user request, because the
    // fixed +6 dB Delta boost (SelectiveClipper::deltaGainDb) can be startlingly loud
    // with aggressive clipping. Added to deltaGainDb (in dB) before conversion to linear
    // gain in processBlock's Delta branch; 0.0 dB default reproduces the original fixed
    // behavior exactly. This supersedes the older "DELTA_GAIN_DB must not become a
    // parameter" note in Wishcraft_Limiter_Spec.md, which predates this request.
    juce::AudioParameterFloat* deltaTrimParam = nullptr;

    // JSFX slider6: lookahead_ms -- Limiter Lookahead. Wired the same way the JSFX
    // itself wires it: a change is treated exactly like an os_choice change, triggering
    // a full reconfigureEngine()/state reset (see processBlock's top-of-block check)
    // and changing the reported PDC latency (Limiter::getLaBudgetBase() feeds
    // setLatencySamples()).
    juce::AudioParameterFloat* lookaheadParam = nullptr;

    // TEMPORARY debug readouts, updated once per block: no real GUI/meter display exists
    // yet, so these ride JUCE's generic parameter list as a way to verify the numbers.
    // The real values live in Metering's atomics (metering.getGrHeldL(), etc.) -- these
    // parameters just mirror them for visibility. grMeterL/R source from the Held
    // (peak-hold) value, not the live GR, matching the spec's "text readouts show the
    // held value" requirement.
    juce::AudioParameterFloat* grMeterLParam = nullptr;
    juce::AudioParameterFloat* grMeterRParam = nullptr;
    juce::AudioParameterFloat* peakMeterLParam = nullptr;
    juce::AudioParameterFloat* peakMeterRParam = nullptr;
    juce::AudioParameterFloat* charMeterLParam = nullptr;
    juce::AudioParameterFloat* charMeterRParam = nullptr;
    juce::AudioParameterFloat* dynamicRangeParam = nullptr;
    juce::AudioParameterFloat* lufsParam = nullptr;
    juce::AudioParameterBool* overYellowParam = nullptr;
    juce::AudioParameterBool* overRedParam = nullptr;

    PolyphaseOversampler oversamplerL, oversamplerR;
    SelectiveClipper selectiveClipper;
    Limiter limiter;
    TruePeakLimiter truePeakLimiter; // smooth-gain-reduction true-peak stage, before Safety Clip
    SafetyClip safetyClip; // one shared instance, fixed 0.0 dBFS backstop -- see SafetyClip.h
    Metering metering;

    int currentOsChoiceIndex = -1; // forces a reconfigure on the first block
    double currentLookaheadMs = -1.0; // forces a reconfigure on the first block
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WishcraftMasteringLimiterAudioProcessor)
};

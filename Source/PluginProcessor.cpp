#include "PluginProcessor.h"

#include <cmath>

//==============================================================================
WishcraftMasteringLimiterAudioProcessor::WishcraftMasteringLimiterAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // JSFX slider1:os_choice=1<0,1,1{2x,4x}> -- default index 1 is "4x".
    addParameter (osChoiceParam = new juce::AudioParameterChoice (
        juce::ParameterID { "os_choice", 1 },
        "Oversampling Factor",
        juce::StringArray { "2x", "4x" },
        1));

    // JSFX slider2:threshold_db=-3<-24,0,0.01>-Selective Clip Threshold (dB)
    addParameter (thresholdParam = new juce::AudioParameterFloat (
        juce::ParameterID { "threshold_db", 1 },
        "Selective Clip Threshold",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        -3.0f));

    // JSFX slider3:character=50<0,100,0.1>-Selectivity (0=Transparent .. 100=Aggressive)
    addParameter (selectivityParam = new juce::AudioParameterFloat (
        juce::ParameterID { "selectivity", 1 },
        "Selectivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f));

    // JSFX slider5:ceiling_db=-1<-18,0,0.01>-Limiter Ceiling (dB)
    addParameter (ceilingParam = new juce::AudioParameterFloat (
        juce::ParameterID { "ceiling_db", 1 },
        "Limiter Ceiling",
        juce::NormalisableRange<float> (-18.0f, 0.0f, 0.01f),
        -1.0f));

    // JSFX slider7:release_pct=30<0,100,0.1>-Release
    addParameter (releaseParam = new juce::AudioParameterFloat (
        juce::ParameterID { "release_pct", 1 },
        "Release",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f));

    // JSFX slider8:input_gain_db=0<0,24,0.1>-Input Gain (Drive)
    addParameter (inputGainParam = new juce::AudioParameterFloat (
        juce::ParameterID { "input_gain_db", 1 },
        "Input Gain (Drive)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
        0.0f));

    // JSFX slider9:output_ceiling_db=-1<-3,0,0.01>-True Peak Output Ceiling (safety clip)
    // -- displayed as "Peak Ceiling": the Safety Clip guarantees the discrete sample
    // peak, not a certified true/inter-sample-peak bound (see Limiter.h's ISP_MARGIN_DB
    // comment), so the display name doesn't claim more than what's actually delivered.
    addParameter (outputCeilingParam = new juce::AudioParameterFloat (
        juce::ParameterID { "output_ceiling_db", 1 },
        "Peak Ceiling",
        juce::NormalisableRange<float> (-3.0f, 0.0f, 0.01f),
        -1.0f));

    // JSFX slider10:link_pct=75<0,100,0.1>-Stereo Link
    addParameter (linkParam = new juce::AudioParameterFloat (
        juce::ParameterID { "link_pct", 1 },
        "Stereo Link",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        75.0f));

    // JSFX slider14:limiter_auto_gain=0<0,1,1{Off,On}>-Limiter Auto Gain
    addParameter (limiterAutoGainParam = new juce::AudioParameterBool (
        juce::ParameterID { "limiter_auto_gain", 1 },
        "Limiter Auto Gain",
        false));

    // JSFX slider11:sc_low_shelf_db=0<-6,6,0.1>-Sidechain Low Shelf (dB)
    addParameter (scLowShelfParam = new juce::AudioParameterFloat (
        juce::ParameterID { "sc_low_shelf_db", 1 },
        "Sidechain Low Shelf",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f),
        0.0f));

    // JSFX slider12:sc_high_shelf_db=0<-6,6,0.1>-Sidechain High Shelf (dB)
    addParameter (scHighShelfParam = new juce::AudioParameterFloat (
        juce::ParameterID { "sc_high_shelf_db", 1 },
        "Sidechain High Shelf",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f),
        0.0f));

    // JSFX slider13:bypass=0<0,1,1{Off,On}>-Bypass (latency-compensated)
    addParameter (bypassParam = new juce::AudioParameterBool (
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false));

    // TEMPORARY debug readouts, updated once per block. Marked non-automatable since
    // they're outputs, not controls -- will be replaced by a real meter once the GUI
    // stage exists. Values shown are the HELD (peak-hold) readings, matching the spec's
    // "text readouts show the held value, not the live one".
    auto meterAttributes = juce::AudioParameterFloatAttributes().withAutomatable (false);
    addParameter (grMeterLParam = new juce::AudioParameterFloat (
        juce::ParameterID { "gr_meter_l", 1 },
        "GR Meter L (dB)",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        0.0f,
        meterAttributes));
    addParameter (grMeterRParam = new juce::AudioParameterFloat (
        juce::ParameterID { "gr_meter_r", 1 },
        "GR Meter R (dB)",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        0.0f,
        meterAttributes));

    addParameter (peakMeterLParam = new juce::AudioParameterFloat (
        juce::ParameterID { "peak_meter_l", 1 },
        "Peak Meter L (dB)",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        -60.0f,
        meterAttributes));
    addParameter (peakMeterRParam = new juce::AudioParameterFloat (
        juce::ParameterID { "peak_meter_r", 1 },
        "Peak Meter R (dB)",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        -60.0f,
        meterAttributes));

    addParameter (charMeterLParam = new juce::AudioParameterFloat (
        juce::ParameterID { "char_meter_l", 1 },
        "Selective Clip Activity L (dB)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f),
        0.0f,
        meterAttributes));
    addParameter (charMeterRParam = new juce::AudioParameterFloat (
        juce::ParameterID { "char_meter_r", 1 },
        "Selective Clip Activity R (dB)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f),
        0.0f,
        meterAttributes));

    addParameter (dynamicRangeParam = new juce::AudioParameterFloat (
        juce::ParameterID { "dynamic_range_db", 1 },
        "Dynamic Range (dB)",
        juce::NormalisableRange<float> (0.0f, 30.0f, 0.01f),
        0.0f,
        meterAttributes));

    addParameter (lufsParam = new juce::AudioParameterFloat (
        juce::ParameterID { "lufs_st", 1 },
        "Short-Term LUFS",
        juce::NormalisableRange<float> (-70.0f, 0.0f, 0.01f),
        -70.0f,
        meterAttributes));

    auto boolMeterAttributes = juce::AudioParameterBoolAttributes().withAutomatable (false);
    addParameter (overYellowParam = new juce::AudioParameterBool (
        juce::ParameterID { "over_yellow", 1 },
        "Peak Over (Ceiling)",
        false,
        boolMeterAttributes));
    addParameter (overRedParam = new juce::AudioParameterBool (
        juce::ParameterID { "over_red", 1 },
        "Peak Over (0 dBFS)",
        false,
        boolMeterAttributes));
}

WishcraftMasteringLimiterAudioProcessor::~WishcraftMasteringLimiterAudioProcessor()
{
}

//==============================================================================
const juce::String WishcraftMasteringLimiterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool WishcraftMasteringLimiterAudioProcessor::acceptsMidi() const
{
    return false;
}

bool WishcraftMasteringLimiterAudioProcessor::producesMidi() const
{
    return false;
}

bool WishcraftMasteringLimiterAudioProcessor::isMidiEffect() const
{
    return false;
}

double WishcraftMasteringLimiterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int WishcraftMasteringLimiterAudioProcessor::getNumPrograms()
{
    return 1;   // Some hosts don't cope well with 0 programs.
}

int WishcraftMasteringLimiterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void WishcraftMasteringLimiterAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String WishcraftMasteringLimiterAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void WishcraftMasteringLimiterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void WishcraftMasteringLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;
    selectiveClipper.prepare (sampleRate);
    // JSFX @init: threshold_smoothed = threshold_db (no ramp-in on load).
    selectiveClipper.setInitialThreshold ((double) thresholdParam->get());
    selectiveClipper.setInitialShelf ((double) scLowShelfParam->get(), (double) scHighShelfParam->get());

    limiter.prepare (sampleRate);
    limiter.setInitialSmoothedParams ((double) ceilingParam->get(),
                                       (double) outputCeilingParam->get(),
                                       (double) inputGainParam->get(),
                                       (double) scLowShelfParam->get(),
                                       (double) scHighShelfParam->get());
    // JSFX @init: safety_ceiling_lin is first computed in @sample from the already-
    // correct (non-ramped) output_ceiling_smoothed set just above -- matching that here.
    safetyClip.setOutputCeilingSmoothed ((double) outputCeilingParam->get());

    metering.prepare (sampleRate);

    currentOsChoiceIndex = -1; // forces reconfigureEngine() to run on the first block
    reconfigureEngine (osChoiceParam->getIndex());
}

void WishcraftMasteringLimiterAudioProcessor::releaseResources()
{
}

bool WishcraftMasteringLimiterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The JSFX declares exactly two in_pin/out_pin (left/right) -- fixed stereo
    // I/O, no mono option -- so the port matches that rather than allowing mono.
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// JSFX @slider's (os_choice != last_os_choice) check, run at the top of the block
// instead since JUCE has no per-parameter-change callback guarantee as tight as
// REAPER's @slider -- functionally the same "only reconfigure when it actually
// changed" behaviour reconfigure()/@init implement.
void WishcraftMasteringLimiterAudioProcessor::reconfigureEngine (int osChoiceIndex)
{
    currentOsChoiceIndex = osChoiceIndex;
    const int factor = (osChoiceIndex == 0) ? 2 : 4;
    const double osRate = currentSampleRate * factor;

    oversamplerL.setFactor (factor);
    oversamplerR.setFactor (factor);
    selectiveClipper.setFactor (factor, osRate);
    limiter.setFactor (factor, osRate);
    metering.setFactor (osRate);

    // JSFX @block: pdc_delay = DELAY_SAMPLES_BASE + CHAR_BUDGET_BASE + LA_BUDGET_BASE.
    // All three contributions are now ported. Factor-independent by construction, same
    // as the JSFX -- switching 2x/4x never changes the reported latency.
    setLatencySamples (PolyphaseOversampler::delaySamplesBase
                        + selectiveClipper.getCharBudgetBase()
                        + limiter.getLaBudgetBase());
}

void WishcraftMasteringLimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int osChoiceIndex = osChoiceParam->getIndex();
    if (osChoiceIndex != currentOsChoiceIndex)
        reconfigureEngine (osChoiceIndex);

    jassert (buffer.getNumChannels() == 2);
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    const int factor = oversamplerL.getFactor();
    double upL[PolyphaseOversampler::maxFactor];
    double upR[PolyphaseOversampler::maxFactor];

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        // JSFX @sample (top-of-block): all these are recomputed once per host sample,
        // reused across all `factor` oversampled ticks below.
        const double scLowShelfDb = (double) scLowShelfParam->get();
        const double scHighShelfDb = (double) scHighShelfParam->get();
        selectiveClipper.setParameters ((double) thresholdParam->get(), (double) selectivityParam->get(),
                                         scLowShelfDb, scHighShelfDb);
        limiter.setParameters ((double) ceilingParam->get(),
                                (double) releaseParam->get(),
                                (double) linkParam->get(),
                                (double) inputGainParam->get(),
                                (double) outputCeilingParam->get(),
                                limiterAutoGainParam->get(),
                                scLowShelfDb, scHighShelfDb);
        // safety_ceiling_lin is derived from the same output_ceiling_smoothed the
        // Limiter just updated above -- pulled from there rather than re-smoothed here.
        safetyClip.setOutputCeilingSmoothed (limiter.getOutputCeilingSmoothed());
        const bool bypassOn = bypassParam->get();

        oversamplerL.upsample ((double) left[n],  upL);
        oversamplerR.upsample ((double) right[n], upR);

        double outL = 0.0, outR = 0.0;
        double outDryRawL = 0.0, outDryRawR = 0.0;
        // Last oversampled tick's dry/clipped-pre-makeup pair, for the Character
        // Activity meter -- matches the JSFX reading dry_L/char_L once per host sample
        // as whatever they were left at by the final tick of the block.
        double lastDryL = 0.0, lastDryR = 0.0, lastCharL = 0.0, lastCharR = 0.0;

        for (int j = 0; j < factor; ++j)
        {
            // Stage 7: Selective Clipper -> Input Gain -> Limiter -> two-stage Safety
            // Clip. The Bypass raw-dry path is computed every tick regardless of Bypass
            // state (matching the JSFX's "full wet path always runs" design, just
            // without its CPU-optimization of skipping the unused downsample FIR).
            double clipL, clipR, dryL, dryR, charL, charR;
            selectiveClipper.processTick (upL[j], upR[j], clipL, clipR, dryL, dryR, charL, charR);
            lastDryL = dryL; lastDryR = dryR; lastCharL = charL; lastCharR = charR;

            double stageL, stageR, dryRawL, dryRawR;
            limiter.processTick (clipL, clipR, dryL, dryR, stageL, stageR, dryRawL, dryRawR);

            // Peak meter reads the Limiter's output BEFORE the oversampled-domain
            // safety clip -- matches the JSFX exactly (tells you how hard the safety
            // net had to work, not just that it worked).
            metering.updatePeakTick (stageL, stageR);

            // Oversampled-domain safety clip -- applied only to the processed signal,
            // not the raw-dry Bypass reference (matches the JSFX exactly).
            stageL = safetyClip.clip (stageL);
            stageR = safetyClip.clip (stageR);

            const double downL = oversamplerL.downsample (j, stageL);
            const double downR = oversamplerR.downsample (j, stageR);
            const double downRawL = oversamplerL.downsampleSecondary (j, dryRawL);
            const double downRawR = oversamplerR.downsampleSecondary (j, dryRawR);

            if (j == 0)
            {
                outL = downL;
                outR = downR;
                outDryRawL = downRawL;
                outDryRawR = downRawR;
            }
        }

        // Post-downsample backstop clip -- both paths, so whichever one gets selected
        // below is already correctly clipped.
        outL = safetyClip.clip (outL);
        outR = safetyClip.clip (outR);
        outDryRawL = safetyClip.clip (outDryRawL);
        outDryRawR = safetyClip.clip (outDryRawR);

        // Bypass > Delta > Normal. Delta (listen_mode) isn't ported yet, so this
        // reduces to Bypass > Normal for now.
        double finalOutL, finalOutR;
        if (bypassOn)
        {
            finalOutL = outDryRawL;
            finalOutR = outDryRawR;
        }
        else
        {
            finalOutL = outL;
            finalOutR = outR;
        }

        left[n]  = (float) finalOutL;
        right[n] = (float) finalOutR;

        // Everything else: GR display/hold, Character Activity, all peak-holds, Over
        // indicators, Dynamic Range, and short-term LUFS (measured on the actual
        // audible output, so it reflects Bypass too, matching the JSFX).
        metering.updateOncePerHostSample (limiter.getLastGrL(), limiter.getLastGrR(),
                                           lastDryL, lastDryR, lastCharL, lastCharR,
                                           limiter.getOutputCeilingSmoothed(),
                                           finalOutL, finalOutR);
    }

    // TEMPORARY: mirror Metering's held/derived values onto the debug readout
    // parameters so they're visible in REAPER's generic FX param list. Clamped to each
    // param's declared range before converting -- several of these (Peak, LUFS) are
    // genuinely unbounded below in the underlying calculation (e.g. the peak meter eases
    // toward roughly -200dB during silence, matching the JSFX's own floor), but the
    // debug readout's range only needs to cover the musically-relevant span.
    auto mirror = [] (juce::AudioParameterFloat* param, float value)
    {
        param->setValueNotifyingHost (param->range.convertTo0to1 (juce::jlimit (param->range.start, param->range.end, value)));
    };
    mirror (grMeterLParam, metering.getGrHeldL());
    mirror (grMeterRParam, metering.getGrHeldR());
    mirror (peakMeterLParam, metering.getPeakHeldL());
    mirror (peakMeterRParam, metering.getPeakHeldR());
    mirror (charMeterLParam, metering.getCharHeldL());
    mirror (charMeterRParam, metering.getCharHeldR());
    mirror (dynamicRangeParam, metering.getDynamicRangeDb());
    mirror (lufsParam, metering.getLufsShortTermDb());
    overYellowParam->setValueNotifyingHost (metering.getOverYellow() ? 1.0f : 0.0f);
    overRedParam->setValueNotifyingHost (metering.getOverRed() ? 1.0f : 0.0f);
}

//==============================================================================
bool WishcraftMasteringLimiterAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* WishcraftMasteringLimiterAudioProcessor::createEditor()
{
    // No custom GUI this session -- JUCE's stock generic editor is enough to
    // switch os_choice (2x/4x) for null testing, without writing any bespoke
    // GUI code ahead of the later GUI stage.
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void WishcraftMasteringLimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void WishcraftMasteringLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WishcraftMasteringLimiterAudioProcessor();
}

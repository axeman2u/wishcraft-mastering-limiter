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
    // -- only used this stage to cap Limiter Auto Gain; the safety clip itself is a later stage.
    addParameter (outputCeilingParam = new juce::AudioParameterFloat (
        juce::ParameterID { "output_ceiling_db", 1 },
        "True Peak Output Ceiling",
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

    // TEMPORARY (Stage 4 only): read-only GR readout, updated once per block. Marked
    // non-automatable since it's an output, not a control -- will be replaced by a real
    // meter once the GUI stage exists.
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

    limiter.prepare (sampleRate);
    limiter.setInitialSmoothedParams ((double) ceilingParam->get(),
                                       (double) outputCeilingParam->get(),
                                       (double) inputGainParam->get());

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
        selectiveClipper.setParameters ((double) thresholdParam->get(), (double) selectivityParam->get());
        limiter.setParameters ((double) ceilingParam->get(),
                                (double) releaseParam->get(),
                                (double) linkParam->get(),
                                (double) inputGainParam->get(),
                                (double) outputCeilingParam->get(),
                                limiterAutoGainParam->get());

        oversamplerL.upsample ((double) left[n],  upL);
        oversamplerR.upsample ((double) right[n], upR);

        double outL = 0.0, outR = 0.0;

        for (int j = 0; j < factor; ++j)
        {
            // Stage 4: Selective Clipper -> Input Gain -> Limiter -- straight to output,
            // no Sidechain EQ or safety clipping yet.
            double clipL, clipR, dryL, dryR;
            selectiveClipper.processTick (upL[j], upR[j], clipL, clipR, dryL, dryR);

            double stageL, stageR;
            limiter.processTick (clipL, clipR, dryL, dryR, stageL, stageR);

            const double downL = oversamplerL.downsample (j, stageL);
            const double downR = oversamplerR.downsample (j, stageR);

            if (j == 0)
            {
                outL = downL;
                outR = downR;
            }
        }

        left[n]  = (float) outL;
        right[n] = (float) outR;
    }

    // TEMPORARY (Stage 4 only): push the last block's final (post-Link) GR to the
    // read-only meter parameters so it's visible in REAPER's generic FX param list.
    grMeterLParam->setValueNotifyingHost (grMeterLParam->range.convertTo0to1 ((float) limiter.getLastGrL()));
    grMeterRParam->setValueNotifyingHost (grMeterRParam->range.convertTo0to1 ((float) limiter.getLastGrR()));
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

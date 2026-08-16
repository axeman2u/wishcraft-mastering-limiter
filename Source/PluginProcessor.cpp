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
    lookaheadL.prepare (sampleRate);
    lookaheadR.prepare (sampleRate);

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

    oversamplerL.setFactor (factor);
    oversamplerR.setFactor (factor);

    // JSFX reconfigure(): LA_BUDGET_BASE = ceil(lookahead_ms * 0.001 * srate);
    // la_buf_size = LA_BUDGET_BASE * factor.
    const int laBudgetBase = (int) std::ceil (lookaheadMsStage3 * 0.001 * currentSampleRate);
    const int laBufSize = laBudgetBase * factor;
    lookaheadL.setActiveLength (laBufSize);
    lookaheadR.setActiveLength (laBufSize);

    // JSFX @block: pdc_delay = DELAY_SAMPLES_BASE + CHAR_BUDGET_BASE + LA_BUDGET_BASE.
    // CHAR_BUDGET_BASE (the Selective Clipper's buffer) isn't ported yet, so this
    // stage's reported latency is smaller than the full JSFX's by exactly that
    // amount until a later stage adds it. Factor-independent by construction, same
    // as the JSFX -- switching 2x/4x never changes the reported latency.
    setLatencySamples (PolyphaseOversampler::delaySamplesBase + laBudgetBase);
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
        oversamplerL.upsample ((double) left[n],  upL);
        oversamplerR.upsample ((double) right[n], upR);

        double outL = 0.0, outR = 0.0;

        for (int j = 0; j < factor; ++j)
        {
            // Stage 3 scaffold: the Limiter's lookahead buffer is ported as a pure
            // delay line -- no gain reduction, clipping, or Selective Clipper yet,
            // so the signal passes through unmodified aside from the delay itself.
            const double stageL = lookaheadL.process (upL[j]);
            const double stageR = lookaheadR.process (upR[j]);

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

#include "PluginProcessor.h"

#include <cmath>

#include "PluginEditor.h"
#include "TrialLicense.h"

//==============================================================================
WishcraftMasteringLimiterAudioProcessor::WishcraftMasteringLimiterAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    osChoiceParam        = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("os_choice"));
    thresholdParam       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("threshold_db"));
    selectivityParam     = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("selectivity"));
    ceilingParam         = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("ceiling_db"));
    releaseParam         = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("release_pct"));
    lookaheadParam       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("lookahead_ms"));
    inputGainParam       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("input_gain_db"));
    outputCeilingParam   = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("output_ceiling_db"));
    linkParam            = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("link_pct"));
    gainMatchParam       = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter ("limiter_auto_gain"));
    scLowShelfParam      = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("sc_low_shelf_db"));
    scHighShelfParam     = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("sc_high_shelf_db"));
    bypassParam          = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter ("bypass"));
    listenModeParam      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("listen_mode"));
    deltaTrimParam       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("delta_trim_db"));

    grMeterLParam       = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("gr_meter_l"));
    grMeterRParam       = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("gr_meter_r"));
    peakMeterLParam     = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("peak_meter_l"));
    peakMeterRParam     = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("peak_meter_r"));
    charMeterLParam     = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("char_meter_l"));
    charMeterRParam     = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("char_meter_r"));
    dynamicRangeParam   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("dynamic_range_db"));
    lufsParam           = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("lufs_st"));
    overYellowParam     = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter ("over_yellow"));
    overRedParam        = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter ("over_red"));

    jassert (osChoiceParam != nullptr && thresholdParam != nullptr && selectivityParam != nullptr
             && ceilingParam != nullptr && releaseParam != nullptr && lookaheadParam != nullptr
             && inputGainParam != nullptr
             && outputCeilingParam != nullptr && linkParam != nullptr && gainMatchParam != nullptr
             && scLowShelfParam != nullptr && scHighShelfParam != nullptr && bypassParam != nullptr
             && listenModeParam != nullptr && deltaTrimParam != nullptr
             && grMeterLParam != nullptr && grMeterRParam != nullptr && peakMeterLParam != nullptr
             && peakMeterRParam != nullptr && charMeterLParam != nullptr && charMeterRParam != nullptr
             && dynamicRangeParam != nullptr && lufsParam != nullptr && overYellowParam != nullptr
             && overRedParam != nullptr);

   #if WISHCRAFT_TRIAL_BUILD
    // Checked once here, not in processBlock -- see TrialLicense.h and this class's
    // trialBlocked/trialDaysRemaining doc comments.
    {
        const auto status = TrialLicense::checkStatus();
        trialBlocked.store (status.blocked, std::memory_order_relaxed);
        trialDaysRemaining.store (status.daysRemaining, std::memory_order_relaxed);
    }
   #endif
}

// All 14 JSFX sliders, each matching its slider line's range/default exactly, plus the
// TEMPORARY debug meter readouts -- registered together in one ParameterLayout so a
// single AudioProcessorValueTreeState (see PluginProcessor.h) is the sole owner of every
// processor parameter, giving correct host automation and state save/load for all of
// them (previously only get/setStateInformation were stubbed out -- nothing was actually
// saved or restored).
juce::AudioProcessorValueTreeState::ParameterLayout WishcraftMasteringLimiterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // JSFX slider1:os_choice=1<0,1,1{2x,4x}>-Oversampling Factor -- extended to a third
    // 8x option (not in the original JSFX) since confirmed CPU headroom (<2% at 4x)
    // leaves ample room, and finer oversampled resolution benefits the True Peak
    // Limiter's peak estimate. Default index (1 = 4x) unchanged.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "os_choice", 1 },
        "Oversampling Factor",
        juce::StringArray { "2x", "4x", "8x" },
        1));

    // JSFX slider2:threshold_db=-3<-24,0,0.01>-Selective Clip Threshold (dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "threshold_db", 1 },
        "Selective Clip Threshold",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        -3.0f));

    // JSFX slider3:character=50<0,100,0.1>-Selectivity (0=Transparent .. 100=Aggressive)
    // -- the JSFX slider variable is "character"; the UI label (and this param's name)
    // is "Selectivity", matching the "Selective Clipper" GUI section's actual control.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "selectivity", 1 },
        "Selectivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f));

    // JSFX slider4:listen_mode=0<0,1,1{Normal,Delta (what was removed)}>-Listen Mode --
    // see processBlock's Bypass > Delta > Normal selection for the actual signal-path
    // implementation.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "listen_mode", 1 },
        "Listen Mode",
        juce::StringArray { "Normal", "Delta (what was removed)" },
        0));

    // NOT a JSFX slider -- added after the port. The fixed +6 dB Delta boost
    // (SelectiveClipper::deltaGainDb) can be startlingly loud when clipping
    // aggressively; this trim is added on top of that fixed value (see processBlock's
    // Delta branch), so 0.0 dB default reproduces the original fixed behavior exactly.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delta_trim_db", 1 },
        "Delta Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f),
        0.0f));

    // JSFX slider5:ceiling_db=-1<-18,0,0.01>-Limiter Ceiling (dB) -- displayed as
    // "Threshold" (param ID kept as ceiling_db for automation/state compatibility): a
    // smoothed target the signal can transiently overshoot, not a hard ceiling -- see
    // PluginProcessor.h's ceilingParam comment. Default changed from the JSFX's -1.0 dB
    // to -0.1 dB per user testing across varying-density/dynamic-range material,
    // rendering true peak consistently between -1 and -0.5 dBTP alongside TP Limit's
    // matching -0.1 dB default below.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ceiling_db", 1 },
        "Threshold",
        juce::NormalisableRange<float> (-18.0f, 0.0f, 0.01f),
        -0.1f));

    // JSFX slider6:lookahead_ms=3<0.5,20,0.1>-Limiter Lookahead (ms). A change is
    // treated exactly like an os_choice change (full reconfigure/state reset, see
    // processBlock's top-of-block check and PluginProcessor.h's lookaheadParam
    // comment), matching the JSFX's own @slider behavior.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lookahead_ms", 1 },
        "Limiter Lookahead",
        juce::NormalisableRange<float> (0.5f, 20.0f, 0.1f),
        3.0f));

    // JSFX slider7:release_pct=30<0,100,0.1>-Release
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release_pct", 1 },
        "Release",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f));

    // JSFX slider8:input_gain_db=0<0,24,0.1>-Input Gain (Drive)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input_gain_db", 1 },
        "Input Gain (Drive)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
        0.0f));

    // JSFX slider9:output_ceiling_db=-1<-3,0,0.01>-True Peak Output Ceiling -- displayed
    // as "TP Limit": drives TruePeakLimiter's target ceiling (smooth gain reduction,
    // genuinely true-peak-aware -- see TruePeakLimiter.h), adjustable up to 0.0 dB.
    // Default changed from the JSFX's -1.0 dB to -0.1 dB per user testing (see
    // ceilingParam's matching default change above).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output_ceiling_db", 1 },
        "TP Limit",
        juce::NormalisableRange<float> (-3.0f, 0.0f, 0.01f),
        -0.1f));

    // JSFX slider10:link_pct=75<0,100,0.1>-Stereo Link
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "link_pct", 1 },
        "Stereo Link",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        75.0f));

    // JSFX slider11:sc_low_shelf_db=0<-6,6,0.1>-Sidechain Low Shelf (dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sc_low_shelf_db", 1 },
        "Sidechain Low Shelf",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f),
        0.0f));

    // JSFX slider12:sc_high_shelf_db=0<-6,6,0.1>-Sidechain High Shelf (dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sc_high_shelf_db", 1 },
        "Sidechain High Shelf",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f),
        0.0f));

    // JSFX slider13:bypass=0<0,1,1{Off,On}>-Bypass (latency-compensated)
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false));

    // JSFX slider14:limiter_auto_gain=0<0,1,1{Off,On}>-Limiter Auto Gain -- displayed as
    // "Gain Match" (renamed per explicit user request; param ID kept for automation/
    // state compatibility). Now gain-matches toward TP Limit rather than the raw
    // pre-processing source -- see Limiter.h's class-level doc comment for why.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "limiter_auto_gain", 1 },
        "Gain Match",
        false));

    // TEMPORARY debug readouts, updated once per block. Marked non-automatable since
    // they're outputs, not controls -- will be replaced by a real meter once the GUI
    // stage exists. Values shown are the HELD (peak-hold) readings, matching the spec's
    // "text readouts show the held value, not the live one".
    auto meterAttributes = juce::AudioParameterFloatAttributes().withAutomatable (false);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gr_meter_l", 1 },
        "GR Meter L (dB)",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        0.0f,
        meterAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gr_meter_r", 1 },
        "GR Meter R (dB)",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.01f),
        0.0f,
        meterAttributes));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "peak_meter_l", 1 },
        "Peak Meter L (dB)",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        -60.0f,
        meterAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "peak_meter_r", 1 },
        "Peak Meter R (dB)",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        -60.0f,
        meterAttributes));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "char_meter_l", 1 },
        "Selective Clip Activity L (dB)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f),
        0.0f,
        meterAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "char_meter_r", 1 },
        "Selective Clip Activity R (dB)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f),
        0.0f,
        meterAttributes));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "dynamic_range_db", 1 },
        "Dynamic Range (dB)",
        juce::NormalisableRange<float> (0.0f, 30.0f, 0.01f),
        0.0f,
        meterAttributes));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lufs_st", 1 },
        "Short-Term LUFS",
        juce::NormalisableRange<float> (-70.0f, 0.0f, 0.01f),
        -70.0f,
        meterAttributes));

    auto boolMeterAttributes = juce::AudioParameterBoolAttributes().withAutomatable (false);
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "over_yellow", 1 },
        "Peak Over (Ceiling)",
        false,
        boolMeterAttributes));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "over_red", 1 },
        "Peak Over (0 dBFS)",
        false,
        boolMeterAttributes));

    return layout;
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

    // The single "program" slot exists only because some AU hosts don't cope well with
    // reporting 0 programs (see getNumPrograms()) -- the JSFX has no programs/presets
    // concept at all, so rather than leaving this a no-op that looks like a broken
    // preset in the host's menu, selecting it resets every real control (the 14 JSFX
    // sliders, not the debug meter readouts) back to its JSFX default value, matching
    // what a freshly-loaded, untouched JSFX instance would show.
    static constexpr const char* realParamIds[] = {
        "os_choice", "threshold_db", "selectivity", "listen_mode", "ceiling_db",
        "lookahead_ms", "release_pct", "input_gain_db", "output_ceiling_db", "link_pct",
        "sc_low_shelf_db", "sc_high_shelf_db", "bypass", "limiter_auto_gain", "delta_trim_db"
    };
    for (auto* id : realParamIds)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getDefaultValue());
}

const juce::String WishcraftMasteringLimiterAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "Default";
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

    truePeakLimiter.prepare (sampleRate);

    metering.prepare (sampleRate);

    currentOsChoiceIndex = -1; // forces reconfigureEngine() to run on the first block
    currentLookaheadMs = -1.0;
    reconfigureEngine (osChoiceParam->getIndex(), (double) lookaheadParam->get());
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
// changed" behaviour reconfigure()/@init implement. lookahead_ms is watched the same
// way and treated identically (a hard reset, not a smoothed change), matching the
// JSFX's own @slider exactly.
void WishcraftMasteringLimiterAudioProcessor::reconfigureEngine (int osChoiceIndex, double lookaheadMs)
{
    currentOsChoiceIndex = osChoiceIndex;
    currentLookaheadMs = lookaheadMs;
    const int factor = (osChoiceIndex == 0) ? 2 : (osChoiceIndex == 1) ? 4 : 8;
    const double osRate = currentSampleRate * factor;

    oversamplerL.setFactor (factor);
    oversamplerR.setFactor (factor);
    selectiveClipper.setFactor (factor, osRate);
    limiter.setFactor (factor, osRate, lookaheadMs);
    truePeakLimiter.setFactor (factor, osRate);
    metering.setFactor (osRate);

    // JSFX @block: pdc_delay = DELAY_SAMPLES_BASE + CHAR_BUDGET_BASE + LA_BUDGET_BASE.
    // Now also includes the True Peak Limiter's own lookahead budget (a genuine addition
    // over the JSFX, not a straight port). Factor-independent by construction, same as
    // the JSFX (switching 2x/4x/8x never changes the reported latency on its own) --
    // but LA_BUDGET_BASE itself now genuinely varies with the Lookahead knob, same as
    // the JSFX's own reported latency does.
    setLatencySamples (PolyphaseOversampler::delaySamplesBase
                        + selectiveClipper.getCharBudgetBase()
                        + limiter.getLaBudgetBase()
                        + truePeakLimiter.getLaBudgetBase());
}

void WishcraftMasteringLimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

   #if WISHCRAFT_TRIAL_BUILD
    // Trial period ended (or the license file is missing/tampered/backdated -- see
    // TrialLicense.h) -- silence and bail before touching anything else. Cheap atomic
    // read; the actual filesystem check only ever happens once, in the constructor.
    if (trialBlocked.load (std::memory_order_relaxed))
    {
        buffer.clear();
        return;
    }
   #endif

    const int osChoiceIndex = osChoiceParam->getIndex();
    const double lookaheadMs = (double) lookaheadParam->get();
    if (osChoiceIndex != currentOsChoiceIndex || ! juce::exactlyEqual (lookaheadMs, currentLookaheadMs))
        reconfigureEngine (osChoiceIndex, lookaheadMs);

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
                                gainMatchParam->get(),
                                scLowShelfDb, scHighShelfDb);
        // TruePeakLimiter's target is derived from the same output_ceiling_smoothed the
        // Limiter just updated above -- pulled from there rather than re-smoothed here.
        truePeakLimiter.setTargetCeiling (limiter.getOutputCeilingSmoothed());
        const bool bypassOn = bypassParam->get();
        const bool listenModeOn = listenModeParam->getIndex() > 0;
        const double deltaTrimDb = (double) deltaTrimParam->get();
        // Shared by the per-tick metering tap below and the final per-host-sample Delta
        // output further down, so both agree on the same gain.
        const double deltaGainLin = std::pow (10.0, (SelectiveClipper::deltaGainDb + deltaTrimDb) / 20.0);

        oversamplerL.upsample ((double) left[n],  upL);
        oversamplerR.upsample ((double) right[n], upR);

        double outL = 0.0, outR = 0.0;
        double outDryRawL = 0.0, outDryRawR = 0.0;
        double outDryGainedL = 0.0, outDryGainedR = 0.0;
        // Last oversampled tick's dry/clipped-pre-makeup pair, for the Character
        // Activity meter -- matches the JSFX reading dry_L/char_L once per host sample
        // as whatever they were left at by the final tick of the block.
        double lastDryL = 0.0, lastDryR = 0.0, lastCharL = 0.0, lastCharR = 0.0;

        for (int j = 0; j < factor; ++j)
        {
            // Selective Clipper -> Input Gain -> Limiter -> two-stage Safety Clip. The
            // Bypass raw-dry and Delta gained-dry/Character-only paths are all computed
            // every tick regardless of mode (matching the JSFX's "full wet path always
            // runs" design, just without its CPU-optimization of skipping unused
            // downsample FIRs).
            double clipL, clipR, dryL, dryR, charL, charR;
            selectiveClipper.processTick (upL[j], upR[j], clipL, clipR, dryL, dryR, charL, charR);
            lastDryL = dryL; lastDryR = dryR; lastCharL = charL; lastCharR = charR;

            double stageL, stageR, dryRawL, dryRawR;
            double charOnlyDelayedL, charOnlyDelayedR, dryGainedDelayedL, dryGainedDelayedR;
            limiter.processTick (clipL, clipR, dryL, dryR, stageL, stageR, dryRawL, dryRawR,
                                  charOnlyDelayedL, charOnlyDelayedR, dryGainedDelayedL, dryGainedDelayedR);

            // True Peak Limiter: smooth gain reduction targeting TP Limit, protecting
            // the Limiter's actual output -- BEFORE any Delta substitution below. This
            // must run before the substitution, not after: Delta's "what was removed"
            // math is gainedDry - charOnly, and charOnly needs to stay an untouched
            // reference for that subtraction to mean anything, the same principle as
            // Bypass's raw-dry reference staying untouched by any processing. Running
            // this after the substitution (an earlier mistake) fed a continuously
            // gain-modulated version of charOnly into the subtraction instead -- since
            // TP Limit engages far more often than the old rare hard-clip-only safety
            // net did, that modulation was audible as the full program bleeding into
            // Delta's output.
            truePeakLimiter.processTick (stageL, stageR, stageL, stageR);

            // Delta Listen Mode: substitute the Limiter's output with the delay-matched,
            // gained Selective Clipper output BEFORE metering/safety-clip/downsample --
            // matches the JSFX's `listen_mode > 0.5 ? stage_L = charonly_la_L`. This
            // makes Delta compare dry against what the SELECTIVE CLIPPER did, not
            // against the Limiter's gain reduction (confirmed by reading @sample's
            // final spl0/spl1 selection, not just the "Delta" naming/spec wording).
            // charOnlyDelayedL/R here is the same untouched reference TruePeakLimiter
            // never saw above.
            if (listenModeOn)
            {
                stageL = charOnlyDelayedL;
                stageR = charOnlyDelayedR;
            }

            // Peak meter reads this value BEFORE the oversampled-domain safety clip --
            // matches the original reasoning (tells you how hard the final backstop had
            // to work, not just that it worked). On the Normal path this reflects the
            // actual TruePeakLimiter-protected true peak. In Delta mode it must reflect
            // the actual audible Delta signal (gained-dry minus processed, times
            // deltaGainLin) rather than the raw unsubtracted charOnly reference -- an
            // earlier version metered stageL/R directly here, which never moved with
            // Delta Trim at all, leaving the meter and Over indicators stuck showing the
            // pre-subtraction/pre-gain reference regardless of how loud Delta's actual
            // output was. dryGainedDelayedL/R and charOnlyDelayedL/R are already
            // available every tick (from limiter.processTick above), so this evaluates
            // the same subtraction the final per-host-sample output uses, just at full
            // oversampled resolution instead of waiting for the downsampled result.
            if (listenModeOn)
                metering.updatePeakTick ((dryGainedDelayedL - charOnlyDelayedL) * deltaGainLin,
                                          (dryGainedDelayedR - charOnlyDelayedR) * deltaGainLin);
            else
                metering.updatePeakTick (stageL, stageR);

            // Oversampled-domain safety clip -- applied only to the processed signal,
            // not the raw-dry Bypass reference (matches the JSFX exactly).
            stageL = safetyClip.clip (stageL);
            stageR = safetyClip.clip (stageR);

            const double downL = oversamplerL.downsample (j, stageL);
            const double downR = oversamplerR.downsample (j, stageR);
            // The expensive convolution for these two only actually runs when their
            // output is needed this block (matches the JSFX's own CPU-optimization
            // comment: "raw-dry FIR only when Bypass active, gained-dry FIR only when
            // Delta active") -- the buffer WRITE still happens unconditionally above
            // this branch either way, so there's no seamlessness cost when a mode
            // switches on mid-stream.
            const double downRawL = oversamplerL.downsampleSecondary (j, dryRawL, bypassOn);
            const double downRawR = oversamplerR.downsampleSecondary (j, dryRawR, bypassOn);
            const double downGainedDryL = oversamplerL.downsampleTertiary (j, dryGainedDelayedL, listenModeOn);
            const double downGainedDryR = oversamplerR.downsampleTertiary (j, dryGainedDelayedR, listenModeOn);

            if (j == 0)
            {
                outL = downL;
                outR = downR;
                outDryRawL = downRawL;
                outDryRawR = downRawR;
                outDryGainedL = downGainedDryL;
                outDryGainedR = downGainedDryR;
            }
        }

        // Post-downsample backstop clip -- both paths, so whichever one gets selected
        // below is already correctly clipped. outDryGainedL/R is deliberately NOT
        // clipped here -- matches the JSFX, which uses out_dry_L/R raw in the delta
        // subtraction and only safety-clips the boosted RESULT afterward.
        outL = safetyClip.clip (outL);
        outR = safetyClip.clip (outR);
        outDryRawL = safetyClip.clip (outDryRawL);
        outDryRawR = safetyClip.clip (outDryRawR);

        // Bypass > Delta > Normal.
        double finalOutL, finalOutR;
        if (bypassOn)
        {
            finalOutL = outDryRawL;
            finalOutR = outDryRawR;
        }
        else if (listenModeOn)
        {
            // "What was removed" by the Selective Clipper: gained dry minus the
            // (Delta-substituted, already safety-clipped) Character-only output,
            // boosted by DELTA_GAIN_DB (+ deltaTrimDb, see above) since the raw
            // difference is typically very quiet -- matches the JSFX's spl0 =
            // (out_dry_L - out_L) * delta_gain_lin.
            finalOutL = (outDryGainedL - outL) * deltaGainLin;
            finalOutR = (outDryGainedR - outR) * deltaGainLin;
            finalOutL = safetyClip.clip (finalOutL);
            finalOutR = safetyClip.clip (finalOutR);
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
    return new WishcraftMasteringLimiterAudioProcessorEditor (*this);
}

//==============================================================================
void WishcraftMasteringLimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WishcraftMasteringLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WishcraftMasteringLimiterAudioProcessor();
}

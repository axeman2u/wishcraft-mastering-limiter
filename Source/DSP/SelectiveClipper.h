#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Biquad.h"
#include "PolyphaseOversampler.h"

// Ports the JSFX's Selective Clipper ("Character") stage: a duration-gated pre-clip with
// sidechain-filtered detection, permanently-on Auto Makeup Gain, and a delay-matched dry
// reference for that makeup gain. Mirrors char_process / bare_delay / apply_makeup and the
// char_state / char_flag / char_dry_buf machinery from the JSFX as closely as possible.
//
// CHAR_MAX_MS and DELTA_GAIN_DB are fixed internal constants per the spec -- never exposed
// as parameters. DELTA_GAIN_DB is not yet used by anything (Delta Listen Mode is a later
// stage); it's defined here now only because the spec calls it out as a constant that must
// never become a slider.
class SelectiveClipper
{
public:
    static constexpr double charMaxMs      = 5.0;  // JSFX CHAR_MAX_MS
    static constexpr double charMarginBase = 2.0;   // JSFX CHAR_MARGIN_BASE
    static constexpr double deltaGainDb    = 6.0;    // JSFX DELTA_GAIN_DB (reserved, unused)

    static constexpr double scPivotHz       = 300.0; // JSFX SC_PIVOT_HZ
    static constexpr double makeupAttackMs  = 5.0;   // JSFX MAKEUP_PEAK_ATTACK_MS
    static constexpr double makeupReleaseMs = 300.0; // JSFX MAKEUP_PEAK_RELEASE_MS
    static constexpr double paramSmoothMs   = 15.0;  // JSFX PARAM_SMOOTH_MS

    // Allocates worst-case buffers (factor=4) for the current sample rate, so switching
    // oversampling factor never reallocates on the audio thread. Call once from
    // prepareToPlay(), before setFactor()/setInitialThreshold().
    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;

        const int charBudgetBaseAlloc = (int) std::ceil (charMaxMs * 0.001 * sampleRate) + (int) charMarginBase;
        const int charBufAlloc = charBudgetBaseAlloc * PolyphaseOversampler::maxFactor;
        left.prepareCapacity (charBufAlloc);
        right.prepareCapacity (charBufAlloc);

        paramSmoothCoeff = 1.0 - std::exp (-1.0 / (paramSmoothMs * 0.001 * sampleRate));
    }

    // JSFX @init: threshold_smoothed = threshold_db (no ramp-in on load).
    void setInitialThreshold (double thresholdDb)
    {
        thresholdSmoothed = thresholdDb;
        thresholdLin = std::pow (10.0, thresholdSmoothed / 20.0);
    }

    // JSFX reconfigure(): recomputes everything that depends on factor/sample rate, and
    // clears all state/buffers -- an oversampling factor change is a deliberate hard reset,
    // matching the JSFX.
    void setFactor (int factor, double osRate)
    {
        currentFactor = factor;
        charBudgetBase = (int) std::ceil (charMaxMs * 0.001 * sampleRate) + (int) charMarginBase;
        charBufSize = charBudgetBase * factor;

        left.reset (charBufSize);
        right.reset (charBufSize);
        wposChar = 0;

        // Sidechain EQ hardcoded at 0 dB (not exposed as a parameter this stage) --
        // structurally identical to the JSFX's sc_filter, just inert until a later stage
        // exposes the Sidechain EQ gain controls.
        lowCoeffs = computeLowShelf (scPivotHz, 0.0, osRate);
        highCoeffs = computeHighShelf (scPivotHz, 0.0, osRate);

        makeupAttackCoeff = 1.0 - std::exp (-1.0 / (makeupAttackMs * 0.001 * osRate));
        makeupReleaseCoeff = 1.0 - std::exp (-1.0 / (makeupReleaseMs * 0.001 * osRate));
    }

    int getCharBudgetBase() const noexcept { return charBudgetBase; }

    // JSFX @sample (top-of-block section): threshold_smoothed ramps one-pole per host
    // sample; dur_thresh_samples is recomputed every host sample directly from the raw
    // Selectivity value -- deliberately NOT smoothed, since Selectivity is meant to feel
    // like a mode change, not a continuous glide.
    void setParameters (double thresholdDb, double selectivityPct)
    {
        thresholdSmoothed += (thresholdDb - thresholdSmoothed) * paramSmoothCoeff;
        thresholdLin = std::pow (10.0, thresholdSmoothed / 20.0);
        durThreshSamples = std::floor ((selectivityPct / 100.0) * charBudgetBase + 0.5) * currentFactor;
    }

    // One oversampled tick for both channels: sidechain-filtered detection -> duration-
    // gated clip -> delay-matched dry reference -> Auto Makeup Gain. Matches the JSFX's
    // per-tick Character block exactly, including the shared wposChar advance.
    void processTick (double upL, double upR, double& outL, double& outR)
    {
        const double scCharL = left.sidechain.process (lowCoeffs, highCoeffs, upL);
        const double scCharR = right.sidechain.process (lowCoeffs, highCoeffs, upR);

        const double charL = clipRead (left,  upL, scCharL);
        const double charR = clipRead (right, upR, scCharR);

        const double dryL = bareDelay (left.charDryBuf,  upL);
        const double dryR = bareDelay (right.charDryBuf, upR);

        wposChar = (wposChar + 1) % charBufSize;

        outL = applyMakeup (left.makeupPeakDry,  left.makeupPeakChar,  dryL, charL);
        outR = applyMakeup (right.makeupPeakDry, right.makeupPeakChar, dryR, charR);
    }

private:
    //==============================================================================
    struct Channel
    {
        std::vector<double> charAudio, charFlag, charDryBuf;
        double state0 = 0.0, state1 = 0.0, state2 = 0.0; // in_excursion, start_pos, width
        SidechainFilter sidechain;
        double makeupPeakDry = 0.0, makeupPeakChar = 0.0;

        void prepareCapacity (int maxBufAlloc)
        {
            charAudio.assign ((size_t) maxBufAlloc, 0.0);
            charFlag.assign ((size_t) maxBufAlloc, 0.0);
            charDryBuf.assign ((size_t) maxBufAlloc, 0.0);
        }

        void reset (int activeBufSize)
        {
            std::fill (charAudio.begin(), charAudio.begin() + activeBufSize, 0.0);
            std::fill (charFlag.begin(), charFlag.begin() + activeBufSize, 0.0);
            std::fill (charDryBuf.begin(), charDryBuf.begin() + activeBufSize, 0.0);
            state0 = state1 = state2 = 0.0;
            sidechain = {};
            makeupPeakDry = 0.0;
            makeupPeakChar = 0.0;
        }
    };

    // Matches char_process(): read-before-write at wposChar, retroactively marking a
    // short excursion's buffered samples for clipping once it resolves (ends) under
    // durThreshSamples. mark_excursion() is inlined directly into the "excursion ended"
    // branch below rather than kept as a separate function.
    double clipRead (Channel& ch, double audioVal, double detectVal)
    {
        double outAudio = ch.charAudio[(size_t) wposChar];
        const double outFlag = ch.charFlag[(size_t) wposChar];

        const double absVal = std::abs (detectVal);
        if (absVal > thresholdLin)
        {
            if (ch.state0 == 0.0)
            {
                ch.state0 = 1.0;
                ch.state1 = (double) wposChar;
                ch.state2 = 0.0;
            }
            ch.state2 += 1.0;
        }
        else
        {
            if (ch.state0 == 1.0)
            {
                if (ch.state2 < durThreshSamples)
                {
                    int k = (int) ch.state1;
                    while (k != wposChar)
                    {
                        ch.charFlag[(size_t) k] = 1.0;
                        k = (k + 1) % charBufSize;
                    }
                }
                ch.state0 = 0.0;
            }
        }

        ch.charAudio[(size_t) wposChar] = audioVal;
        ch.charFlag[(size_t) wposChar] = 0.0; // default: pass through, unless retroactively marked

        if (outFlag > 0.5)
        {
            if (outAudio > thresholdLin) outAudio = thresholdLin;
            if (outAudio < -thresholdLin) outAudio = -thresholdLin;
        }

        return outAudio;
    }

    // Matches bare_delay(): plain read-then-write circular delay, no clip logic.
    double bareDelay (std::vector<double>& buf, double val)
    {
        const double out = buf[(size_t) wposChar];
        buf[(size_t) wposChar] = val;
        return out;
    }

    // Matches apply_makeup() with Character's own fixed clamp range [0, 12] dB (never
    // attenuate, only restore what Character itself took, capped at +12 dB).
    double applyMakeup (double& peakDry, double& peakChar, double dryVal, double charVal)
    {
        const double dryAbs = std::abs (dryVal);
        const double charAbs = std::abs (charVal);

        if (dryAbs > peakDry) peakDry += (dryAbs - peakDry) * makeupAttackCoeff;
        else                  peakDry += (dryAbs - peakDry) * makeupReleaseCoeff;

        if (charAbs > peakChar) peakChar += (charAbs - peakChar) * makeupAttackCoeff;
        else                    peakChar += (charAbs - peakChar) * makeupReleaseCoeff;

        peakDry = flushDenormal (peakDry);
        peakChar = flushDenormal (peakChar);

        double gainLin = peakDry / std::max (peakChar, 0.0000001);
        gainLin = std::min (std::max (gainLin, 1.0 /* 10^(0/20) */), std::pow (10.0, 12.0 / 20.0));
        return charVal * gainLin;
    }

    //==============================================================================
    Channel left, right;

    double sampleRate = 44100.0;
    int currentFactor = 2;
    int charBudgetBase = 1;
    int charBufSize = 1;
    int wposChar = 0;

    double thresholdSmoothed = -3.0;
    double thresholdLin = 0.7079457843841379; // 10^(-3/20)
    double durThreshSamples = 0.0;
    double paramSmoothCoeff = 0.0;

    BiquadCoeffs lowCoeffs, highCoeffs;
    double makeupAttackCoeff = 0.0, makeupReleaseCoeff = 0.0;
};

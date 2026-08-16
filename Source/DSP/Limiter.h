#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Biquad.h"
#include "PeakFollowerMakeup.h"
#include "PolyphaseOversampler.h"

// Ports the JSFX's Limiter core: a lookahead delay + program-dependent release using a
// two-time-constant history blend (limiter_compute_gr / limiter_apply), Stereo Link
// (blended after smoothing, min-gain-wins), and Limiter Auto Gain (user-toggleable, capped
// so it can never restore level past the safety margin). Also applies Input Gain (Drive),
// which the spec places after the Selective Clipper and before the Limiter.
//
// The Limiter's own sidechain-filtered detector runs through a SEPARATE filter instance
// from the Selective Clipper's (per spec, they must not share state), driven by the same
// two Sidechain EQ sliders. Notably, and confirmed intentional (asked and confirmed with
// the user in Stage 5), the JSFX feeds the SIDECHAIN-FILTERED signal -- not a separate raw
// signal -- into the lookahead buffer and back out as the limited audio: the spec describes
// Sidechain EQ as "detector-only," but line-for-line the filtered value is literally what
// gets delayed and gain-reduced here. Character's own Sidechain EQ instance IS genuinely
// detector-only (char_process takes the raw and filtered signals as separate arguments);
// the Limiter's is not. So moving the Sidechain EQ sliders away from 0 dB WILL audibly
// color the Limiter's output, not just its detection -- ported to match the JSFX exactly,
// not "fixed" to a stricter detector-only interpretation.
//
// ISP_MARGIN_DB is a fixed constant used only for the Auto Gain cap formula this stage --
// the true-peak safety clip itself (ISP_MARGIN_DB's main use) isn't ported yet.
//
// KNOWN ISSUE (Stage 4, unresolved, deprioritized by request): direct REAPER-rendered
// comparison against the real JSFX shows this Limiter applies ~0.11-0.16 dB MORE gain
// reduction than the JSFX at steady state, for a sustained DC input with the Selective
// Clipper provably inert (Threshold=0dB) -- e.g. JSFX settles to 0.508393 vs this port's
// 0.501187 at Ceiling=-6dB/input=0.9. Ruled out as causes: the 0dB sidechain filter
// (verified exact identity, both analytically and by direct coefficient/DC-gain check),
// the oversampling engine (matches Stage 2's precision), oversampling factor (2x and 4x
// give identical results), Release time (doesn't move the DC steady-state, as expected),
// and *_smoothed parameter initialization order (tested an alternate init -- made the
// match worse, not better). limiter_compute_gr was re-checked character-by-character
// against the source and matches what's ported here. Root cause not found. Small enough
// to be inaudible in practice; would show up on a side-by-side GR meter comparison. Not
// worth further investigation per user decision (no reference JSFX install for most
// users anyway) -- left here so a future stage revisiting this file has the context.
class Limiter
{
public:
    static constexpr double scPivotHz       = 300.0; // JSFX SC_PIVOT_HZ
    static constexpr double makeupAttackMs  = 5.0;   // JSFX MAKEUP_PEAK_ATTACK_MS
    static constexpr double makeupReleaseMs = 300.0; // JSFX MAKEUP_PEAK_RELEASE_MS
    static constexpr double paramSmoothMs   = 15.0;  // JSFX PARAM_SMOOTH_MS
    static constexpr double ispMarginDb     = 0.5;   // JSFX ISP_MARGIN_DB

    static constexpr double attackMs         = 1.0;   // JSFX ATTACK_MS
    static constexpr double historyAttackMs  = 15.0;  // JSFX HISTORY_ATTACK_MS
    static constexpr double historyDecayMs   = 500.0; // JSFX HISTORY_DECAY_MS
    static constexpr double historyReferenceDb = -10.0; // JSFX HISTORY_REFERENCE_DB

    static constexpr double releaseFastMinMs = 20.0,  releaseFastMaxMs = 100.0;
    static constexpr double releaseSlowMinMs = 300.0, releaseSlowMaxMs = 1500.0;

    static constexpr double laMaxMsCeiling = 20.0; // JSFX LA_MAX_MS_CEILING

    // JSFX slider6 (lookahead_ms) default -- not yet exposed as a parameter this stage;
    // the lookahead buffer is sized as if it were fixed here, matching what a freshly-
    // loaded, untouched JSFX instance would use. For a valid A/B, keep the JSFX's own
    // Lookahead slider at its default (3.0 ms) too.
    static constexpr double lookaheadMsFixed = 3.0;

    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;

        const int laBudgetBaseAlloc = (int) std::ceil (laMaxMsCeiling * 0.001 * sampleRate);
        const int laBufAlloc = laBudgetBaseAlloc * PolyphaseOversampler::maxFactor;
        left.prepareCapacity (laBufAlloc);
        right.prepareCapacity (laBufAlloc);

        paramSmoothCoeff = 1.0 - std::exp (-1.0 / (paramSmoothMs * 0.001 * sampleRate));
    }

    // JSFX @init: *_smoothed = the slider's actual current value (no ramp-in on load).
    void setInitialSmoothedParams (double ceilingDb, double outputCeilingDb, double inputGainDb,
                                    double scLowShelfDb, double scHighShelfDb)
    {
        ceilingSmoothed = ceilingDb;
        outputCeilingSmoothed = outputCeilingDb;
        inputGainSmoothed = inputGainDb;
        inputGainLin = std::pow (10.0, inputGainSmoothed / 20.0);
        scLowShelfSmoothed = scLowShelfDb;
        scHighShelfSmoothed = scHighShelfDb;
    }

    // JSFX reconfigure(): recomputes everything that depends on factor/sample rate, and
    // clears all state/buffers -- an oversampling factor change is a deliberate hard reset.
    void setFactor (int factor, double osRate)
    {
        currentFactor = factor;
        this->osRate = osRate;

        laBudgetBase = (int) std::ceil (lookaheadMsFixed * 0.001 * sampleRate);
        laBufSize = laBudgetBase * factor;

        left.reset (laBufSize);
        right.reset (laBufSize);
        wposLa = 0;

        // Sidechain EQ coefficients depend on os_rate, so recompute them here too (also
        // recomputed every setParameters() call at the current smoothed gain -- this just
        // ensures validity immediately after a factor change). Separate coefficient set
        // and separate biquad instances from the Selective Clipper's, per spec.
        lowCoeffs = computeLowShelf (scPivotHz, scLowShelfSmoothed, osRate);
        highCoeffs = computeHighShelf (scPivotHz, scHighShelfSmoothed, osRate);

        attackCoeff = 1.0 - std::exp (-1.0 / (attackMs * 0.001 * osRate));
        historyAttackCoeff = 1.0 - std::exp (-1.0 / (historyAttackMs * 0.001 * osRate));
        historyDecayCoeff = 1.0 - std::exp (-1.0 / (historyDecayMs * 0.001 * osRate));
        makeupAttackCoeff = 1.0 - std::exp (-1.0 / (makeupAttackMs * 0.001 * osRate));
        makeupReleaseCoeff = 1.0 - std::exp (-1.0 / (makeupReleaseMs * 0.001 * osRate));
    }

    int getLaBudgetBase() const noexcept { return laBudgetBase; }

    // The single, shared output_ceiling_smoothed value -- also needed by the Safety Clip
    // stage (safety_ceiling_lin = 10^((output_ceiling_smoothed - ISP_MARGIN_DB)/20)) and
    // by Bypass's post-downsample backstop, both outside this class. Exposed rather than
    // having those re-derive/re-smooth their own copy, matching the JSFX's single global
    // output_ceiling_smoothed used everywhere it's needed.
    double getOutputCeilingSmoothed() const noexcept { return outputCeilingSmoothed; }

    // JSFX @sample (top-of-block section): ceiling/output-ceiling/input-gain ramp one-pole
    // per host sample; release_fast_ms/release_slow_ms and link_frac are recomputed every
    // host sample directly from their raw slider values (cheap linear maps, no smoothing
    // needed of their own -- the JSFX does the same). autoGainOn is a plain on/off switch,
    // read directly with no smoothing, matching the JSFX.
    void setParameters (double ceilingDb, double releasePct, double linkPct,
                         double inputGainDb, double outputCeilingDb, bool autoGainOnIn,
                         double scLowShelfDb, double scHighShelfDb)
    {
        ceilingSmoothed += (ceilingDb - ceilingSmoothed) * paramSmoothCoeff;
        outputCeilingSmoothed += (outputCeilingDb - outputCeilingSmoothed) * paramSmoothCoeff;
        inputGainSmoothed += (inputGainDb - inputGainSmoothed) * paramSmoothCoeff;
        inputGainLin = std::pow (10.0, inputGainSmoothed / 20.0);

        releaseFastMs = releaseFastMinMs + (releasePct / 100.0) * (releaseFastMaxMs - releaseFastMinMs);
        releaseSlowMs = releaseSlowMinMs + (releasePct / 100.0) * (releaseSlowMaxMs - releaseSlowMinMs);

        linkFrac = linkPct / 100.0;
        autoGainOn = autoGainOnIn;

        // Cap Auto Gain so it cannot restore past the ISP-margin ceiling either.
        autoGainMaxBoostDb = std::max (0.0, (outputCeilingSmoothed - ispMarginDb) - ceilingSmoothed);

        scLowShelfSmoothed += (scLowShelfDb - scLowShelfSmoothed) * paramSmoothCoeff;
        scHighShelfSmoothed += (scHighShelfDb - scHighShelfSmoothed) * paramSmoothCoeff;
        lowCoeffs = computeLowShelf (scPivotHz, scLowShelfSmoothed, osRate);
        highCoeffs = computeHighShelf (scPivotHz, scHighShelfSmoothed, osRate);
    }

    // One oversampled tick for both channels: Input Gain -> Limiter's own sidechain-
    // filtered detector -> gain-reduction computer -> Stereo Link blend -> lookahead
    // delay + apply -> optional full-chain Auto Gain. dryL/dryR are the Selective
    // Clipper's (undelayed-further) dry reference, used only as Auto Gain's peak-ratio
    // comparison point -- matches the JSFX using dry_L/dry_R directly rather than a
    // Limiter-delayed copy, since apply_makeup's slow peak followers don't need
    // sample-accurate alignment. dryRawL/dryRawR are ALSO returned: dry_L/dry_R (the
    // Selective Clipper's raw, un-gained dry reference) delayed by this Limiter's own
    // la_buf_size, matching the JSFX's dry_raw_la_L/R -- Bypass's true-passthrough
    // reference, latency-matched to the full chain but never touched by Input Gain,
    // Sidechain EQ, or gain reduction.
    void processTick (double makeupL, double makeupR, double dryL, double dryR,
                       double& outL, double& outR, double& dryRawL, double& dryRawR)
    {
        const double gL = makeupL * inputGainLin;
        const double gR = makeupR * inputGainLin;

        const double limSigL = left.sidechain.process (lowCoeffs, highCoeffs, gL);
        const double limSigR = right.sidechain.process (lowCoeffs, highCoeffs, gR);

        const double grL = limiterComputeGr (left.grSmoothedDb,  left.grHistoryDb,  limSigL);
        const double grR = limiterComputeGr (right.grSmoothedDb, right.grHistoryDb, limSigR);

        const double sharedGrDb = std::min (grL, grR);
        const double finalGrL = grL * (1.0 - linkFrac) + sharedGrDb * linkFrac;
        const double finalGrR = grR * (1.0 - linkFrac) + sharedGrDb * linkFrac;

        const double limL = limiterApply (left.laBuf,  limSigL, finalGrL);
        const double limR = limiterApply (right.laBuf, limSigR, finalGrR);

        dryRawL = bareDelay (left.laDryRawBuf,  dryL);
        dryRawR = bareDelay (right.laDryRawBuf, dryR);

        wposLa = (wposLa + 1) % laBufSize;

        lastGrL = finalGrL;
        lastGrR = finalGrR;

        if (autoGainOn)
        {
            outL = applyPeakRatioMakeup (left.fullGainPeakDry,  left.fullGainPeakChar,  dryL, limL, makeupAttackCoeff, makeupReleaseCoeff, -60.0, autoGainMaxBoostDb);
            outR = applyPeakRatioMakeup (right.fullGainPeakDry, right.fullGainPeakChar, dryR, limR, makeupAttackCoeff, makeupReleaseCoeff, -60.0, autoGainMaxBoostDb);
        }
        else
        {
            outL = limL;
            outR = limR;
        }
    }

    // Temporary, Stage 4 only: last computed post-Link gain reduction (dB, <= 0), for a
    // simple GR readout until a real meter/GUI exists.
    double getLastGrL() const noexcept { return lastGrL; }
    double getLastGrR() const noexcept { return lastGrR; }

private:
    //==============================================================================
    struct Channel
    {
        std::vector<double> laBuf;
        std::vector<double> laDryRawBuf;
        double grSmoothedDb = 0.0, grHistoryDb = 0.0;
        SidechainFilter sidechain;
        double fullGainPeakDry = 0.0, fullGainPeakChar = 0.0;

        void prepareCapacity (int maxBufAlloc)
        {
            laBuf.assign ((size_t) maxBufAlloc, 0.0);
            laDryRawBuf.assign ((size_t) maxBufAlloc, 0.0);
        }

        void reset (int activeBufSize)
        {
            std::fill (laBuf.begin(), laBuf.begin() + activeBufSize, 0.0);
            std::fill (laDryRawBuf.begin(), laDryRawBuf.begin() + activeBufSize, 0.0);
            grSmoothedDb = 0.0;
            grHistoryDb = 0.0;
            sidechain = {};
            fullGainPeakDry = 0.0;
            fullGainPeakChar = 0.0;
        }
    };

    // Matches limiter_compute_gr(): gain COMPUTER only, no buffer/delay involvement.
    double limiterComputeGr (double& grSmoothedDb, double& grHistoryDb, double val)
    {
        const double absVal = std::abs (val);
        const double levelDb = 20.0 * std::log (std::max (absVal, 0.0000000001)) / ln10;
        const double grTargetDb = std::min (0.0, ceilingSmoothed - levelDb);

        const double historyNorm = std::min (std::max (grHistoryDb / historyReferenceDb, 0.0), 1.0);
        const double releaseMsCurrent = releaseFastMs + (releaseSlowMs - releaseFastMs) * historyNorm;
        const double releaseCoeff = 1.0 - std::exp (-1.0 / (releaseMsCurrent * 0.001 * osRate));

        if (grTargetDb < grSmoothedDb)
            grSmoothedDb += (grTargetDb - grSmoothedDb) * attackCoeff;
        else
            grSmoothedDb += (grTargetDb - grSmoothedDb) * releaseCoeff;

        if (grSmoothedDb < grHistoryDb)
            grHistoryDb += (grSmoothedDb - grHistoryDb) * historyAttackCoeff;
        else
            grHistoryDb += (grSmoothedDb - grHistoryDb) * historyDecayCoeff;

        grSmoothedDb = flushDenormal (grSmoothedDb);
        grHistoryDb = flushDenormal (grHistoryDb);

        return grSmoothedDb;
    }

    // Matches limiter_apply(): delay + apply a given (possibly Link-blended) gain
    // reduction, read-before-write, same buf-size-sample delay guarantee as everywhere
    // else.
    double limiterApply (std::vector<double>& laBuf, double val, double grDb)
    {
        const double delayed = laBuf[(size_t) wposLa];
        laBuf[(size_t) wposLa] = val;
        const double grLinear = std::pow (10.0, grDb / 20.0);
        return delayed * grLinear;
    }

    // Matches bare_delay(): plain read-then-write circular delay, no gain involved.
    double bareDelay (std::vector<double>& buf, double val)
    {
        const double out = buf[(size_t) wposLa];
        buf[(size_t) wposLa] = val;
        return out;
    }

    //==============================================================================
    Channel left, right;

    double sampleRate = 44100.0, osRate = 88200.0;
    int currentFactor = 2;
    int laBudgetBase = 1;
    int laBufSize = 1;
    int wposLa = 0;

    double ceilingSmoothed = -1.0;
    double outputCeilingSmoothed = -1.0;
    double inputGainSmoothed = 0.0;
    double inputGainLin = 1.0;
    double releaseFastMs = 20.0, releaseSlowMs = 300.0;
    double linkFrac = 0.75;
    double autoGainMaxBoostDb = 0.0;
    bool autoGainOn = false;
    double paramSmoothCoeff = 0.0;

    double scLowShelfSmoothed = 0.0, scHighShelfSmoothed = 0.0;
    BiquadCoeffs lowCoeffs, highCoeffs;
    double attackCoeff = 0.0, historyAttackCoeff = 0.0, historyDecayCoeff = 0.0;
    double makeupAttackCoeff = 0.0, makeupReleaseCoeff = 0.0;

    double lastGrL = 0.0, lastGrR = 0.0;

    const double ln10 = std::log (10.0);
};

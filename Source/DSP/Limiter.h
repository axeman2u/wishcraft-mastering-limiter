#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Biquad.h"
#include "PeakFollowerMakeup.h"
#include "PolyphaseOversampler.h"

// Ports the JSFX's Limiter core: a lookahead delay + program-dependent release using a
// two-time-constant history blend (limiter_compute_gr / limiter_apply), Stereo Link
// (blended after smoothing, min-gain-wins), and Gain Match (JSFX slider14, originally
// "Limiter Auto Gain" -- renamed per explicit user request, see below; param ID stays
// limiter_auto_gain for automation/state compatibility). Also applies Input Gain
// (Drive), which the spec places after the Selective Clipper and before the Limiter.
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
// Gain Match is strictly cut-only: unlike a compressor's makeup gain, it must never
// restore level above unity, so its cap is a fixed 0.0 dB rather than a formula derived
// from any peak-ceiling value (that used to route through ISP_MARGIN_DB before
// TruePeakLimiter/SafetyClip took over true-peak safety -- see TruePeakLimiter.h).
//
// NOT RETARGETED: a later attempt made this target TP Limit instead of the raw dry/
// source signal (reasoning: the source is typically much quieter than a driven,
// limited "mastered" result, so engaging Gain Match dropped output well below the
// user's monitoring level). That version had a real bug (a peak-follower cold-start/
// silence-recovery transient), which was found and fixed -- but the user reported it
// "still doesn't work right" even after the fix, and asked to revert the DSP behavior
// entirely back to this original dry/source-targeting, cut-only version while keeping
// only the "Gain Match" name. Do not reintroduce TP-Limit targeting without a fresh
// explicit request -- this was tried once and explicitly rejected.
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

    static constexpr double attackMs         = 1.0;   // JSFX ATTACK_MS
    static constexpr double historyAttackMs  = 15.0;  // JSFX HISTORY_ATTACK_MS
    static constexpr double historyDecayMs   = 500.0; // JSFX HISTORY_DECAY_MS
    static constexpr double historyReferenceDb = -10.0; // JSFX HISTORY_REFERENCE_DB

    static constexpr double releaseFastMinMs = 20.0,  releaseFastMaxMs = 100.0;
    static constexpr double releaseSlowMinMs = 300.0, releaseSlowMaxMs = 1500.0;

    static constexpr double laMaxMsCeiling = 20.0; // JSFX LA_MAX_MS_CEILING -- also lookahead_ms's own param max, so this sizes the worst-case allocation

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
    // clears all state/buffers -- an oversampling factor change is a deliberate hard
    // reset. lookaheadMs (JSFX slider6) is now wired the same way the JSFX itself wires
    // it: a lookahead_ms change is treated identically to an os_choice change, calling
    // this same reconfigure/hard-reset path (see PluginProcessor.cpp's top-of-block
    // check) -- not smoothed/ramped, matching the JSFX exactly.
    void setFactor (int factor, double osRate, double lookaheadMs)
    {
        currentFactor = factor;
        this->osRate = osRate;

        laBudgetBase = (int) std::ceil (lookaheadMs * 0.001 * sampleRate);
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

    // The single, shared output_ceiling_smoothed value -- now the True Peak Limiter's
    // target (see TruePeakLimiter::setTargetCeiling), pulled from here rather than
    // re-derived/re-smoothed independently, matching the JSFX's single global
    // output_ceiling_smoothed used everywhere it's needed. Safety Clip no longer reads
    // this -- it's a fixed backstop, decoupled from any user parameter (see SafetyClip.h).
    double getOutputCeilingSmoothed() const noexcept { return outputCeilingSmoothed; }

    // JSFX @sample (top-of-block section): ceiling/output-ceiling/input-gain ramp one-pole
    // per host sample; release_fast_ms/release_slow_ms and link_frac are recomputed every
    // host sample directly from their raw slider values (cheap linear maps, no smoothing
    // needed of their own -- the JSFX does the same). gainMatchOn is a plain on/off switch,
    // read directly with no smoothing, matching the JSFX.
    void setParameters (double ceilingDb, double releasePct, double linkPct,
                         double inputGainDb, double outputCeilingDb, bool gainMatchOnIn,
                         double scLowShelfDb, double scHighShelfDb)
    {
        ceilingSmoothed += (ceilingDb - ceilingSmoothed) * paramSmoothCoeff;
        outputCeilingSmoothed += (outputCeilingDb - outputCeilingSmoothed) * paramSmoothCoeff;
        inputGainSmoothed += (inputGainDb - inputGainSmoothed) * paramSmoothCoeff;
        inputGainLin = std::pow (10.0, inputGainSmoothed / 20.0);

        releaseFastMs = releaseFastMinMs + (releasePct / 100.0) * (releaseFastMaxMs - releaseFastMinMs);
        releaseSlowMs = releaseSlowMinMs + (releasePct / 100.0) * (releaseSlowMaxMs - releaseSlowMinMs);

        linkFrac = linkPct / 100.0;
        gainMatchOn = gainMatchOnIn;

        scLowShelfSmoothed += (scLowShelfDb - scLowShelfSmoothed) * paramSmoothCoeff;
        scHighShelfSmoothed += (scHighShelfDb - scHighShelfSmoothed) * paramSmoothCoeff;
        lowCoeffs = computeLowShelf (scPivotHz, scLowShelfSmoothed, osRate);
        highCoeffs = computeHighShelf (scPivotHz, scHighShelfSmoothed, osRate);
    }

    // One oversampled tick for both channels: Input Gain -> Limiter's own sidechain-
    // filtered detector -> gain-reduction computer -> Stereo Link blend -> lookahead
    // delay + apply -> optional full-chain Gain Match. dryL/dryR are the Selective
    // Clipper's (undelayed-further) dry reference, used only as Gain Match's peak-ratio
    // comparison point -- matches the JSFX using dry_L/dry_R directly rather than a
    // Limiter-delayed copy, since apply_makeup's slow peak followers don't need
    // sample-accurate alignment. dryRawL/dryRawR are ALSO returned: dry_L/dry_R (the
    // Selective Clipper's raw, un-gained dry reference) delayed by this Limiter's own
    // la_buf_size, matching the JSFX's dry_raw_la_L/R -- Bypass's true-passthrough
    // reference, latency-matched to the full chain but never touched by Input Gain,
    // Sidechain EQ, or gain reduction.
    //
    // charOnlyDelayedL/R and dryGainedDelayedL/R are ALSO returned, for Delta Listen
    // Mode: charOnlyDelayedL/R is the GAINED (post Input Gain) Selective Clipper output
    // (makeupL/makeupR post *inputGainLin, i.e. what the JSFX calls makeup_L after its
    // own `makeup_L *= input_gain_lin`), delayed by this Limiter's own la_buf_size --
    // matches the JSFX's charonly_la_L/R exactly, including sharing the SAME wposLa
    // cycle as everything else here, so it lands time-aligned with limL/outL. The
    // caller substitutes this in place of the Limiter's own output when Delta is
    // active (matching the JSFX's `listen_mode > 0.5 ? stage_L = charonly_la_L`),
    // which is why this is a genuinely separate signal rather than a subtraction
    // computed here: Delta compares dry against what the SELECTIVE CLIPPER did, not
    // against the Limiter's gain reduction -- matching the JSFX precisely (this was
    // gotten wrong on a first read of @sample; the charonly_la_L substitution feeds
    // the MAIN downsample path, and only the final dry-minus-that difference,
    // computed by the caller after downsampling, is boosted by DELTA_GAIN_DB).
    // dryGainedDelayedL/R is the GAINED dry reference (dryL/dryR * inputGainLin),
    // delayed the same la_buf_size amount -- matches the JSFX's dry_la_L/R, the
    // subtrahend's other half. This is deliberately a SEPARATE buffer from
    // laDryRawBuf/dryRawL above (gain-then-delay), not derived from dryRawL by
    // multiplying with the CURRENT inputGainLin after the fact (delay-then-gain) --
    // those two only agree once inputGainLin has settled; while Input Gain is
    // actively ramping they diverge, and the JSFX's own gdry_L = dry_L * input_gain_lin
    // happens BEFORE the delay, not after.
    void processTick (double makeupL, double makeupR, double dryL, double dryR,
                       double& outL, double& outR, double& dryRawL, double& dryRawR,
                       double& charOnlyDelayedL, double& charOnlyDelayedR,
                       double& dryGainedDelayedL, double& dryGainedDelayedR)
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

        charOnlyDelayedL = bareDelay (left.charOnlyLaBuf,  gL);
        charOnlyDelayedR = bareDelay (right.charOnlyLaBuf, gR);

        const double gDryL = dryL * inputGainLin;
        const double gDryR = dryR * inputGainLin;
        dryGainedDelayedL = bareDelay (left.laDryBuf,  gDryL);
        dryGainedDelayedR = bareDelay (right.laDryBuf, gDryR);

        wposLa = (wposLa + 1) % laBufSize;

        lastGrL = finalGrL;
        lastGrR = finalGrR;

        if (gainMatchOn)
        {
            // Fixed 0.0 dB cap -- strictly cut-only, unlike a compressor's makeup gain.
            outL = applyPeakRatioMakeup (left.fullGainPeakDry,  left.fullGainPeakChar,  dryL, limL, makeupAttackCoeff, makeupReleaseCoeff, -60.0, 0.0);
            outR = applyPeakRatioMakeup (right.fullGainPeakDry, right.fullGainPeakChar, dryR, limR, makeupAttackCoeff, makeupReleaseCoeff, -60.0, 0.0);
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
        std::vector<double> charOnlyLaBuf; // JSFX charonly_la_bufL/R -- Delta's gained Character-only reference
        std::vector<double> laDryBuf;      // JSFX la_dry_bufL/R -- Delta's gained dry reference
        double grSmoothedDb = 0.0, grHistoryDb = 0.0;
        SidechainFilter sidechain;
        double fullGainPeakDry = 0.0, fullGainPeakChar = 0.0;

        void prepareCapacity (int maxBufAlloc)
        {
            laBuf.assign ((size_t) maxBufAlloc, 0.0);
            laDryRawBuf.assign ((size_t) maxBufAlloc, 0.0);
            charOnlyLaBuf.assign ((size_t) maxBufAlloc, 0.0);
            laDryBuf.assign ((size_t) maxBufAlloc, 0.0);
        }

        void reset (int activeBufSize)
        {
            std::fill (laBuf.begin(), laBuf.begin() + activeBufSize, 0.0);
            std::fill (laDryRawBuf.begin(), laDryRawBuf.begin() + activeBufSize, 0.0);
            std::fill (charOnlyLaBuf.begin(), charOnlyLaBuf.begin() + activeBufSize, 0.0);
            std::fill (laDryBuf.begin(), laDryBuf.begin() + activeBufSize, 0.0);
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
    bool gainMatchOn = false;
    double paramSmoothCoeff = 0.0;

    double scLowShelfSmoothed = 0.0, scHighShelfSmoothed = 0.0;
    BiquadCoeffs lowCoeffs, highCoeffs;
    double attackCoeff = 0.0, historyAttackCoeff = 0.0, historyDecayCoeff = 0.0;
    double makeupAttackCoeff = 0.0, makeupReleaseCoeff = 0.0;

    double lastGrL = 0.0, lastGrR = 0.0;

    const double ln10 = std::log (10.0);
};

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

#include "Biquad.h"

// Ports the JSFX's metering CALCULATIONS -- ballistics, peak-hold, the Dynamic Range (LRA)
// tracker, and short-term LUFS -- as atomic values a future GUI (or, for now, a debug
// readout) can read from a different thread. No drawing/layout is ported here.
//
// Naming: the JSFX calls its oversampled-domain peak meter "tp" (True Peak) throughout.
// Per an explicit decision this session, this port displays it as "Peak" instead -- the
// Safety Clip only guarantees the discrete sample peak exactly, not a certified
// inter-sample/true-peak bound (confirmed by direct measurement: even a fully clipped
// signal can reconstruct over 1 dB above the clip ceiling), so calling it "True Peak"
// would claim a stronger guarantee than what's delivered. Internal variable/comment names
// below still say "tp"/"peak" interchangeably for JSFX traceability; only the
// user-facing label changed.
//
// Update cadence matches the JSFX exactly:
//  - updatePeakTick() runs once per OVERSAMPLED tick (TP_ATTACK_MS/TP_RELEASE_MS are
//    computed at os_rate), fed from the Limiter's output BEFORE the oversampled-domain
//    Safety Clip touches it -- so this meter reflects what the limiter/safety net had to
//    work with, not the post-clip result (matches the JSFX's own reasoning: it tells you
//    how hard the safety net had to work, not just that it worked).
//  - updateOncePerHostSample() runs once per host sample, after the oversampled loop,
//    computing everything else (GR display, Character Activity, all three peak-holds,
//    the Over indicators, Dynamic Range, and short-term LUFS).
class Metering
{
public:
    static constexpr double meterDecayMs = 400.0; // JSFX METER_DECAY_MS

    static constexpr double peakHoldTimeMs = 1000.0;      // JSFX PEAK_HOLD_TIME_MS
    static constexpr double peakHoldFallDbPerSec = 20.0;   // JSFX PEAK_HOLD_FALL_DB_PER_SEC

    static constexpr double lraMaxAttackMs = 3000.0, lraMaxReleaseMs = 16000.0;   // JSFX LRA_MAX_*
    static constexpr double lraMinAttackMs = 3000.0, lraMinReleaseMs = 16000.0;   // JSFX LRA_MIN_*
    static constexpr double lraGateDb = -40.0; // JSFX LRA_GATE_DB

    static constexpr double peakAttackMs = 1.0, peakReleaseMs = 400.0; // JSFX TP_ATTACK_MS/TP_RELEASE_MS

    static constexpr double lufsStSec = 3.0; // JSFX LUFS_ST_SEC (already in seconds, not ms)

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        meterDecayCoeff = 1.0 - std::exp (-1.0 / (meterDecayMs * 0.001 * sampleRate));

        peakHoldTimeSamples = (int) std::ceil (peakHoldTimeMs * 0.001 * sampleRate);
        peakHoldFallPerSampleDb = peakHoldFallDbPerSec / sampleRate;

        lraMaxAttackCoeff = 1.0 - std::exp (-1.0 / (lraMaxAttackMs * 0.001 * sampleRate));
        lraMaxReleaseCoeff = 1.0 - std::exp (-1.0 / (lraMaxReleaseMs * 0.001 * sampleRate));
        lraMinAttackCoeff = 1.0 - std::exp (-1.0 / (lraMinAttackMs * 0.001 * sampleRate));
        lraMinReleaseCoeff = 1.0 - std::exp (-1.0 / (lraMinReleaseMs * 0.001 * sampleRate));

        lufsMsCoeff = 1.0 - std::exp (-1.0 / (lufsStSec * sampleRate));
        computeLufsKweightCoeffs (sampleRate);

        reset();
    }

    // JSFX reconfigure(): recomputes the peak meter's os_rate-dependent coefficients and
    // hard-resets all meter state -- a factor change is a deliberate reset, matching the
    // JSFX (and every other stage of this port).
    void setFactor (double osRate)
    {
        peakAttackCoeff = 1.0 - std::exp (-1.0 / (peakAttackMs * 0.001 * osRate));
        peakReleaseCoeff = 1.0 - std::exp (-1.0 / (peakReleaseMs * 0.001 * osRate));
        reset();
    }

    void reset()
    {
        left.reset();
        right.reset();
        lufsKw1L = {}; lufsKw2L = {};
        lufsKw1R = {}; lufsKw2R = {};
        lufsMsEnv = 0.0;
        publish (grHeldL, 0.0f); publish (grHeldR, 0.0f);
        publish (peakHeldL, -60.0f); publish (peakHeldR, -60.0f);
        publish (charHeldL, 0.0f); publish (charHeldR, 0.0f);
        publish (overYellow, false); publish (overRed, false);
        publish (dynamicRangeDb, 0.0f);
        publish (lufsShortTermDb, -70.0f);

        // GUI-only additions (Stage 9): the LIVE ballistics value behind each hold
        // marker (a meter bar needs both -- the hold is a thin marker line drawn ON
        // TOP of a moving live-fill bar, not the bar itself, matching the JSFX's
        // draw_vmeter(live_frac, hold_frac, ...) taking both), per-channel Over flags
        // (the JSFX draws an independent dot per channel; Stage 7 only published a
        // combined L||R flag, fine for the debug params but not for two GUI dots), and
        // the Dynamic Range span's actual min/max (Stage 7 only published the
        // difference, but the range bar needs to know WHERE on the scale to draw the
        // span, not just how wide it is).
        publish (grLiveL, 0.0f); publish (grLiveR, 0.0f);
        publish (peakLiveL, -60.0f); publish (peakLiveR, -60.0f);
        publish (charLiveL, 0.0f); publish (charLiveR, 0.0f);
        publish (overYellowL, false); publish (overYellowR, false);
        publish (overRedL, false); publish (overRedR, false);
        publish (dynamicRangeMinDb, 0.0f); publish (dynamicRangeMaxDb, 0.0f);
    }

    // Once per oversampled tick: fed from the Limiter's output BEFORE the oversampled-
    // domain Safety Clip runs. Matches JSFX's tp_level_L_db / tp_disp_L update, which
    // happens before "stage_L > safety_ceiling_lin ? ..." in @sample.
    void updatePeakTick (double preClipStageL, double preClipStageR)
    {
        left.updatePeakLevel (preClipStageL, peakAttackCoeff, peakReleaseCoeff);
        right.updatePeakLevel (preClipStageR, peakAttackCoeff, peakReleaseCoeff);
    }

    // Once per host sample, after the oversampled loop. finalGrL/R: the Limiter's last
    // post-Link gain reduction this block (Limiter::getLastGrL/R). dryL/R, charL/R: the
    // Selective Clipper's last-tick dry/clipped-pre-makeup pair (matches the JSFX using
    // dry_L/char_L, NOT the post-makeup-gain output -- makeup gain restores level so
    // effectively that comparing post-makeup output to dry understates real clipping
    // activity, confirmed empirically in an earlier stage). outputCeilingSmoothed: from
    // Limiter::getOutputCeilingSmoothed(), for the Over indicators. outL/outR: the FINAL
    // audible output (post Bypass/safety-clip selection), for short-term LUFS.
    void updateOncePerHostSample (double finalGrL, double finalGrR,
                                   double dryL, double dryR, double charL, double charR,
                                   double outputCeilingSmoothed,
                                   double outL, double outR)
    {
        // GR display: fast-catch (jump immediately to a deeper reduction), slow decay.
        left.dispGr = (finalGrL < left.dispGr) ? finalGrL
                                                : flushDenormal (left.dispGr + (0.0 - left.dispGr) * meterDecayCoeff);
        right.dispGr = (finalGrR < right.dispGr) ? finalGrR
                                                  : flushDenormal (right.dispGr + (0.0 - right.dispGr) * meterDecayCoeff);

        // Character Activity: "spike energy removed" -- dry level minus clipped-pre-
        // makeup level, floored at 0 (never negative; equal levels = no activity).
        const double charActL = std::max (0.0, levelDb (dryL) - levelDb (charL));
        const double charActR = std::max (0.0, levelDb (dryR) - levelDb (charR));
        left.charDisp = (charActL > left.charDisp) ? charActL
                                                     : flushDenormal (left.charDisp + (0.0 - left.charDisp) * meterDecayCoeff);
        right.charDisp = (charActR > right.charDisp) ? charActR
                                                       : flushDenormal (right.charDisp + (0.0 - right.charDisp) * meterDecayCoeff);

        // Peak-hold markers: GR uses "lower is more extreme"; Peak and Character
        // Activity use "higher is more extreme".
        const double grHoldL = peakHoldLow (left.grHoldVal, left.grHoldTimer, left.dispGr);
        const double grHoldR = peakHoldLow (right.grHoldVal, right.grHoldTimer, right.dispGr);
        const double peakHoldLValue = peakHoldHigh (left.peakHoldVal, left.peakHoldTimer, left.peakDisp);
        const double peakHoldRValue = peakHoldHigh (right.peakHoldVal, right.peakHoldTimer, right.peakDisp);
        const double charHoldLValue = peakHoldHigh (left.charHoldVal, left.charHoldTimer, left.charDisp);
        const double charHoldRValue = peakHoldHigh (right.charHoldVal, right.charHoldTimer, right.charDisp);

        // Dynamic Range (LRA-style), gated -- only tracks while the peak display is
        // above LRA_GATE_DB, otherwise holds its last value (matches the JSFX exactly).
        if (left.peakDisp > lraGateDb)
        {
            left.lraMax = lraTrackHigh (left.lraMaxState, left.peakDisp, lraMaxAttackCoeff, lraMaxReleaseCoeff);
            left.lraMin = lraTrackLow (left.lraMinState, left.peakDisp, lraMinAttackCoeff, lraMinReleaseCoeff);
        }
        if (right.peakDisp > lraGateDb)
        {
            right.lraMax = lraTrackHigh (right.lraMaxState, right.peakDisp, lraMaxAttackCoeff, lraMaxReleaseCoeff);
            right.lraMin = lraTrackLow (right.lraMinState, right.peakDisp, lraMinAttackCoeff, lraMinReleaseCoeff);
        }
        const double lraCombinedMax = std::max (left.lraMax, right.lraMax);
        const double lraCombinedMin = std::min (left.lraMin, right.lraMin);
        const double lraRangeDb = std::max (0.0, lraCombinedMax - lraCombinedMin);

        // Two-tier Over indicator, checked against the HELD peak value, not live --
        // yellow past TP Limit (outputCeilingSmoothed -- the same target the True Peak
        // Limiter aims for; should now trigger only rarely, when its own internal
        // margin gets used up), red past true 0 dBFS (hard digital clipping).
        const bool yellowL = (peakHoldLValue > outputCeilingSmoothed) && (peakHoldLValue <= 0.0);
        const bool yellowR = (peakHoldRValue > outputCeilingSmoothed) && (peakHoldRValue <= 0.0);
        const bool redL = peakHoldLValue > 0.0;
        const bool redR = peakHoldRValue > 0.0;

        // Short-term LUFS: K-weighted stereo power sum over the FINAL audible output.
        const double lufsZL = lufsKweight (lufsKw1L, lufsKw2L, outL);
        const double lufsZR = lufsKweight (lufsKw1R, lufsKw2R, outR);
        const double lufsMs = lufsZL * lufsZL + lufsZR * lufsZR;
        lufsMsEnv = flushDenormal (lufsMsEnv + (lufsMs - lufsMsEnv) * lufsMsCoeff);
        const double lufsDb = -0.691 + 10.0 * std::log (std::max (lufsMsEnv, 1e-20)) / ln10;

        publish (grHeldL, (float) grHoldL);
        publish (grHeldR, (float) grHoldR);
        publish (peakHeldL, (float) peakHoldLValue);
        publish (peakHeldR, (float) peakHoldRValue);
        publish (charHeldL, (float) charHoldLValue);
        publish (charHeldR, (float) charHoldRValue);
        publish (overYellow, yellowL || yellowR);
        publish (overRed, redL || redR);
        publish (dynamicRangeDb, (float) lraRangeDb);
        publish (lufsShortTermDb, (float) lufsDb);

        publish (grLiveL, (float) left.dispGr);
        publish (grLiveR, (float) right.dispGr);
        publish (peakLiveL, (float) left.peakDisp);
        publish (peakLiveR, (float) right.peakDisp);
        publish (charLiveL, (float) left.charDisp);
        publish (charLiveR, (float) right.charDisp);
        publish (overYellowL, yellowL);
        publish (overYellowR, yellowR);
        publish (overRedL, redL);
        publish (overRedR, redR);
        publish (dynamicRangeMinDb, (float) lraCombinedMin);
        publish (dynamicRangeMaxDb, (float) lraCombinedMax);
    }

    // Thread-safe reads for the (future) editor.
    float getGrHeldL() const noexcept { return grHeldL.load (std::memory_order_relaxed); }
    float getGrHeldR() const noexcept { return grHeldR.load (std::memory_order_relaxed); }
    float getPeakHeldL() const noexcept { return peakHeldL.load (std::memory_order_relaxed); }
    float getPeakHeldR() const noexcept { return peakHeldR.load (std::memory_order_relaxed); }
    float getCharHeldL() const noexcept { return charHeldL.load (std::memory_order_relaxed); }
    float getCharHeldR() const noexcept { return charHeldR.load (std::memory_order_relaxed); }
    bool  getOverYellow() const noexcept { return overYellow.load (std::memory_order_relaxed); }
    bool  getOverRed() const noexcept { return overRed.load (std::memory_order_relaxed); }
    float getDynamicRangeDb() const noexcept { return dynamicRangeDb.load (std::memory_order_relaxed); }
    float getLufsShortTermDb() const noexcept { return lufsShortTermDb.load (std::memory_order_relaxed); }

    // GUI-only additions (Stage 9) -- see the reset()/updateOncePerHostSample() comments
    // above for why these exist alongside the held/combined values above.
    float getGrLiveL() const noexcept { return grLiveL.load (std::memory_order_relaxed); }
    float getGrLiveR() const noexcept { return grLiveR.load (std::memory_order_relaxed); }
    float getPeakLiveL() const noexcept { return peakLiveL.load (std::memory_order_relaxed); }
    float getPeakLiveR() const noexcept { return peakLiveR.load (std::memory_order_relaxed); }
    float getCharLiveL() const noexcept { return charLiveL.load (std::memory_order_relaxed); }
    float getCharLiveR() const noexcept { return charLiveR.load (std::memory_order_relaxed); }
    bool  getOverYellowL() const noexcept { return overYellowL.load (std::memory_order_relaxed); }
    bool  getOverYellowR() const noexcept { return overYellowR.load (std::memory_order_relaxed); }
    bool  getOverRedL() const noexcept { return overRedL.load (std::memory_order_relaxed); }
    bool  getOverRedR() const noexcept { return overRedR.load (std::memory_order_relaxed); }
    float getDynamicRangeMinDb() const noexcept { return dynamicRangeMinDb.load (std::memory_order_relaxed); }
    float getDynamicRangeMaxDb() const noexcept { return dynamicRangeMaxDb.load (std::memory_order_relaxed); }

private:
    //==============================================================================
    struct Channel
    {
        double peakDisp = -60.0;   // JSFX tp_disp_L/R
        double dispGr = 0.0;       // JSFX disp_gr_L/R
        double charDisp = 0.0;     // JSFX char_disp_L/R

        double grHoldVal = 0.0, grHoldTimer = 0.0;
        double peakHoldVal = -60.0, peakHoldTimer = 0.0;
        double charHoldVal = 0.0, charHoldTimer = 0.0;

        double lraMax = 0.0, lraMaxState = 0.0;
        double lraMin = 0.0, lraMinState = 0.0;

        void reset()
        {
            peakDisp = -60.0;
            dispGr = 0.0;
            charDisp = 0.0;
            grHoldVal = 0.0; grHoldTimer = 0.0;
            peakHoldVal = -60.0; peakHoldTimer = 0.0;
            charHoldVal = 0.0; charHoldTimer = 0.0;
            lraMax = 0.0; lraMaxState = 0.0;
            lraMin = 0.0; lraMinState = 0.0;
        }

        // Matches the JSFX's tp_level_L_db / tp_disp_L attack-release update, run once
        // per oversampled tick.
        void updatePeakLevel (double val, double attackCoeff, double releaseCoeff)
        {
            const double levelDbVal = 20.0 * std::log (std::max (std::abs (val), 1e-10)) / std::log (10.0);
            peakDisp = (levelDbVal > peakDisp) ? peakDisp + (levelDbVal - peakDisp) * attackCoeff
                                                : peakDisp + (levelDbVal - peakDisp) * releaseCoeff;
        }
    };

    static double levelDb (double val)
    {
        return 20.0 * std::log (std::max (std::abs (val), 1e-10)) / std::log (10.0);
    }

    // Matches peak_hold_high(): jumps instantly to a new extreme (higher = more
    // extreme), holds, then falls back at fall_per_sample_db per sample. No denormal
    // flush -- the JSFX's own peak_hold_high/low don't call dn() either.
    double peakHoldHigh (double& holdVal, double& holdTimer, double currentVal) const
    {
        if (currentVal > holdVal)
        {
            holdVal = currentVal;
            holdTimer = peakHoldTimeSamples;
        }
        else if (holdTimer > 0.0)
        {
            holdTimer -= 1.0;
        }
        else
        {
            holdVal -= peakHoldFallPerSampleDb;
            if (holdVal < currentVal) holdVal = currentVal;
        }
        return holdVal;
    }

    // Matches peak_hold_low(): mirror of the above, lower = more extreme (used for GR,
    // where deeper reduction is more negative).
    double peakHoldLow (double& holdVal, double& holdTimer, double currentVal) const
    {
        if (currentVal < holdVal)
        {
            holdVal = currentVal;
            holdTimer = peakHoldTimeSamples;
        }
        else if (holdTimer > 0.0)
        {
            holdTimer -= 1.0;
        }
        else
        {
            holdVal += peakHoldFallPerSampleDb;
            if (holdVal > currentVal) holdVal = currentVal;
        }
        return holdVal;
    }

    // Matches lra_track_high()/lra_track_low(): eases toward current_val, never jumps.
    // Unlike peak-hold, these DO flush denormals, matching the JSFX.
    static double lraTrackHigh (double& state, double currentVal, double attackCoeff, double releaseCoeff)
    {
        state = (currentVal > state) ? state + (currentVal - state) * attackCoeff
                                      : state + (currentVal - state) * releaseCoeff;
        state = flushDenormal (state);
        return state;
    }

    static double lraTrackLow (double& state, double currentVal, double attackCoeff, double releaseCoeff)
    {
        state = (currentVal < state) ? state + (currentVal - state) * attackCoeff
                                      : state + (currentVal - state) * releaseCoeff;
        state = flushDenormal (state);
        return state;
    }

    // Matches compute_lufs_kweight_coefs(): stage 1 is just compute_high_shelf at a
    // fixed +3.999dB/1681Hz (the JSFX's own "will normalize" scratch locals for an
    // alternate stage-1 formula are computed but never used -- dead code, not ported).
    // Stage 2 is a manual RBJ highpass at ~38Hz, Q=0.5.
    void computeLufsKweightCoeffs (double srateVal)
    {
        lufsKw1Coef = computeHighShelf (1681.0, 3.999, srateVal);

        constexpr double pi = 3.14159265358979323846;
        const double w0 = 2.0 * pi * 38.0 / srateVal;
        const double sw = std::sin (w0), cw = std::cos (w0);
        const double ab = sw / (2.0 * 0.5); // Q = 0.5
        const double b0 = (1.0 + cw) / 2.0;
        const double b1 = -(1.0 + cw);
        const double b2 = (1.0 + cw) / 2.0;
        const double a0 = 1.0 + ab;
        const double a1 = -2.0 * cw;
        const double a2 = 1.0 - ab;
        lufsKw2Coef = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    double lufsKweight (BiquadState& kw1, BiquadState& kw2, double x) const
    {
        const double y1 = kw1.process (lufsKw1Coef, x);
        return kw2.process (lufsKw2Coef, y1);
    }

    template <typename T>
    static void publish (std::atomic<T>& target, T value) { target.store (value, std::memory_order_relaxed); }

    //==============================================================================
    Channel left, right;

    double sampleRate = 44100.0;
    double meterDecayCoeff = 0.0;

    int peakHoldTimeSamples = 0;
    double peakHoldFallPerSampleDb = 0.0;

    double lraMaxAttackCoeff = 0.0, lraMaxReleaseCoeff = 0.0;
    double lraMinAttackCoeff = 0.0, lraMinReleaseCoeff = 0.0;

    double peakAttackCoeff = 0.0, peakReleaseCoeff = 0.0;

    double lufsMsCoeff = 0.0;
    double lufsMsEnv = 0.0;
    BiquadCoeffs lufsKw1Coef, lufsKw2Coef;
    BiquadState lufsKw1L, lufsKw2L, lufsKw1R, lufsKw2R;

    const double ln10 = std::log (10.0);

    std::atomic<float> grHeldL { 0.0f }, grHeldR { 0.0f };
    std::atomic<float> peakHeldL { -60.0f }, peakHeldR { -60.0f };
    std::atomic<float> charHeldL { 0.0f }, charHeldR { 0.0f };
    std::atomic<bool> overYellow { false }, overRed { false };
    std::atomic<float> dynamicRangeDb { 0.0f };
    std::atomic<float> lufsShortTermDb { -70.0f };

    std::atomic<float> grLiveL { 0.0f }, grLiveR { 0.0f };
    std::atomic<float> peakLiveL { -60.0f }, peakLiveR { -60.0f };
    std::atomic<float> charLiveL { 0.0f }, charLiveR { 0.0f };
    std::atomic<bool> overYellowL { false }, overYellowR { false };
    std::atomic<bool> overRedL { false }, overRedR { false };
    std::atomic<float> dynamicRangeMinDb { 0.0f }, dynamicRangeMaxDb { 0.0f };
};

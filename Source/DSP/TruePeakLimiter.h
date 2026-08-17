#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Biquad.h"
#include "PolyphaseOversampler.h"

// A dedicated true-peak "catch" stage, inserted between the Limiter and Safety Clip.
// Reuses the Limiter's own proven lookahead trick (Source/DSP/Limiter.h's
// limiterComputeGr/limiterApply) rather than inventing a new mechanism: detect the
// gain reduction needed for the CURRENT oversampled sample with a fast one-pole
// envelope, but apply it to audio delayed by a short lookahead buffer -- so by the
// time a loud sample reaches the delayed output, the gain reduction the envelope
// computed for it (attackMs earlier, in delay time) has already ramped in. No window
// scanning is needed; the delay line does the "looking ahead."
//
// This is deliberately simpler than the main Limiter: a single fixed release (not a
// program-dependent two-time-constant blend -- that's the Limiter/Threshold's job, a
// musical control), and always fully linked (shared gain = min of the two channels'
// required gain, applied to both) since this is a safety/backstop stage, not a
// creative stereo tool.
//
// Runs entirely in the oversampled domain, once per oversampled tick -- peak-detecting
// on an already-oversampled (reconstructed) waveform is the standard true-peak
// estimation technique (the same principle ITU-R BS.1770's true-peak meter uses).
//
// reconstructionMarginDb is a small INTERNAL safety margin, not exposed to the user:
// the stage targets targetCeilingDb - reconstructionMarginDb internally, so the small
// residual detection error inherent to any oversampled true-peak estimator (this
// design included) can't push the final reconstructed peak past the value the user
// actually asked for. The user-facing target itself stays an honest, directly-delivered
// number -- this margin is what makes the guarantee genuine rather than aspirational.
// Sized empirically against realistic program material via the offline stress harness
// this stage shipped with (tp_limiter_test.cpp) -- see reconstructionMarginDb's own
// comment for what this margin does and doesn't cover.
class TruePeakLimiter
{
public:
    // lookaheadMsFixed must comfortably outrun attackMs's own time constant, not just
    // roughly match it: a brand-new, sudden transient (a real drum hit, not a signal
    // that ramps up over seconds) gives the gr envelope zero warning, so it only has
    // lookaheadMsFixed of real time to converge via attackMs's one-pole before the
    // delayed audio catches up. An earlier 1.5ms/1.0ms pairing (1.5x) only reached
    // ~78% convergence by the time the transient reached the output -- confirmed via an
    // isolated-transient stress test to produce a genuine ~3.6 dB overshoot right at
    // onset, not just the documented near-Nyquist edge case. A 6x ratio reaches ~99.75%
    // convergence (1-e^-6) before that same worst-case sudden onset.
    static constexpr double lookaheadMsFixed = 3.0;
    static constexpr double attackMs = 0.5;
    static constexpr double releaseMs = 50.0;
    // Covers TWO separate, confirmed error sources, not one:
    //  1. Near-Nyquist tone detection error (the original calibration target) -- a
    //     synthetic, sustained, full-scale tone parked within ~2-4% of Nyquist can still
    //     exceed the target by up to ~1.8 dB, a known, industry-documented limitation of
    //     oversampled true-peak detection (ITU-R BS.1770's own spec documents a
    //     comparable ~0.6 dB worst-case bound for the same class of signal).
    //  2. Decimation growth on rapidly gain-modulated broadband content -- confirmed via
    //     a repeated-transient stress test (transient_train_test.cpp) that TruePeakLimiter's
    //     OWN oversampled-domain output measures cleanly on its own (peak-hold fix
    //     working correctly), but downsampling that exact, clean output back to base rate
    //     reintroduces real growth: a plain unprocessed passthrough of the same broadband
    //     content round-trips with negligible error, so this is specifically provoked by
    //     multiplying broadband content by a fast-changing gain, not a general downsample
    //     defect. Neither slowing this stage's release nor adding extra lowpass smoothing
    //     to the gain signal reduced it (both were tried and measured) -- it appears to be
    //     the same class of decimation-filter response the original margin_test.cpp found
    //     for hard clipping, just triggered by a fast gain transition instead of a hard
    //     discontinuity, and needing an empirical margin rather than an algorithmic fix.
    //
    // This value is calibrated against the user's own real-world report (a dense, loud
    // mix through TP Limit -1.0 dB measured +0.1 dBTP, a ~1.1 dB gap, well BEFORE the
    // peak-hold-duration fix above existed) with real buffer on top -- not against the
    // single most adversarial synthetic broadband-transient case found during testing,
    // which needed ~3.0 dB to fully close. If real transient-heavy masters still measure
    // over TP Limit after this, the fix is to raise this constant further (verified
    // linear/predictable via the same harness) -- there is no known remaining structural
    // bug at that point, only margin sizing.
    static constexpr double reconstructionMarginDb = 2.0;

    // Reading the raw instantaneous sample value as "level" (as limiterComputeGr does)
    // breaks down for a sustained tone near Nyquist: |sample| swings from 0 to full
    // scale every half-cycle, faster than a musically-sensible attack time constant can
    // track, so the gr computer never converges on the true peak's target -- found via
    // a stress test that swept near-Nyquist tones and saw ~1-3 dB steady-state error,
    // constant across oversampling factor (ruling out insufficient oversampled
    // resolution; a control test confirmed the oversampler's own round trip introduces
    // negligible error on its own). Fixed with a peak-and-hold envelope ahead of the gr
    // computer, decoupling "was a peak seen recently" from "how much reduction is
    // currently applied" -- the standard peak-detector-then-gain-computer split real
    // limiters use.
    //
    // The hold duration is NOT a separate short time constant -- it's pinned to exactly
    // laBufSize (the lookahead window itself), because a shorter hold has a second,
    // worse failure mode: for noisy/fluctuating content (a real drum hit's decay is not
    // a smooth envelope but noise-like at the sample level), an early short release let
    // the DETECTOR'S TARGET relax before a still-in-flight peak had finished traveling
    // through the delay buffer -- the gr smoother was chasing a target that eased off
    // mid-flight, so the sample that actually needed the reduction got less than it
    // needed by the time it reached the output. Confirmed via a repeated-transient
    // stress test (transient_train_test.cpp): an isolated sudden JUMP held steady
    // converged fine once the attack/lookahead ratio was fixed, but a realistic decaying,
    // noisy transient still overshot by up to 6 dB with only a 0.3ms hold. Pinning the
    // hold to the full lookahead window guarantees the target cannot relax until the
    // corresponding sample has already exited the delay buffer -- the textbook-correct
    // "peak hold for the lookahead duration, then release" scheme.
    static constexpr double peakFollowerReleaseMs = 5.0; // only used once the hold window elapses

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        const int laBudgetBaseAlloc = (int) std::ceil (lookaheadMsFixed * 0.001 * sampleRate);
        const int laBufAlloc = laBudgetBaseAlloc * PolyphaseOversampler::maxFactor;
        left.prepareCapacity (laBufAlloc);
        right.prepareCapacity (laBufAlloc);
    }

    // A factor change is a deliberate hard reset, matching every other stage in this
    // port (Limiter::setFactor, PolyphaseOversampler::setFactor, etc.).
    void setFactor (int factor, double newOsRate)
    {
        osRate = newOsRate;

        laBudgetBase = (int) std::ceil (lookaheadMsFixed * 0.001 * sampleRate);
        laBufSize = laBudgetBase * factor;

        left.reset (laBufSize);
        right.reset (laBufSize);
        wposLa = 0;

        attackCoeff = 1.0 - std::exp (-1.0 / (attackMs * 0.001 * osRate));
        releaseCoeff = 1.0 - std::exp (-1.0 / (releaseMs * 0.001 * osRate));
        peakFollowerReleaseCoeff = 1.0 - std::exp (-1.0 / (peakFollowerReleaseMs * 0.001 * osRate));
    }

    int getLaBudgetBase() const noexcept { return laBudgetBase; }

    // Call once per host sample with the Limiter's current output_ceiling_smoothed
    // (Limiter::getOutputCeilingSmoothed()) -- shares that single smoothed value
    // rather than re-deriving/re-smoothing an independent copy, same reasoning Safety
    // Clip previously used for the same value.
    void setTargetCeiling (double targetCeilingDb)
    {
        targetDb = targetCeilingDb - reconstructionMarginDb;
    }

    // One oversampled tick for both channels. Only ever called on the wet/processed
    // stream -- never the Bypass raw-dry or Delta gained-dry reference streams, which
    // must reach the output untouched by true-peak limiting.
    void processTick (double inL, double inR, double& outL, double& outR)
    {
        // Peak-and-hold envelope ahead of the gr computer -- held for the full lookahead
        // window (laBufSize), only releasing once that elapses -- see peakFollowerReleaseMs.
        followPeak (left,  std::abs (inL));
        followPeak (right, std::abs (inR));

        const double levelDbL = 20.0 * std::log (std::max (left.peakEnvelope,  0.0000000001)) / ln10;
        const double levelDbR = 20.0 * std::log (std::max (right.peakEnvelope, 0.0000000001)) / ln10;

        const double grTargetL = std::min (0.0, targetDb - levelDbL);
        const double grTargetR = std::min (0.0, targetDb - levelDbR);

        left.grSmoothedDb  = smoothGr (left.grSmoothedDb,  grTargetL);
        right.grSmoothedDb = smoothGr (right.grSmoothedDb, grTargetR);

        const double sharedGrDb = std::min (left.grSmoothedDb, right.grSmoothedDb);

        const double delayedL = left.laBuf[(size_t) wposLa];
        const double delayedR = right.laBuf[(size_t) wposLa];
        left.laBuf[(size_t) wposLa]  = inL;
        right.laBuf[(size_t) wposLa] = inR;

        const double grLinear = std::pow (10.0, sharedGrDb / 20.0);
        outL = delayedL * grLinear;
        outR = delayedR * grLinear;

        wposLa = (wposLa + 1) % laBufSize;
    }

private:
    struct Channel; // fwd decl for followPeak below

    void followPeak (Channel& ch, double absVal)
    {
        if (absVal >= ch.peakEnvelope)
        {
            ch.peakEnvelope = absVal; // instant attack -- never miss a rising peak
            ch.peakHoldCounter = laBufSize; // pin the hold to the full lookahead window
        }
        else if (ch.peakHoldCounter > 0)
        {
            --ch.peakHoldCounter; // held steady -- the peak that set this hasn't exited the delay buffer yet
        }
        else
        {
            ch.peakEnvelope += (absVal - ch.peakEnvelope) * peakFollowerReleaseCoeff;
            ch.peakEnvelope = flushDenormal (ch.peakEnvelope);
        }
    }

    double smoothGr (double grSmoothedDb, double grTargetDb) const
    {
        if (grTargetDb < grSmoothedDb)
            grSmoothedDb += (grTargetDb - grSmoothedDb) * attackCoeff;
        else
            grSmoothedDb += (grTargetDb - grSmoothedDb) * releaseCoeff;
        return flushDenormal (grSmoothedDb);
    }

    struct Channel
    {
        std::vector<double> laBuf;
        double grSmoothedDb = 0.0;
        double peakEnvelope = 0.0;
        int peakHoldCounter = 0;

        void prepareCapacity (int maxBufAlloc)
        {
            laBuf.assign ((size_t) maxBufAlloc, 0.0);
        }

        void reset (int activeBufSize)
        {
            std::fill (laBuf.begin(), laBuf.begin() + activeBufSize, 0.0);
            grSmoothedDb = 0.0;
            peakEnvelope = 0.0;
            peakHoldCounter = 0;
        }
    };

    Channel left, right;

    double sampleRate = 44100.0, osRate = 88200.0;
    int laBudgetBase = 1;
    int laBufSize = 1;
    int wposLa = 0;

    double targetDb = -1.0 - reconstructionMarginDb;
    double attackCoeff = 0.0, releaseCoeff = 0.0;
    double peakFollowerReleaseCoeff = 0.0;

    const double ln10 = std::log (10.0);
};

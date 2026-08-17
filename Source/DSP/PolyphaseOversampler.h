#pragma once

#include <cmath>
#include <vector>

// Ports the JSFX's oversampling engine (@init's DELAY_SAMPLES_BASE / MAX_FACTOR /
// MAX_NTAPS / RAW_HIST_LEN block, plus compute_coeffs / poly_up / down_push /
// down_convolve) as closely to the original structure as possible, so the two stay
// null-testable against each other. One instance handles one audio channel; the
// coefficient computation is duplicated per instance rather than shared, since it
// only runs on factor changes, not per-sample.
//
// DELAY_SAMPLES_BASE is a fixed base-rate-equivalent group delay for the combined
// upsample+downsample FIR round trip -- by construction, independent of which
// factor (2x/4x) is active, matching the JSFX's "budgets by construction" pattern.
class PolyphaseOversampler
{
public:
    static constexpr int delaySamplesBase = 121;
    static constexpr int maxFactor        = 4;
    static constexpr int maxNumTaps       = delaySamplesBase * maxFactor + 1;
    static constexpr int rawHistLen       = delaySamplesBase + 1;

    PolyphaseOversampler()
    {
        rawHist.assign ((size_t) (2 * rawHistLen), 0.0);
        downBuf.assign ((size_t) (2 * maxNumTaps), 0.0);
        downBuf2.assign ((size_t) (2 * maxNumTaps), 0.0);
        downBuf3.assign ((size_t) (2 * maxNumTaps), 0.0);
        setFactor (2);
    }

    // factor must be 2 or 4 (JSFX os_choice: 0 -> 2x, 1 -> 4x).
    void setFactor (int newFactor)
    {
        factor = (newFactor <= 2) ? 2 : 4;
        numTaps = delaySamplesBase * factor + 1;
        computeCoeffs();
        reset();
    }

    void reset()
    {
        std::fill (rawHist.begin(), rawHist.end(), 0.0);
        std::fill (downBuf.begin(), downBuf.end(), 0.0);
        std::fill (downBuf2.begin(), downBuf2.end(), 0.0);
        std::fill (downBuf3.begin(), downBuf3.end(), 0.0);
        histWritePos = 0;
        downWritePos = 0;
        downWritePos2 = 0;
        downWritePos3 = 0;
    }

    int getFactor() const noexcept  { return factor; }
    int getNumTaps() const noexcept { return numTaps; }

    // Upsamples one base-rate input sample into `factor` oversampled-rate samples,
    // written into upsampledOut[0 .. factor).
    void upsample (double input, double* upsampledOut)
    {
        rawHist[(size_t) histWritePos] = input * factor;
        rawHist[(size_t) (histWritePos + rawHistLen)] = input * factor;
        const int histBase = histWritePos + rawHistLen;

        for (int j = 0; j < factor; ++j)
            upsampledOut[j] = polyUp (histBase, j);

        histWritePos = (histWritePos + 1) % rawHistLen;
    }

    // Pushes the j-th oversampled-rate sample of this host sample (j in
    // [0, factor)) into the decimation buffer. Only j == 0 actually produces a
    // decimated output -- matching the JSFX, which convolves once per host sample
    // (the other factor-1 phases are computed and discarded by construction of
    // decimation, so there's no point running the FIR sum for them).
    double downsample (int j, double value)
    {
        downBuf[(size_t) downWritePos] = value;
        downBuf[(size_t) (downWritePos + numTaps)] = value;

        const double out = (j == 0) ? downConvolve() : 0.0;

        downWritePos = (downWritePos + 1) % numTaps;
        return out;
    }

    // A second, independent decimation stream sharing this instance's coefficients but
    // with its own buffer/position -- matches the JSFX's pattern of multiple down_bufs
    // (down_bufL, down_dry_bufL, down_dry_raw_bufL, ...) all sharing coeffs/wpos_down.
    // Rather than trying to share a single write position across call sites, this keeps
    // its own downWritePos2 that simply advances in lockstep (same j, same call count
    // per host sample as the primary downsample()) -- always numerically identical to
    // downWritePos, just decoupled so callers don't have to interleave calls carefully.
    // Used for Bypass's raw-dry reference (out_dry_raw_L/R).
    //
    // needsOutput: the buffer WRITE always happens (matches the JSFX's "keep all
    // downsample buffers filled so mode switches stay seamless with no stutter" --
    // skipping writes would mean the FIR's internal history is stale/zero the moment
    // this stream's mode gets switched on, producing an audible glitch for one
    // numTaps-long window). Only the expensive O(numTaps) convolution itself is
    // skipped when the caller doesn't currently need this stream's output -- matches
    // the JSFX's own CPU-optimization comment ("raw-dry FIR only when Bypass active"),
    // which the spec explicitly says isn't mandatory to replicate but is a real,
    // low-risk win once a second consumer (downsampleTertiary) made computing every
    // stream unconditionally add up (confirmed via a 4x-oversampling stutter after
    // Delta Listen Mode's stream was added on top of this one).
    double downsampleSecondary (int j, double value, bool needsOutput = true)
    {
        downBuf2[(size_t) downWritePos2] = value;
        downBuf2[(size_t) (downWritePos2 + numTaps)] = value;

        const double out = (j == 0 && needsOutput) ? downConvolveSecondary() : 0.0;

        downWritePos2 = (downWritePos2 + 1) % numTaps;
        return out;
    }

    // A third, independent decimation stream, same pattern (and same needsOutput
    // reasoning) as downsampleSecondary(). Used for Delta Listen Mode's gained-dry
    // reference (out_dry_L/R) -- needed alongside downsampleSecondary() (Bypass's
    // raw-dry), not instead of it, since Bypass and Delta each need their own
    // differently-gained dry signal.
    double downsampleTertiary (int j, double value, bool needsOutput = true)
    {
        downBuf3[(size_t) downWritePos3] = value;
        downBuf3[(size_t) (downWritePos3 + numTaps)] = value;

        const double out = (j == 0 && needsOutput) ? downConvolveTertiary() : 0.0;

        downWritePos3 = (downWritePos3 + 1) % numTaps;
        return out;
    }

private:
    double polyUp (int basePos, int phase) const
    {
        double sum = 0.0;
        int i = 0;
        int tapIdx = phase;
        while (tapIdx < numTaps)
        {
            sum += coeffs[(size_t) tapIdx] * rawHist[(size_t) (basePos - i)];
            tapIdx += factor;
            ++i;
        }
        return sum;
    }

    double downConvolve() const
    {
        const int rs = downWritePos + 1;
        double sum = 0.0;
        for (int i = 0; i < numTaps; ++i)
            sum += coeffs[(size_t) i] * downBuf[(size_t) (rs + i)];
        return sum;
    }

    double downConvolveSecondary() const
    {
        const int rs = downWritePos2 + 1;
        double sum = 0.0;
        for (int i = 0; i < numTaps; ++i)
            sum += coeffs[(size_t) i] * downBuf2[(size_t) (rs + i)];
        return sum;
    }

    double downConvolveTertiary() const
    {
        const int rs = downWritePos3 + 1;
        double sum = 0.0;
        for (int i = 0; i < numTaps; ++i)
            sum += coeffs[(size_t) i] * downBuf3[(size_t) (rs + i)];
        return sum;
    }

    void computeCoeffs()
    {
        constexpr double pi = 3.14159265358979323846;
        const double twoPi = 2.0 * pi;
        const double fc = 1.0 / (2.0 * factor);
        const double center = (numTaps - 1) / 2.0;

        coeffs.assign ((size_t) numTaps, 0.0);
        double sum = 0.0;

        for (int n = 0; n < numTaps; ++n)
        {
            const double m = n - center;
            const double sincVal = (std::abs (m) < 0.5)
                                        ? (2.0 * fc)
                                        : (std::sin (twoPi * fc * m) / (pi * m));
            const double window = 0.42 - 0.5 * std::cos (twoPi * n / (numTaps - 1))
                                        + 0.08 * std::cos (2.0 * twoPi * n / (numTaps - 1));
            coeffs[(size_t) n] = sincVal * window;
            sum += coeffs[(size_t) n];
        }

        for (auto& c : coeffs)
            c /= sum;
    }

    int factor = 2;
    int numTaps = delaySamplesBase * 2 + 1;
    int histWritePos = 0;
    int downWritePos = 0;
    int downWritePos2 = 0;
    int downWritePos3 = 0;

    std::vector<double> coeffs;
    std::vector<double> rawHist;
    std::vector<double> downBuf;
    std::vector<double> downBuf2;
    std::vector<double> downBuf3;
};

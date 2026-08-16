#pragma once

#include <cmath>

// Ports the JSFX's two-stage true-peak-aware clip: an oversampled-domain stage (applied
// to the processed signal before it's pushed into the downsample FIR, so a peak gets
// caught before decimation can smear it) and a post-downsample backstop (applied after,
// catching whatever the FIR's own passband ripple/ringing lets slip back toward the
// ceiling). Both stages clip to the same safety_ceiling_lin.
//
// This class deliberately owns no smoothing of its own. The JSFX computes
// safety_ceiling_lin from a single, shared output_ceiling_smoothed -- the exact same
// smoothed value the Limiter already tracks for its Auto Gain cap. Re-deriving a second,
// independently-smoothed copy here would risk it silently drifting out of sync with the
// Limiter's; instead the caller pulls output_ceiling_smoothed from Limiter::
// getOutputCeilingSmoothed() once per host sample and pushes it in here.
class SafetyClip
{
public:
    static constexpr double ispMarginDb = 0.5; // JSFX ISP_MARGIN_DB

    // Call once per host sample with the Limiter's current output_ceiling_smoothed.
    void setOutputCeilingSmoothed (double outputCeilingSmoothedDb)
    {
        ceilingLin = std::pow (10.0, (outputCeilingSmoothedDb - ispMarginDb) / 20.0);
    }

    double clip (double val) const
    {
        if (val > ceilingLin)  return ceilingLin;
        if (val < -ceilingLin) return -ceilingLin;
        return val;
    }

    double getCeilingLin() const noexcept { return ceilingLin; }

private:
    // Matches the default output_ceiling_db=-1 so a freshly-constructed instance is
    // already correct before the first setOutputCeilingSmoothed() call.
    double ceilingLin = std::pow (10.0, (-1.0 - ispMarginDb) / 20.0);
};

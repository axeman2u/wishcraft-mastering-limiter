#pragma once

#include <cmath>

// The final digital-domain backstop, applied after the real true-peak work is done
// upstream by TruePeakLimiter (Source/DSP/TruePeakLimiter.h). Two-stage placement is
// preserved from the original port: an oversampled-domain clip (applied to the
// processed signal before it's pushed into the downsample FIR) and a post-downsample
// clip (catching whatever the FIR's own passband ripple lets slip back toward 0 dBFS).
//
// Deliberately a FIXED constant, not derived from output_ceiling_db/output_ceiling_
// smoothed: that coupling used to make this the plugin's only true-peak protection,
// which margin_test.cpp/margin_test_01.cpp proved can't actually deliver a true-peak
// guarantee no matter how tight its margin is (hard-clipping in the oversampled domain
// causes genuine inter-sample overshoot on reconstruction, independent of margin size).
// Now that TruePeakLimiter does that job with smooth gain reduction, this stage only
// needs to stop literal digital-domain overs -- true 0 dBFS is the natural, maximally
// permissive line for that, and it should trigger only rarely.
class SafetyClip
{
public:
    static constexpr double backstopCeilingDb = 0.0;

    double clip (double val) const
    {
        if (val > ceilingLin)  return ceilingLin;
        if (val < -ceilingLin) return -ceilingLin;
        return val;
    }

    double getCeilingLin() const noexcept { return ceilingLin; }

private:
    double ceilingLin = std::pow (10.0, backstopCeilingDb / 20.0);
};

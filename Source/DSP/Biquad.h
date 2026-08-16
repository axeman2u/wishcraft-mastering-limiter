#pragma once

#include <algorithm>
#include <cmath>

// Denormal protection matching the JSFX's dn(): flushes to a hard zero once a value has
// decayed into subnormal-float territory (threshold ~-300 dB, nowhere near audible).
inline double flushDenormal (double v)
{
    return std::abs (v) < 0.000000000000001 ? 0.0 : v;
}

//==============================================================================
// Direct Form I biquad, matching the JSFX's biquad_process() exactly -- including that
// only the y-feedback path is flushed for denormals (x1/x2 are just delayed copies of
// real input, never denormal-prone on their own).
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

struct BiquadState
{
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    double process (const BiquadCoeffs& c, double x)
    {
        double y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
        y = flushDenormal (y);
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

// RBJ Audio EQ Cookbook shelf formulas, matching compute_low_shelf/compute_high_shelf
// exactly -- S=1 (a fixed, moderately steep shelf slope, not user-exposed). The
// "(1/1 - 1)" term in the JSFX is always exactly 0 (S hardcoded to 1); kept in literal
// form here for numerical parity with the source rather than algebraically simplified.
inline BiquadCoeffs computeLowShelf (double freq, double gainDb, double sampleRate)
{
    constexpr double pi = 3.14159265358979323846;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double sqrtA = std::sqrt (A);
    const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);

    const double b0 =     A * ((A + 1) - (A - 1) * cw + 2 * sqrtA * alpha);
    const double b1 = 2 * A * ((A - 1) - (A + 1) * cw);
    const double b2 =     A * ((A + 1) - (A - 1) * cw - 2 * sqrtA * alpha);
    const double a0 =         (A + 1) + (A - 1) * cw + 2 * sqrtA * alpha;
    const double a1 =    -2 * ((A - 1) + (A + 1) * cw);
    const double a2 =         (A + 1) + (A - 1) * cw - 2 * sqrtA * alpha;

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline BiquadCoeffs computeHighShelf (double freq, double gainDb, double sampleRate)
{
    constexpr double pi = 3.14159265358979323846;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * freq / sampleRate;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double sqrtA = std::sqrt (A);
    const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);

    const double b0 =     A * ((A + 1) + (A - 1) * cw + 2 * sqrtA * alpha);
    const double b1 =-2 * A * ((A - 1) + (A + 1) * cw);
    const double b2 =     A * ((A + 1) + (A - 1) * cw - 2 * sqrtA * alpha);
    const double a0 =         (A + 1) - (A - 1) * cw + 2 * sqrtA * alpha;
    const double a1 =     2 * ((A - 1) - (A + 1) * cw);
    const double a2 =         (A + 1) - (A - 1) * cw - 2 * sqrtA * alpha;

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

// Cascaded low-shelf then high-shelf -- one instance per detection point per channel,
// matching the JSFX's sc_filter(). Detector-only: never touches the audio path itself.
struct SidechainFilter
{
    BiquadState low, high;

    double process (const BiquadCoeffs& lowCoeffs, const BiquadCoeffs& highCoeffs, double x)
    {
        const double y1 = low.process (lowCoeffs, x);
        return high.process (highCoeffs, y1);
    }
};

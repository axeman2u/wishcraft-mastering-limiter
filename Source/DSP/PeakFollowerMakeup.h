#pragma once

#include <algorithm>
#include <cmath>

#include "Biquad.h" // for flushDenormal

// Peak-follower level tracker on both a dry and a processed signal (fast attack catches
// the actual peak, slower release holds a "recent peak" estimate), with the ratio applied
// as makeup gain -- matches the JSFX's apply_makeup(). Used by the Selective Clipper's
// permanently-on Auto Makeup Gain (clamp [0, 12] dB -- never attenuate, only restore what
// clipping took).
inline double applyPeakRatioMakeup (double& peakDry, double& peakChar,
                                     double dryVal, double charVal,
                                     double attackCoeff, double releaseCoeff,
                                     double minGainDb, double maxGainDb)
{
    const double dryAbs = std::abs (dryVal);
    const double charAbs = std::abs (charVal);

    if (dryAbs > peakDry) peakDry += (dryAbs - peakDry) * attackCoeff;
    else                  peakDry += (dryAbs - peakDry) * releaseCoeff;

    if (charAbs > peakChar) peakChar += (charAbs - peakChar) * attackCoeff;
    else                    peakChar += (charAbs - peakChar) * releaseCoeff;

    peakDry = flushDenormal (peakDry);
    peakChar = flushDenormal (peakChar);

    double gainLin = peakDry / std::max (peakChar, 0.0000001);
    gainLin = std::min (std::max (gainLin, std::pow (10.0, minGainDb / 20.0)), std::pow (10.0, maxGainDb / 20.0));
    return charVal * gainLin;
}

// Peak-follower on just the processed signal, gain-matched toward a FIXED target level
// (not a second peak-followed signal) -- backs the Limiter's user-toggleable Gain Match.
// Unlike applyPeakRatioMakeup, the target here doesn't need its own peak-following: it's
// already a smoothed control-rate value (the TP Limit ceiling), not fluctuating audio.
// Can both cut AND boost (see minGainDb/maxGainDb), unlike Auto Makeup Gain above --
// Gain Match's whole point is landing near a fixed loud reference regardless of whether
// the current Threshold setting naturally sits above or below it.
inline double applyGainMatch (double& peakChar, double targetLin, double charVal,
                               double attackCoeff, double releaseCoeff,
                               double minGainDb, double maxGainDb)
{
    const double charAbs = std::abs (charVal);

    if (charAbs > peakChar) peakChar += (charAbs - peakChar) * attackCoeff;
    else                    peakChar += (charAbs - peakChar) * releaseCoeff;

    peakChar = flushDenormal (peakChar);

    double gainLin = targetLin / std::max (peakChar, 0.0000001);
    gainLin = std::min (std::max (gainLin, std::pow (10.0, minGainDb / 20.0)), std::pow (10.0, maxGainDb / 20.0));
    return charVal * gainLin;
}

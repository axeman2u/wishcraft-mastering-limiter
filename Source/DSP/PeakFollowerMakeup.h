#pragma once

#include <algorithm>
#include <cmath>

#include "Biquad.h" // for flushDenormal

// Peak-follower level tracker on both a dry and a processed signal (fast attack catches
// the actual peak, slower release holds a "recent peak" estimate), with the ratio applied
// as makeup gain -- matches the JSFX's apply_makeup(). Reused for both the Selective
// Clipper's permanently-on Auto Makeup Gain and the Limiter's user-toggleable Gain Match
// (originally "Auto Gain" -- renamed per explicit user request, DSP behavior unchanged);
// each caller supplies its own clamp range (Character: [0, 12] dB, never attenuate, only
// restore what it took; Gain Match: [-60, 0], which can also reduce gain, since Drive
// alone can otherwise make the processed signal louder than dry with nothing to pull it
// back down).
//
// NOT RETARGETED: an attempt was made to have Gain Match target TP Limit instead of the
// dry/source signal here (a separate applyGainMatch() function, since removed) -- see
// Limiter.h's class-level doc comment for the full story of why that was tried, fixed
// once, and then explicitly reverted per user request. Don't reintroduce it without a
// fresh explicit request.
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

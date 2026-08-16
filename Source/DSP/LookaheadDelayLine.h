#pragma once

#include <algorithm>
#include <vector>

#include "PolyphaseOversampler.h"

// Ports the Limiter's lookahead buffer's *timing* only -- the JSFX's la_bufL/R,
// sized via LA_MAX_MS_CEILING / LA_BUDGET_BASE_ALLOC (@init) and LA_BUDGET_BASE
// (reconfigure()). No gain reduction is applied here yet (that's Stage 3+), so
// this is currently a plain circular delay, equivalent to the JSFX's bare_delay().
//
// Capacity is preallocated for the worst case (LA_MAX_MS_CEILING at the current
// sample rate, times the max oversampling factor) so that switching oversampling
// factor -- or, in a later stage, the lookahead-ms parameter -- never reallocates
// on the audio thread; only the active length (<= capacity) changes.
class LookaheadDelayLine
{
public:
    void prepare (double sampleRate)
    {
        constexpr double laMaxMsCeiling = 20.0;
        const int laBudgetBaseAlloc = (int) std::ceil (laMaxMsCeiling * 0.001 * sampleRate);
        const int laBufAlloc = laBudgetBaseAlloc * PolyphaseOversampler::maxFactor;

        buffer.assign ((size_t) std::max (1, laBufAlloc), 0.0);
        length = (int) buffer.size();
        writePos = 0;
    }

    // activeLength must not exceed the capacity established in prepare().
    void setActiveLength (int activeLength)
    {
        length = std::min ((int) buffer.size(), std::max (1, activeLength));
        writePos = 0;
        std::fill (buffer.begin(), buffer.end(), 0.0);
    }

    double process (double input)
    {
        const double out = buffer[(size_t) writePos];
        buffer[(size_t) writePos] = input;
        writePos = (writePos + 1) % length;
        return out;
    }

    int getActiveLength() const noexcept { return length; }

private:
    std::vector<double> buffer;
    int length = 1;
    int writePos = 0;
};

#pragma once
// Count-based distances adapted from oh-my-bmcu a003d33351a9370bea3246fe65a091a4f353f800.
// No motion timeouts, stall checks or state transitions belong here.
#include <stdint.h>

#define MOTION_MM_PER_COUNT (3.14159265358979323846f * 7.5f / 4096.0f)

// Shortest distance between wrapping positions. Each move must be shorter than
// 2^31 counts (about 12 km); the firmware's longest distance check is 10 m.
static inline uint32_t motion_count_distance(uint32_t pos, uint32_t start)
{
    const uint32_t d = pos - start;
    return (d > 0x80000000u) ? (0u - d) : d;
}

static inline float motion_counts_to_m(uint32_t counts)
{
    return (float)counts * (MOTION_MM_PER_COUNT * 0.001f);
}

static inline float motion_travel_m(uint32_t pos, uint32_t start)
{
    return motion_counts_to_m(motion_count_distance(pos, start));
}

// Signed count displacement, without an overflowing signed subtraction or an
// out-of-range unsigned-to-signed cast. Positive counts oppose filament meters.
static inline float motion_delta_m(uint32_t pos, uint32_t start)
{
    const uint32_t d = pos - start;
    return (d > 0x80000000u) ? -motion_counts_to_m(0u - d) : motion_counts_to_m(d);
}

// Rebuild printer telemetry from the full signed count, never from rounded
// per-sample float sums. The 1 m origin matches _filament::init(). Double is
// only an intermediate conversion: the unchanged wire field is still float32.
static inline float motion_odometer_m(int64_t counts)
{
    return (float)(1.0 + (double)counts * ((double)MOTION_MM_PER_COUNT * -0.001));
}

#pragma once
// Plausibility limit for AS5600 angle steps, used by AS5600_distance_updata. Hardware-free, tested on
// the host (test/test_as5600_jump).
//
// The soft-I2C driver only sees a NACK on the address and register bytes. A data byte corrupted on the
// wire comes back as an answered read: above 12 bits updata_angle() rejects it, but within 0..4095 it
// would be counted as a real move of up to half a turn (2048 counts, 11.8 mm). The two legs of such a
// glitch cancel in the odometer, but not for the code that acts on single steps: one step of 87 counts
// (0.5 mm) or more locks the before_pull_back sign, after which the retract it counts is taken off the
// pull back, which can then end short or not run at all. A pull back can also end early on the glitch
// itself, and DM Stage-2 counts both legs.
#include <stdint.h>

#define AS5600_HALF_TURN 2048

// Fastest believable gear speed: 150 counts per ms. One count is pi * 7.5 mm / 4096 = 5.75 um of
// filament, so this is 863 mm/s, 2200 rpm of the 7.5 mm drive wheel. The firmware never commands more
// than 60 mm/s (about 10 counts/ms), a 14x margin. The open-loop moves (PWM 850-1000: DM autoload,
// redetect, auto unload, kicks) are not speed-controlled, so the limit is kept high: even a 1000 rpm
// drive wheel (393 mm/s) keeps a 2x margin. A read corrupted into a random angle still passes only
// 301/4096 = 7.3% of the time, and then adds at most 0.86 mm instead of up to 11.8 mm.
#define AS5600_MAX_COUNTS_PER_MS 150u

// After this many rejected steps in a row the baseline itself is suspect (the read it came from may
// have been corrupted), so tracking restarts from the current read. It loses at most 3 reads of real
// motion (about 3 ms, 0.18 mm at 60 mm/s) instead of counting a fake jump, and the distance can never
// stop advancing for good.
#define AS5600_JUMP_REBASE 3u

// Largest believable move over elapsed_us since the last accepted read. It grows with the elapsed time,
// so a skipped read or a slow main-loop pass does not turn real motion into a jump. At least one 1 ms
// poll's worth; at most half a turn (from 13.65 ms on), which is all the wrap can resolve.
static inline int32_t as5600_max_step(uint32_t elapsed_us)
{
    const uint32_t full_us = ((uint32_t)AS5600_HALF_TURN * 1000u) / AS5600_MAX_COUNTS_PER_MS;
    if (elapsed_us >= full_us) return AS5600_HALF_TURN;

    uint32_t n = (elapsed_us * AS5600_MAX_COUNTS_PER_MS + 999u) / 1000u;
    if (n < AS5600_MAX_COUNTS_PER_MS) n = AS5600_MAX_COUNTS_PER_MS;
    return (int32_t)n;
}

// as5600_step_check() results.
enum
{
    AS5600_STEP_ACCEPT = 0, // count the step
    AS5600_STEP_REJECT,     // skip it: keep the baseline, its time and the speed
    AS5600_STEP_REBASE      // take this read as the new baseline, without counting the step
};

// One step of one channel: diff counts (wrapped to +-2048) since the last accepted read, elapsed_us
// ago. jumps counts the rejected steps in a row and is updated here.
static inline uint8_t as5600_step_check(uint8_t *jumps, int32_t diff, uint32_t elapsed_us)
{
    const int32_t lim = as5600_max_step(elapsed_us);
    if ((diff <= lim) && (diff >= -lim))
    {
        *jumps = 0u;
        return AS5600_STEP_ACCEPT;
    }

    if (++*jumps < AS5600_JUMP_REBASE) return AS5600_STEP_REJECT;

    *jumps = 0u;
    return AS5600_STEP_REBASE;
}

// Host tests for src/as5600_jump.h: an AS5600 step faster than any real gear move must not be counted,
// the next good read must be diffed against the last accepted one over the real elapsed time, real
// motion (also after a slow main-loop pass) must never be rejected, and a corrupted baseline must not
// stop the distance for good.

#include <stdint.h>
#include <unity.h>

#include "as5600_jump.h"

// ===== as5600_max_step =====

static void test_max_step_has_a_one_poll_floor(void)
{
    TEST_ASSERT_EQUAL_INT32(150, as5600_max_step(0u));
    TEST_ASSERT_EQUAL_INT32(150, as5600_max_step(500u));
    TEST_ASSERT_EQUAL_INT32(150, as5600_max_step(1000u));
}

static void test_max_step_grows_with_elapsed_time(void)
{
    TEST_ASSERT_EQUAL_INT32(151, as5600_max_step(1001u)); // rounded up, never below the real limit
    TEST_ASSERT_EQUAL_INT32(300, as5600_max_step(2000u));
    TEST_ASSERT_EQUAL_INT32(1500, as5600_max_step(10000u));
}

static void test_max_step_stops_at_half_a_turn(void)
{
    TEST_ASSERT_EQUAL_INT32(2048, as5600_max_step(13652u));
    TEST_ASSERT_EQUAL_INT32(2048, as5600_max_step(13653u));
    TEST_ASSERT_EQUAL_INT32(2048, as5600_max_step(1000000u));
    TEST_ASSERT_EQUAL_INT32(2048, as5600_max_step(UINT32_MAX)); // no overflow
}

// ===== as5600_step_check =====

static void test_step_within_the_limit_is_accepted_both_ways(void)
{
    uint8_t jumps = 2u;
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, as5600_step_check(&jumps, 150, 1000u));
    TEST_ASSERT_EQUAL_UINT8(0u, jumps);
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, as5600_step_check(&jumps, -150, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, as5600_step_check(&jumps, 0, 1000u));
}

static void test_third_implausible_step_in_a_row_rebases(void)
{
    uint8_t jumps = 0u;
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, as5600_step_check(&jumps, 151, 1000u));
    TEST_ASSERT_EQUAL_UINT8(1u, jumps);
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, as5600_step_check(&jumps, -151, 1000u));
    TEST_ASSERT_EQUAL_UINT8(2u, jumps);
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REBASE, as5600_step_check(&jumps, 2000, 1000u));
    TEST_ASSERT_EQUAL_UINT8(0u, jumps);
}

static void test_an_accepted_step_restarts_the_rebase_count(void)
{
    uint8_t jumps = 0u;
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, as5600_step_check(&jumps, 1000, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, as5600_step_check(&jumps, 1000, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, as5600_step_check(&jumps, 10, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, as5600_step_check(&jumps, 1000, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, as5600_step_check(&jumps, 1000, 1000u));
}

static void test_half_a_turn_after_a_long_gap_is_accepted(void)
{
    uint8_t jumps = 0u;
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, as5600_step_check(&jumps, 2048, 20000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, as5600_step_check(&jumps, -2048, 20000u));
}

// ===== one channel, the way AS5600_distance_updata uses it =====

// Accepted reads move the baseline and its time; a rejected read leaves both; a rebase moves both
// without counting. total is the odometer; abs_total is what a sum of |steps| (DM Stage-2) adds up.
typedef struct
{
    int32_t  base;
    uint32_t base_us;
    uint8_t  jumps;
    int32_t  total;
    int32_t  abs_total;
} chan_t;

static chan_t ch;

static void chan_init(int32_t raw, uint32_t t_us)
{
    ch.base = raw;
    ch.base_us = t_us;
    ch.jumps = 0u;
    ch.total = 0;
    ch.abs_total = 0;
}

static uint8_t chan_read(int32_t raw, uint32_t t_us)
{
    int32_t diff = raw - ch.base;
    if (diff > 2048) diff -= 4096;
    if (diff < -2048) diff += 4096;

    const uint8_t step = as5600_step_check(&ch.jumps, diff, t_us - ch.base_us);
    if (step == AS5600_STEP_REJECT) return step;

    ch.base = raw;
    ch.base_us = t_us;
    if (step == AS5600_STEP_ACCEPT)
    {
        ch.total += diff;
        ch.abs_total += (diff < 0) ? -diff : diff;
    }
    return step;
}

static void test_one_corrupted_read_is_not_counted(void)
{
    // Without the limit: +1990 then -1970, abs_total 3970 counts (22.8 mm of DM Stage-2).
    chan_init(1000, 0u);
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(1010, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, chan_read(3000, 2000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(1030, 3000u)); // 20 counts over 2 ms
    TEST_ASSERT_EQUAL_INT32(30, ch.total);
    TEST_ASSERT_EQUAL_INT32(30, ch.abs_total);
}

static void test_a_read_of_zero_from_a_held_sda_is_not_counted(void)
{
    chan_init(2000, 0u);
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(2010, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, chan_read(0, 2000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(2020, 3000u));
    TEST_ASSERT_EQUAL_INT32(20, ch.total);
    TEST_ASSERT_EQUAL_INT32(20, ch.abs_total);
}

static void test_a_corrupted_baseline_recovers_after_three_reads(void)
{
    chan_init(3500, 0u); // the first accepted read was bad
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, chan_read(1000, 1000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REJECT, chan_read(1010, 2000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_REBASE, chan_read(1020, 3000u));
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(1030, 4000u));
    TEST_ASSERT_EQUAL_INT32(10, ch.total);
}

static void test_fast_real_motion_is_always_counted(void)
{
    // 393 mm/s (a 1000 rpm drive wheel, 68 counts/ms), across the 4096 wrap.
    chan_init(4000, 0u);
    int32_t raw = 4000;
    for (uint32_t t = 1000u; t <= 20000u; t += 1000u)
    {
        raw = (raw + 68) % 4096;
        TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(raw, t));
    }
    TEST_ASSERT_EQUAL_INT32(20 * 68, ch.total);

    // A 10 ms main-loop stall at the same speed: 680 counts in one step.
    raw = (raw + 680) % 4096;
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(raw, 30000u));
    TEST_ASSERT_EQUAL_INT32(20 * 68 + 680, ch.total);
}

static void test_reverse_motion_is_counted_negative(void)
{
    chan_init(10, 0u);
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(4090, 1000u)); // -16 across the wrap
    TEST_ASSERT_EQUAL_UINT8(AS5600_STEP_ACCEPT, chan_read(4000, 2000u));
    TEST_ASSERT_EQUAL_INT32(-106, ch.total);
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_max_step_has_a_one_poll_floor);
    RUN_TEST(test_max_step_grows_with_elapsed_time);
    RUN_TEST(test_max_step_stops_at_half_a_turn);
    RUN_TEST(test_step_within_the_limit_is_accepted_both_ways);
    RUN_TEST(test_third_implausible_step_in_a_row_rebases);
    RUN_TEST(test_an_accepted_step_restarts_the_rebase_count);
    RUN_TEST(test_half_a_turn_after_a_long_gap_is_accepted);
    RUN_TEST(test_one_corrupted_read_is_not_counted);
    RUN_TEST(test_a_read_of_zero_from_a_held_sda_is_not_counted);
    RUN_TEST(test_a_corrupted_baseline_recovers_after_three_reads);
    RUN_TEST(test_fast_real_motion_is_always_counted);
    RUN_TEST(test_reverse_motion_is_counted_negative);
    return UNITY_END();
}

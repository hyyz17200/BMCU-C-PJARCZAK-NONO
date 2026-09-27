// Printer telemetry must retain tiny encoder steps even when float32 cannot
// represent each one. Only the final wire value may round; counts never do.
#include <math.h>
#include <stdint.h>
#include <unity.h>
#include "motion_distance.h"

void setUp(void) {}
void tearDown(void) {}

static void test_origin_direction_and_round_trip(void)
{
    TEST_ASSERT_EQUAL_FLOAT(1.0f, motion_odometer_m(0));
    TEST_ASSERT_TRUE(motion_odometer_m(-100) > 1.0f);
    TEST_ASSERT_TRUE(motion_odometer_m(100) < 1.0f);
    // The wide count crosses the old 32-bit position wrap in both directions.
    int64_t counts = 0;
    counts -= INT64_C(0x100000001);
    TEST_ASSERT_TRUE(motion_odometer_m(counts) > 24000.0f);
    counts += INT64_C(0x200000002);
    TEST_ASSERT_TRUE(motion_odometer_m(counts) < -24000.0f);
    counts -= INT64_C(0x100000001);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, motion_odometer_m(counts));
}

static void test_2048_and_5000_do_not_lose_one_count_steps(void)
{
    const double starts_m[] = {2048.0, 5000.0, -5000.0, 50000.0};
    for (unsigned i = 0; i < sizeof(starts_m) / sizeof(starts_m[0]); ++i)
    {
        const int64_t start = (int64_t)((1.0 - starts_m[i]) / ((double)MOTION_MM_PER_COUNT * 0.001));
        int64_t count = start;
        const float first = motion_odometer_m(count);
        float old = first;
        for (unsigned n = 0; n < 10000u; ++n)
        {
            --count; // one encoder step feeds forward
            old += MOTION_MM_PER_COUNT * 0.001f;
        }
        TEST_ASSERT_TRUE(old == first); // the former per-step sum sticks
        const float last = motion_odometer_m(count);
        TEST_ASSERT_TRUE(last > first);
        const float ulp = nextafterf(last, INFINITY) - last;
        TEST_ASSERT_FLOAT_WITHIN(ulp, motion_counts_to_m(10000u), last - first);
        // Reversing exactly the same count restores the original report.
        for (unsigned n = 0; n < 10000u; ++n) ++count;
        TEST_ASSERT_TRUE(motion_odometer_m(count) == first);
    }
}

static void test_telemetry_is_rounded_once_near_5000_metres(void)
{
    const int64_t centre = -(int64_t)(4999.0 / ((double)MOTION_MM_PER_COUNT * 0.001));
    for (int step = -10000; step <= 10000; ++step)
    {
        const int64_t count = centre + step;
        const long double exact = 1.0L - (long double)count * ((long double)MOTION_MM_PER_COUNT * 0.001L);
        const float reported = motion_odometer_m(count);
        const long double error = fabsl((long double)reported - exact);
        // Half of float32's 0.48828125 mm grid, plus numerical comparison noise.
        TEST_ASSERT_TRUE(error <= 0.000244140626L);
    }
}

static void test_long_feed_is_not_reduced_modulo_32_bits(void)
{
    // 5000 m and 50000 m of net feed, then the same count back: no reset/wrap.
    const double lengths_m[] = {5000.0, 50000.0};
    for (unsigned i = 0; i < sizeof(lengths_m) / sizeof(lengths_m[0]); ++i)
    {
        const int64_t feed = (int64_t)llround(lengths_m[i] / ((double)MOTION_MM_PER_COUNT * 0.001));
        const float reported = motion_odometer_m(-feed);
        const float ulp = nextafterf(reported, INFINITY) - reported;
        TEST_ASSERT_FLOAT_WITHIN(ulp * 0.5f + 0.000006f, (float)(1.0 + lengths_m[i]), reported);
        TEST_ASSERT_EQUAL_FLOAT(1.0f, motion_odometer_m(-feed + feed));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_origin_direction_and_round_trip);
    RUN_TEST(test_2048_and_5000_do_not_lose_one_count_steps);
    RUN_TEST(test_telemetry_is_rounded_once_near_5000_metres);
    RUN_TEST(test_long_feed_is_not_reduced_modulo_32_bits);
    return UNITY_END();
}

// Regression tests for the distance-only backport. Motor timeouts and stall
// exits are deliberately not part of this suite or src/motion_distance.h.
#include <math.h>
#include <stdint.h>
#include <unity.h>

#include "motion_distance.h"

void setUp(void) {}
void tearDown(void) {}

static void test_count_distance_wraps_in_both_directions(void)
{
    const uint32_t starts[] = {0u, 5u, 0x7FFFFFF0u, 0x80000000u, 0xFFFFF000u, UINT32_MAX};
    const uint32_t steps[] = {0u, 1u, 16515u, 20861u, 1738397u};
    for (unsigned i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i)
    {
        for (unsigned j = 0; j < sizeof(steps) / sizeof(steps[0]); ++j)
        {
            TEST_ASSERT_EQUAL_UINT32(steps[j], motion_count_distance(starts[i] + steps[j], starts[i]));
            TEST_ASSERT_EQUAL_UINT32(steps[j], motion_count_distance(starts[i] - steps[j], starts[i]));
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0x80000000u, motion_count_distance(0x80000000u, 0u));
}

static void test_signed_displacement_preserves_compensation_direction(void)
{
    const float step = motion_counts_to_m(100u);
    // Odometer sign is the opposite of the raw count, also at uint32_t wrap.
    TEST_ASSERT_EQUAL_FLOAT(-step, -motion_delta_m(50u, UINT32_MAX - 49u));
    TEST_ASSERT_EQUAL_FLOAT(step, -motion_delta_m(UINT32_MAX - 49u, 50u));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, motion_delta_m(123u, 123u));
    // Existing compensation locks its sign only above 0.5 mm in one pass.
    TEST_ASSERT_TRUE(motion_counts_to_m(86u) < 0.0005f);
    TEST_ASSERT_TRUE(motion_counts_to_m(87u) > 0.0005f);
}

static void test_distance_scale_and_target_boundaries(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1.0e-10f, 5.7524e-6f, motion_counts_to_m(1u));
    TEST_ASSERT_TRUE(motion_counts_to_m(16514u) < 0.095f);
    TEST_ASSERT_TRUE(motion_counts_to_m(16515u) >= 0.095f);
    TEST_ASSERT_TRUE(motion_counts_to_m(20860u) < 0.120f);
    TEST_ASSERT_TRUE(motion_counts_to_m(20861u) >= 0.120f);
}

// Independent ideal gear model, quantized to whole AS5600 counts every 1 ms.
// Both old telemetry and the new distance source receive the same steps.
typedef struct
{
    double mm;
    int32_t read_count;
    uint32_t pos;
    float meters;
} gear_t;

static void gear_step(gear_t *g, float speed_mm_s)
{
    g->mm += (double)speed_mm_s * 0.001;
    const int32_t read = (int32_t)floor(-g->mm / (double)MOTION_MM_PER_COUNT);
    const int32_t diff = read - g->read_count;
    g->read_count = read;
    g->pos += (uint32_t)diff;
    g->meters += ((float)diff * -MOTION_MM_PER_COUNT) * 0.001f;
}

static void test_solo_pull_is_independent_of_large_odometer(void)
{
    const float starts[] = {1.0f, 20.0f, 50.0f, 130.0f, 300.0f, 600.0f, 5000.0f};
    const uint32_t origin = 0xFFFFF000u;
    uint32_t reference_ms = 0u;
    for (unsigned i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i)
    {
        gear_t g = {0.0, 0, origin, starts[i]};
        uint32_t t;
        for (t = 1u; t <= 10000u; ++t)
        {
            // Same 60 -> 12 mm/s command over the last 15 mm as the firmware.
            float ramp = (0.095f - motion_travel_m(g.pos, origin)) / 0.015f;
            if (ramp > 1.0f) ramp = 1.0f;
            gear_step(&g, -(12.0f + 48.0f * ramp));
            if (motion_travel_m(g.pos, origin) >= 0.095f) break;
        }
        TEST_ASSERT_TRUE(t < 10000u);
        TEST_ASSERT_FLOAT_WITHIN(0.02f, 95.0f, (float)-g.mm);
        if (i == 0u) reference_ms = t;
        TEST_ASSERT_EQUAL_UINT32(reference_ms, t);
        if (starts[i] == 5000.0f) TEST_ASSERT_EQUAL_FLOAT(5000.0f, g.meters);
    }
}

static void test_old_odometer_can_stop_short_of_target_forever(void)
{
    gear_t g = {0.0, 0, 0u, 5000.0f};
    for (unsigned t = 0; t < 10000u; ++t) gear_step(&g, -60.0f);
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, g.meters);
    TEST_ASSERT_FLOAT_WITHIN(0.00001f, 0.600f, motion_travel_m(g.pos, 0u));
}

static void test_send_length_cap_is_ten_metres_across_wrap(void)
{
    const uint32_t last_below = 1738396u;
    const uint32_t origin = 123u;
    TEST_ASSERT_TRUE(motion_travel_m(origin - last_below, origin) < 10.0f);
    TEST_ASSERT_TRUE(motion_travel_m(origin - last_below - 1u, origin) >= 10.0f);
}

static void test_dm_120mm_countdown_survives_large_odometer(void)
{
    const uint32_t origin = 0x800u;
    gear_t g = {0.0, 0, origin, 5000.0f};
    float remain = 0.120f;
    uint32_t last = origin;
    unsigned t;
    for (t = 1u; t <= 3000u; ++t)
    {
        gear_step(&g, 60.0f);
        remain -= motion_travel_m(g.pos, last);
        last = g.pos;
        if (remain <= 0.0f) break;
    }
    TEST_ASSERT_EQUAL_UINT32(2000u, t);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, (float)g.mm);
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, g.meters);
}

static void test_dm_reverse_step_restores_remaining_length(void)
{
    const uint32_t start = 0x800u;
    const uint32_t pushed = start - 10000u;
    const uint32_t retracted = pushed + 3000u;
    float remain = 0.120f - motion_travel_m(pushed, start);
    remain += motion_travel_m(retracted, pushed);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-7f, 0.120f - motion_counts_to_m(7000u), remain);
    // A fresh stage starts a fresh count snapshot, independent of the old one.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, motion_travel_m(retracted, retracted));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_count_distance_wraps_in_both_directions);
    RUN_TEST(test_signed_displacement_preserves_compensation_direction);
    RUN_TEST(test_distance_scale_and_target_boundaries);
    RUN_TEST(test_solo_pull_is_independent_of_large_odometer);
    RUN_TEST(test_old_odometer_can_stop_short_of_target_forever);
    RUN_TEST(test_send_length_cap_is_ten_metres_across_wrap);
    RUN_TEST(test_dm_120mm_countdown_survives_large_odometer);
    RUN_TEST(test_dm_reverse_step_restores_remaining_length);
    return UNITY_END();
}

#include <cassert>
#include <cmath>
#include <initializer_list>
#include "Motion_control.h"
#include "ams.h"
#include "as5600_sample.h"
#include "motion_distance.h"

// Hardware/state inputs; the algorithms and their count snapshots are extracted
// directly from Motion_control.cpp by test_motion_distance.py.
struct AS5600_soft_IIC_many { enum { offline = -1 }; };
struct SensorStub
{
    bool online[4] = {true, true, true, true};
    int magnet_stu[4] = {};
    uint16_t raw_angle[4] = {1000, 1000, 1000, 1000};
    void updata_stu() {}
    void updata_angle() {}
} MC_AS5600;

uint32_t time_hw_tpms = 18000u, time_hw_tpus = 18u;
_ams ams[ams_max_number];
bool filament_channel_inserted[4] = {true, true, true, true};
uint8_t MC_ONLINE_key_stu[4] = {1, 1, 1, 1};
float MC_PULL_pct_f[4] = {60, 50, 50, 50};
uint64_t g_last_on_use_exit_ms[4] = {};
uint8_t g_on_use_jam_latch[4] = {}, g_on_use_low_latch[4] = {};
uint32_t g_on_use_hi_pwm_us[4] = {};
#if BMCU_DM_TWO_MICROSWITCH
uint8_t dm_fail_latch[4] = {}, dm_loaded[4] = {};
#endif

#include "motion_test_declarations.inc"

struct MotorStub
{
    filament_motion_enum motion = filament_motion_enum::filament_motion_stop;
    void set_motion(filament_motion_enum next, uint64_t, uint64_t) { motion = next; }
} MOTOR_CONTROL[4];
void MC_STU_RGB_set_latch(uint8_t, uint8_t, uint8_t, uint8_t, uint64_t, uint8_t) {}
uint8_t unload_hold_started[4] = {};
static void unload_hold_start(uint8_t ch) { unload_hold_started[ch] = 1u; }

#include "motion_test_functions.inc"

static void near(float actual, float expected)
{
    assert(std::fabs(actual - expected) < 1.0e-6f);
}

int main()
{
    auto &a = ams[motion_control_ams_num];
    const int64_t at_5000m = -(int64_t)(4999.0 / ((double)MOTION_MM_PER_COUNT * 0.001));
    // Accepted steps cross low-word wrap at positive and negative full counts.
    const int64_t initial_odometer[4] = {
        INT64_C(0x300000000) - 5, -INT64_C(0x300000000) + 5, at_5000m, -at_5000m
    };
    for (unsigned i = 0; i < 4; ++i)
    {
        as5600_odometer_count[i] = initial_odometer[i];
        a.filament[i].meters = motion_odometer_m(initial_odometer[i]);
    }
    const auto travelled = [&](uint8_t ch) -> uint32_t {
        return as5600_position(ch) - (uint32_t)initial_odometer[ch];
    };
    uint32_t ticks = 0u;
    const auto poll = [&]() { ticks += time_hw_tpms; AS5600_distance_updata(ticks); };
    poll(); // first healthy sample
    poll(); // health recovery and angle baseline
    for (unsigned i = 0; i < 4; ++i) assert(travelled(i) == 0u);

    MC_AS5600.raw_angle[0] = 1010u;
    MC_AS5600.raw_angle[1] = 990u;
    MC_AS5600.raw_angle[2] = 0u;
    MC_AS5600.online[2] = false;
    MC_AS5600.raw_angle[3] = 65535u;
    poll();
    assert(travelled(0) == 10u);
    assert(travelled(1) == 0u - 10u);
    assert(travelled(2) == 0u && travelled(3) == 0u);
    assert(as5600_position(0) == 5u);
    assert(as5600_position(1) == UINT32_MAX - 4u);
    assert(as5600_odometer_count[0] == initial_odometer[0] + 10);
    assert(as5600_odometer_count[1] == initial_odometer[1] - 10);
    assert(as5600_odometer_count[2] == initial_odometer[2]);
    assert(as5600_odometer_count[3] == initial_odometer[3]);
    for (unsigned i = 0; i < 4; ++i)
        assert(a.filament[i].meters == motion_odometer_m(as5600_odometer_count[i]));

    MC_AS5600.online[2] = true;
    MC_AS5600.raw_angle[2] = 1020u;
    MC_AS5600.raw_angle[3] = 980u;
    poll();
    assert(travelled(2) == 20u && travelled(3) == 0u - 20u);
    // Implausible angle never advances the production count, even on rebase.
    MC_AS5600.raw_angle[0] = 2500u;
    poll(); poll(); poll();
    assert(travelled(0) == 10u);
    MC_AS5600.raw_angle[0] = 2510u;
    poll();
    assert(travelled(0) == 20u);
    MC_AS5600.online[0] = false;
    MC_AS5600.raw_angle[0] = 0u;
    poll(); poll(); poll();
    assert(!AS5600_is_good(0));
    assert(travelled(0) == 20u);
    MC_AS5600.online[0] = true;
    MC_AS5600.raw_angle[0] = 3000u;
    poll(); poll(); // recovery rebases instead of adding the offline travel
    assert(travelled(0) == 20u);
    MC_AS5600.raw_angle[0] = 3010u;
    poll();
    assert(travelled(0) == 30u);
    const int32_t totals[4] = {30, -10, 20, -20};
    for (unsigned i = 0; i < 4; ++i)
    {
        assert(as5600_odometer_count[i] == initial_odometer[i] + totals[i]);
        assert(a.filament[i].meters == motion_odometer_m(as5600_odometer_count[i]));
    }

    // Preserve compensation in both encoder directions, through count wrap and
    // at a telemetry value too large to observe any of these steps.
    a.now_filament_num = 0u;
    a.filament[0].meters = 1.0e9f;
    for (int epoch : {-5, 0, 5})
    for (int sign : {-1, 1})
    {
        filament_now_position[0] = filament_idle;
        g_on_use_jam_latch[0] = 0u;
        as5600_odometer_count[0] = epoch * INT64_C(0x100000000) +
                                  (sign > 0 ? UINT32_MAX - 50u : 50u);
        a.filament[0].motion = _filament_motion::before_pull_back;
        motor_motion_switch(100u);
        as5600_odometer_count[0] += sign * 100; // >0.5 mm: lock direction
        motor_motion_switch(101u);
        as5600_odometer_count[0] += sign * 4000; // total >15 mm
        motor_motion_switch(102u);
        as5600_odometer_count[0] -= sign * 100; // reverse does not add compensation
        motor_motion_switch(103u);
        const float compensation = motion_counts_to_m(4100u);
        near(before_pb_retracted_m[0], compensation);
        a.filament[0].motion = _filament_motion::pull_back;
        motor_motion_switch(104u);
        near(filament_pull_back_target[0], 0.095f - compensation);
        near(before_pb_retracted_m[0], 0.0f);
        assert(filament_pull_back_cnt[0] == as5600_position(0));
    }

    // Existing jam target and target/switch exits remain, without a time budget.
    g_on_use_jam_latch[0] = 1u;
    motor_motion_switch(200u);
    near(filament_pull_back_target[0], 0.100f);
    g_on_use_jam_latch[0] = 0u;
    motor_motion_switch(201u);
    const int64_t origin = as5600_odometer_count[0];
    as5600_odometer_count[0] = origin + 16514;
    motor_motion_filamnet_pull_back_to_online_key(100000u);
    assert(filament_now_position[0] == filament_pulling_back);
    as5600_odometer_count[0] = origin + 16515;
    motor_motion_filamnet_pull_back_to_online_key(100001u);
    assert(filament_now_position[0] == filament_redetect);
    MC_ONLINE_key_stu[0] = 0u;
    motor_motion_filamnet_pull_back_to_online_key(200000u);
    assert(filament_now_position[0] == filament_redetect);
    assert(MOTOR_CONTROL[0].motion == filament_motion_enum::filament_motion_redetect);
    assert(!unload_hold_started[0]);
    MC_ONLINE_key_stu[0] = 1u;
    motor_motion_filamnet_pull_back_to_online_key(200001u);
    assert(filament_now_position[0] == filament_idle);
    assert(unload_hold_started[0]); // the finished unload starts the unload hold
}

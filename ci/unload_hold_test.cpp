// The unload hold, run through the whole production src/Motion_control.cpp, compiled unchanged against
// ci/motion_control_test_hardware.h, with set_motion() from bambu_bus_ams.cpp and the loaded-channel state
// from main.cpp (both extracted verbatim by ci/test_unload_hold.py). The A1 prints with channel 1, unloads
// it and loads channel 4 (or channel 1 again); a small model moves channel 1's filament.
// Usage: unload_hold_test <scenario>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "motion_control_test_hardware.h"

GPIO_TypeDef hw_gpio[4];
TIM_TypeDef  hw_tim[5];
uint16_t     hw_ccr[5][5];

uint32_t time_hw_tpus = 18u;
uint32_t time_hw_tpms = 18000u;
uint64_t hw_ticks     = 18000u;
extern "C" void delay(uint32_t ms) { hw_ticks += (uint64_t)ms * time_hw_tpms; }
extern "C" void delay_us(uint32_t us) { hw_ticks += (uint64_t)us * time_hw_tpus; }

// Order read by MC_PULL_ONLINE_read: [6] channel 1 buffer, [7] channel 1 key, then channels 2, 3, 4 downward.
static float adc[8];
bool ADC_DMA_is_inited(void) { return true; }
void ADC_DMA_gpio_analog(void) {}
void ADC_DMA_filter_reset(void) {}
void ADC_DMA_wait_full(void) {}
const float* ADC_DMA_get_value(void) { return adc; }

WS2812_class RGBOUT[4];
void RGB_update() {}

static uint8_t g_loaded_ch = 0xFF;
static uint8_t g_state_dirty = 0;
#include "loaded_state.inc"

#include "ams.cpp"
#include "Motion_control.cpp"

bool Flash_Motion_read(void* out, uint16_t bytes)
{
    Motion_control_save_struct s;
    memset(&s, 0, sizeof(s));
    for (int i = 0; i < 4; i++) { s.Motion_control_dir[i] = 1; s.dm_key_none_cv[i] = 60u; }
    s.check = 0x40614061u;
    memcpy(out, &s, bytes < sizeof(s) ? bytes : sizeof(s));
    return true;
}
bool Flash_Motion_write(const void*, uint16_t) { return true; }
bool Flash_NVM_full_clear(void) { return true; }

static int32_t angle1 = 1000; // channel 1's AS5600; the other channels do not move
AS5600_soft_IIC_many::AS5600_soft_IIC_many()
{
    online = online_buf; magnet_stu = magnet_buf; raw_angle = raw_buf; data = data_buf; numbers = kMax;
    error = error_buf; port_SDA = port_SDA_buf; port_SCL = port_SCL_buf; pin_SDA = pin_SDA_buf; pin_SCL = pin_SCL_buf;
    for (int i = 0; i < kMax; i++) { online_buf[i] = true; magnet_buf[i] = normal; raw_buf[i] = 1000; data_buf[i] = 0; error_buf[i] = 0; }
}
AS5600_soft_IIC_many::~AS5600_soft_IIC_many() {}
void AS5600_soft_IIC_many::init(GPIO_TypeDef* const*, const uint16_t*, GPIO_TypeDef* const*, const uint16_t*, int num) { numbers = num; }
void AS5600_soft_IIC_many::updata_stu() {}
void AS5600_soft_IIC_many::updata_angle() { raw_buf[0] = (uint16_t)(((angle1 % 4096) + 4096) % 4096); }

uint8_t bambubus_ams_map[4] = {0, 1, 2, 3};
#include "set_motion.inc"

// Channel 1's filament, in mm along the path, + toward the nozzle. tip: 0 = the A1 extruder gears, > 0 = held
// by them. slack: filament stored in the BMCU buffer, which reads 50 + 2.5 * slack % (+-20 mm = 0..100 %).
// The gear moves 0.1 mm/s per PWM step above 300 (60 mm/s at 900). A free tip only moves once the buffer force
// beats the PTFE friction (9 mm of buffer travel), so a pull back leaves the buffer at 27.5 %.
static const double kMmPerCount = 3.14159265358979323846 * 7.5 / 4096.0;
static const double kFriction = 9.0;
static double tip = 30.0, slack = 0.8, count_frac = 0.0, gear = 0.0;
static double extruder = 0.0;         // A1 extruder, mm/s, + = into the nozzle
static double buffer_forced = -1.0;   // buffer reading forced by the scenario, %
static float  key1 = 2.2f;            // DM key: above 1.7 V both switches, 1.4..1.7 V the first one only

static int channel1_pwm()
{
    const uint16_t a = hw_ccr[4][3], b = hw_ccr[4][4];
    return (a == 1000u && b == 1000u) ? 0 : (int)a - (int)b;
}

static void move_filament_1ms()
{
    const int pwm = channel1_pwm();
    const int mag = pwm < 0 ? -pwm : pwm;
    const double v = (mag > 300) ? 0.1 * (mag - 300) : 0.0;
    double dg = (pwm < 0 ? v : -v) * 0.001; // feeding is negative PWM (dir = +1)
    const double de = extruder * 0.001;

    if (tip > 0.0)
    {
        tip += de;
        slack += dg - de;
        if (tip < 0.0) tip = 0.0;
    }
    else
    {
        slack += dg;
        double m = 0.0;
        if (slack > kFriction)       { m = slack - kFriction; slack = kFriction; }
        else if (slack < -kFriction) { m = slack + kFriction; slack = -kFriction; }
        if (tip + m > 0.0 && extruder <= 0.0) { slack += tip + m; tip = 0.0; } // jammed at the extruder
        else                                  { tip += m; }
    }
    if (slack > 20.0) { dg -= slack - 20.0; slack = 20.0; } // buffer bottomed out: the gear slips
    if (slack < -20.0) slack = -20.0;

    gear += dg;
    count_frac -= dg / kMmPerCount;
    const int32_t c = (int32_t)count_frac;
    count_frac -= c;
    angle1 += c;

    double pct = (buffer_forced >= 0.0) ? buffer_forced : 50.0 + 2.5 * slack;
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    adc[6] = (float)((pct <= 50.0) ? 1.00 + pct / 50.0 * 0.65 : 1.65 + (pct - 50.0) / 50.0 * 0.35);
    adc[7] = key1;
}

static int failures = 0;
static void check(bool ok, const char* what)
{
    if (!ok) { fprintf(stderr, "FAILED: %s\n", what); failures++; }
}

int main(int argc, char** argv)
{
    const char* s = (argc > 1) ? argv[1] : "";
    const bool stale    = !strcmp(s, "stale_stage2");  // Stage-2 armed while channel 1 was printing
    const bool window   = !strcmp(s, "window");        // buffer reads low from the end of the unload on
    const bool raised   = !strcmp(s, "raised_buffer"); // buffer raised during the change
    const bool reload   = !strcmp(s, "reload");        // the printer loads channel 1 again
    const bool removal  = !strcmp(s, "removal");       // channel 1 taken out, a new spool inserted
    if (!stale && !window && !raised && !reload && !removal && strcmp(s, "baseline"))
    {
        fprintf(stderr, "unknown scenario\n");
        return 2;
    }

    for (int i = 0; i < 3; i++) { adc[2 * i] = 1.65f; adc[2 * i + 1] = 2.2f; } // channels 4..2 parked
    move_filament_1ms();

    // Boot as src/main.cpp does with channel 1 still loaded (the last print did not unload).
    ams_init();
    g_loaded_ch = 0;
    ams[0].now_filament_num  = 0;
    ams[0].filament_use_flag = 0x04;
    ams[0].pressure          = 0x2B00;
    ams[0].filament[0].motion = _filament_motion::on_use;
    Motion_control_init();

    uint32_t t_park = 0, t_hold_end = 0;
    bool stage2_after_park = false;
    double tip_park = 0.0, gear_park = 0.0, max_tip = -1e9, max_gear_held = -1e9;
    double gear_hold_end = 0.0, gear_2s_after_hold = 0.0, gear_raise = 0.0, min_gear_raise = 1e9, gear_spool = 0.0;
    filament_now_position_enum last = filament_now_position[0];

    for (uint32_t t = 0; t < 16000u; t++)
    {
        hw_ticks += time_hw_tpms;

        key1 = 2.2f;
        if (stale && t >= 1000u && t < 1150u) key1 = 1.55f;
        if (removal && t >= 9000u && t < 11000u) key1 = 0.10f;
        if (removal && t >= 11000u && gear - gear_spool < 30.0) key1 = 1.55f; // 2nd switch 30 mm further in
        if (removal && t == 9000u) { tip = -900.0; slack = 0.0; }
        if (removal && t == 11000u) gear_spool = gear;

        buffer_forced = -1.0;
        if (window && t_park) buffer_forced = 20.0;
        if (raised && t >= 9000u && t < 9300u) buffer_forced = 90.0;
        if (raised && t == 9000u) gear_raise = gear;

        extruder = 0.0;
        if (t >= 3000u && t < 4500u && tip > 0.0) extruder = -30.0; // cut: the extruder pushes it back up
        if (reload && t >= 11000u) extruder = 8.0;                  // the extruder takes channel 1 again

        if ((t % 50u) == 0u)
        {
            if (t < 3000u)       set_motion(0, 0x07, 0x7F, 0);
            else if (t < 4500u)  set_motion(0, 0x09, 0x3F, 0);
            else if (t < 8000u)  set_motion(0xFF, 0x03, 0x00, 0);
            else if (!reload)
            {
                if (t < 10000u)      set_motion(3, 0x03, 0x00, 0);
                else if (t < 11000u) set_motion(3, 0x09, 0xA5, 0);
                else                 set_motion(3, 0x07, 0x7F, 0); // printing with channel 4
            }
            else
            {
                if (t < 11000u)      set_motion(0, 0x03, 0x00, 0);
                else if (t < 12000u) set_motion(0, 0x09, 0xA5, 0);
                else                 set_motion(0, 0x07, 0x7F, 0);
            }
        }

        Motion_control_run(0);
        move_filament_1ms();

#if BMCU_DM_TWO_MICROSWITCH
        if (stale && t == 2999u) check(dm_loaded[0] == 0u, "the key dip armed Stage-2 before the unload");
        if (t_park && dm_auto_state[0] == DM_AUTO_S2_PUSH && !removal) stage2_after_park = true;
#endif
        const filament_now_position_enum pos = filament_now_position[0];
        if (!t_park && last == filament_redetect && pos == filament_idle)
        {
            t_park = t; tip_park = tip; gear_park = gear;
            check(g_unload_hold[0] == 1u, "the finished unload starts the hold");
        }
        last = pos;
        if (t_park && !t_hold_end && !g_unload_hold[0]) { t_hold_end = t; gear_hold_end = gear; }
        if (t_park && tip > max_tip) max_tip = tip;
        if (t_park && !t_hold_end && gear > max_gear_held) max_gear_held = gear;
        if (t_hold_end && t == t_hold_end + 2000u) gear_2s_after_hold = gear;
        if (raised && t >= 9000u && t < 9300u && gear < min_gear_raise) min_gear_raise = gear;
    }

    check(t_park != 0u, "channel 1 was unloaded");
    check(t_hold_end != 0u, "the hold ended");
    check(max_gear_held - gear_park < 0.5, "no feeding while the hold lasts");
    check(!stage2_after_park, "no Stage-2 push after the unload");

    if (!reload && !removal)
        check(t_hold_end >= 11000u, "the hold lasts until channel 4 prints");
    if (!window && !reload && !removal)
        check(max_tip - tip_park < 2.0, "channel 1 stays behind the splitter while channel 4 prints");
    if (window)
        check(gear_2s_after_hold - gear_hold_end > 5.0, "after the change the V10.5 idle control feeds again");
    if (raised)
        check(gear_raise - min_gear_raise > 5.0, "a raised buffer is still pulled back during the hold");
    if (reload)
    {
        check(t_hold_end >= 8000u && t_hold_end < 8100u, "loading channel 1 again ends the hold");
        check(tip > 0.0, "channel 1 reaches the extruder again");
    }
    if (removal)
    {
        check(t_hold_end >= 10000u && t_hold_end < 11000u, "taking the filament out ends the hold");
#if BMCU_DM_TWO_MICROSWITCH
        check(dm_loaded[0] == 1u && gear - gear_spool >= 140.0, "a new spool still autoloads");
#endif
    }

    printf("%s: unload end %u ms, hold end %u ms, channel 1 max forward %.1f mm%s\n", s, t_park, t_hold_end,
           max_tip - tip_park, failures ? " FAILED" : "");
    return failures ? 1 : 0;
}

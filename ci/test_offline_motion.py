"""Compile the production unload/drive dispatch with motor and sensor stubs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OfflineMotionTests(unittest.TestCase):
    def test_offline_cancels_every_manual_drive(self):
        source = (ROOT / "src/Motion_control.cpp").read_text(encoding="utf-8")
        constants = source[source.index("static constexpr float    AUTO_UNLOAD_START_PCT"):
                           source.index("bool filament_channel_inserted")]
        start = source.index("    if (!error)\n", source.index("static void motor_motion_run"))
        end = source.index("        uint8_t r = 0u", start)
        # Actual current production branches, including every PWM dispatch; omit only LED rendering.
        body = source[start:end] + "    }\n}\n"
        cpp = r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
constexpr uint8_t kChCount=4;
enum class filament_motion_enum { filament_motion_stop, filament_motion_pressure_ctrl_idle };
bool offline, good[4], filament_channel_inserted[4];
uint8_t MC_ONLINE_key_stu[4]; float MC_PULL_pct_f[4]; int pwm[4];
void Motion_control_set_PWM(uint8_t ch, int value) { assert(!offline || value==0); pwm[ch]=value; }
struct PID { unsigned resets=0; void clear() { ++resets; } };
struct _MOTOR_CONTROL {
    filament_motion_enum motion=filament_motion_enum::filament_motion_pressure_ctrl_idle;
    PID PID_speed, PID_pressure; float dir=1; uint8_t pwm_zeroed=0;
    static float x_prev[4];
    void set_motion(filament_motion_enum m, int, uint64_t) { motion=m; }
    void run(float, uint64_t) { assert(!offline || motion==filament_motion_enum::filament_motion_stop); }
} MOTOR_CONTROL[4];
float _MOTOR_CONTROL::x_prev[4];
bool AS5600_is_good(uint8_t i) { return good[i]; }
bool motor_motion_filamnet_pull_back_to_online_key(uint64_t) { return false; }
void motor_motion_switch(uint64_t) {
    for (auto &m : MOTOR_CONTROL) m.motion=filament_motion_enum::filament_motion_pressure_ctrl_idle;
}
void MC_STU_RGB_set_latch(uint8_t,uint8_t,uint8_t,uint8_t,uint64_t,uint8_t) {}
''' + constants + r'''
void step(int error, uint64_t time_now, bool have_time_step=true) {
    offline=error!=0;
    float time_E=have_time_step ? 0.001f : 0.0f;
''' + body + r'''
int main() {
    for (bool sensor : {false,true}) for (bool dt : {false,true})
    for (uint8_t key : {0,1,2,3}) for (float level : {30.0f,50.0f,90.0f}) {
        for (unsigned i=0;i<4;i++) {
            good[i]=sensor; filament_channel_inserted[i]=true;
            MC_ONLINE_key_stu[i]=key; MC_PULL_pct_f[i]=level;
            auto_unload_arm[i]=auto_unload_active[i]=auto_unload_blocked[i]=1;
            auto_unload_arm_t0_ms[i]=auto_unload_active_t0_ms[i]=auto_unload_empty_t0_ms[i]=100;
            pwm[i]=850; _MOTOR_CONTROL::x_prev[i]=850;
        }
        step(-1,200,dt);
        for (unsigned i=0;i<4;i++) {
            assert(pwm[i]==0 && _MOTOR_CONTROL::x_prev[i]==0 && MOTOR_CONTROL[i].pwm_zeroed);
            assert(!auto_unload_arm[i] && !auto_unload_active[i] && !auto_unload_blocked[i]);
            assert(!auto_unload_arm_t0_ms[i] && !auto_unload_active_t0_ms[i] && !auto_unload_empty_t0_ms[i]);
        }
    }
    // Reconnect at neutral: a previous active/armed unload has been cancelled.
    for (unsigned i=0;i<4;i++) { good[i]=true; MC_PULL_pct_f[i]=50; MC_ONLINE_key_stu[i]=1; }
    step(0,300);
    for (unsigned i=0;i<4;i++) assert(!auto_unload_active[i] && pwm[i]==0);
    // A new online lift-and-release still starts the original 850 PWM unload.
    for (auto &v : MC_PULL_pct_f) v=90;
    step(0,400);
    for (auto &v : MC_PULL_pct_f) v=50;
    step(0,500);
    for (unsigned i=0;i<4;i++) assert(auto_unload_active[i] && pwm[i]==850);
    step(-1,501,false);
    // Online empty-slot pull is unchanged; offline never permits its 700 PWM.
    for (unsigned i=0;i<4;i++) { MC_ONLINE_key_stu[i]=0; MC_PULL_pct_f[i]=90; }
    step(0,600);
    for (int v : pwm) assert(v==700);
    step(-1,601,false);
    for (int v : pwm) assert(v==0);
}
'''
        compiler = os.environ.get("CXX") or shutil.which("g++")
        self.assertIsNotNone(compiler, "Set CXX to a host g++ compiler")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            file, exe = work / "test.cpp", work / "test.exe"
            file.write_text(cpp, encoding="utf-8")
            result = subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                                     str(file), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

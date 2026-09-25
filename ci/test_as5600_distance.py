"""Exercise production AS5600 sampling with injected sensor readings."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AS5600DistanceTests(unittest.TestCase):
    def test_invalid_samples_recovery_and_wrap(self):
        source = (ROOT / "src/Motion_control.cpp").read_text(encoding="utf-8")
        constants = "\n".join(line for line in source.splitlines() if line.startswith((
            "static constexpr uint8_t  kChCount",
            "static constexpr float    kAS5600_PI",
            "static constexpr float kAS5600_MM_PER_CNT",
        )))
        health = source[source.index("static uint8_t g_as5600_good"):
                        source.index("// ---- liniowe")]
        start = source.index("int32_t as5600_distance_save")
        sampling = source[start:source.index("// ===== stany logiki filamentu", start)]
        cpp = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
''' + constants + health + r'''
struct AS5600_soft_IIC_many {
    enum { offline=-1, normal=0 };
    bool online[4]; int magnet_stu[4]; uint16_t raw_angle[4];
    void updata_angle() {} // Inject the driver's post-read values directly.
    void updata_stu() {}
} MC_AS5600;
float speed_as5600[4];
struct Filament { float meters=0; };
struct AMS { Filament filament[4]; } ams[1];
constexpr unsigned motion_control_ams_num=0;
uint32_t time_hw_tpms=18000, time_hw_tpus=18;
''' + sampling + r'''
void near(float actual, float expected) {
    assert(std::fabs(actual-expected) < 0.000001f*(1.0f+std::fabs(expected)));
}
void step() {
    static uint32_t tick=0;
    tick+=time_hw_tpms;
    AS5600_distance_updata(tick);
}
void expect_delta(float before, int counts) {
    near(ams[0].filament[0].meters, before+counts*kAS5600_MM_PER_CNT*0.001f);
    near(speed_as5600[0], counts*kAS5600_MM_PER_CNT*1000.0f);
}
int main() {
    for (unsigned ch=0; ch<4; ++ch) {
        MC_AS5600.online[ch]=true;
        MC_AS5600.magnet_stu[ch]=AS5600_soft_IIC_many::normal;
        MC_AS5600.raw_angle[ch]=2048;
        g_as5600_good[ch]=1;
        g_as5600_okstreak[ch]=kAS5600_OK_RECOVER;
    }
    step(); step(); // Establish time and angle baselines.
    MC_AS5600.raw_angle[0]+=16;
    step(); expect_delta(0,16);

    // One or two failed reads must not integrate the driver's zero angle.
    for (unsigned failures=1; failures<=2; ++failures) {
        const float before=ams[0].filament[0].meters;
        MC_AS5600.online[0]=false;
        MC_AS5600.raw_angle[0]=0;
        for (unsigned n=0; n<failures; ++n) {
            const float other=ams[0].filament[1].meters;
            MC_AS5600.raw_angle[1]+=8;
            step(); expect_delta(before,0);
            assert(AS5600_is_good(0)); // Preserve the three-failure health debounce.
            near(ams[0].filament[1].meters,other+8*kAS5600_MM_PER_CNT*0.001f);
        }
        MC_AS5600.online[0]=true;
        MC_AS5600.raw_angle[0]=3000;
        step(); expect_delta(before,0); // Recovery establishes a fresh baseline.
        MC_AS5600.raw_angle[0]+=16;
        step(); expect_delta(before,16);
    }

    // A longer outage keeps the existing two-good-read recovery requirement.
    float before=ams[0].filament[0].meters;
    MC_AS5600.online[0]=false;
    MC_AS5600.raw_angle[0]=0;
    for (unsigned n=1; n<=3; ++n) {
        step(); expect_delta(before,0);
        assert(AS5600_is_good(0)==(n<3));
    }
    MC_AS5600.online[0]=true;
    MC_AS5600.raw_angle[0]=1000;
    step(); expect_delta(before,0); assert(!AS5600_is_good(0));
    MC_AS5600.raw_angle[0]=2000;
    step(); expect_delta(before,0); assert(AS5600_is_good(0));
    MC_AS5600.raw_angle[0]+=16;
    step(); expect_delta(before,16);

    // Missing magnet status invalidates even a successfully read angle.
    before=ams[0].filament[0].meters;
    MC_AS5600.magnet_stu[0]=AS5600_soft_IIC_many::offline;
    MC_AS5600.raw_angle[0]=0;
    step(); expect_delta(before,0); assert(AS5600_is_good(0));
    MC_AS5600.magnet_stu[0]=AS5600_soft_IIC_many::normal;
    MC_AS5600.raw_angle[0]=4090;
    step(); expect_delta(before,0);

    // Valid motion still handles both directions across the 12-bit angle wrap.
    MC_AS5600.raw_angle[0]=6;
    step(); expect_delta(before,12);
    before=ams[0].filament[0].meters;
    MC_AS5600.raw_angle[0]=4090;
    step(); expect_delta(before,-12);
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

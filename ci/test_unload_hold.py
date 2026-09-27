"""Run the unload hold through the whole production Motion_control.cpp with host hardware stubs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = ("baseline", "stale_stage2", "window", "raised_buffer", "reload", "removal", "pullback_dropout")


def between(source, start, end):
    if source.count(start) != 1 or source.count(end) != 1:
        raise ValueError(f"Production test boundary changed: {start!r}, {end!r}")
    return source[source.index(start):source.index(end, source.index(start))]


class UnloadHoldTests(unittest.TestCase):
    def test_unload_hold_on_production_motion_control(self):
        compiler = os.environ.get("CXX") or shutil.which("g++")
        self.assertIsNotNone(compiler, "Set CXX to a host g++ compiler")
        bus = (ROOT / "src/bambu_bus_ams.cpp").read_text(encoding="utf-8")
        main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            # Production sources and headers unchanged; only the SDK, SysTick and LED headers are the host model.
            for path in (ROOT / "src").glob("*.h"):
                shutil.copyfile(path, work / path.name)
            for name in ("Motion_control.cpp", "ams.cpp"):
                shutil.copyfile(ROOT / "src" / name, work / name)
            for name in ("ch32v20x.h", "ch32v20x_gpio.h", "ws2812.h", "hal/time_hw.h"):
                path = work / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "motion_control_test_hardware.h"\n', encoding="utf-8")
            # The printer command mapping and the loaded-channel state, extracted verbatim on each run.
            (work / "set_motion.inc").write_text(between(
                bus, "static uint32_t time_sendout_onuse_ticks[4]", "// 3D C5 0C C8 03 00 07 00 7F 02 36 54"),
                encoding="utf-8")
            (work / "loaded_state.inc").write_text(between(
                main, "void ams_state_set_loaded(uint8_t filament_ch)", "static void ams_state_save_run()"),
                encoding="utf-8")

            for dm in (1, 0):
                exe = work / f"unload_hold_dm{dm}.exe"
                # MC_PULL_ONLINE_read only uses its tick argument on DM builds.
                command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                           f"-DBMCU_DM_TWO_MICROSWITCH={dm}", "-DBMCU_ONLINE_LED_FILAMENT_RGB=0",
                           "-DBMCU_P1S=0", "-DBMCU_SOFT_LOAD=0", "-DBAMBU_BUS_AMS_NUM=0",
                           "-DAMS_RETRACT_LEN=0.095f", "-I" + str(work), "-Ici",
                           "ci/unload_hold_test.cpp", "-o", str(exe)]
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                # Stage-2 exists only on dual-switch (DM) builds.
                for scenario in SCENARIOS if dm else [s for s in SCENARIOS if s != "stale_stage2"]:
                    with self.subTest(dm=dm, scenario=scenario):
                        result = subprocess.run([str(exe), scenario], capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

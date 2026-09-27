"""Run the production AS5600 update and pull-back code with host hardware stubs."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def between(source, start, end):
    if source.count(start) != 1 or source.count(end) != 1:
        raise ValueError(f"Production test boundary changed: {start!r}, {end!r}")
    return source[source.index(start):source.index(end, source.index(start))]


class MotionDistanceTests(unittest.TestCase):
    def test_production_count_update_and_compensated_pull(self):
        source = (ROOT / "src/Motion_control.cpp").read_text(encoding="utf-8")
        constants = []
        for name in ("kChCount", "kAS5600_PI", "kAS5600_MM_PER_CNT", "PULL_V_FAST",
                     "PULL_V_END", "PULL_RAMP_M", "g_pull_remain_m", "g_pull_speed_set"):
            matches = re.findall(r"^static [^\n=]*\b" + name + r"\b[^\n]*;[^\n]*$", source, re.M)
            self.assertEqual(len(matches), 1, name)
            constants.append(matches[0])
        declarations = "\n".join(constants) + "\n" + between(
            source, "float speed_as5600[4]", "// ---- liniowe zwalnianie") + between(
            source, "enum class filament_motion_enum", "class _MOTOR_CONTROL")
        functions = between(source, "static inline float absf", "static inline uint8_t dm_key_v_to_centi_ceil")
        functions += between(source, "static as5600_track_t g_as5600_track", "static inline void stu_apply_baseline")
        compiler = os.environ.get("CXX") or shutil.which("g++")
        self.assertIsNotNone(compiler, "Set CXX to a host g++ compiler")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            # Extract verbatim on each run: no copied production algorithm can go stale.
            (work / "motion_test_declarations.inc").write_text(declarations, encoding="utf-8")
            (work / "motion_test_functions.inc").write_text(functions, encoding="utf-8")
            exe = work / "test.exe"
            for dm in (0, 1):
                command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                           f"-DBMCU_DM_TWO_MICROSWITCH={dm}", "-DAMS_RETRACT_LEN=0.095f",
                           "-I" + str(work), "-Isrc", "ci/motion_distance_integration_test.cpp",
                           "-o", str(exe)]
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(exe)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

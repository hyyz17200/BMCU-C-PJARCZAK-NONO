from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PrinterBusTests(unittest.TestCase):
    def compile_and_run(self, sources, hardware=False):
        compiler = os.environ.get("CXX") or shutil.which("g++")
        if not compiler:
            self.fail("Set CXX to a host g++ compiler to run the printer transport tests")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            command = [compiler, "-std=c++11", "-x", "c++"]
            if hardware:
                # Compile the production driver unchanged against a register model.
                for name in ("_bus_hardware.cpp", "_bus_hardware.h"):
                    shutil.copyfile(ROOT / "src" / name, work / name)
                headers = [
                    "ch32v20x.h", "ch32v20x_rcc.h", "ch32v20x_gpio.h",
                    "ch32v20x_usart.h", "ch32v20x_dma.h", "ch32v20x_misc.h",
                    "core_riscv.h", "hal/irq_wch.h", "hal/time_hw.h",
                ]
                for name in headers:
                    path = work / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text('#include "printer_bus_test_hardware.h"\n')
                # Host pointers are wider than the MCU DMA address registers.
                command += ["-fpermissive", "-D__attribute__(x)=", "-I" + str(work)]
            executable = work / "test.exe"
            command += ["-Ici", "-Isrc", *sources, "-o", str(executable)]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_transport_register_model(self):
        self.compile_and_run([
            "ci/printer_bus_transport_test.cpp", "src/crc_bus.c",
        ], hardware=True)

    def test_preserved_registration_policy(self):
        self.compile_and_run(["ci/ams_online_detect_policy_test.cpp"])


if __name__ == "__main__":
    unittest.main()

from contextlib import redirect_stderr
import hashlib
import importlib.util
import io
import itertools
import os
from pathlib import Path
import runpy
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("firmware_output", ROOT / "scripts/firmware_output.py")
output = importlib.util.module_from_spec(spec)
spec.loader.exec_module(output)


class BuildToolsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.root_patch = patch.object(output, "ROOT", self.root)
        self.root_patch.start()

    def tearDown(self):
        self.root_patch.stop()
        self.temp.cleanup()

    def old_output(self):
        target = self.root / "dist/firmwares"
        target.mkdir(parents=True)
        (target / "old.bin").write_bytes(b"old firmware")
        return target

    def test_failed_build_leaves_old_output(self):
        target = self.old_output()
        stage = output.begin()
        (stage / "partial.bin").write_bytes(b"incomplete")
        output.cleanup(stage)
        self.assertEqual((target / "old.bin").read_bytes(), b"old firmware")

    def test_publish_and_manifest(self):
        target = self.old_output()
        stage = output.begin()
        (stage / "new.bin").write_bytes(b"new firmware")
        output.publish(stage)
        self.assertFalse((target / "old.bin").exists())
        lines = (target / "manifest.txt").read_text().splitlines()
        self.assertEqual(len(lines), 2)
        sha, crc, size, rel = lines[1].split(" ", 3)
        data = (target / rel).read_bytes()
        self.assertEqual((sha, crc, int(size)),
                         (hashlib.sha256(data).hexdigest(), f"{zlib.crc32(data):08X}", len(data)))
        self.assertEqual(rel, "new.bin")
        self.assertEqual(list((self.root / "dist").iterdir()), [target])

    def test_failed_swap_restores_previous_output(self):
        target = self.old_output()
        stage = output.begin()
        (stage / "new.bin").write_bytes(b"new")
        rename = Path.rename

        def fail_promotion(path, destination):
            if path == stage:
                raise OSError("injected publish failure")
            return rename(path, destination)

        with patch.object(Path, "rename", fail_promotion):
            with self.assertRaises(OSError):
                output.publish(stage)
        self.assertEqual((target / "old.bin").read_bytes(), b"old firmware")
        self.assertEqual((stage / "new.bin").read_bytes(), b"new")
        output.cleanup(stage)

    def test_rejects_unowned_paths_and_concurrent_publish(self):
        target = self.old_output()
        for path in [self.root, target, self.root / "other",
                     self.root.parent / (".firmwares-" + "1" * 32)]:
            with self.assertRaises(ValueError):
                output.cleanup(path)
        stage = output.begin()
        (self.root / "dist/.firmwares-publish.lock").touch()
        with self.assertRaises(FileExistsError):
            output.publish(stage)
        self.assertEqual((target / "old.bin").read_bytes(), b"old firmware")
        self.assertTrue(stage.is_dir())

    def test_rejects_linked_stage(self):
        stage = self.root / "dist" / (".firmwares-" + "1" * 32)
        stage.parent.mkdir()
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "keep").write_bytes(b"keep")
        try:
            stage.symlink_to(outside, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Host cannot create directory symlinks: {error}")
        with self.assertRaises(ValueError):
            output.cleanup(stage)
        self.assertEqual((outside / "keep").read_bytes(), b"keep")

    def test_build_script_preserves_matrix_and_survives_failure(self):
        bash = os.environ.get("GIT_BASH")
        if not bash:
            bash = str(Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/bin/bash.exe") if os.name == "nt" else shutil.which("bash")
        self.assertTrue(bash and Path(bash).is_file(), "Install bash or set GIT_BASH")
        shutil.copy2(ROOT / "build_all_firmwares.sh", self.root)
        (self.root / "scripts").mkdir()
        shutil.copy2(ROOT / "scripts/firmware_output.py", self.root / "scripts")
        for guide in ROOT.glob("which_to_choose_*.txt"):
            shutil.copy2(guide, self.root)
        commands = self.root / "commands"
        commands.mkdir()
        python = commands / "python3"
        python.write_text("#!/usr/bin/env bash\nexec " + shlex.quote(Path(sys.executable).as_posix()) + ' "$@"\n')
        pio = commands / "pio"
        pio.write_text('''#!/usr/bin/env bash
set -eu
[[ "${FAIL_BUILD:-0}" == "1" ]] && exit 42
printf '%s %s %s %s %s %s\\n' "$BMCU_DM_TWO_MICROSWITCH" "$BMCU_ONLINE_LED_FILAMENT_RGB" "$DBMCU_P1S" "$BMCU_SOFT_LOAD" "$BAMBU_BUS_AMS_NUM" "$AMS_RETRACT_LEN" > "$PLATFORMIO_BUILD_DIR/fw/firmware.bin"
''')
        python.chmod(0o755)
        pio.chmod(0o755)
        (self.root / "build-cache/fw").mkdir(parents=True)
        env = dict(os.environ, PLATFORMIO_BUILD_DIR="build-cache", BMCU_SOFT_LOAD="1", FAIL_BUILD="1")
        command = [bash, "-c", 'export PATH="$PWD/commands:$PATH"; bash build_all_firmwares.sh']
        target = self.old_output()
        result = subprocess.run(command, cwd=self.root, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((target / "old.bin").read_bytes(), b"old firmware")
        self.assertEqual(list((self.root / "dist").iterdir()), [target])
        env["FAIL_BUILD"] = "0"
        result = subprocess.run(command, cwd=self.root, env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        bins = list(target.rglob("*.bin"))
        self.assertEqual(len(bins), 520)
        self.assertEqual(len({p.read_text() for p in bins}), 520)
        for file in bins:
            dm, rgb, p1s, soft, ams, retract = file.read_text().split()
            rel = file.relative_to(target).as_posix()
            self.assertEqual(soft, "0")
            self.assertEqual(dm, "0" if "/NO_AUTOLOAD/" in rel else "1")
            self.assertEqual(rgb, "0" if "/FILAMENT_RGB_OFF/" in rel else "1")
            self.assertEqual(p1s, "1" if rel.startswith("high_force_load(P1S)/") else "0")
            self.assertEqual(ams, "0" if "/SOLO/" in rel else str("ABCD".index(rel.split("/AMS_")[1][0])))
            self.assertEqual(retract, file.stem.rsplit("_", 1)[1])
        for line in (target / "manifest.txt").read_text().splitlines()[1:]:
            sha, crc, size, rel = line.split(" ", 3)
            self.assertNotEqual(rel, "manifest.txt")
            data = (target / rel).read_bytes()
            self.assertEqual((sha, crc, int(size)),
                             (hashlib.sha256(data).hexdigest(), f"{zlib.crc32(data):08X}", len(data)))


class EnvironmentTests(unittest.TestCase):
    def test_windows_toolchain_guard(self):
        class Env:
            clean = False
            metadata = False
            package = "test-toolchain"
            def IsCleanTarget(self): return self.clean
            def IsIntegrationDump(self): return self.metadata
            def PioPlatform(self): return self
            def get_package_dir(self, name): return self.package
            def Exit(self, code): raise SystemExit(code)

        env = Env()
        script = str(ROOT / "scripts/check_build_packages.py")
        def check():
            return runpy.run_path(script, init_globals={"env": env, "Import": lambda *_: None})

        with patch.object(sys, "platform", "win32"), patch.object(subprocess, "run") as run, redirect_stderr(io.StringIO()):
            run.return_value = subprocess.CompletedProcess([], 0, "d2836398c87fdc9832fd04026588c26da199b902\n", "")
            check()
            run.return_value = subprocess.CompletedProcess([], 0, "wrong revision\n", "")
            with self.assertRaises(SystemExit): check()
            run.side_effect = FileNotFoundError("git missing")
            with self.assertRaises(SystemExit): check()
            env.package = None
            with self.assertRaises(SystemExit): check()
            run.reset_mock()
            env.clean = True
            check()
            env.clean, env.metadata = False, True
            check()
            env.metadata = False
            with patch.object(sys, "platform", "linux"):
                check()
            run.assert_not_called()

    def test_clean_and_metadata_do_not_require_variant(self):
        class Returned(Exception): pass
        class Env:
            def IsCleanTarget(self): return clean
            def IsIntegrationDump(self): return metadata
            def Exit(self, code): raise AssertionError("validation should have been skipped")
        def returned(): raise Returned()
        for clean, metadata in [(True, False), (False, True)]:
            with patch.dict(os.environ, {}, clear=True), self.assertRaises(Returned):
                runpy.run_path(str(ROOT / "scripts/check_fw_env.py"), init_globals={
                    "env": Env(), "Import": lambda *_: None, "Return": returned})

    def test_variant_validation(self):
        class Env:
            def IsCleanTarget(self): return False
            def IsIntegrationDump(self): return False
            def Exit(self, code): raise SystemExit(code)

        keys = ("BMCU_DM_TWO_MICROSWITCH", "BMCU_ONLINE_LED_FILAMENT_RGB", "DBMCU_P1S",
                "BMCU_SOFT_LOAD", "BAMBU_BUS_AMS_NUM", "AMS_RETRACT_LEN")

        def validate(values):
            with patch.dict(os.environ, values, clear=True), redirect_stderr(io.StringIO()):
                runpy.run_path(str(ROOT / "scripts/check_fw_env.py"),
                               init_globals={"env": Env(), "Import": lambda *_: None})

        for dm, rgb, mode, ams, retract in itertools.product("01", "01", [("0", "0"), ("0", "1"), ("1", "0")], "0123", ["0.095f", "0.10f", "0.90f"]):
            validate(dict(zip(keys, [dm, rgb, *mode, ams, retract])))
        base = dict(zip(keys, ["1", "1", "0", "0", "0", "0.095f"]))
        for bad in [{}, dict(base, BMCU_SOFT_LOAD="ON"), dict(base, DBMCU_P1S="1", BMCU_SOFT_LOAD="1"),
                    dict(base, AMS_RETRACT_LEN="95"), dict(base, AMS_RETRACT_LEN="nan"), dict(base, BAMBU_BUS_AMS_NUM="4")]:
            with self.assertRaises(SystemExit):
                validate(bad)


if __name__ == "__main__":
    unittest.main()

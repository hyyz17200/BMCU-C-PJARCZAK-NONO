# Local hardening checks

Run `python ci/test_build_tools.py` with Python 3 and Bash (Git Bash on Windows).
It injects build and publication failures, checks path protection and dependency
validation, and exercises all 1020 packaging configurations (three load modes,
DM, filament RGB, AMS_AUTO and AMS_A-D, 17 retract lengths) with a simulated
compiler. This packaging check does not compile or validate 1020 firmware binaries.

The platform and SDK Git revisions are pinned in `platformio.ini`. Windows builds
also reject a compiler revision other than the verified package:

```sh
pio pkg install --global --tool 'toolchain-riscv @ https://github.com/Community-PIO-CH32V/toolchain-riscv-windows.git#d2836398c87fdc9832fd04026588c26da199b902'
```

With these pins, the unmodified V11 source (`5b8e235`) builds byte-identical to
the upstream release binary `standard(A1)/AUTOLOAD/FILAMENT_RGB_OFF/AMS_AUTO/ams_auto_0.095f.bin`.
Linux/macOS retain the platform's OS-specific compiler selection; those compiler
versions have not been pinned or verified here.

`env:fw` takes every variant option from environment variables. `scripts/check_fw_env.py`
rejects an unset or invalid value before compiling: the 0/1 switches, `BAMBU_BUS_AMS_NUM`
0..3 or 4 (AUTO), a retract length in metres, and P1S together with soft load.

`build_all_firmwares.sh` requires all four `which_to_choose_*.txt` guides, `pio`,
and `python3`. It builds the upstream V11 matrix, stages it under `dist`, then
publishes to `dist/firmwares` only after all builds succeed. `manifest.txt` records
SHA-256, CRC32 and byte size for each output file. Failed builds leave previous
output intact; failed promotion attempts restore it. Publication is serialized by
an exclusive lock. A process or power interruption can leave a stage, backup or
lock under `dist`; inspect these before manual recovery. Run one matrix build at a
time: the publication lock does not isolate PlatformIO's compiler workspace.
Firmware compilation and host tests do not establish printer or motor acceptance.

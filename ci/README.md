# Local hardening checks

Run `python ci/test_printer_bus.py` with a host C++ compiler on PATH. This exercises
the production bus transport using hardware stubs. Run `pio test -e native`
for the WS2812 frame-cache, AS5600 sample/direction and count-based distance tests.
Run `python ci/test_motion_distance.py` to exercise the production AS5600 count
update and compensated pull-back functions with host hardware stubs in DM and
non-DM configurations. It extracts these functions directly from the current
source, checks rejected samples and count wrap, and preserves the existing
compensation, jam target and redetect exits. It adds no motor timeouts or stall
limits. The numerical tests cover long-uptime float odometer loss, the 95 mm
pull, 10 m send cap and 120 mm DM Stage-2 distance. The filament-odometer tests
cover one-count steps near 2048/5000 m, signed travel beyond 32-bit wrap, and
float32 conversion/scaling error below 1 mm near 5000 m in either direction.
One signed 64-bit count per channel feeds both paths: motion snapshots retain
only its low 32 bits, while telemetry is rebuilt directly with float32 arithmetic
on accepted nonzero moves. No double conversion or second accumulator is used;
the packet format and 1 m boot origin stay unchanged.

Run `python ci/test_unload_hold.py` to compile the whole production
`src/Motion_control.cpp` unchanged against `ci/motion_control_test_hardware.h`,
with `set_motion()` from `src/bambu_bus_ams.cpp` and the loaded-channel state
from `src/main.cpp` extracted verbatim, in DM and non-DM configurations. A small
filament model runs an A1 filament change: channel 1 is unloaded, channel 4 is
loaded and prints. From the end of the unload until another channel reaches
on_use, the printer loads channel 1 again, or the filament is taken out, nothing
may feed channel 1, including a DM Stage-2 armed while it printed; a raised
buffer is still pulled back, and after the change the V10.5 idle control applies
again. The model does not establish printer or motor acceptance.

Run `python ci/test_build_tools.py` with Python 3 and Bash (Git Bash on Windows).
It injects build and publication failures, checks path protection and dependency
validation, and exercises all 520 packaging configurations with a simulated compiler.
This packaging check does not compile or validate 520 firmware binaries.

The platform and SDK Git revisions are pinned in `platformio.ini`. Windows builds
also reject a compiler revision other than the verified package:

```sh
pio pkg install --global --tool 'toolchain-riscv @ https://github.com/Community-PIO-CH32V/toolchain-riscv-windows.git#d2836398c87fdc9832fd04026588c26da199b902'
```

Linux/macOS retain the platform's OS-specific compiler selection; those compiler
versions have not been pinned or verified here. The `native` test target is not
flashable.

`build_all_firmwares.sh` requires all four `which_to_choose_*.txt` guides, `pio`,
and `python3`. It preserves the existing standard/P1S, DM, RGB, slot and retract
matrix and explicitly disables soft load. It stages under `dist`, then publishes
to `dist/firmwares` only after all builds succeed. `manifest.txt` records SHA-256,
CRC32 and byte size for each output file. Failed builds leave previous output
intact; failed promotion attempts restore it. Publication is serialized by an
exclusive lock. A process or power interruption can leave a stage, backup or lock
under `dist`; inspect these before manual recovery. Run one matrix build at a time:
the publication lock does not isolate PlatformIO's compiler workspace. Firmware compilation and host
tests do not establish printer or motor acceptance.

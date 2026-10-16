"""Reject Windows toolchain drift before compiling; retain other hosts' native toolchains.

Platform/SDK revisions are pinned in platformio.ini. The platform selects an OS-specific
toolchain before extra_scripts run, so validate the installed Windows revision here rather
than force a Windows executable package on Linux/macOS. Those toolchains are not locked.
"""
import subprocess
import sys

Import("env")

WINDOWS_TOOLCHAIN = "d2836398c87fdc9832fd04026588c26da199b902"

if not (env.IsCleanTarget() or env.IsIntegrationDump()) and sys.platform == "win32":
    package = env.PioPlatform().get_package_dir("toolchain-riscv")
    revision = None
    if package:
        try:
            result = subprocess.run(["git", "-C", package, "rev-parse", "HEAD"],
                                    capture_output=True, text=True, check=False)
            if result.returncode == 0:
                revision = result.stdout.strip()
        except OSError:
            pass
    if revision != WINDOWS_TOOLCHAIN:
        sys.stderr.write(
            "Windows toolchain revision differs from the verified build. Install the pinned package:\n"
            "pio pkg install --global --tool 'toolchain-riscv @ "
            "https://github.com/Community-PIO-CH32V/toolchain-riscv-windows.git#"
            + WINDOWS_TOOLCHAIN + "'\n")
        env.Exit(1)

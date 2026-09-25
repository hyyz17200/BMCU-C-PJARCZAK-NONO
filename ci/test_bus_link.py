"""Exercise the production heartbeat handoff and host selection without MCU hardware."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class BusLinkTests(unittest.TestCase):
    def test_production_heartbeat_and_host_policy(self):
        bus = (ROOT / "src/bambu_bus_ams.cpp").read_text(encoding="utf-8")
        main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        # Compile current production fragments, not a maintained copy of them.
        declarations = bus[bus.index("static bus_link_t"):bus.index("void bambubus_heartbeat_seen_fast")]
        tail = bus[bus.index("    bool hb_new = false;"):bus.index("\n    return stu;", bus.index("    bool hb_new = false;"))]
        source = r'''
#include <cassert>
#include <cstdint>
#include "bus_link.h"
#include "ams_online_detect_policy.h"
#include "ahub_bus.h"
#include "bambu_bus_ams.h"
#include "_bus_hardware.h"
uint16_t bus_host_device_type = host_device_type_none;
uint32_t tick = 0;
uint32_t time_ticks32() { return tick; }
uint32_t ms_to_ticks32(uint32_t ms) { return ms * 18000u; }
uint32_t irq_save_wch() { return 0; }
void irq_restore_wch(uint32_t) {}
ams_online_detect::State registration = {};
unsigned resets = 0;
void online_detect_reset() { ++resets; ams_online_detect::reset(registration); }
''' + declarations + function(bus, "void bambubus_heartbeat_seen_fast") + "\n" + function(main, "static bool host_link_offline") + r'''
bambubus_package_type poll(uint32_t now, bambubus_package_type stu = bambubus_package_type::none) {
    static bool hb_unreported = false;
''' + tail + r'''
    return stu;
}
int main() {
    using B = bambubus_package_type;
    using H = ahubus_package_type;
    assert(host_link_offline(H::none, B::none));
    assert(poll(0) == B::error);
    assert(poll(0x80000001u) == B::error);
    assert(poll(0xffffffffu) == B::error);
    assert(resets == 0);

    // ISR heartbeat newer than the loop's sampled now, including across wrap.
    tick = 5; bambubus_heartbeat_seen_fast();
    assert(poll(0xfffffff0u, B::version) == B::version);
    assert(poll(6) == B::heartbeat); // packet must not swallow protocol discovery
    bus_host_device_type = host_device_type_ams;
    assert(!host_link_offline(H::error, B::none));
    assert(host_link_offline(H::none, B::error));
    assert(poll(tick + ms_to_ticks32(1000)) == B::none);
    assert(poll(tick + ms_to_ticks32(1000) + 1) == B::error);
    assert(resets == 1);

    // Registration confirmed during an outage must not be reset on every pass.
    ams_online_detect::latch_confirm(registration, tick);
    for (uint32_t t : {0x70000000u, 0x80000000u, 0xfffffff0u, 0u, 6u}) {
        assert(poll(t) == B::error);
        assert(registration.registered && resets == 1);
    }
    tick = 100; bambubus_heartbeat_seen_fast();
    assert(poll(tick) == B::heartbeat);
    assert(poll(tick + ms_to_ticks32(1000) + 1) == B::error);
    assert(resets == 2 && !registration.registered);

    // Reproduce the old 150 s uptime / 140 s last-heartbeat failure.
    tick = 140u * 18000000u; bambubus_heartbeat_seen_fast();
    assert(poll(tick) == B::heartbeat);
    assert(poll(150u * 18000000u) == B::error);
    assert(host_link_offline(H::none, B::error));

    // AHUB uses the same latch but the other protocol cannot mask its loss.
    bus_link_t ahub = {};
    assert(bus_link_poll(&ahub, 0xffffffffu, 100) == BUS_LINK_NOT_SEEN);
    bus_host_device_type = host_device_type_ahub;
    assert(!host_link_offline(H::none, B::error));
    assert(host_link_offline(H::error, B::none));
    bus_link_heartbeat(&ahub, 0xfffffff0u);
    assert(bus_link_poll(&ahub, 5, 100) == BUS_LINK_ALIVE);
    assert(bus_link_poll(&ahub, 1000, 100) == BUS_LINK_LOST);
    assert(bus_link_poll(&ahub, 0xfffffff0u, 100) == BUS_LINK_LOST);
}
'''
        source = "#include <initializer_list>\n" + source
        compiler = os.environ.get("CXX") or shutil.which("g++")
        self.assertIsNotNone(compiler, "Set CXX to a host g++ compiler")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            cpp, exe = work / "test.cpp", work / "test.exe"
            cpp.write_text(source, encoding="utf-8")
            result = subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                                     "-Isrc", str(cpp), "-o", str(exe)], cwd=ROOT,
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

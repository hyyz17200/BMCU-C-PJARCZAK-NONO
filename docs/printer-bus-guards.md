# Printer bus fault recovery with RXNE reception

Base: `856bbd5` (V10.5 plus the existing AMS version, RGB throttle and registration
recovery patches). This replaces the withdrawn full RX DMA backport.

Production changes are limited to `_bus_hardware.cpp`, `_bus_hardware.h` and one
TX watchdog call in `main.cpp`:

- Detect TX DMA transfer errors and a 25 ms elapsed TX timeout in the main loop.
- Stop TX DMA, clear its flags and USART TC, release DE and restore the idle state
  on faults. Normal TC completion uses the same cleanup without waiting for timeout.
- Mask interrupts across TX startup and watchdog checks to avoid racing TC.
- Enable USART ORE/NE/FE/PE error handling. Read status then data once, discard the
  corrupted byte and reset only the in-progress RX parser, including heartbeat skip
  state. Preserve any complete frame already published to a protocol handler.

Reception remains RXNE interrupt driven with the original two receive buffers.
There is no RX DMA, separate framer, RX polling, deferred response, pre-TX receive
flush, bus-quiet persistence gating, new registration heuristic or LED scheduler.
The blocking 50 us Bambu response delay, persistence policy, Motion Control,
AMS08 version `01.00.06.83` and the two existing registration fixes are unchanged.
No Link, Pico or management functionality is included.

## Validation

Set `CXX` to host g++ (with its runtime DLL directory in PATH on Windows), then run
`python ci/test_printer_bus.py -v`. The tests compile the actual driver against a
register model and run the existing registration policy vectors. Driver cases cover
RXNE delivery, all four RX errors, errors without RXNE, damaged heartbeats, retention
of published frames, TX DMA faults, timeout boundary and tick rollover, simultaneous
RX/TX faults, stale TX flags, normal TC and subsequent successful RX/TX.

The A1/AUTOLOAD/RGB_OFF/slot-0/0.095f build uses 13,368 bytes of static RAM and
50,380 bytes of Flash: +8 RAM and +184 Flash relative to `856bbd5` with the same
configuration. Artifacts and hashes are recorded separately in ignored `dist`.

The timeout is polled by the main loop, not an independent hardware watchdog.
Offline tests and firmware builds do not establish hardware timing or resolution
of the printer communication warning; physical validation remains pending.

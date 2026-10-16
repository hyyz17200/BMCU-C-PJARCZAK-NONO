# BMCU-C-PJARCZAK-NONO

**A personal fork of Paweł Jarczak's BMCU 370C firmware.**

This repository, [hyyz17200/BMCU-C-PJARCZAK-NONO](https://github.com/hyyz17200/BMCU-C-PJARCZAK-NONO), is a personal copy kept by [CJ (`hyyz17200`)](https://github.com/hyyz17200).

> [!IMPORTANT]
> **Original repository:** [github.com/jarczakpawel/BMCU-C-PJARCZAK](https://github.com/jarczakpawel/BMCU-C-PJARCZAK)
>
> Author: [Paweł Jarczak](https://github.com/jarczakpawel) (`jarczakpawel`)
>
> Go the original repo page to find more about guide, and cosider to sponser his work.
>

## Changes

This fork is based on upstream **V11** (`5b8e235`).

- Prebuilt firmwares are removed. Build output goes to `dist/firmwares`.
- Variant builds reject an invalid mode, AMS number, or retract length before compiling. The platform, SDK, and Windows toolchain revisions are pinned. With these pins, the unmodified V11 source rebuilds the upstream release binary byte for byte. A matrix build is staged, and it replaces `dist/firmwares` only after every build has succeeded.
- An AS5600 step faster than any real gear move (more than 150 counts per ms, 863 mm/s) is skipped.
  - V11 already drops failed reads and reads above 12 bits. A read corrupted inside 0–4095 was still counted as a move of up to 11.8 mm.
  - One such step is enough to lock the before-pull-back retract count. The pull back that follows can then end short or not run at all, and the tip stays in the splitter.
  - After three skipped steps in a row, tracking restarts from the current read.

The earlier V10.5-based fork is preserved on branch `bak-before-v11`. V11 provides its own fixes for failed AS5600 reads, count-based motion distances and filament odometer telemetry, pull-back empty confirmation, TX fault recovery, and debug-UART C linkage. This fork uses those upstream implementations.

To keep this fork close to upstream, these local protections and optimizations were deliberately not ported:

- Persistent unload hold: V11's DM autoload gate prevents stale Stage-2 reactivation, but does not block idle PID forward feeding on a parked channel.
- WS2812 requested/shown frame caching and the 10 ms refresh limit: this fork uses V11's existing LED cache and bus-idle refresh gate.
- Per-byte USART RX error handling: V11's DMA transport replaces the old RXNE driver, but does not carry over its explicit ORE/FE/PE recovery.

## Build

Build one variant, for example my personal workhorse: standard (A1), AUTOLOAD, filament RGB off, AMS_AUTO, 9.5 cm:

```sh
BAMBU_BUS_AMS_NUM=4 AMS_RETRACT_LEN=0.095f BMCU_DM_TWO_MICROSWITCH=1 \
BMCU_ONLINE_LED_FILAMENT_RGB=0 DBMCU_P1S=0 BMCU_SOFT_LOAD=0 pio run -e fw
```

`BAMBU_BUS_AMS_NUM` is 0–3 for AMS_A–AMS_D, or 4 for AMS_AUTO. Set `BMCU_SOFT_LOAD=1` for the soft_load(A1) mode, or `DBMCU_P1S=1` for high_force_load(P1S). Do not set both. `./build_all_firmwares.sh` builds the whole matrix. Host checks are described in [`ci/README.md`](./ci/README.md).

## Disclaimer

You are using this firmware and performing any modifications at your own risk.
Make sure you understand what you are doing.
I am not responsible for any damage, failed prints, hardware issues, or data loss.
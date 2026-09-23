# BMCU-C-PJARCZAK-NONO

**A personal fork of Paweł Jarczak's BMCU 370C firmware.**

This repository, [hyyz17200/BMCU-C-PJARCZAK-NONO](https://github.com/hyyz17200/BMCU-C-PJARCZAK-NONO), is a personal copy kept by [CJ (`hyyz17200`)](https://github.com/hyyz17200). The firmware, guides, and changelog here come from the original project. Releases, bug reports, and discussion belong on that project.

> [!IMPORTANT]
> **Original repository:** [github.com/jarczakpawel/BMCU-C-PJARCZAK](https://github.com/jarczakpawel/BMCU-C-PJARCZAK)
>
> Author: [Paweł Jarczak](https://github.com/jarczakpawel) (`jarczakpawel`)
>
> Project: **BMCU-C-PJARCZAK** — BMCU 370C (Hall) firmware for Bambu Lab A1 / A1 mini / P1S, with buffer calibration.
>
> Related tools published by the original author:
>
> BMCU Flasher: https://github.com/jarczakpawel/BMCU-Flasher
> Bambu Studio build that hides the AMS compatibility HMS warning: https://github.com/jarczakpawel/BambuStudio-BMCU


# New Feature


**The notes below are the original project's usage guide, kept so this copy can still be used. They describe Paweł Jarczak's firmware. The Ko-fi and Revolut links fund his work.**

---

# Upstream guide

This BMCU firmware has been tested and verified by the original author with the latest Bambu Lab A1 firmware.

> [!WARNING]
> Bambu Lab is limiting local BMCU interoperability through firmware updates.
>
> About how printer updates remove functions that were available at purchase:
> [BMCU vs firmware locks](./bmcu-vs-firmware-locks.md)

The printer must be configured as **AMS**, not AMS Lite. AMS Lite is incompatible with this firmware.

## Support the original author

Bambu Lab continues tightening compatibility around BMCU, and there is a growing risk that BMCU may eventually become unusable in that ecosystem. The original author is raising funds to buy a test printer so he can build BMCU support for open-source Klipper-based printers.

The $500 goal does not need to be reached in full. If he saves the remaining amount himself, he will cover the rest.

<p align="center">
  <a href="https://ko-fi.com/jarczakpawel/goal?g=0">
    <img src="./banner-klipper.png" alt="Support Paweł Jarczak's work toward BMCU on Klipper." width="460">
  </a>
</p>

<p align="center">
  <a href="https://ko-fi.com/jarczakpawel/goal?g=0"><strong>Support Paweł Jarczak on Ko-fi</strong></a>
  ·
  <a href="https://revolut.me/paweqxdkx"><strong>Support Paweł Jarczak via Revolut</strong></a>
</p>

Direct Revolut support avoids Ko-fi fees, so more of the contribution goes to the original project.

These links belong to Paweł Jarczak. They are not a fundraiser for this personal fork.

# First start (V10.3+)

At the first startup after flashing, **all channels must be empty**.

From **V10.3**, the firmware calibrates empty-channel detection during first boot.

If it was flashed with filament inserted:

- remove all filament
- hold any one buffer for about **5 seconds** to re-calibrate

# 2nd generation printers

Drawings are often unclear about whether they show the plug or the socket. Signal A and Signal B are frequently wired the wrong way around.

If the BMCU is not detected by a 2nd generation printer, try swapping Signal A and Signal B, and confirm the wiring before powering the printer.

# HMS warning

This firmware version **triggers an HMS warning immediately after printer startup**.

- This HMS warning does not block BMCU operation
- It does not require restarting the printer
- It does not affect printing
- The printer works normally with the warning present
- The warning is visual only (the HMS icon)

The original author treats this warning as known, accepted behavior.

He also published a Bambu Studio build that hides this specific AMS compatibility warning. Other HMS warnings stay visible.

https://github.com/jarczakpawel/BambuStudio-BMCU

---

## Supported printers

The original author confirmed correct operation on both 1st generation and 2nd generation printers.

### 1st generation printers

Support is confirmed for 1st generation printers.

### 2nd generation printers

Correct operation has been confirmed on:

- Bambu Lab P2S
- Bambu Lab H2D

The original project expects the same behavior on the other printers from both generations.

---

## Download

Ready-to-use firmware is published in the **Releases** of the original repository, together with `.txt` guides for choosing a build:

https://github.com/jarczakpawel/BMCU-C-PJARCZAK/releases

Start with the printer mode folder (`standard(A1)`, `soft_load(A1)`, or `high_force_load(P1S)`), then choose AUTOLOAD, filament RGB, and the slot (SOLO or AMS_A / AMS_B / AMS_C / AMS_D).

Prebuilt files are also in the [`firmwares/`](./firmwares) tree of this repository. The original author's release page is the place to check for the current set.

## Flashing

To flash any version of the BMCU (USB or TTL) on Windows, Linux, macOS, or Android, use **BMCU Flasher**:

https://github.com/jarczakpawel/BMCU-Flasher

Precompiled binaries are in that repository's **Releases**. Flashing does not require wchisptool.

Two ways to flash:

- **Online flashing** from the built-in wizard. The flasher downloads the firmware, so the `.bin` files do not need to be fetched by hand.
- **Local flashing** from a firmware file downloaded from the original releases or from [`firmwares/`](./firmwares).

The flasher also runs on Android.

- Do not flash the BMCU while it is connected to the printer.
- Do not connect or disconnect the BMCU while the printer is powered on. That can damage the BMCU, the printer mainboard, or both.

---

## SOLO firmware

Example file: `solo_0.095f.bin`

This build is for a single BMCU.

- Recommended for single-BMCU setups
- Filament retraction length: 9.5 cm

---

## Filament retraction

Measure retraction from the end of the AMS splitter inside the printer (the plastic AMS part where four PTFE tubes enter).

Example:

- Distance from the BMCU to the end of the AMS splitter: about 9.0 cm
- SOLO firmware retracts the filament about 0.5 cm past the splitter
- Total retraction length: 9.5 cm

When choosing another length, measure from the end of the AMS splitter and add the extra distance your tube path needs, on the order of 9 cm for a short path.

---

## AMS_A / AMS_B / AMS_C / AMS_D firmware

These builds are for:

- Multi-BMCU setups
- Longer filament retraction distances

For SOLO operation with a longer retraction, use AMS_A instead of SOLO.

---

## Calibration (first start)

Calibration is required. Without it, the BMCU will not detect filament correctly.

Video from the original project:

https://www.youtube.com/watch?v=Hn_DNzSmhuc

## Re-calibration

1. Remove all filaments from all channels
2. Hold any one buffer in position for about 5 seconds

---

## Safety

- Do not flash the BMCU while it is connected to the printer
- Do not disconnect the BMCU while the printer is powered on
- Do not update printer firmware while the BMCU is connected
- Connect and disconnect the BMCU only when the printer is fully powered off and unplugged

These recommendations come from community reports collected by the original project. Not every failure case has been tested.

Switching the printer from AMS Lite to AMS while the BMCU was connected did not cause trouble in the original author's testing. The original guide still says to do that change with the BMCU disconnected.

---

## Disclaimer

Using this firmware and modifying the hardware is at your own risk. The original author is not responsible for damage, failed prints, hardware faults, or data loss. This personal fork does not add a warranty on top of that.

---

## Before opening a bug report

File firmware bugs on the original repository:

https://github.com/jarczakpawel/BMCU-C-PJARCZAK/issues

Check the basics first:

- The flashed file is the variant the guides describe, and the flashing steps were followed.
- The board really is a **BMCU 370C with Hall sensors**.
  - The reliable check is to open the module and look at the PCB.
  - Some sellers mix in older **370x** boards. One or two modules in a set can be 370x.
- For printer-side trouble, confirm the latest printer firmware and try a factory reset. That often clears odd AMS behavior.
- If filament detection looks wrong, boot the printer once without the BMCU, then connect the BMCU and test again.
- Run a few real tests before opening an issue. Some printers fail a normal firmware update and need an SD-card update instead.

---

# Upstream changelog

Versions below are releases of [BMCU-C-PJARCZAK](https://github.com/jarczakpawel/BMCU-C-PJARCZAK) by Paweł Jarczak. The `version` file in this tree reads `10.50.00.00` (V10.5).

## V10.5

### User-visible changes

- Automatic filament unload when the buffer is lifted manually.
- The serial is generated from the MCU hardware UID, so devices no longer share the same SN.
- Calibration performs a full NVM cleanup.
- Fixed the rare case where the system LED could blink incorrectly on some BMCU units.
- Fixed the external fan issue on Bambu Lab P2S.

## V10.4

### User-visible changes

- Support for **Bambu Lab P2S**, verified in real tests.
- The H2 series was expected to work as well, because 1st generation AMS support is confirmed there. H2D was later confirmed.

### Fixes

- The "filament in use" flag is cleared when filament runs out during printing.
- Retraction when the buffer is pulled up manually, including when the channel has no filament.

## V10.3

### User-visible changes

- New mode: **soft_load(A1)**.
  - Aimed at A1 / A1 Mini.
  - Lower filament loading force than `standard(A1)`.
  - Useful on BMCU units with weaker lever springs, where a harder push clicks or grinds during load.
- Empty-channel calibration stores the "no filament" point separately for each channel.
- Calibration detects and saves Hall polarity per channel, so magnet orientation in the buffer is handled automatically.

### Stability

- PWM timer preload is configured on all motor channels, and PWM updates are buffered before the timer update.
- AS5600 polling is rate-limited to about 1 ms.
- The motion loop reuses shared tick snapshots.
- High-load / jam timing during on_use accumulates PWM time in microseconds.
- The motion loop uses a wrap-safe tick delta and clamps oversized time steps.

`soft_load(A1)` is a lower-force option. If the printer rejects filament because the push is too weak, `standard(A1)` with a stronger lever spring is the alternative the original notes recommend. On some A1 / A1 Mini units, `soft_load(A1)` is the build those users keep.

## V10.2

### User-visible changes

- Filament run-out no longer trips jam protection. Previously the motor could keep running after the filament ended, enter jam protection, and block automatic refill.
- Jam protection separates a real jam from a temporary motor stop. High motor load alone does not count as a jam.
- Less unnecessary flash rewriting.
- Faster ADC/DMA updates with lower CPU overhead.

### Technical changes

- Filament metadata uses an append-only journal: 40 bytes (10 words) per record, CRC32, 6 records per flash page, erase only when the page is full.
- Loaded-channel persistence uses a lightweight slot log.
- Unchanged data is skipped.
- ADC DMA publish path simplified.
- CRC tables are static compile-time tables.
- Timing paths use wrap-safe 32-bit timers.

## V10

- Spool jam handling pauses the print immediately so the snag can be fixed and the print resumed.
- Loaded-channel state is an append-only slot log: 8 bytes per update, up to 192 updates before a page erase.
- Filament metadata is a CRC-protected log: 64 bytes per update, 2 pages per filament (8 records).
- Only the modified channel is written.
- Records are validated, and a partial write is ignored.

## V9

- Higher filament loading force.
- Better loading on materials such as Sunlu PLA+.
- Spool-jam protection:
  - Lock mode if the buffer drops too low during printing.
  - Lock mode if the motor runs at high speed for about 8 seconds.
  - The lock releases when the buffer returns to neutral.

## V8

- Print resume after a printer power loss.
- Better P1S behavior on a long or bent PTFE path.
- AUTOLOAD on single-switch boards, triggered by a buffer tap.
- More stable loading, including low-torque BMCU variants.

## V7

- Loaded filaments are remembered across power-off, so unload-at-end in G-code can be turned off. See https://wiki.bambulab.com/en/ams/manual/ams-not-unloading-to-save-filament
- Filament loading stabilized across the hardware variants the author had tested.
- Filament RGB colors on the module LEDs.

### AUTOLOAD

- **Two microswitches (DM):** the first switch starts AUTOLOAD. The BMCU feeds until the second switch, behind the extruder, confirms the filament is fully in, then feeds 120 mm. If the buffer shows a snag on the housing or PTFE edge, it retracts and retries up to 3 times.
- **Single-switch boards:** the first stage is manual. After the filament is fully in the extruder, stage 2 matches the DM path, including anti-snag retries.

### Technical changes

- ADC1 and ADC2 scan in parallel. Filtered updates dropped from about 28 ms to about 5 ms.
- AS5600 reads made more robust.
- Time comparisons reviewed for timer wrap.

## V6

### Framework

- Arduino Core removed. The firmware runs on the WCH CH32 SDK (noneos).
- Hardware timers, DMA, and interrupts are used directly.
- Flash writes use the WCH fast API.

### ADC, bus, and storage

- DMA writes are separated from CPU reads. The filter runs in the DMA half/full callbacks.
- BambuBus and AHUB parse a snapshot of the RX buffer, with a fixed CPU cost per frame.
- A bad packet does not reset the whole bus state.
- Flash is programmed page by page (256 bytes) instead of erasing a 4 KB sector, and only when the data changed.
- Hardware CRC checks flash on read.
- AMS data is split into separate records.

### Motion and the rest

- Soft-I2C / AS5600 rewritten with explicit ACK/NACK, START/STOP, and per-channel error isolation.
- Smoother motor control and a neutral calibration position for the buffer.
- CRC8 / CRC16 implemented as C lookup tables.

## Original author's note

Paweł Jarczak started this firmware as a personal CH32 learning project. It grew well past that scope. He describes many of the solutions as intentionally overengineered, written first for his own use, and used heavily during development.

Credit for the firmware, the changelog above, the flasher, and the support links goes to:

**[Paweł Jarczak / BMCU-C-PJARCZAK](https://github.com/jarczakpawel/BMCU-C-PJARCZAK)**

This repository remains a personal fork:

**[CJ / BMCU-C-PJARCZAK-NONO](https://github.com/hyyz17200/BMCU-C-PJARCZAK-NONO)**

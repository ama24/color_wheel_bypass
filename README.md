# DLP Projector Spoofer — ESP32-S3 Super Mini

Part of a direct laser imaging (DLI) photolithography conversion project. This firmware replaces the light source control PCB and I2C temperature sensor of a commercial DLP projector, allowing the original UV lamp and red laser to be swapped for a TTL-controlled 405 nm UV laser while keeping the main board and DMD in normal operation.

## Project overview

The projector's main board continuously monitors two subsystems:

- **SK01 (13-pin connector)** — the light source control PCB, which communicates over UART at 19200 baud and drives analogue signals (LLITZ, LDUP) confirming the lamp is operating.
- **SK05 (20-pin connector)** — a thermal management board that reports fan and temperature data over I2C. One sensor (address 0x4D) reports four temperature channels; if the laser heatsink channel ever returns 0xFF, the main board initiates thermal shutdown.

If either subsystem is absent or reports a fault, the projector shuts down within seconds. This firmware runs on an ESP32-S3 Super Mini and emulates both subsystems simultaneously.

## What is spoofed

| Signal / bus | Connector | Method |
|---|---|---|
| PRSV (presence detect) | SK01 pin 13 | 10 kΩ pull-up to V5P0LD — hardware only |
| SENS (colour-wheel index, ~120 Hz) | SK02 pin 5 | `esp_timer` one-shot state machine |
| LDUP (light-output PWM) | SK01 pin 10 | `esp_timer` multi-phase state machine |
| LLITZ (intensity reference) | SK01 pin 9 | GPIO driven HIGH after UART init burst |
| PHSENSE (photo sense, ~2.85 V) | SK05 pin 1 | GPIO HIGH via 10 kΩ / 68 kΩ resistor divider |
| RX0LD / TX0LD (UART 19200 baud 8N1) | SK01 pins 6/7 | Full packet replay on UART1 (GPIO15/16) |
| I2C 0x4D temperature sensor | SK05 pins 6/7 | I2C slave on GPIO11/12 |

### Not spoofed (no action needed)

- SK05 TX/RX UART (pins 2/3) — stayed idle (logic HIGH) throughout all captures
- SK05 FG1/FG2 tachometers (pins 18/19) — main board does not read these; keep LOW
- SK05 FPCN1–5 fan enables (pins 4/5/13/14/15) — keep the thermal board connected; fans protect the DMD
- SK05 I2C EEPROM 0x54 — keep original IC; contains boot configuration read at startup
- SK05 I2C peripheral 0x41 — keep original IC

## Hardware modifications required

1. **Remove the 0x4D temperature sensor IC** from the thermal management board.
2. **Solder wires** from ESP32 GPIO11 (SDA) and GPIO12 (SCL) to the SK05 I2C bus pads where the 0x4D IC was. Do not use GPIO35/36 — these are hardwired to the onboard PSRAM on the ESP32-S3FH4R2 and are not available as user pins.
3. **Install a 10 kΩ pull-up resistor** from V5P0LD (SK01 pin 11, 5 V) to PRSV (SK01 pin 13).
4. **Install a resistor divider** for PHSENSE: 10 kΩ from 3.3 V to GPIO10, 68 kΩ from GPIO10 to GND — gives ~2.87 V at SK05 pin 1.
5. **Power the ESP32** from V5P0LD (5 V) via a 3.3 V LDO regulator (e.g. AMS1117-3.3).

## GPIO pin assignments

| GPIO | Signal | Connector | Direction | Notes |
|------|--------|-----------|-----------|-------|
| 5 | SENS | SK02 pin 5 | Output | Colour-wheel index pulse ~120 Hz — starts on LDPCN rise |
| 6 | LDPCN | SK01 pin 4 | Input | Rising-edge ISR from main board (~3 s after power-on) |
| 7 | LLITZ | SK01 pin 9 | Output (TBD) | Intensity reference — **probe direction before wiring** |
| 8 | LDUP | SK01 pin 10 | Output (TBD) | Light-output PWM — **probe direction before wiring** |
| 10 | PHSENSE | SK05 pin 1 | Output | Drive HIGH → 2.87 V via 10 kΩ / 68 kΩ divider |
| 11 | I2C SDA | SK05 pin 7 | Bidirectional | I2C slave (0x4D temperature sensor) |
| 12 | I2C SCL | SK05 pin 6 | Input | I2C clock from main board |
| 15 | RX0LD (UART1 TX) | SK01 pin 6 | Output | Spoofer transmits to main board |
| 16 | TX0LD (UART1 RX) | SK01 pin 7 | Input | Spoofer receives from main board |
| 43 | USB debug TX | — | Output | UART0 serial monitor (via USB-C) |
| 44 | USB debug RX | — | Input | UART0 serial monitor (via USB-C) |

Free GPIOs (available for future use): 1, 2, 4, 9, 13, 14, 17, 18, 21

**Do not use:** GPIO19/20 (USB D±), GPIO26–32 (flash/PSRAM), GPIO35/36 (onboard PSRAM on ESP32-S3FH4R2)

## UART protocol summary

Both channels run at **19200 baud, 8N1**. Packets are delimited by 5 ms of inter-byte silence.

**Boot sequence:**

| Time | Direction | Action |
|------|-----------|--------|
| +3.0 s | — | LDPCN goes HIGH — spoofer wakes |
| +3.4 s | → main board | Announce: `03 40 08 F0` |
| +4.3 s | ← main board | 6 init queries (device ID, config, calibration) |
| +4.3 s | → main board | 6 init responses played back in order |
| +4.8 s | — | LLITZ, LDUP, PHSENSE asserted |
| +5.2 s | ← main board | Periodic 490 ms polls begin |
| +5.2 s | → main board | Telemetry sequence (32 unique packets, then locked steady-state) |
| +22.0 s | ← main board | `02 00` graceful shutdown |
| +22.0 s | → main board | `01` shutdown ACK; all signals de-asserted |

**Critical rule:** Never send any packet containing the byte sequence `0x88 0x0F`. This is the lamp-fault telemetry pattern — the main board will initiate shutdown within ~10 seconds of receiving it.

## I2C temperature sensor (0x4D)

The main board polls four temperature channels every ~510 ms by writing a register address and reading back 1 byte:

| Register | Channel | Safe value | Notes |
|----------|---------|------------|-------|
| 0x3E / 0x3F | Ch1 (main board ambient) | 0x2E / 0x80 | 46.5 °C |
| 0x4E / 0x4F | Ch2 (secondary area) | 0x2F / 0x80 | 47.5 °C |
| 0x5E / 0x5F | Ch3 (laser heatsink) | 0x4A / 0x80 | 74.5 °C — **NEVER return 0xFF** |
| 0x6E / 0x6F | Ch4 (UV source area) | 0x2E / 0x80 | 46.5 °C |

If 0x5E returns 0xFF, the main board initiates thermal shutdown after ~10 seconds.

## Source file layout

```
src/
├── config.h              — all pin numbers, baud rates, timing constants, I2C values
├── packets.h             — 45 RX0LD byte arrays + RX_TABLE[] / RX_LENS[] lookup tables
├── main.cpp              — LDPCN ISR, RTOS primitives, task spawning
├── signals.h / .cpp      — sens_start/stop(), ldup_start/stop() via esp_timer
├── uart_primary.h / .cpp — Task A: full UART state machine on Core 0
└── i2c_slave.h / .cpp    — I2C slave at 0x4D for temperature sensor emulation
```

## Building

Open in VS Code with the PlatformIO extension. Select the **esp32s3** environment and click Build / Upload.

> **Windows note:** Build from the VS Code PlatformIO extension, not from the command line. The Windows shell interprets paths starting with `/Users` as invalid command switches and the build will fail.

```
PlatformIO sidebar → esp32s3 → Build
PlatformIO sidebar → esp32s3 → Upload
PlatformIO sidebar → Monitor (115200 baud)
```

`CORE_DEBUG_LEVEL=3` is set in `platformio.ini` for bringup. Change to `0` for production.

## Verification checklist

1. **Build** — PlatformIO esp32s3 → Build, confirm no errors
2. **SENS** — scope on GPIO5: 120 Hz, 91% duty cycle, starts only after LDPCN HIGH
3. **LDUP** — scope on GPIO8: 1180/1108 µs start pulse then repeating 236/3959/236/2710/236/1107 µs loop
4. **LDPCN trigger** — drive GPIO6 HIGH externally; serial monitor should show announce sent, init sequence, LLITZ asserted
5. **UART** — logic analyser on GPIO15/16 at 19200 baud: verify RX0LD sequence matches nominal; confirm no `88 0F` bytes
6. **LLITZ** — multimeter/scope on GPIO7: goes HIGH after init burst, stays HIGH
7. **Shutdown** — inject `02 00` on GPIO16: verify GPIO7 falls, LDUP stops, `01` sent
8. **I2C** — I2C analyser on SK05 pins 6/7: address 0x4D responds; register 0x5E returns 0x4A
9. **PHSENSE** — multimeter at SK05 pin 1: ~2.85 V when GPIO10 is HIGH
10. **Full system** — connect to projector with UV laser; verify stable operation for 20+ seconds with no fault shutdown

## Things to verify before final wiring

- **LLITZ direction (SK01 pin 9):** Probe with a high-impedance scope during boot. If the main board drives it HIGH, change GPIO7 to input-only (monitor only). If the control PCB drives it, the current output configuration is correct.
- **LDUP direction and waveform (SK01 pin 10):** Same direction check. Then capture the actual waveform with an oscilloscope and update `LDUP_P0_H` through `LDUP_P7_L` in `src/config.h` if the placeholder values differ.
- **SENS frequency:** Measure the actual colour-wheel index pulse frequency during normal projector operation before removing the wheel. Update `SENS_HIGH_US` / `SENS_LOW_US` in `src/config.h` if needed.

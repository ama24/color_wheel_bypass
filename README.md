# DLP Projector Spoofer — ESP32-S3 Super Mini

Firmware for converting a commercial DLP projector into a **direct laser imaging (DLI) system** for UV photolithography. The original light engine (UV lamp, red laser, colour wheel) is removed and replaced with a TTL-controlled 405 nm UV laser. The DMD and main board are retained as a programmable spatial light modulator.

The projector's main board continuously monitors two subsystems and shuts down within seconds if either goes silent or reports a fault. This firmware runs on an **ESP32-S3 Super Mini** and emulates both subsystems simultaneously, keeping the main board in normal operating state indefinitely.

---

## How It Works

### What the main board monitors

| Connector | Subsystem | Failure mode |
|-----------|-----------|--------------|
| **SK01** (13-pin) | Light source control PCB — UART 19200 baud + analogue signals | Shutdown within ~5 s if silent |
| **SK05** (20-pin) | Thermal management board — I2C temperature sensor + fan controller | Shutdown within ~10 s if Ch3 temp returns 0xFF |

### What the ESP32 spoofs

| Signal | Connector | Method | Notes |
|--------|-----------|--------|-------|
| PRSV — presence detect | SK01 pin 13 | 10 kΩ pull-up to V5P0LD | Hardware only, no GPIO |
| SENS — colour-wheel index ~120 Hz | SK02 pin 5 | `esp_timer` state machine | Starts on LDPCN rising edge |
| LDUP — light-output PWM | SK01 pin 10 | `esp_timer` multi-phase waveform | Active while lamp is on |
| LLITZ — intensity reference | SK01 pin 9 | GPIO driven HIGH | Asserted after UART init burst |
| PHSENSE — photo sense ~2.87 V | SK05 pin 1 | GPIO HIGH via resistor divider | 10 kΩ / 68 kΩ from 3.3 V |
| RX0LD / TX0LD — UART 19200 baud | SK01 pins 6/7 | Full packet replay on UART1 | Content-matched responses |
| I2C 0x4D — temperature sensor | SK05 pins 6/7 | I2C0 slave (Wire) | 4 channels; Ch3 NEVER 0xFF |
| I2C 0x41 — fan controller | SK05 pins 6/7 | I2C1 slave (Wire1) | ACKs write commands |
| FG1 / FG2 — fan tachometers | SK05 pins 18/19 | GPIO driven LOW | Main board does not read these |

### What stays physical

| Device | Reason |
|--------|--------|
| **0x54 EEPROM** (SK05) | Contains ~300 bytes of projector-specific boot configuration read at startup. Cannot be spoofed — the data is unique to each unit. Desolder from thermal board and wire standalone. |

---

## Hardware Requirements

- ESP32-S3 Super Mini (ESP32-S3**FH4R2** variant — 4 MB flash + 2 MB PSRAM)
- AMS1117-3.3 (or equivalent) 3.3 V LDO regulator
- 10 kΩ resistor × 2 (PRSV pull-up + PHSENSE divider top)
- 68 kΩ resistor × 1 (PHSENSE divider bottom)
- 2 × dupont jumper wires (I2C bus bridge)
- Fine-pitch soldering equipment for thermal board modifications

---

## Hardware Modifications

### 1. Thermal board (SK05)

The entire thermal board is removed from the signal chain. Three ICs must be handled:

**Remove — IC replaced by ESP32:**
- `0x4D` temperature sensor — desolder and discard
- `0x41` fan controller IC — desolder and discard

**Keep — physical chip required:**
- `0x54` EEPROM — desolder carefully and preserve. Wire it standalone:

| EEPROM pin | Connection |
|------------|------------|
| VCC | 3.3 V |
| GND | GND |
| SDA | ESP32 GP10 (SK05 I2C bus) |
| SCL | ESP32 GP12 (SK05 I2C bus) |
| A0 | GND |
| A1 | GND |
| A2 | 3.3 V |

> A2 = HIGH sets the address to 0x54 (`0b1010100`). Verify A0–A2 pinout against your specific EEPROM part number.

### 2. Resistor divider — PHSENSE

Produces ~2.87 V at SK05 pin 1 (target: 2.85 V).

```
3.3 V ── 10 kΩ ── GP9 ── 68 kΩ ── GND
                    │
               SK05 pin 1
```

### 3. Presence detect — PRSV

```
V5P0LD (SK01 pin 11, 5 V) ── 10 kΩ ── SK01 pin 13 (PRSV)
```

No GPIO involved. This signals to the main board that a control PCB is present.

### 4. Power supply

Power the ESP32 from V5P0LD (5 V available on SK01 pin 11) through an AMS1117-3.3 or equivalent LDO:

```
V5P0LD ── AMS1117-3.3 ── ESP32 3V3 pin
```

### 5. I2C bus bridge

The ESP32 uses two I2C hardware controllers to answer two I2C addresses on the same physical bus. Wire two dupont jumpers on the **right-side header**:

```
GP10 ──┐  SDA bridge (dupont)
GP11 ──┘  → SK05 pin 7

GP12 ──┐  SCL bridge (dupont)
GP13 ──┘  → SK05 pin 6
```

Both pairs are adjacent pins on the right header — one dupont wire each.

---

## GPIO Pin Assignment

### Board layout — ESP32-S3 Super Mini (USB-C at top, viewed from above)

| Function | Left header | | Right header | Function |
|---:|:---:|:---:|:---:|:---|
| USB debug TX | **TX (GP43)** | | **+5V** | Power in → LDO → 3.3 V |
| USB debug RX | **RX (GP44)** | | **GND** | Ground |
| FG1 — fan tach 1 (LOW) | **GP1** | | **+3V3** | 3.3 V out |
| FG2 — fan tach 2 (LOW) | **GP2** | | **GP13** | I2C1 SCL → 0x41 ┐ dupont |
| RX0LD TX → main board | **GP3** | | **GP12** | I2C0 SCL → 0x4D ┘ 12↔13 |
| TX0LD RX ← main board | **GP4** | | **GP11** | I2C1 SDA → 0x41 ┐ dupont |
| SENS — colour wheel ~120 Hz | **GP5** | | **GP10** | I2C0 SDA → 0x4D ┘ 10↔11 |
| LDPCN — light enable input | **GP6** | | **GP9** | PHSENSE — photo sense output |
| LLITZ — intensity ref output | **GP7** | | **GP8** | LDUP — light PWM output |

### Back pads (solder points)

| Pad | Status |
|-----|--------|
| GP14–18, GP21, GP38–42, GP45–48 | Free |
| **GP33–37** | **PSRAM — do NOT use** |
| GP19–20 | USB D± — do NOT use |
| GP26–32 | Flash — do NOT use |

---

## Signal Reference

### SENS — colour-wheel index pulse
- **Waveform:** 7600 µs HIGH / 731 µs LOW ≈ 120.03 Hz, 91.2% duty cycle
- **Starts:** on LDPCN rising edge (not at power-on)
- **Stops:** on graceful shutdown command

### LDUP — light-output PWM
- **Waveform:** one-time start pulse then repeating 6-phase loop (all values µs):

```
Start:  HIGH 1180 → LOW 1108
Loop:   HIGH  236 → LOW 3959 → HIGH 236 → LOW 2710 → HIGH 236 → LOW 1107 → (repeat loop)
```

> ⚠ These values are from an earlier oscilloscope capture and should be verified against your specific projector with a scope before relying on them. Update `LDUP_P0_H` through `LDUP_P7_L` in `src/config.h` if they differ.

### LLITZ — intensity reference
- Driven HIGH after the 6-packet UART init burst (~4.8 s from LDPCN)
- Driven LOW on shutdown command
- **Direction unconfirmed** — probe SK01 pin 9 with a high-impedance scope during normal projector operation before wiring. If the main board drives it HIGH, set GP7 as input-only.

### PHSENSE — photo sense
- GP9 driven HIGH → ~2.87 V at SK05 pin 1 via 10 kΩ / 68 kΩ divider
- Asserted with LLITZ, de-asserted on shutdown

### UART — SK01 UART1
- 19200 baud, 8N1, half-duplex request/response
- GP3 = TX (ESP32 → main board), GP4 = RX (main board → ESP32)
- Every incoming packet is matched against a known pattern table; the correct pre-recorded response is sent by content, not by counter

### I2C — SK05 temperature sensor (0x4D)
- Main board writes a 1-byte register address, then reads 1 byte back
- Polling interval: ~510 ms, 4 channels per cycle

| Register | Channel | Value returned | Temperature |
|----------|---------|----------------|-------------|
| 0x3E / 0x3F | Ch1 — main board ambient | 0x2E / 0x80 | 46.5 °C |
| 0x4E / 0x4F | Ch2 — secondary area | 0x2F / 0x80 | 47.5 °C |
| 0x5E / 0x5F | Ch3 — laser heatsink | 0x4A / 0x80 | 74.5 °C |
| 0x6E / 0x6F | Ch4 — UV source area | 0x2E / 0x80 | 46.5 °C |

### I2C — SK05 fan controller (0x41)
- Main board writes fan speed commands (reg 0x01 = 0xF8, reg 0x02 = 0xF8, every ~2 s)
- No read requests observed in captured traffic — ESP32 ACKs all writes and returns 0x00 on reads

---

## UART Boot Sequence

| Time | Direction | Event |
|------|-----------|-------|
| +3.0 s | — | LDPCN goes HIGH — spoofer wakes, SENS starts |
| +3.4 s | → main board | Announce: `03 40 08 F0` |
| +4.3 s | ← main board | 6 init queries (device ID, config, calibration) |
| +4.3 s | → main board | 6 matched responses |
| +4.8 s | — | LLITZ, LDUP, PHSENSE asserted |
| +5.2 s | ← main board | Steady-state polls begin (~490 ms interval) |
| +5.2 s | → main board | 31 unique telemetry packets, then locked steady-state |
| +22.0 s | ← main board | `02 00` graceful shutdown |
| +22.0 s | → main board | `01` shutdown ACK; all signals de-asserted |

---

## Source Code Layout

```
src/
├── config.h           — all pin numbers, baud rate, timing, I2C addresses, temp values
├── packets.h          — 45 RX0LD byte arrays, TX match tables, lookup tables, labels
├── main.cpp           — GPIO setup, RTOS primitives, ISR installation, task spawning
├── signals.h/.cpp     — SENS and LDUP via esp_timer one-shot state machines
├── uart_primary.h/.cpp— Task A: content-matched UART state machine on Core 0
└── i2c_slave.h/.cpp   — I2C0 slave 0x4D (Wire) + I2C1 slave 0x41 (Wire1)
```

### FreeRTOS task map

| Core | Task | Priority | Role |
|------|------|----------|------|
| 0 | `uart_primary` | 10 | UART state machine |
| 0 | `esp_timer` | 22 | SENS and LDUP waveform callbacks |
| 0 | I2C ISR | interrupt | Wire/Wire1 slave request handlers |
| 1 | Arduino `loop()` | 1 | Idle — `vTaskDelay(portMAX_DELAY)` |

---

## Building

> **Windows:** Build exclusively from the VS Code PlatformIO extension. The Windows shell interprets paths starting with `/Users` as invalid command switches — command-line builds will fail.

1. Open the project folder in VS Code
2. PlatformIO sidebar → **esp32s3** → **Build**
3. Confirm zero errors
4. PlatformIO sidebar → **esp32s3** → **Upload**

`CORE_DEBUG_LEVEL=3` is enabled by default for bringup. Set it to `0` in `platformio.ini` before production deployment.

---

## Serial Monitor / Debug Output

Connect USB before powering the projector. Open the serial monitor at **115200 baud**.

> If USB is not connected at power-on, debug output is silently discarded — this is by design so the boot sequence is not delayed waiting for USB enumeration. Connect USB and cycle projector power to capture a full boot log.

Example output:

```
┌─────────────────────────────────────┐
│  DLP SPOOFER  waiting for LDPCN     │
└─────────────────────────────────────┘

[   3142 ms]  LDPCN ↑
[   3142 ms]  SENS started (~120 Hz)
[   3143 ms]  →  announce             03 40 08 F0

── INIT BURST ───────────────────────
[   3891 ms]  ←  device query         03 01 9D 2D E2
[   3892 ms]  →  device ID            03 42 AB 6F 44 01 01 0D 63 B0
...

── LAMP ON ──────────────────────────
[   4821 ms]  ▲  LLITZ ON   LDUP ON   PHSENSE ON

── STEADY STATE ─────────────────────
[   5310 ms]  ←  poll A               03 01 04 3E 30
[   5311 ms]  →  telemetry #7         03 C8 85 30 00 A5 48 6A 97 43 D4 FF
[   5800 ms]  ←  heartbeat            03 20 D3 95 F0
[   5800 ms]  →  heartbeat echo       03 20 D3 95 F0
[  22015 ms]  ←  graceful shutdown    02 00
[  22015 ms]  →  shutdown ACK         01

── SHUTDOWN ─────────────────────────
[  22016 ms]  ▼  LLITZ OFF  LDUP OFF  PHSENSE OFF
```

Any packet the spoofer does not recognise is logged as `??? UNRECOGNISED` with its full hex dump — use this to identify new packet types from your specific projector revision.

---

## Verification Checklist

Complete in order. Do not connect to the projector until steps 1–8 pass.

| # | Test | Pass condition |
|---|------|----------------|
| 1 | **Build** | PlatformIO reports 0 errors |
| 2 | **SENS** | Scope on GP5: 120 Hz, 91% duty, starts only after LDPCN HIGH |
| 3 | **LDUP** | Scope on GP8: 1180/1108 µs start pulse, then repeating 236/3959/236/2710/236/1107 µs loop |
| 4 | **LDPCN trigger** | Drive GP6 HIGH externally — serial monitor shows announce sent, init sequence, LLITZ ON |
| 5 | **UART content** | Logic analyser on GP3/GP4 at 19200 baud — verify responses match init queries; confirm `88 0F` never appears |
| 6 | **LLITZ** | Scope/meter on GP7: goes HIGH after init, stays HIGH |
| 7 | **LDUP direction** | Probe SK01 pin 10 before wiring — confirm ESP32 drives it (not main board) |
| 8 | **I2C** | I2C analyser on SK05 pins 6/7 — address 0x4D responds; reg 0x5E returns 0x4A; address 0x41 ACKs writes |
| 9 | **PHSENSE** | Multimeter at SK05 pin 1: ~2.85 V when GP9 is HIGH |
| 10 | **Full system** | Connect to projector with UV laser in place — stable operation for 20+ seconds, no fault shutdown, DMD responds to display input |

---

## Known Issues and Things to Watch

### LDUP waveform values are unverified
The phase timings in `src/config.h` (`LDUP_P0_H` through `LDUP_P7_L`) were captured from an earlier oscilloscope session and may not match your specific projector revision. Capture the waveform on SK01 pin 10 during normal operation **before** removing the original control PCB and update the constants if they differ. An incorrect LDUP waveform may not immediately cause shutdown but will produce incorrect lamp-on telemetry over time.

### LLITZ direction is unconfirmed
`src/config.h` marks PIN_LLITZ direction as TBD. If the main board drives SK01 pin 9 HIGH (rather than the control PCB), wiring GP7 as an output creates a bus conflict. Probe with a high-impedance oscilloscope before making the connection. If the main board is the driver, change `GPIO_MODE_OUTPUT` to `GPIO_MODE_INPUT` in `uart_primary.cpp`.

### SENS frequency may vary
The 120 Hz / 7600 µs / 731 µs values were measured from one projector. The main board checks that the colour-wheel pulse is present but may not verify exact frequency — verify on your unit before removing the colour wheel assembly and update `SENS_HIGH_US` / `SENS_LOW_US` in `src/config.h` if needed.

### 0x54 EEPROM is mandatory
The main board reads ~300 bytes of calibration and configuration data from the 0x54 EEPROM at startup. This data is unique to each projector and cannot be reconstructed or spoofed generically. The original chip **must** be present and wired correctly. Loss of this chip or wiring errors at startup will cause the main board to fault before LDPCN is ever asserted.

### Critical byte sequence — UART
The byte sequence `0x88 0x0F` must never appear in any packet transmitted by the ESP32 on GP3. This is the lamp-fault telemetry pattern. The main board initiates shutdown within approximately 10 seconds of receiving it. The pre-recorded packet table has been verified to be free of this sequence, but verify again if you modify any `RX_SEQ_*` entries in `src/packets.h`.

### Critical register — I2C
I2C register `0x5E` (Ch3, laser heatsink temperature) must never return `0xFF`. The main board initiates thermal shutdown approximately 10 seconds after receiving this value. The current constant `TEMP_CH3_MSB = 0x4A` (74 °C) is safe. Do not change it to a value that could be misinterpreted as a fault.

### GPIO33–37 back pads are PSRAM
The ESP32-S3FH4R2 variant used on the Super Mini hard-wires GPIO33–37 to the onboard PSRAM. These pads are exposed on the back of the board but are not available as general-purpose I/O. Using them will corrupt PSRAM and cause random crashes.

### No serial output when running without USB
The USB CDC serial wait was intentionally removed to prevent blocking task creation past the LDPCN window. When the board is powered from the projector without a USB host connected, all `Serial.printf` output is silently discarded. To capture a boot log, connect USB before cycling projector power.

### Build from VS Code PlatformIO only (Windows)
The Windows command processor interprets path arguments beginning with `/Users` as command-line switches. Running `pio run` from a terminal on Windows will fail. Use the PlatformIO sidebar in VS Code exclusively.

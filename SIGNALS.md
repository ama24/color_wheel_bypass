# Signal Reference — DLP Projector Spoofer
## ESP32-S3 Super Mini

---

## Board Pinout & Connections

```
                          ┌──── USB-C ────┐
                     GND ─┤●             ●├─ 5 V  ◄── V5P0LD (SK01 pin 11) via AMS1117-3.3
                     3V3 ─┤●             ●├─ GND
           (free)      4 ─┤●             ●├─ 21    (free)
  ──────────────────── 5 ─┤●             ●├─ 18    (free)
  ──────────────────── 6 ─┤●             ●├─ 17    (free)
  ──────────────────── 7 ─┤●             ●├─ 16 ───────────────────────────────────
  ──────────────────── 8 ─┤●             ●├─ 15 ───────────────────────────────────
           (free)      9 ─┤●             ●├─ 14 ───────────────────────────────────
        USB debug TX  43 ─┤●             ●├─ 13 ───────────────────────────────────
        USB debug RX  44 ─┤●             ●├─ 12 ───────────────────────────────────
           (free)      1 ─┤●             ●├─ 11 ───────────────────────────────────
           (free)      2 ─┤●             ●├─ 10 ───────────────────────────────────
                          └───────────────┘

  GPIO  Signal        Dir  Destination
  ────  ──────────    ───  ──────────────────────────────────────────────
   5    SENS          OUT  SK02 pin 5   — colour-wheel index pulse
   6    LDPCN          IN  SK01 pin 4   — light driver enable from main board
   7    LLITZ         OUT  SK01 pin 9   — lamp-on intensity reference
   8    LDUP          OUT  SK01 pin 10  — light-output PWM
  10    PHSENSE       OUT  SK05 pin 1   — photo sense stub (via resistor divider)
  11    I2C SDA      BIDR  SK05 pin 7   — I2C bus (slave 0x4D + EEPROM 0x54)
  12    I2C SCL        IN  SK05 pin 6   — I2C clock (main board is master)
  13    I2C1 SDA†    BIDR  (jumper → GPIO11) Wire1 slave 0x41
  14    I2C1 SCL†      IN  (jumper → GPIO12) Wire1 slave 0x41
  15    RX0LD TX      OUT  SK01 pin 6   — UART1: spoofer → main board
  16    TX0LD RX       IN  SK01 pin 7   — UART1: main board → spoofer

  † GPIO13 and GPIO14 must be bridged to GPIO11 and GPIO12 with short jumper wires
    so that Wire (0x4D) and Wire1 (0x41) share the same physical I2C bus.
```

---

## System Boot Timeline

```
  Power on
     │
     ├──── +0.0 s ── All signals LOW / idle
     │
     ├──── +2.8 s ── LUDUP burst (power stabilised)
     │
     ├──── +3.0 s ── LDPCN ──────────────────────────────────────────────────────► HIGH
     │                         Main board enables light driver subsystem
     │                         ESP32 wakes from IDLE, starts SENS, sends ANNOUNCE
     │
     ├──── +3.4 s ── RX0LD ── spoofer sends: 03 40 08 F0  (announce)
     │
     ├──── +4.3 s ── UART init burst (6 exchanges: device ID, config, calibration)
     │
     ├──── +4.8 s ── LLITZ ──────────────────────────────────────────────────────► HIGH
     │               LDUP  ──────────────────────────────────────────────────────► ACTIVE
     │               PHSENSE ───────────────────────────────────────────────────► HIGH
     │
     ├──── +5.2 s ── 490 ms poll cycle begins (TX0LD ↔ RX0LD)
     │
     ├──── +5.2 s ── SENS  ──────────────────────────────────────────────────────► RUNNING
     │
     ╎   (normal operation — laser ON, DMD active)
     │
     ├──── +22.0 s ─ TX0LD receives 02 00  (graceful shutdown)
     │               RX0LD responds    01
     │
     └──── +22.0 s ─ LLITZ, LDUP, PHSENSE, SENS ─────────────────────────────── LOW/STOP
```

---

## Signal Waveforms

### GPIO 6 — LDPCN  ◄ INPUT from main board

```
  This is an INPUT. The ESP32 does not drive it.
  Rising edge triggers the spoofer state machine via ISR.

  3.3V        ┌─────────────────────────────────────────────── … stays HIGH
              │
   0 V ───────┘
        ~3.0 s after power-on
        ↑
        ISR fires here — spoofer wakes, SENS starts, ANNOUNCE sent
```

---

### GPIO 5 — SENS  ► OUTPUT  →  SK02 pin 5

```
  Colour-wheel rotation index pulse. Starts on LDPCN rising edge.
  Mimics the Hall-effect sensor output of the original colour wheel.

  3.3V ─┐                   ┌──────────────────┐                   ┌─────
        │                   │                  │                   │
        │   7 600 µs        │                  │   7 600 µs        │
   0 V  └───────────────────┘                  └───────────────────┘
        ◄──── HIGH 7600 µs ─►◄ LOW 731 µs ────►◄──── HIGH 7600 µs ─►◄ 731 …

  ◄────────────────── Period: 8 331 µs ─────────────────────────────►
                      Frequency: ~120.03 Hz     Duty cycle: 91.2 %

  Starts: on LDPCN rising edge
  Stops:  on graceful or abnormal shutdown
```

---

### GPIO 7 — LLITZ  ► OUTPUT  →  SK01 pin 9

```
  Intensity reference. Tells the main board the lamp is ON.
  If it does not go HIGH within 275 ms of ignition attempt → fault shutdown.

  3.3V             ┌──────────────────────────────────────────┐
                   │                                          │
                   │  asserted ~1.5 s after LDPCN             │  de-asserted at shutdown
   0 V ────────────┘                                          └────────────────

               ↑ LDPCN HIGH                   shutdown CMD ↑
               │← ~1.5 s →│← lamp ON (14+ s) →│
```

---

### GPIO 8 — LDUP  ► OUTPUT  →  SK01 pin 10

```
  Multi-phase PWM confirming light output is active.
  Has a one-time start pulse (Phases 0–1) then a repeating loop (Phases 2–7).

  ┌──── ONE-TIME START ────┬──────────────────── REPEATING LOOP ───────────────────────────►
  │                        │
  │  Ph0: HIGH  1180 µs    │  Ph2: H   Ph3: LOW 3959 µs   Ph4: H   Ph5: LOW 2710 µs
  │                        │  236 µs                       236 µs
  │                        │            Ph6: H   Ph7: LOW 1107 µs ──► back to Ph2
  │                        │            236 µs
  │                        │
3.3V ┌──────────┐          ┌─┐                   ┌─┐              ┌─┐
     │          │          │ │                   │ │              │ │
  0V ┘          └──────────┘ └───────────────────┘ └──────────────┘ └──── 1107 ──►(Ph2)
     ◄─ 1180 µs─►◄─ 1108 µs►◄ 236►◄──── 3959 µs ───►◄ 236►◄── 2710 µs ──►◄ 236►

  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ Phase │  Level │  Duration  │  Next                                          │
  ├───────┼────────┼────────────┼────────────────────────────────────────────────┤
  │   0   │  HIGH  │  1 180 µs  │  → Phase 1  (one-time)                         │
  │   1   │  LOW   │  1 108 µs  │  → Phase 2  (one-time)                         │
  │   2   │  HIGH  │    236 µs  │  → Phase 3  ◄─┐                                │
  │   3   │  LOW   │  3 959 µs  │  → Phase 4    │  repeating                     │
  │   4   │  HIGH  │    236 µs  │  → Phase 5    │  loop                          │
  │   5   │  LOW   │  2 710 µs  │  → Phase 6    │                                │
  │   6   │  HIGH  │    236 µs  │  → Phase 7    │                                │
  │   7   │  LOW   │  1 107 µs  │  → Phase 2  ──┘                                │
  └──────────────────────────────────────────────────────────────────────────────┘

  ⚠  Timings are placeholder values — capture with oscilloscope and update
     LDUP_P*_H / LDUP_P*_L in src/config.h before connecting to projector.

  Active: same on/off timing as LLITZ.
```

---

### GPIO 10 — PHSENSE  ► OUTPUT  →  SK05 pin 1  (via resistor divider)

```
  Photo sense stub. Main board expects ~2.85 V steady when light source is active.
  GPIO drives HIGH (3.3 V); a resistor divider scales it to ~2.87 V.

  Resistor divider:
    3.3 V ──┬── 10 kΩ ──┬── GPIO10 ──── to SK05 pin 1
            │           │
           GND        68 kΩ
                        │
                       GND

    Voltage at SK05 pin 1 = 3.3 V × 68k / (10k + 68k) ≈ 2.87 V ✓

  SK05 pin 1 voltage:

  2.87 V ───────────────────────────────────────────────────────────
                                                                     │
   0 V                                                               └── (at shutdown)

  Active: same on/off timing as LLITZ.
```

---

### GPIO 15 — RX0LD TX  ► OUTPUT  →  SK01 pin 6  (UART1, 19 200 baud, 8N1)

```
  The spoofer drives this line. The main board receives here.
  Packet framing: bytes separated by ≤ 5 ms gaps. > 5 ms silence = new packet.

  ┌─── IDLE ────┬── ANNOUNCE ──────────────────────────────────────────────────
  │             │
  │   (STOP)    │  Byte: 0x03
  │             │  ┌ start ┐ D0 D1 D2 D3 D4 D5 D6 D7 ┌ stop ┐
  3.3V ─────────┘  │       │  1  1  0  0  0  0  0  0  │      │
   0 V             └───────┘                           └──────┘
                   ◄ 52 µs ►◄────────────── 8 × 52 µs ───────►◄ 52 µs ►

  Each bit cell = 52.08 µs  (1 / 19 200 baud)
  Packet: start bit (LOW) → 8 data bits LSB-first → stop bit (HIGH)

  ── Boot sequence packets sent on RX0LD ─────────────────────────────────────

  +3.4s  [0]  03 40 08 F0                    ← ANNOUNCE (power-on ready)
  +4.3s  [1]  03 42 AB 6F 44 01 01 0D 63 B0  ← device ID
  +4.3s  [2]  03 40 11 92                    ← config ACK
  +4.3s  [3]  03 20 9E EC 02 ED              ← multi-reg ACK
  +4.3s  [4]  03 C8 CB C1 00 27 43 A5 88 D6 1A 43 FD  ← cal data
  +4.3s  [5]  03 A0 C3 24 F8                ← cal ACK
  +4.4s  [6]  03 20 6D 2A B2                ← init complete ACK
  +4.8s+ [7…] 03 C8 85 30 …                 ← telemetry (32 unique packets)
  +19s+  [38…] 03 48 09 02 00 1F 08 0C 6A BD E8 FF  ← steady-state (repeats)
  +22s   [44] 01                            ← shutdown ACK

  ⛔  NEVER transmit any packet containing the bytes 88 0F — this is the
      lamp-fault pattern. The main board shuts down within ~10 s on receipt.
```

---

### GPIO 16 — TX0LD RX  ◄ INPUT  ←  SK01 pin 7  (UART1, 19 200 baud, 8N1)

```
  This is an INPUT. The main board drives this line.
  The ESP32 listens and uses incoming packets to advance the state machine.

  Key packets received:

  +4.3s  03 01 9D 2D E2          init: device query
  +4.3s  03 01 11 92             init: config register read
  +4.3s  03 03 E6 8A 27 00 ED   init: multi-register write
  +4.3s  03 01 90 82 FE          init: status register read
  +4.3s  03 02 A3 38 24 08       init: calibration write
  +4.3s  03 02 B5 A7 92 FF       init: calibration write
  +5.2s  03 01 04 3E 30          periodic poll  (~490 ms interval)
  +6.6s  03 20 D3 95 F0          heartbeat      (echo back unchanged)
  +22.0s 02 00                   graceful shutdown
```

---

### GPIO 11/12 — I2C SDA/SCL  ↔  SK05 pins 7/6  (I2C slave at 0x4D and 0x54)

```
  The main board is the I2C master. The ESP32 is slave at 0x4D.
  The physical EEPROM (0x54), desoldered from the thermal board, also sits on
  this bus.

  I2C clock (SCL) — driven by main board:

  3.3V ─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
   0 V  └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─

  Typical temperature register read transaction (~510 ms poll):

  SDA:
  ┌────┐                                                 ┌────
  │    │ S  A  A  A  A  A  A  A  W  A  R  R  R  R  R  R  R  R  A  P
  │    └─┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──
  │      │0 │1 │0 │0 │1 │1 │0 │1 │ ↑│  register addr  │  data byte  │
  │      │     addr 0x4D (7-bit)   │ W│ACK│  e.g. 0x5E  │  e.g. 0x4A │
  └──────┘
  S = START  A = address bit  W = write  ACK = slave acks  P = STOP/NACK

  Polling registers (every ~510 ms, 4 channels):
  ┌──────────┬──────────┬──────────────────────────────────┬───────────────┐
  │ Register │ Channel  │ Meaning                          │ Spoofed value │
  ├──────────┼──────────┼──────────────────────────────────┼───────────────┤
  │   0x3E   │ Ch1 int  │ Main board / ambient (°C)        │ 0x2E  (46°C)  │
  │   0x3F   │ Ch1 frac │ Fractional (0x80 = +0.5°C)       │ 0x80          │
  │   0x4E   │ Ch2 int  │ Secondary area (°C)              │ 0x2F  (47°C)  │
  │   0x4F   │ Ch2 frac │                                  │ 0x80          │
  │   0x5E   │ Ch3 int  │ Laser heatsink (°C) ← CRITICAL   │ 0x4A  (74°C)  │
  │   0x5F   │ Ch3 frac │ ⚠ NEVER return 0xFF here          │ 0x80          │
  │   0x6E   │ Ch4 int  │ UV source area (°C)              │ 0x2E  (46°C)  │
  │   0x6F   │ Ch4 frac │                                  │ 0x80          │
  └──────────┴──────────┴──────────────────────────────────┴───────────────┘
```

---

### GPIO 13/14 — I2C1 SDA/SCL bridge  ↔  (jumper to GPIO11/12)  (I2C slave at 0x41)

```
  Wire1 (I2C1 hardware peripheral) runs on GPIO13/14 as slave at 0x41.
  GPIO13 and GPIO14 must be physically shorted to GPIO11 and GPIO12
  with jumper wires so both I2C controllers share the same bus.

  Why a bridge? The ESP32 GPIO matrix prevents two I2C peripherals from
  sharing output control of the same GPIO pins — so Wire1 gets its own
  GPIO pair and they are externally joined. Open-drain I2C is safe to
  wire-OR like this.

  GPIO13 ──── short wire ────► GPIO11 (SDA bus)
  GPIO14 ──── short wire ────► GPIO12 (SCL bus)

  0x41 transaction observed from capture (fan controller / GPIO expander):

  Boot init (once):
    Main board writes:  reg 00 ← 0x81
                        reg 03 ← 0x40
                        reg 04 ← 0xC0

  Periodic (every ~2 s):
    Main board writes:  reg 01 ← 0xF8
                        reg 02 ← 0xF8

  ESP32 Wire1 response: ACK all writes, return 0x00 for any reads.

  SDA during a write to reg 01:
  START │ addr=0x41+W │ ACK │ reg=0x01 │ ACK │ data=0xF8 │ ACK │ STOP
        └─────────────┘     └──────────┘     └───────────┘
                 ESP32 Wire1 ACKs each byte
```

---

## Hardware Checklist — Thermal Board Removal

```
  ┌─────────────────────────────────────────────────────────────────────────┐
  │  Component         Action                                               │
  ├─────────────────────────────────────────────────────────────────────────┤
  │  Thermal board     Remove entirely from projector                       │
  │  0x4D temp sensor  Spoofed by Wire on GPIO11/12  ✓ (firmware)          │
  │  0x41 peripheral   Spoofed by Wire1 on GPIO13/14 ✓ (firmware)          │
  │  0x54 EEPROM       Desolder from thermal board. Wire standalone:        │
  │                      VCC → 3.3 V,  GND → GND                          │
  │                      SDA → GPIO11, SCL → GPIO12                        │
  │                      A0  → GND,  A1 → GND,  A2 → 3.3 V               │
  │                      (preserves address 0b1010100 = 0x54)              │
  │  FG1/FG2 (tach)    Not monitored by main board — leave disconnected    │
  │  FPCN1–5 (enables) Outputs from main board — leave disconnected        │
  │  Fan power rails   Not needed — leave disconnected                     │
  │  GPIO bridge       Solder short wire GPIO13↔GPIO11 (SDA)               │
  │                    Solder short wire GPIO14↔GPIO12 (SCL)               │
  └─────────────────────────────────────────────────────────────────────────┘
```

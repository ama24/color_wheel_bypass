#pragma once

// ===========================================================================
//  config.h — All pin assignments, baud rates, and timing constants
//  ESP32-S3 Super Mini  ·  DLP Projector Spoofer
// ===========================================================================

// ---------------------------------------------------------------------------
// GPIO assignments
// Safe S3 user GPIOs: 1–18, 21, 38–48
// Avoid: 19/20 (USB D±), 26–32 (flash/PSRAM)
// ---------------------------------------------------------------------------
#define PIN_SENS          5    // SK02 pin 5  — colour-wheel index pulse output (~120 Hz)
#define PIN_LDPCN         6    // SK01 pin 4  — light driver enable input from main board
#define PIN_LLITZ         7    // SK01 pin 9  — intensity reference (direction TBD — probe first)
#define PIN_LDUP          8    // SK01 pin 10 — light output PWM (direction TBD — probe first)
#define PIN_RX0LD_TX     15    // SK01 pin 6  — UART1 TX: spoofer → main board
#define PIN_TX0LD_RX     16    // SK01 pin 7  — UART1 RX: main board → spoofer
#define PIN_PHSENSE      10    // SK05 pin 1  — photo sense analogue stub (drive HIGH via divider)
#define PIN_I2C_SDA      11    // SK05 pin 7  — I2C data  (slave at 0x4D)
#define PIN_I2C_SCL      12    // SK05 pin 6  — I2C clock (slave at 0x4D)
// NOTE: GPIO35/36 are tied to onboard PSRAM on ESP32-S3FH4R2 — do NOT use them.

// ---------------------------------------------------------------------------
// SK01 hardware-only signals (no GPIO)
//   PRSV  (pin 13): 10 kΩ pull-up from V5P0LD (5 V) — presence detect
//   V5P0LD(pin 11): power input to ESP32 via 3.3 V LDO (e.g. AMS1117-3.3)
//   PHSENSE divider: 3.3 V → 10 kΩ → GPIO10 → 68 kΩ → GND → ~2.87 V at SK05 pin 1
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// SK01 primary UART (UART1)
// ---------------------------------------------------------------------------
#define PRIMARY_UART_NUM        1          // Arduino Serial1
#define PRIMARY_UART_BAUD   19200
#define UART_PKT_TIMEOUT_MS     5          // inter-byte silence = end of packet

// ---------------------------------------------------------------------------
// SENS waveform timing (µs)
// ---------------------------------------------------------------------------
#define SENS_HIGH_US    7600ULL
#define SENS_LOW_US      731ULL

// ---------------------------------------------------------------------------
// LDUP multi-phase waveform timing (µs)
// WARNING: captured values are placeholders — verify with oscilloscope.
// Spec: ~10 269 transitions over 18.9 s → ~543 edges/s → ~271 Hz if uniform.
// ---------------------------------------------------------------------------
#define LDUP_P0_H   1180ULL    // one-time start: HIGH
#define LDUP_P1_L   1108ULL    // one-time start: LOW
#define LDUP_P2_H    236ULL    // repeating loop ┐
#define LDUP_P3_L   3959ULL    //                │
#define LDUP_P4_H    236ULL    //                │
#define LDUP_P5_L   2710ULL    //                │
#define LDUP_P6_H    236ULL    //                │
#define LDUP_P7_L   1107ULL    // loop end ──────┘ → wraps to P2

// ---------------------------------------------------------------------------
// State machine timing
// ---------------------------------------------------------------------------
#define POLL_INTERVAL_MS        490        // steady-state poll interval
#define RX_STEADY_IDX            38        // rx_idx >= this → send RX_STEADY

// ---------------------------------------------------------------------------
// SK05 I2C temperature sensor spoof (address 0x4D)
// Physical mod: remove 0x4D IC from thermal board, wire ESP32 SDA/SCL in its place.
// Ch3 (laser heatsink) MUST never return 0xFF — that triggers thermal shutdown.
// ---------------------------------------------------------------------------
#define I2C_TEMP_ADDR   0x4D
#define TEMP_CH1_MSB    0x2E   // 46 °C — main board / ambient
#define TEMP_CH2_MSB    0x2F   // 47 °C — secondary board area
#define TEMP_CH3_MSB    0x4A   // 74 °C — laser heatsink  ← NEVER 0xFF
#define TEMP_CH4_MSB    0x2E   // 46 °C — UV source area

// ---------------------------------------------------------------------------
// FreeRTOS task parameters
// ---------------------------------------------------------------------------
#define UART_TASK_STACK   4096
#define UART_TASK_PRI       10
#define UART_TASK_CORE       0

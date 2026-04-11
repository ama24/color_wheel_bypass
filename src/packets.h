#pragma once
#include <stdint.h>

// ===========================================================================
//  packets.h — Full UART packet sequences for SK01 spoofer
//  Source: nominal boot capture, DLP projector reverse engineering
//
//  RX0LD: spoofer transmits these (control PCB → main board)  — 45 entries
//  TX0LD: main board transmits these (reference only)         — not stored
//
//  CRITICAL: Never send any packet containing the byte sequence 0x88 0x0F.
//  That is the fault telemetry pattern — it triggers projector shutdown.
// ===========================================================================

// ---------------------------------------------------------------------------
// RX0LD sequence — spoofer transmits (indices 0–44)
// Replay in order: 0=announce, 1–6=init, 7–37=telemetry, 38–43=steady(×6), 44=shutdown
// After index 43, lock permanently to RX_STEADY for all subsequent polls.
// ---------------------------------------------------------------------------

// [0] Proactive announce — sent immediately on LDPCN rising edge
static const uint8_t RX_SEQ_0[]  = {0x03,0x40,0x08,0xF0};                                                   // 4

// [1] Device ID / firmware version
static const uint8_t RX_SEQ_1[]  = {0x03,0x42,0xAB,0x6F,0x44,0x01,0x01,0x0D,0x63,0xB0};                   // 10

// [2] Config register ACK
static const uint8_t RX_SEQ_2[]  = {0x03,0x40,0x11,0x92};                                                   // 4

// [3] Multi-register config ACK
static const uint8_t RX_SEQ_3[]  = {0x03,0x20,0x9E,0xEC,0x02,0xED};                                        // 6

// [4] Calibration data response
static const uint8_t RX_SEQ_4[]  = {0x03,0xC8,0xCB,0xC1,0x00,0x27,0x43,0xA5,0x88,0xD6,0x1A,0x43,0xFD};   // 13

// [5] Calibration ACK
static const uint8_t RX_SEQ_5[]  = {0x03,0xA0,0xC3,0x24,0xF8};                                             // 5

// [6] Init complete ACK
static const uint8_t RX_SEQ_6[]  = {0x03,0x20,0x6D,0x2A,0xB2};                                             // 5

// [7–37] Telemetry packets — unique per poll cycle
static const uint8_t RX_SEQ_7[]  = {0x03,0xC8,0x85,0x30,0x00,0xA5,0x48,0x6A,0x97,0x43,0xD4,0xFF};         // 12
static const uint8_t RX_SEQ_8[]  = {0x03,0x08,0xA6,0x30,0x00,0x4A,0xE8,0x6A,0x94,0x43,0x51,0xE8};         // 12
static const uint8_t RX_SEQ_9[]  = {0x03,0x28,0xD2,0x02,0x00,0xA9,0x2C,0x43,0x59,0x28,0x0D};              // 11
static const uint8_t RX_SEQ_10[] = {0x03,0xA8,0xE7,0x30,0x00,0xA9,0x28,0x43,0xD6,0x02,0x43};              // 11
static const uint8_t RX_SEQ_11[] = {0x03,0x20,0xD3,0x95,0xF0};                                             // 5  (heartbeat echo — in sequence)
static const uint8_t RX_SEQ_12[] = {0x03,0xA8,0x69,0x02,0x00,0xA9,0x27,0x43,0x59,0x28,0x0D};              // 11
static const uint8_t RX_SEQ_13[] = {0x03,0xE8,0xAE,0x02,0x00,0xA4,0x88,0x6A,0x91,0x43,0x1F,0xE8};         // 12
static const uint8_t RX_SEQ_14[] = {0x03,0xC8,0x7D,0x30,0x00,0xD4,0x24,0x43,0xAC,0xEE,0xE8};              // 11
static const uint8_t RX_SEQ_15[] = {0x03,0x88,0x76,0x30,0x00,0xD4,0x20,0x43,0xB1,0x28,0x0C};              // 11
static const uint8_t RX_SEQ_16[] = {0x03,0x28,0x91,0x02,0x00,0x46,0x68,0xD4,0x85,0x43,0x1E,0xE8};         // 12
static const uint8_t RX_SEQ_17[] = {0x03,0xA8,0x1A,0x30,0x00,0x45,0xE8,0xD4,0x85,0x43,0x0C};              // 11
static const uint8_t RX_SEQ_18[] = {0x03,0xE8,0x72,0x02,0x00,0x51,0x08,0xD4,0x81,0x43,0x0C};              // 11
static const uint8_t RX_SEQ_19[] = {0x03,0x08,0x97,0x02,0x00,0xD4,0x12,0x43,0x35,0xDB,0xE8};              // 11
static const uint8_t RX_SEQ_20[] = {0x03,0x28,0x2B,0x30,0x00,0xA8,0x0B,0x43,0x57,0x28,0xC7,0xFF};         // 12
static const uint8_t RX_SEQ_21[] = {0x03,0x68,0x42,0xC1,0x00,0x0F,0x43,0x50,0x48,0x35,0xDB,0xE8};         // 12
static const uint8_t RX_SEQ_22[] = {0x03,0xA8,0xA9,0x30,0x00,0xA8,0x02,0x43,0xAB,0xD7,0xE8};              // 11
static const uint8_t RX_SEQ_23[] = {0x03,0xE8,0x09,0x30,0x00,0x42,0x28,0x0D,0x56,0xA8,0xC7,0xFF};         // 12
static const uint8_t RX_SEQ_24[] = {0x03,0xE8,0xD6,0x30,0x00,0x42,0xA8,0x0C,0xAC,0xA8,0xC7,0xFF};         // 12
static const uint8_t RX_SEQ_25[] = {0x03,0x48,0x75,0xC1,0x00,0x07,0x43,0x3F,0x88,0xD5,0xD1,0xE8};         // 12
static const uint8_t RX_SEQ_26[] = {0x03,0x88,0xB1,0x30,0x00,0xA0,0x48,0x0C,0x55,0xE8,0x0C};              // 11
static const uint8_t RX_SEQ_27[] = {0x03,0x48,0xC9,0x30,0x00,0x40,0x68,0x0C,0xAA,0x88,0x0C};              // 11
static const uint8_t RX_SEQ_28[] = {0x03,0xC8,0xDF,0x02,0x00,0x0D,0x1F,0x88,0x35,0xC9,0xE8};              // 11
static const uint8_t RX_SEQ_29[] = {0x03,0x48,0x3B,0x30,0x00,0x7F,0xE8,0x0C,0xAA,0xC7,0xE8};              // 11
static const uint8_t RX_SEQ_30[] = {0x03,0x68,0x49,0x02,0x00,0x7F,0xC8,0x0C,0x50,0xE8,0x0C};              // 11
static const uint8_t RX_SEQ_31[] = {0x03,0x68,0xE5,0xC1,0x00,0xFC,0x48,0x0C,0x6A,0xC6,0xE8};              // 11
static const uint8_t RX_SEQ_32[] = {0x03,0x08,0x54,0x02,0x00,0x0C,0x8F,0x3A,0x43,0x1C,0xE8};              // 11
static const uint8_t RX_SEQ_33[] = {0x03,0xE8,0x25,0x02,0x00,0x3F,0xC8,0x0C,0x4D,0x68,0x0C};              // 11
static const uint8_t RX_SEQ_34[] = {0x03,0x48,0xCD,0x02,0x00,0x0C,0x3C,0x48,0x6A,0xC4,0xE8};              // 11
static const uint8_t RX_SEQ_35[] = {0x03,0x48,0x79,0x30,0x00,0x0C,0x3C,0x88,0x6A,0xC1,0xE8};              // 11
static const uint8_t RX_SEQ_36[] = {0x03,0x88,0x52,0x30,0x00,0x1F,0xE8,0xC7,0x28,0x43,0x63,0xFF};         // 12
static const uint8_t RX_SEQ_37[] = {0x03,0x28,0xA9,0x30,0x00,0x7D,0xA8,0xC7,0x24,0x43,0x37,0xE8};         // 12

// [38–43] Steady-state lock — 6 identical packets, then loop on RX_STEADY forever
// These six entries in the table point to RX_STEADY for convenient indexing.

// Steady-state healthy telemetry (sent for all polls once locked)
static const uint8_t RX_STEADY[] = {0x03,0x48,0x09,0x02,0x00,0x1F,0x08,0x0C,0x6A,0xBD,0xE8,0xFF};         // 12

// [44] Graceful shutdown ACK
static const uint8_t RX_SHUTDOWN[] = {0x01};                                                                 // 1

// ---------------------------------------------------------------------------
// Lookup tables — pointer + length, indexed 0–44
// Indices 38–43 all point to RX_STEADY.
// ---------------------------------------------------------------------------
static const uint8_t * const RX_TABLE[] = {
    RX_SEQ_0,   // 0  — announce
    RX_SEQ_1,   // 1  — device ID
    RX_SEQ_2,   // 2  — config ACK
    RX_SEQ_3,   // 3  — multi-reg ACK
    RX_SEQ_4,   // 4  — cal data
    RX_SEQ_5,   // 5  — cal ACK
    RX_SEQ_6,   // 6  — init complete
    RX_SEQ_7,   // 7
    RX_SEQ_8,   // 8
    RX_SEQ_9,   // 9
    RX_SEQ_10,  // 10
    RX_SEQ_11,  // 11 — heartbeat echo
    RX_SEQ_12,  // 12
    RX_SEQ_13,  // 13
    RX_SEQ_14,  // 14
    RX_SEQ_15,  // 15
    RX_SEQ_16,  // 16
    RX_SEQ_17,  // 17
    RX_SEQ_18,  // 18
    RX_SEQ_19,  // 19
    RX_SEQ_20,  // 20
    RX_SEQ_21,  // 21
    RX_SEQ_22,  // 22
    RX_SEQ_23,  // 23
    RX_SEQ_24,  // 24
    RX_SEQ_25,  // 25
    RX_SEQ_26,  // 26
    RX_SEQ_27,  // 27
    RX_SEQ_28,  // 28
    RX_SEQ_29,  // 29
    RX_SEQ_30,  // 30
    RX_SEQ_31,  // 31
    RX_SEQ_32,  // 32
    RX_SEQ_33,  // 33
    RX_SEQ_34,  // 34
    RX_SEQ_35,  // 35
    RX_SEQ_36,  // 36
    RX_SEQ_37,  // 37
    RX_STEADY,  // 38 — steady-state lock begins
    RX_STEADY,  // 39
    RX_STEADY,  // 40
    RX_STEADY,  // 41
    RX_STEADY,  // 42
    RX_STEADY,  // 43
    RX_SHUTDOWN // 44 — shutdown ACK
};

static const uint8_t RX_LENS[] = {
     4,  // 0
    10,  // 1
     4,  // 2
     6,  // 3
    13,  // 4
     5,  // 5
     5,  // 6
    12,  // 7
    12,  // 8
    11,  // 9
    11,  // 10
     5,  // 11
    11,  // 12
    12,  // 13
    11,  // 14
    11,  // 15
    12,  // 16
    11,  // 17
    11,  // 18
    11,  // 19
    12,  // 20
    12,  // 21
    11,  // 22
    12,  // 23
    12,  // 24
    12,  // 25
    11,  // 26
    11,  // 27
    11,  // 28
    11,  // 29
    11,  // 30
    11,  // 31
    11,  // 32
    11,  // 33
    11,  // 34
    11,  // 35
    12,  // 36
    12,  // 37
    12,  // 38
    12,  // 39
    12,  // 40
    12,  // 41
    12,  // 42
    12,  // 43
     1,  // 44
};

static_assert(sizeof(RX_TABLE) / sizeof(RX_TABLE[0]) == 45, "RX_TABLE must have 45 entries");
static_assert(sizeof(RX_LENS)  / sizeof(RX_LENS[0])  == 45, "RX_LENS must have 45 entries");

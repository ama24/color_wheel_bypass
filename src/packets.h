#pragma once
#include <stdint.h>
#include <string.h>

// ===========================================================================
//  packets.h — Full UART packet tables for SK01 spoofer
//
//  RX0LD: spoofer transmits (ESP32 → main board)   — 45 entries, indices 0–44
//  TX0LD: main board transmits (main board → ESP32) — patterns for matching
//
//  CRITICAL: Never send any packet containing 0x88 0x0F.
//  That is the fault telemetry pattern — triggers projector shutdown.
// ===========================================================================


// ===========================================================================
//  TX0LD — patterns received FROM the main board
//  Used for content-based matching so the spoofer always sends the correct
//  reply regardless of packet ordering or repetition.
// ===========================================================================

// Match entry: compare first pat_len bytes of the incoming packet.
// rx_idx: index into RX_TABLE to send.
// rx_idx == 0xFF means "send next telemetry packet" (advance telem counter).
struct TxMatch {
    const uint8_t* pat;
    uint8_t        pat_len;
    uint8_t        rx_idx;
    const char*    label;
};

// ---------------------------------------------------------------------------
// Init phase queries (sent once during boot, in this order)
// ---------------------------------------------------------------------------
static const uint8_t TX_DEVICE_QUERY[]  = {0x03, 0x01, 0x9D};   // device ID query
static const uint8_t TX_CONFIG_READ[]   = {0x03, 0x01, 0x11};   // config register read
static const uint8_t TX_MULTI_WRITE[]   = {0x03, 0x03};         // multi-register write
static const uint8_t TX_STATUS_READ[]   = {0x03, 0x01, 0x90};   // status register read
static const uint8_t TX_CAL_WRITE_1[]   = {0x03, 0x02, 0xA3};   // calibration write #1
static const uint8_t TX_CAL_WRITE_2[]   = {0x03, 0x02, 0xB5};   // calibration write #2

static const TxMatch TX_INIT_TABLE[] = {
    { TX_DEVICE_QUERY, 3, 1, "device query"  },
    { TX_CONFIG_READ,  3, 2, "config read"   },
    { TX_MULTI_WRITE,  2, 3, "multi-write"   },
    { TX_STATUS_READ,  3, 4, "status read"   },
    { TX_CAL_WRITE_1,  3, 5, "cal write 1"  },
    { TX_CAL_WRITE_2,  3, 6, "cal write 2"  },
};
static const uint8_t TX_INIT_TABLE_LEN = 6;

// ---------------------------------------------------------------------------
// Steady-state poll variants (~490 ms interval)
// rx_idx 0xFF = send next telemetry; 11 = heartbeat echo (special)
// ---------------------------------------------------------------------------
static const uint8_t TX_HEARTBEAT[]    = {0x03, 0x20, 0xD3, 0x95, 0xF0};
static const uint8_t TX_POLL_A[]       = {0x03, 0x01, 0x04, 0x3E, 0x30};
static const uint8_t TX_POLL_B[]       = {0x03, 0x01, 0x04, 0x3E, 0xFE};
static const uint8_t TX_POLL_C[]       = {0x03, 0x01, 0x04, 0x82, 0xFE};
static const uint8_t TX_STATUS_SS[]    = {0x03, 0x01, 0x90, 0x82, 0xFE};
static const uint8_t TX_EXT_STATUS[]   = {0x03, 0x40, 0x90, 0x82, 0xFE};
static const uint8_t TX_SHUTDOWN[]     = {0x02, 0x00};
static const uint8_t TX_SHUTDOWN_ABN[] = {0x00, 0x00};

static const TxMatch TX_STEADY_TABLE[] = {
    { TX_HEARTBEAT,   5, 11,   "heartbeat"    },  // rx_idx 11 = echo
    { TX_POLL_A,      5, 0xFF, "poll A"       },
    { TX_POLL_B,      5, 0xFF, "poll B"       },
    { TX_POLL_C,      5, 0xFF, "poll C"       },
    { TX_STATUS_SS,   5, 0xFF, "status read"  },
    { TX_EXT_STATUS,  5, 0xFF, "ext status"   },
};
static const uint8_t TX_STEADY_TABLE_LEN = 6;


// ===========================================================================
//  RX0LD — spoofer transmits (indices 0–44)
// ===========================================================================

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

// [7–37] Unique telemetry packets
static const uint8_t RX_SEQ_7[]  = {0x03,0xC8,0x85,0x30,0x00,0xA5,0x48,0x6A,0x97,0x43,0xD4,0xFF};         // 12
static const uint8_t RX_SEQ_8[]  = {0x03,0x08,0xA6,0x30,0x00,0x4A,0xE8,0x6A,0x94,0x43,0x51,0xE8};         // 12
static const uint8_t RX_SEQ_9[]  = {0x03,0x28,0xD2,0x02,0x00,0xA9,0x2C,0x43,0x59,0x28,0x0D};              // 11
static const uint8_t RX_SEQ_10[] = {0x03,0xA8,0xE7,0x30,0x00,0xA9,0x28,0x43,0xD6,0x02,0x43};              // 11
static const uint8_t RX_SEQ_11[] = {0x03,0x20,0xD3,0x95,0xF0};                                             // 5  ← heartbeat echo
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

// Steady-state lock (sent for all polls once unique telemetry is exhausted)
static const uint8_t RX_STEADY[]   = {0x03,0x48,0x09,0x02,0x00,0x1F,0x08,0x0C,0x6A,0xBD,0xE8,0xFF};       // 12

// Shutdown ACK
static const uint8_t RX_SHUTDOWN[] = {0x01};                                                                 // 1

// ---------------------------------------------------------------------------
// Lookup tables — pointer + length + label, indexed 0–44
// ---------------------------------------------------------------------------
static const uint8_t * const RX_TABLE[] = {
    RX_SEQ_0,   RX_SEQ_1,   RX_SEQ_2,   RX_SEQ_3,   RX_SEQ_4,
    RX_SEQ_5,   RX_SEQ_6,   RX_SEQ_7,   RX_SEQ_8,   RX_SEQ_9,
    RX_SEQ_10,  RX_SEQ_11,  RX_SEQ_12,  RX_SEQ_13,  RX_SEQ_14,
    RX_SEQ_15,  RX_SEQ_16,  RX_SEQ_17,  RX_SEQ_18,  RX_SEQ_19,
    RX_SEQ_20,  RX_SEQ_21,  RX_SEQ_22,  RX_SEQ_23,  RX_SEQ_24,
    RX_SEQ_25,  RX_SEQ_26,  RX_SEQ_27,  RX_SEQ_28,  RX_SEQ_29,
    RX_SEQ_30,  RX_SEQ_31,  RX_SEQ_32,  RX_SEQ_33,  RX_SEQ_34,
    RX_SEQ_35,  RX_SEQ_36,  RX_SEQ_37,
    RX_STEADY,  RX_STEADY,  RX_STEADY,              // 38–40
    RX_STEADY,  RX_STEADY,  RX_STEADY,              // 41–43
    RX_SHUTDOWN                                      // 44
};

static const uint8_t RX_LENS[] = {
     4, 10,  4,  6, 13,  5,  5,                     // 0–6
    12, 12, 11, 11,  5, 11, 12, 11, 11, 12, 11, 11, // 7–18
    11, 12, 12, 11, 12, 12, 12, 11, 11, 11, 11, 11, // 19–30
    11, 11, 11, 11, 11, 12, 12,                      // 31–37
    12, 12, 12, 12, 12, 12,                          // 38–43
     1,                                              // 44
};

static const char* const RX_LABELS[] = {
    "announce",        // 0
    "device ID",       // 1
    "config ACK",      // 2
    "multi-reg ACK",   // 3
    "cal data",        // 4
    "cal ACK",         // 5
    "init complete",   // 6
    "telemetry #7",    // 7
    "telemetry #8",    // 8
    "telemetry #9",    // 9
    "telemetry #10",   // 10
    "heartbeat echo",  // 11
    "telemetry #12",   // 12
    "telemetry #13",   // 13
    "telemetry #14",   // 14
    "telemetry #15",   // 15
    "telemetry #16",   // 16
    "telemetry #17",   // 17
    "telemetry #18",   // 18
    "telemetry #19",   // 19
    "telemetry #20",   // 20
    "telemetry #21",   // 21
    "telemetry #22",   // 22
    "telemetry #23",   // 23
    "telemetry #24",   // 24
    "telemetry #25",   // 25
    "telemetry #26",   // 26
    "telemetry #27",   // 27
    "telemetry #28",   // 28
    "telemetry #29",   // 29
    "telemetry #30",   // 30
    "telemetry #31",   // 31
    "telemetry #32",   // 32
    "telemetry #33",   // 33
    "telemetry #34",   // 34
    "telemetry #35",   // 35
    "telemetry #36",   // 36
    "telemetry #37",   // 37
    "steady",          // 38
    "steady",          // 39
    "steady",          // 40
    "steady",          // 41
    "steady",          // 42
    "steady",          // 43
    "shutdown ACK",    // 44
};

static_assert(sizeof(RX_TABLE)  / sizeof(RX_TABLE[0])  == 45, "RX_TABLE must have 45 entries");
static_assert(sizeof(RX_LENS)   / sizeof(RX_LENS[0])   == 45, "RX_LENS must have 45 entries");
static_assert(sizeof(RX_LABELS) / sizeof(RX_LABELS[0]) == 45, "RX_LABELS must have 45 entries");

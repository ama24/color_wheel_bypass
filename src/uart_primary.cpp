#include "uart_primary.h"
#include "config.h"
#include "packets.h"
#include "signals.h"
#include <Arduino.h>
#include "driver/gpio.h"

// ===========================================================================
//  uart_primary.cpp — SK01 UART state machine  (content-based matching)
//
//  Every packet received from the main board is matched against a known
//  pattern table. The correct reply is selected by content, not by counter,
//  so the spoofer stays correct even if the main board repeats or reorders.
//
//  All activity is logged to USB serial (Serial / UART0) with millisecond
//  timestamps and full hex dumps so boot sequences are easy to follow.
//
//  Serial format:
//    [  3142 ms]  ←  device query   03 01 9D 2D E2
//    [  3143 ms]  →  device ID      03 42 AB 6F 44 01 01 0D 63 B0
// ===========================================================================

static SemaphoreHandle_t s_ldpcn_sem = nullptr;

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------

static void fmt_hex(const uint8_t* buf, uint8_t n, char* out, size_t sz)
{
    size_t pos = 0;
    for (uint8_t i = 0; i < n && pos + 3 < sz; i++) {
        if (i) out[pos++] = ' ';
        snprintf(out + pos, sz - pos, "%02X", buf[i]);
        pos += 2;
    }
    out[pos] = '\0';
}

// Received from main board
static void log_rx(const uint8_t* buf, uint8_t n, const char* label)
{
    char hex[64];
    fmt_hex(buf, n, hex, sizeof(hex));
    Serial.printf("[%7lu ms]  ←  %-18s  %s\n", millis(), label, hex);
}

// Transmitted to main board
static void log_tx(const uint8_t* buf, uint8_t n, const char* label)
{
    char hex[64];
    fmt_hex(buf, n, hex, sizeof(hex));
    Serial.printf("[%7lu ms]  →  %-18s  %s\n", millis(), label, hex);
}

static void log_event(const char* msg)
{
    Serial.printf("[%7lu ms]  %s\n", millis(), msg);
}

// ---------------------------------------------------------------------------
// Packet matching
// ---------------------------------------------------------------------------

// Returns the first matching TxMatch entry or nullptr.
static const TxMatch* match_pkt(const uint8_t* buf, uint8_t n,
                                 const TxMatch* table, uint8_t table_len)
{
    for (uint8_t i = 0; i < table_len; i++) {
        if (n >= table[i].pat_len &&
            memcmp(buf, table[i].pat, table[i].pat_len) == 0) {
            return &table[i];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// UART helpers
// ---------------------------------------------------------------------------

static uint8_t read_packet(uint8_t* buf, uint8_t max_len)
{
    uint8_t n = 0;
    while (!Serial1.available()) vTaskDelay(1 / portTICK_PERIOD_MS);
    while (n < max_len) {
        if (Serial1.available()) {
            buf[n++] = (uint8_t)Serial1.read();
        } else {
            vTaskDelay(UART_PKT_TIMEOUT_MS / portTICK_PERIOD_MS);
            if (!Serial1.available()) break;
        }
    }
    return n;
}

static uint8_t read_packet_timeout(uint8_t* buf, uint8_t max_len,
                                    uint32_t first_byte_timeout_ms)
{
    uint32_t waited = 0;
    while (!Serial1.available()) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        if (++waited >= first_byte_timeout_ms) return 0;
    }
    return read_packet(buf, max_len);
}

// Send a packet and log it.
static void send_pkt(const uint8_t* data, uint8_t len, const char* label)
{
    Serial1.write(data, len);
    Serial1.flush();
    log_tx(data, len, label);
}

// ---------------------------------------------------------------------------
// Lamp on / off
// ---------------------------------------------------------------------------

static void lamp_on()
{
    gpio_set_level(static_cast<gpio_num_t>(PIN_LLITZ),   1);
    ldup_start();
    gpio_set_level(static_cast<gpio_num_t>(PIN_PHSENSE), 1);
    log_event("▲  LLITZ ON   LDUP ON   PHSENSE ON");
}

static void lamp_off()
{
    gpio_set_level(static_cast<gpio_num_t>(PIN_LLITZ),   0);
    ldup_stop();
    sens_stop();
    gpio_set_level(static_cast<gpio_num_t>(PIN_PHSENSE), 0);
    log_event("▼  LLITZ OFF  LDUP OFF  PHSENSE OFF");
}

// ---------------------------------------------------------------------------
// Main task
// ---------------------------------------------------------------------------

static void uart_task(void* /*arg*/)
{
    uint8_t buf[32];
    uint8_t n;

    for (;;) {

        // ══════════════════════════════════════════════════════════════════
        //  IDLE — wait for LDPCN rising edge
        // ══════════════════════════════════════════════════════════════════
        Serial.printf("\n┌─────────────────────────────────────┐\n");
        Serial.printf("│  DLP SPOOFER  waiting for LDPCN     │\n");
        Serial.printf("└─────────────────────────────────────┘\n\n");

        xSemaphoreTake(s_ldpcn_sem, portMAX_DELAY);
        log_event("LDPCN ↑");

        // Flush any bytes that arrived before we were ready
        while (Serial1.available()) Serial1.read();

        // ══════════════════════════════════════════════════════════════════
        //  ANNOUNCE
        // ══════════════════════════════════════════════════════════════════
        Serial.printf("\n── ANNOUNCE ─────────────────────────\n");
        sens_start();
        log_event("SENS started (~120 Hz)");
        send_pkt(RX_TABLE[0], RX_LENS[0], RX_LABELS[0]);

        // ══════════════════════════════════════════════════════════════════
        //  INIT BURST — 6 matched exchanges
        // ══════════════════════════════════════════════════════════════════
        Serial.printf("\n── INIT BURST ───────────────────────\n");
        bool init_ok = true;

        for (uint8_t step = 0; step < 6; step++) {
            n = read_packet(buf, sizeof(buf));

            if (n == 0) {
                Serial.printf("[%7lu ms]  ✗  timeout at init step %u\n",
                              millis(), step);
                init_ok = false;
                break;
            }

            // Match incoming packet against known init queries
            const TxMatch* m = match_pkt(buf, n,
                                          TX_INIT_TABLE, TX_INIT_TABLE_LEN);

            if (m) {
                log_rx(buf, n, m->label);
                send_pkt(RX_TABLE[m->rx_idx], RX_LENS[m->rx_idx],
                         RX_LABELS[m->rx_idx]);
            } else {
                // Unrecognised packet — log the raw bytes and fall back to
                // the sequential response for this step so we don't stall.
                log_rx(buf, n, "??? UNRECOGNISED");
                uint8_t fallback = step + 1;
                Serial.printf("[%7lu ms]  ⚠  no match — fallback to [%u]\n",
                              millis(), fallback);
                send_pkt(RX_TABLE[fallback], RX_LENS[fallback],
                         RX_LABELS[fallback]);
            }
        }

        if (!init_ok) {
            lamp_off();
            continue;  // back to IDLE
        }

        // ══════════════════════════════════════════════════════════════════
        //  LAMP ON
        // ══════════════════════════════════════════════════════════════════
        Serial.printf("\n── LAMP ON ──────────────────────────\n");
        lamp_on();

        // ══════════════════════════════════════════════════════════════════
        //  STEADY STATE — content-matched poll/response loop
        //
        //  telem_idx tracks which unique telemetry packet to send next.
        //  Range 7–37: unique packets. Once exhausted, lock to RX_STEADY.
        //  Heartbeat (03 20 D3 95 F0) always gets RX_SEQ_11 echoed back
        //  and does NOT advance telem_idx.
        // ══════════════════════════════════════════════════════════════════
        Serial.printf("\n── STEADY STATE ─────────────────────\n");

        uint8_t telem_idx = 7;
        bool    running   = true;

        while (running) {
            n = read_packet_timeout(buf, sizeof(buf), POLL_INTERVAL_MS);

            // ── Poll timeout ─────────────────────────────────────────────
            if (n == 0) {
                log_event("⏱  poll timeout");
                send_pkt(RX_STEADY, sizeof(RX_STEADY), "steady (timeout)");
                continue;
            }

            // ── Graceful shutdown: 02 00 ──────────────────────────────────
            if (n >= 2 && buf[0] == TX_SHUTDOWN[0] && buf[1] == TX_SHUTDOWN[1]) {
                log_rx(buf, n, "graceful shutdown");
                send_pkt(RX_SHUTDOWN, sizeof(RX_SHUTDOWN), RX_LABELS[44]);
                running = false;
                break;
            }

            // ── Abnormal shutdown: 00 00 ─────────────────────────────────
            if (n >= 2 && buf[0] == TX_SHUTDOWN_ABN[0] &&
                          buf[1] == TX_SHUTDOWN_ABN[1]) {
                log_rx(buf, n, "abnormal shutdown");
                uint8_t ack = 0x00;
                Serial1.write(ack);
                Serial1.flush();
                log_tx(&ack, 1, "shutdown ACK");
                running = false;
                break;
            }

            // ── Match against steady-state poll table ────────────────────
            const TxMatch* m = match_pkt(buf, n,
                                          TX_STEADY_TABLE, TX_STEADY_TABLE_LEN);
            log_rx(buf, n, m ? m->label : "unknown poll");

            // Heartbeat: always echo RX_SEQ_11, do not advance telem_idx
            if (m && m->rx_idx == 11) {
                send_pkt(RX_TABLE[11], RX_LENS[11], RX_LABELS[11]);
                continue;
            }

            // Regular poll: send next unique telemetry, then lock to steady
            if (telem_idx <= 37) {
                send_pkt(RX_TABLE[telem_idx], RX_LENS[telem_idx],
                         RX_LABELS[telem_idx]);
                telem_idx++;
                if (telem_idx == 38) {
                    log_event("── telemetry exhausted — locked to steady ──");
                }
            } else {
                send_pkt(RX_STEADY, sizeof(RX_STEADY), "steady");
            }
        }

        // ══════════════════════════════════════════════════════════════════
        //  SHUTDOWN
        // ══════════════════════════════════════════════════════════════════
        Serial.printf("\n── SHUTDOWN ─────────────────────────\n");
        lamp_off();
    }
}

// ---------------------------------------------------------------------------
void uart_primary_init(SemaphoreHandle_t ldpcn_sem)
{
    s_ldpcn_sem = ldpcn_sem;

    // USB serial for debug output.
    // Do NOT block waiting for USB enumeration — LDPCN fires ~3 s after
    // projector power-on and we must not delay task creation past that point.
    // When running without USB the output is silently discarded; connect a
    // serial monitor after power-on to catch the boot log.
    Serial.begin(115200);

    // UART1: 19200 baud 8N1, TX=GP3 (RX0LD), RX=GP4 (TX0LD)
    Serial1.begin(PRIMARY_UART_BAUD, SERIAL_8N1, PIN_TX0LD_RX, PIN_RX0LD_TX);

    // LLITZ and PHSENSE outputs, start LOW
    gpio_config_t cfg = {};
    cfg.pin_bit_mask  = (1ULL << PIN_LLITZ) | (1ULL << PIN_PHSENSE);
    cfg.mode          = GPIO_MODE_OUTPUT;
    cfg.pull_up_en    = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(static_cast<gpio_num_t>(PIN_LLITZ),   0);
    gpio_set_level(static_cast<gpio_num_t>(PIN_PHSENSE), 0);

    xTaskCreatePinnedToCore(
        uart_task,
        "uart_primary",
        UART_TASK_STACK,
        nullptr,
        UART_TASK_PRI,
        nullptr,
        UART_TASK_CORE
    );
}

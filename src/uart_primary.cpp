#include "uart_primary.h"
#include "config.h"
#include "packets.h"
#include "signals.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_log.h"

// ===========================================================================
//  uart_primary.cpp — SK01 UART state machine
//
//  States:
//    IDLE         — blocked on binary semaphore from LDPCN ISR
//    ANNOUNCE     — send RX_SEQ[0] immediately, then wait for init
//    INIT_BURST   — 6 TX→RX exchanges: init_idx 0→5, rx_idx 1→6
//    LLITZ_ON     — assert LLITZ + LDUP + PHSENSE, enter steady state
//    STEADY_STATE — play through RX sequence, then lock to RX_STEADY
//    SHUTDOWN     — de-assert signals, → IDLE
//
//  Packet framing: read bytes until UART_PKT_TIMEOUT_MS silence.
// ===========================================================================

static const char* TAG = "uart_primary";

static SemaphoreHandle_t s_ldpcn_sem = nullptr;

// ---------------------------------------------------------------------------
// Read one packet into buf. Returns byte count, 0 on timeout with no bytes.
// Blocks until first byte arrives (no timeout on first byte — the 490ms
// poll interval acts as the outer loop guard).
// ---------------------------------------------------------------------------
static uint8_t read_packet(uint8_t* buf, uint8_t max_len)
{
    uint8_t n = 0;

    // Wait for first byte (blocking)
    while (!Serial1.available()) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    while (n < max_len) {
        if (Serial1.available()) {
            buf[n++] = (uint8_t)Serial1.read();
        } else {
            // Inter-byte gap: wait UART_PKT_TIMEOUT_MS for next byte
            vTaskDelay(UART_PKT_TIMEOUT_MS / portTICK_PERIOD_MS);
            if (!Serial1.available()) break;  // silence = end of packet
        }
    }
    return n;
}

// Same as above but with a first-byte timeout (ms). Returns 0 on timeout.
static uint8_t read_packet_timeout(uint8_t* buf, uint8_t max_len, uint32_t first_byte_timeout_ms)
{
    uint32_t waited = 0;
    while (!Serial1.available()) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        waited++;
        if (waited >= first_byte_timeout_ms) return 0;
    }
    return read_packet(buf, max_len);
}

// ---------------------------------------------------------------------------
// Send a packet from the RX lookup table by index
// ---------------------------------------------------------------------------
static inline void send_rx(uint8_t idx)
{
    Serial1.write(RX_TABLE[idx], RX_LENS[idx]);
    Serial1.flush();
    ESP_LOGD(TAG, "TX RX[%u] (%u bytes)", idx, RX_LENS[idx]);
}

// ---------------------------------------------------------------------------
// Send the steady-state packet directly (used when locked)
// ---------------------------------------------------------------------------
static inline void send_steady()
{
    Serial1.write(RX_STEADY, sizeof(RX_STEADY));
    Serial1.flush();
}

// ---------------------------------------------------------------------------
// Assert/de-assert lamp-on signals
// ---------------------------------------------------------------------------
static void lamp_on()
{
    gpio_set_level(static_cast<gpio_num_t>(PIN_LLITZ),   1);
    ldup_start();
    gpio_set_level(static_cast<gpio_num_t>(PIN_PHSENSE), 1);
    ESP_LOGI(TAG, "LLITZ + LDUP + PHSENSE asserted");
}

static void lamp_off()
{
    gpio_set_level(static_cast<gpio_num_t>(PIN_LLITZ),   0);
    ldup_stop();
    sens_stop();
    gpio_set_level(static_cast<gpio_num_t>(PIN_PHSENSE), 0);
    ESP_LOGI(TAG, "LLITZ + LDUP + PHSENSE de-asserted");
}

// ---------------------------------------------------------------------------
// Main task
// ---------------------------------------------------------------------------
static void uart_task(void* /*arg*/)
{
    uint8_t buf[32];
    uint8_t n;

    for (;;) {
        // ── IDLE ────────────────────────────────────────────────────────────
        ESP_LOGI(TAG, "IDLE — waiting for LDPCN");
        xSemaphoreTake(s_ldpcn_sem, portMAX_DELAY);
        ESP_LOGI(TAG, "LDPCN HIGH — beginning announce");

        // Flush any stale bytes
        while (Serial1.available()) Serial1.read();

        // ── ANNOUNCE ────────────────────────────────────────────────────────
        sens_start();
        send_rx(0);  // 03 40 08 F0
        ESP_LOGI(TAG, "ANNOUNCE sent");

        // ── INIT_BURST ──────────────────────────────────────────────────────
        uint8_t rx_idx = 1;
        bool init_ok = true;

        for (uint8_t init_step = 0; init_step < 6; init_step++) {
            n = read_packet(buf, sizeof(buf));
            if (n == 0) {
                ESP_LOGW(TAG, "INIT timeout at step %u", init_step);
                init_ok = false;
                break;
            }
            ESP_LOGD(TAG, "INIT RX step %u: %u bytes, first=0x%02X", init_step, n, buf[0]);
            send_rx(rx_idx++);
        }

        if (!init_ok) {
            lamp_off();
            continue;  // back to IDLE
        }

        ESP_LOGI(TAG, "INIT_BURST complete — asserting LLITZ");

        // ── LLITZ_ON ────────────────────────────────────────────────────────
        lamp_on();

        // ── STEADY_STATE ────────────────────────────────────────────────────
        bool running = true;
        while (running) {
            n = read_packet_timeout(buf, sizeof(buf), POLL_INTERVAL_MS);

            if (n == 0) {
                // Poll timeout — send steady-state proactively
                send_steady();
                continue;
            }

            // Shutdown command: 02 00
            if (n >= 2 && buf[0] == 0x02 && buf[1] == 0x00) {
                ESP_LOGI(TAG, "Graceful shutdown received");
                // Send shutdown ACK
                Serial1.write(RX_SHUTDOWN, sizeof(RX_SHUTDOWN));
                Serial1.flush();
                running = false;
                break;
            }

            // Abnormal shutdown: 00 00
            if (n >= 2 && buf[0] == 0x00 && buf[1] == 0x00) {
                ESP_LOGW(TAG, "Abnormal shutdown received (00 00)");
                Serial1.write((uint8_t)0x00);
                Serial1.flush();
                running = false;
                break;
            }

            // Send next RX response
            if (rx_idx < RX_STEADY_IDX) {
                send_rx(rx_idx++);
            } else {
                send_steady();
            }
        }

        // ── SHUTDOWN ────────────────────────────────────────────────────────
        lamp_off();
        ESP_LOGI(TAG, "SHUTDOWN complete — returning to IDLE");
    }
}

// ---------------------------------------------------------------------------
void uart_primary_init(SemaphoreHandle_t ldpcn_sem)
{
    s_ldpcn_sem = ldpcn_sem;

    // Configure UART1: 19200 baud, 8N1, TX=PIN_RX0LD_TX, RX=PIN_TX0LD_RX
    Serial1.begin(PRIMARY_UART_BAUD, SERIAL_8N1, PIN_TX0LD_RX, PIN_RX0LD_TX);

    // Configure LLITZ and PHSENSE as push-pull outputs, start LOW
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

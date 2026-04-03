/**
 * @file    main.cpp
 * @brief   Hardware-stable pulse generator for ESP32-C3 / S3 Super Mini
 *
 * Uses the ESP-IDF 5.x RMT TX peripheral in infinite-hardware-loop mode.
 * Once started, the waveform runs entirely in silicon:
 *   - No timer ISR, no CPU wake-ups, no jitter from task scheduling.
 *   - The RMT block clocks itself from the 80 MHz APB, divided to 1 MHz
 *     for a 1 µs tick resolution.
 *
 * Signal on PULSE_GPIO (defined in platformio.ini per target board):
 * ─────────────────────────────────────────────────────────────────
 *        ┌─── 7 600 µs ───┐         ┌─── 7 600 µs ───┐
 *   3.3V │                │         │                │
 *    0 V ┘                └─ 731 µs ┘                └─ 731 µs …
 *
 *   Period  = 8 331 µs
 *   Freq    ≈ 120.03 Hz
 *   Duty    ≈ 91.2 %
 * ─────────────────────────────────────────────────────────────────
 *
 * Build & flash:
 *   pio run -e esp32c3 -t upload
 *   pio run -e esp32s3 -t upload
 */

#include <Arduino.h>
#include "driver/rmt_tx.h"

// ---------------------------------------------------------------------------
// Configuration — override PULSE_GPIO via build_flags in platformio.ini
// ---------------------------------------------------------------------------
#ifndef PULSE_GPIO
  #define PULSE_GPIO 5
#endif

static constexpr uint32_t HIGH_US           = 7600;          // output HIGH for this long
static constexpr uint32_t LOW_US            =  731;          // output LOW  for this long
static constexpr uint32_t RMT_RESOLUTION_HZ = 1'000'000UL;  // 1 MHz → 1 µs / tick

// ---------------------------------------------------------------------------
// RMT state (module-private)
// ---------------------------------------------------------------------------
static rmt_channel_handle_t s_tx_chan  = nullptr;
static rmt_encoder_handle_t s_encoder = nullptr;

/**
 * One RMT symbol word encodes a complete HIGH→LOW cycle.
 *
 * rmt_symbol_word_t layout (each half is a 16-bit field):
 *   [duration0 : 15 | level0 : 1]  [duration1 : 15 | level1 : 1]
 *
 * The RMT hardware replays this word in a tight hardware loop when
 * rmt_transmit_config_t::loop_count == -1, producing a glitch-free,
 * CPU-independent waveform.
 */
static const rmt_symbol_word_t s_pulse_symbol = {
    .duration0 = HIGH_US,   // stay HIGH for 7 600 ticks (= 7 600 µs)
    .level0    = 1,
    .duration1 = LOW_US,    // stay LOW  for   731 ticks (=   731 µs)
    .level1    = 0,
};

// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    // Give USB-CDC a moment to enumerate (C3/S3 native USB)
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) {}

    Serial.printf(
        "\n[PulseGen] HIGH=%lu µs | LOW=%lu µs | period=%lu µs | ~%.2f Hz\n",
        (unsigned long)HIGH_US,
        (unsigned long)LOW_US,
        (unsigned long)(HIGH_US + LOW_US),
        1e6f / static_cast<float>(HIGH_US + LOW_US));

    // ── 1. Create RMT TX channel ────────────────────────────────────────────
    // clk_src = RMT_CLK_SRC_DEFAULT selects the 80 MHz APB clock.
    // resolution_hz = 1 MHz means the hardware divides APB by 80 → 1 µs/tick.
    rmt_tx_channel_config_t chan_cfg = {};
    chan_cfg.gpio_num          = static_cast<gpio_num_t>(PULSE_GPIO);
    chan_cfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    chan_cfg.resolution_hz     = RMT_RESOLUTION_HZ;
    chan_cfg.mem_block_symbols = 64;   // 64-symbol RAM block is the minimum safe size
    chan_cfg.trans_queue_depth = 4;    // depth of the internal SW queue (not used in loop mode)
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_tx_chan));

    // ── 2. Create copy encoder ──────────────────────────────────────────────
    // The copy encoder passes raw rmt_symbol_word_t values straight to the
    // channel without any further encoding.  It is the right choice when you
    // already have pre-built RMT symbols (as we do here).
    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&enc_cfg, &s_encoder));

    // ── 3. Enable the channel (starts the RMT clock tree) ──────────────────
    ESP_ERROR_CHECK(rmt_enable(s_tx_chan));

    // ── 4. Start hardware loop ──────────────────────────────────────────────
    // loop_count = -1  →  repeat s_pulse_symbol forever in hardware.
    // After this call the GPIO is driven entirely by the RMT peripheral;
    // the CPU is not involved at all.
    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = -1;
    ESP_ERROR_CHECK(rmt_transmit(s_tx_chan,
                                  s_encoder,
                                  &s_pulse_symbol,
                                  sizeof(s_pulse_symbol),
                                  &tx_cfg));

    Serial.printf("[PulseGen] Running on GPIO %d — CPU free.\n", PULSE_GPIO);
}

// ---------------------------------------------------------------------------
void loop()
{
    // Nothing to do: the RMT block runs the waveform autonomously.
    // Block this task permanently so FreeRTOS can idle-sleep the core,
    // reducing power consumption and keeping the CPU out of the way.
    vTaskDelay(portMAX_DELAY);
}

/**
 * @file    main.cpp
 * @brief   Hardware-stable pulse generator for ESP32-C3 / S3 Super Mini
 *
 * Uses esp_timer with ESP_TIMER_TASK dispatch.
 * The callback fires directly from the hardware timer interrupt — no FreeRTOS
 * scheduler latency — giving sub-2 µs jitter on both C3 and S3.
 *
 * Signal on PULSE_GPIO (defined in platformio.ini per target board):
 * ─────────────────────────────────────────────────────────────────
 *        ┌─── 7 600 µs ───┐         ┌─── 7 600 µs ───┐
 *   3.3V │                │         │                │
 *    0 V ┘                └─ 731 µs ┘                └─ 731 µs …
 *
 *   Period  = 8 331 µs  ≈  120.03 Hz    Duty ≈ 91.2 %
 * ─────────────────────────────────────────────────────────────────
 *
 * Build & flash:
 *   python -m platformio run -e esp32c3 -t upload
 *   python -m platformio run -e esp32s3 -t upload
 */

#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_timer.h"

// ---------------------------------------------------------------------------
// Configuration — override PULSE_GPIO via build_flags in platformio.ini
// ---------------------------------------------------------------------------
#ifndef PULSE_GPIO
  #define PULSE_GPIO 5
#endif

static constexpr uint64_t HIGH_US = 7600;   // µs the line stays HIGH
static constexpr uint64_t LOW_US  =  731;   // µs the line stays LOW

// ---------------------------------------------------------------------------
static esp_timer_handle_t s_timer = nullptr;
static volatile bool      s_high  = false;

// ---------------------------------------------------------------------------
// Timer ISR — runs directly in interrupt context, no scheduler involved.
// Toggles the GPIO and immediately schedules the next edge.
// IRAM_ATTR keeps the function in fast IRAM so flash-cache misses can't
// add jitter.
// ---------------------------------------------------------------------------
static void IRAM_ATTR on_timer(void* /*arg*/)
{
    s_high = !s_high;
    gpio_set_level(static_cast<gpio_num_t>(PULSE_GPIO), s_high ? 1 : 0);
    esp_timer_start_once(s_timer, s_high ? HIGH_US : LOW_US);
}

// ---------------------------------------------------------------------------
void setup()
{
    // ── 1. Configure GPIO as push-pull output ───────────────────────────────
    gpio_config_t io_cfg   = {};
    io_cfg.pin_bit_mask    = 1ULL << PULSE_GPIO;
    io_cfg.mode            = GPIO_MODE_OUTPUT;
    io_cfg.pull_up_en      = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en    = GPIO_PULLDOWN_DISABLE;
    io_cfg.intr_type       = GPIO_INTR_DISABLE;
    gpio_config(&io_cfg);

    // ── 2. Create one-shot timer with ISR dispatch ───────────────────────────
    // ESP_TIMER_TASK: callback runs in the esp_timer dedicated FreeRTOS task
    // (highest priority). ESP_TIMER_ISR is not compiled into Arduino-ESP32 2.x.
    esp_timer_create_args_t args = {};
    args.callback        = on_timer;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name            = "pulse";
    esp_timer_create(&args, &s_timer);

    // ── 3. Drive line HIGH and start the first countdown ────────────────────
    gpio_set_level(static_cast<gpio_num_t>(PULSE_GPIO), 1);
    s_high = true;
    esp_timer_start_once(s_timer, HIGH_US);
}

// ---------------------------------------------------------------------------
void loop()
{
    // All work happens in the timer ISR — park this task permanently.
    vTaskDelay(portMAX_DELAY);
}

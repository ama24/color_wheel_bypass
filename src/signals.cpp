#include "signals.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// ===========================================================================
//  signals.cpp — SENS and LDUP esp_timer state machines
//
//  Both timers use ESP_TIMER_TASK dispatch (ESP_TIMER_ISR is not compiled
//  into Arduino-ESP32 2.x). IRAM_ATTR keeps callbacks in fast IRAM so
//  flash-cache misses cannot add jitter.
// ===========================================================================

// ---------------------------------------------------------------------------
// SENS — colour-wheel index pulse
// HIGH 7600 µs / LOW 731 µs  ≈  120.03 Hz, 91.2% duty
// ---------------------------------------------------------------------------
static esp_timer_handle_t s_sens_timer  = nullptr;
static volatile bool      s_sens_high   = false;
static volatile bool      s_sens_active = false;

static void IRAM_ATTR sens_cb(void* /*arg*/)
{
    if (!s_sens_active) return;
    s_sens_high = !s_sens_high;
    gpio_set_level(static_cast<gpio_num_t>(PIN_SENS), s_sens_high ? 1 : 0);
    esp_timer_start_once(s_sens_timer, s_sens_high ? SENS_HIGH_US : SENS_LOW_US);
}

void sens_start()
{
    if (s_sens_active) return;

    if (!s_sens_timer) {
        esp_timer_create_args_t args = {};
        args.callback        = sens_cb;
        args.dispatch_method = ESP_TIMER_TASK;
        args.name            = "sens";
        esp_timer_create(&args, &s_sens_timer);
    }

    // Configure GPIO
    gpio_config_t cfg   = {};
    cfg.pin_bit_mask    = 1ULL << PIN_SENS;
    cfg.mode            = GPIO_MODE_OUTPUT;
    cfg.pull_up_en      = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en    = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type       = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    s_sens_active = true;
    gpio_set_level(static_cast<gpio_num_t>(PIN_SENS), 1);
    s_sens_high = true;
    esp_timer_start_once(s_sens_timer, SENS_HIGH_US);
}

void sens_stop()
{
    s_sens_active = false;
    if (s_sens_timer) esp_timer_stop(s_sens_timer);
    gpio_set_level(static_cast<gpio_num_t>(PIN_SENS), 0);
}

// ---------------------------------------------------------------------------
// LDUP — NOT USED (v1.3 direction correction)
//
// SK01 pin 10 is a MAIN BOARD OUTPUT (~42 kHz PWM). The functions below are
// retained for reference but ldup_start() / ldup_stop() are never called.
// GPIO8 is intentionally left unconfigured (high-impedance input) so it does
// not conflict with the main board's driver.
//
// Original phase table (µs) — reference only:
//   0: HIGH P0_H  → 1
//   1: LOW  P1_L  → 2   (one-time start pulse)
//   2: HIGH P2_H  → 3  ┐
//   3: LOW  P3_L  → 4  │
//   4: HIGH P4_H  → 5  ├─ repeating loop
//   5: LOW  P5_L  → 6  │
//   6: HIGH P6_H  → 7  │
//   7: LOW  P7_L  → 2  ┘
// ---------------------------------------------------------------------------
static const uint64_t LDUP_PHASE_US[8] = {
    LDUP_P0_H, LDUP_P1_L,
    LDUP_P2_H, LDUP_P3_L,
    LDUP_P4_H, LDUP_P5_L,
    LDUP_P6_H, LDUP_P7_L,
};
// Phase 0,2,4,6 are HIGH; 1,3,5,7 are LOW
static const uint8_t LDUP_LEVEL[8] = {1, 0, 1, 0, 1, 0, 1, 0};

static esp_timer_handle_t s_ldup_timer  = nullptr;
static volatile uint8_t   s_ldup_phase  = 0;
static volatile bool      s_ldup_active = false;

static void IRAM_ATTR ldup_cb(void* /*arg*/)
{
    if (!s_ldup_active) return;

    // Advance to the next phase FIRST, then drive the GPIO and schedule its
    // duration.  The timer that fired was for the phase we just finished;
    // we now enter the next one.
    if (s_ldup_phase == 7) {
        s_ldup_phase = 2;   // wrap: repeating loop restarts at phase 2
    } else {
        s_ldup_phase++;
    }

    gpio_set_level(static_cast<gpio_num_t>(PIN_LDUP), LDUP_LEVEL[s_ldup_phase]);
    esp_timer_start_once(s_ldup_timer, LDUP_PHASE_US[s_ldup_phase]);
}

void ldup_start()
{
    if (s_ldup_active) return;

    if (!s_ldup_timer) {
        esp_timer_create_args_t args = {};
        args.callback        = ldup_cb;
        args.dispatch_method = ESP_TIMER_TASK;
        args.name            = "ldup";
        esp_timer_create(&args, &s_ldup_timer);
    }

    gpio_config_t cfg   = {};
    cfg.pin_bit_mask    = 1ULL << PIN_LDUP;
    cfg.mode            = GPIO_MODE_OUTPUT;
    cfg.pull_up_en      = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en    = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type       = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    s_ldup_active = true;
    s_ldup_phase  = 0;

    // Start at phase 0: drive HIGH, schedule transition to phase 1
    gpio_set_level(static_cast<gpio_num_t>(PIN_LDUP), 1);
    esp_timer_start_once(s_ldup_timer, LDUP_PHASE_US[0]);
}

void ldup_stop()
{
    s_ldup_active = false;
    if (s_ldup_timer) esp_timer_stop(s_ldup_timer);
    gpio_set_level(static_cast<gpio_num_t>(PIN_LDUP), 0);
}

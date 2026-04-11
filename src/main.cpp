/**
 * @file    main.cpp
 * @brief   DLP Projector Spoofer — ESP32-S3 Super Mini
 *
 * Emulates the light source control PCB (SK01) and I2C temperature sensor
 * (SK05) of a DLP projector to allow the original light engine to be replaced
 * with a TTL-controlled 405 nm UV laser for photolithography.
 *
 * Subsystems spoofed:
 *   SK01 — 13-pin control PCB connector
 *     PRSV    : hardware pull-up (10 kΩ to V5P0LD) — no GPIO
 *     SENS    : ~120 Hz colour-wheel index pulse (GPIO5, esp_timer)
 *     LDUP    : multi-phase light-output PWM (GPIO8, esp_timer)
 *     LLITZ   : intensity reference HIGH (GPIO7, driven from Task A)
 *     PHSENSE : photo sense stub ~2.87 V (GPIO10 via resistor divider)
 *     RX0LD   : full UART replay at 19200 baud 8N1 (UART1 TX, GPIO15)
 *     TX0LD   : receive main-board commands (UART1 RX, GPIO16)
 *
 *   SK05 — 20-pin thermal board (fully removed)
 *     I2C 0x4D : temperature sensor emulation (I2C0 GPIO11/12)
 *     I2C 0x41 : fan controller emulation    (I2C1 GPIO13/14 bridged to GPIO11/12)
 *     I2C 0x54 : EEPROM — original IC kept, wired standalone to GPIO11/12
 *     FG1/FG2  : fan tachometer outputs driven LOW (GPIO1/2)
 *
 * Build: VS Code PlatformIO → esp32s3 → Build / Upload
 */

#include <Arduino.h>
#include "config.h"
#include "signals.h"
#include "uart_primary.h"
#include "i2c_slave.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char* TAG = "main";

// Binary semaphore: given by LDPCN ISR, taken by uart_primary task
static SemaphoreHandle_t s_ldpcn_sem = nullptr;

// ---------------------------------------------------------------------------
// LDPCN ISR — fires on rising edge of SK01 pin 4
// Wakes the UART task without scheduler latency.
// ---------------------------------------------------------------------------
static void IRAM_ATTR ldpcn_isr(void* /*arg*/)
{
    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_ldpcn_sem, &higher_prio_woken);
    if (higher_prio_woken) portYIELD_FROM_ISR();
}

// ---------------------------------------------------------------------------
void setup()
{
    // ── 1. LDPCN input with rising-edge interrupt ────────────────────────
    gpio_config_t ldpcn_cfg   = {};
    ldpcn_cfg.pin_bit_mask    = 1ULL << PIN_LDPCN;
    ldpcn_cfg.mode            = GPIO_MODE_INPUT;
    ldpcn_cfg.pull_up_en      = GPIO_PULLUP_DISABLE;
    ldpcn_cfg.pull_down_en    = GPIO_PULLDOWN_ENABLE;  // pull LOW when idle
    ldpcn_cfg.intr_type       = GPIO_INTR_POSEDGE;
    gpio_config(&ldpcn_cfg);

    // ── 2. Create RTOS primitives ────────────────────────────────────────
    s_ldpcn_sem = xSemaphoreCreateBinary();

    // ── 3. Install GPIO ISR service and attach LDPCN handler ────────────
    gpio_install_isr_service(0);
    gpio_isr_handler_add(static_cast<gpio_num_t>(PIN_LDPCN), ldpcn_isr, nullptr);

    // ── 4. FG1/FG2 — fan tachometer outputs, drive LOW ──────────────────
    // Main board does not read tachometer signals (confirmed from captures).
    // Drive LOW to avoid floating-pin noise on SK05 pins 18/19.
    gpio_config_t fg_cfg   = {};
    fg_cfg.pin_bit_mask    = (1ULL << PIN_FG1) | (1ULL << PIN_FG2);
    fg_cfg.mode            = GPIO_MODE_OUTPUT;
    fg_cfg.pull_up_en      = GPIO_PULLUP_DISABLE;
    fg_cfg.pull_down_en    = GPIO_PULLDOWN_DISABLE;
    fg_cfg.intr_type       = GPIO_INTR_DISABLE;
    gpio_config(&fg_cfg);
    gpio_set_level(static_cast<gpio_num_t>(PIN_FG1), 0);
    gpio_set_level(static_cast<gpio_num_t>(PIN_FG2), 0);

    // ── 5. Start I2C slaves (0x4D temp sensor + 0x41 fan controller) ────
    i2c_slave_init();

    // ── 6. Spawn UART primary task (Core 0) ─────────────────────────────
    uart_primary_init(s_ldpcn_sem);

    ESP_LOGI(TAG, "Spoofer ready — waiting for LDPCN HIGH");
}

// ---------------------------------------------------------------------------
void loop()
{
    // All work happens in the UART task and ISR/timer callbacks.
    vTaskDelay(portMAX_DELAY);
}

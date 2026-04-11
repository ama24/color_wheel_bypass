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
 *   SK05 — 20-pin thermal board connector
 *     I2C 0x4D : temperature sensor emulation (GPIO35/36)
 *                Physical mod: remove 0x4D IC, wire ESP32 SDA/SCL in its place.
 *                Keep EEPROM (0x54) and 0x41 peripheral on thermal board.
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

    // ── 4. Start I2C slave (0x4D temperature sensor emulation) ──────────
    i2c_slave_init();

    // ── 5. Spawn UART primary task (Core 0) ─────────────────────────────
    uart_primary_init(s_ldpcn_sem);

    ESP_LOGI(TAG, "Spoofer ready — waiting for LDPCN HIGH");
}

// ---------------------------------------------------------------------------
void loop()
{
    // All work happens in the UART task and ISR/timer callbacks.
    vTaskDelay(portMAX_DELAY);
}

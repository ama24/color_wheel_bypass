#include "i2c_slave.h"
#include "config.h"
#include <Wire.h>
#include "esp_log.h"

// ===========================================================================
//  i2c_slave.cpp — Emulates I2C temperature sensor at address 0x4D (SK05)
//
//  Protocol (observed from I2C capture):
//    Main board writes 1 byte (register address), then reads 1 byte.
//    Polling interval: ~510 ms, 4 channels per cycle.
//
//  Register map:
//    0x3E → Ch1 MSB (°C integer)   0x3F → Ch1 LSB (fractional, 0x80 = 0.5°C)
//    0x4E → Ch2 MSB                0x4F → Ch2 LSB
//    0x5E → Ch3 MSB  ← CRITICAL   0x5F → Ch3 LSB  (NEVER return 0xFF on 0x5E)
//    0x6E → Ch4 MSB                0x6F → Ch4 LSB
//
//  Boot init also writes 0x2B to alert-threshold config regs 0x32/0x42/0x52/0x62.
//  Those writes are silently accepted; default: handler returns 0x00.
//
//  Ch3 (0x5E) is the laser heatsink sensor. If it returns 0xFF the main board
//  initiates thermal shutdown within ~10 seconds.
// ===========================================================================

static const char* TAG = "i2c_slave";
static volatile uint8_t s_reg = 0x00;

static void on_receive(int /*n_bytes*/)
{
    if (Wire.available()) {
        s_reg = (uint8_t)Wire.read();
    }
    // Drain any unexpected extra bytes
    while (Wire.available()) Wire.read();
}

static void on_request()
{
    uint8_t val;
    switch (s_reg) {
        case 0x3E: val = TEMP_CH1_MSB; break;   // Ch1 integer  — 46 °C
        case 0x3F: val = 0x80;         break;   // Ch1 fraction — +0.5 °C
        case 0x4E: val = TEMP_CH2_MSB; break;   // Ch2 integer  — 47 °C
        case 0x4F: val = 0x80;         break;   // Ch2 fraction
        case 0x5E: val = TEMP_CH3_MSB; break;   // Ch3 integer  — 74 °C (NEVER 0xFF)
        case 0x5F: val = 0x80;         break;   // Ch3 fraction
        case 0x6E: val = TEMP_CH4_MSB; break;   // Ch4 integer  — 46 °C
        case 0x6F: val = 0x80;         break;   // Ch4 fraction
        default:   val = 0x00;         break;   // alert config / unknown regs
    }
    Wire.write(val);
    ESP_LOGD(TAG, "I2C reg 0x%02X → 0x%02X", s_reg, val);
}

void i2c_slave_init()
{
    // Wire.begin(sda, scl, address) — Arduino-ESP32 slave mode
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, (uint8_t)I2C_TEMP_ADDR);
    Wire.onReceive(on_receive);
    Wire.onRequest(on_request);
    ESP_LOGI(TAG, "I2C slave ready at 0x%02X (SDA=%d SCL=%d)",
             I2C_TEMP_ADDR, PIN_I2C_SDA, PIN_I2C_SCL);
}

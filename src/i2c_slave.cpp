#include "i2c_slave.h"
#include "config.h"
#include <Wire.h>
#include "esp_log.h"

// ===========================================================================
//  i2c_slave.cpp — I2C slave emulation for two SK05 devices
//
//  Two I2C controllers on the same physical bus (SK05 pins 6/7 = SCL/SDA):
//
//  I2C0  Wire  GPIO10/12  — address 0x4D  temperature sensor
//  I2C1  Wire1 GPIO11/13  — address 0x41  fan-speed controller
//
//  Hardware bridge required: GPIO10↔GPIO11 (SDA) and GPIO12↔GPIO13 (SCL)
//  via dupont jumpers. I2C is open-drain so wire-OR is safe.
//
//  0x54 EEPROM: physically on the MAIN BOARD (confirmed by SK05 disconnection
//  test — 291 ACKed transactions with thermal cable fully removed). No wiring
//  changes needed; it stays where it is.
//
// ---------------------------------------------------------------------------
//  0x4D — temperature sensor (4 channels, read via register address)
//
//    Main board writes 1-byte register address, then reads 1 byte.
//    Polling ~510 ms, 4 channels per cycle.
//
//    0x3E → Ch1 MSB (°C integer)   0x3F → Ch1 LSB (0x80 = +0.5°C)
//    0x4E → Ch2 MSB                0x4F → Ch2 LSB
//    0x5E → Ch3 MSB  ← CRITICAL   0x5F → Ch3 LSB  (NEVER return 0xFF)
//    0x6E → Ch4 MSB                0x6F → Ch4 LSB
//
//    Ch3 (0x5E) is the laser heatsink. 0xFF triggers thermal shutdown in ~10 s.
//
// ---------------------------------------------------------------------------
//  0x41 — fan-speed controller (write-only in observed traffic)
//
//    Main board writes reg 0x01=0xF8 and reg 0x02=0xF8 every ~2 s.
//    All observed transactions are writes; no reads. Slave ACKs all writes.
//
//    OPEN RISK: what happens when 0x41 NACKs is not yet known. The disconnection
//    test crashed on 0x4D before the main board ever tried 0x41, so longer-session
//    behaviour with 0x41 absent is untested. The ESP32 Wire1 slave here ACKs all
//    writes, removing that risk entirely.
// ===========================================================================

static const char* TAG = "i2c_slave";

// ---------------------------------------------------------------------------
// 0x4D — temperature sensor
// ---------------------------------------------------------------------------
static volatile uint8_t s_temp_reg = 0x00;

static void on_temp_receive(int /*n_bytes*/)
{
    if (Wire.available()) {
        s_temp_reg = (uint8_t)Wire.read();
    }
    while (Wire.available()) Wire.read();
}

static void on_temp_request()
{
    uint8_t val;
    switch (s_temp_reg) {
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
    // NOTE: do not call ESP_LOG here — onRequest fires from the I2C ISR and
    // ESP_LOGD acquires a mutex internally, which will panic in ISR context.
}

// ---------------------------------------------------------------------------
// 0x41 — fan-speed controller
// ---------------------------------------------------------------------------
static volatile uint8_t s_periph_reg = 0x00;

static void on_periph_receive(int /*n_bytes*/)
{
    if (Wire1.available()) {
        s_periph_reg = (uint8_t)Wire1.read();
    }
    while (Wire1.available()) Wire1.read();
    // NOTE: no ESP_LOG here — ISR context, mutex not safe.
}

static void on_periph_request()
{
    // No read requests observed in captured traffic — return 0x00 as safe default.
    Wire1.write((uint8_t)0x00);
}

// ---------------------------------------------------------------------------
void i2c_slave_init()
{
    // I2C0 — temperature sensor 0x4D
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, (uint8_t)I2C_TEMP_ADDR);
    Wire.onReceive(on_temp_receive);
    Wire.onRequest(on_temp_request);
    ESP_LOGI(TAG, "I2C0 slave ready at 0x%02X (SDA=GPIO%d SCL=GPIO%d)",
             I2C_TEMP_ADDR, PIN_I2C_SDA, PIN_I2C_SCL);

    // I2C1 — fan controller 0x41
    // GPIO10↔GPIO11 (SDA) and GPIO12↔GPIO13 (SCL) are bridged via dupont
    // jumpers so both controllers share the physical SK05 I2C bus.
    Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, (uint8_t)I2C_PERIPH_ADDR);
    Wire1.onReceive(on_periph_receive);
    Wire1.onRequest(on_periph_request);
    ESP_LOGI(TAG, "I2C1 slave ready at 0x%02X (SDA=GPIO%d SCL=GPIO%d)",
             I2C_PERIPH_ADDR, PIN_I2C1_SDA, PIN_I2C1_SCL);
}

#pragma once

// ===========================================================================
//  i2c_slave.h — I2C slave emulation for SK05 bus devices 0x4D and 0x41
//
//  Two I2C hardware controllers share the same physical SK05 bus:
//    I2C0 (Wire)  — GPIO11 SDA / GPIO12 SCL — slave at 0x4D (temp sensor)
//    I2C1 (Wire1) — GPIO13 SDA / GPIO14 SCL — slave at 0x41 (fan controller)
//
//  Hardware modification required:
//    1. Remove the 0x4D temperature sensor IC from the thermal board.
//    2. Remove the 0x41 fan controller IC from the thermal board.
//    3. Desolder the 0x54 EEPROM from the thermal board; wire it standalone
//       to GPIO11/12 (VCC=3.3V, GND, A0=GND, A1=GND, A2=3.3V → address 0x54).
//    4. Bridge GPIO13↔GPIO11 and GPIO14↔GPIO12 with jumper wires.
//       I2C is open-drain — both controllers on one bus is electrically safe.
//    5. Connect ESP32 GPIO11/12 to SK05 I2C bus pads.
// ===========================================================================

// Call once from setup(). Starts both I2C slaves.
void i2c_slave_init();

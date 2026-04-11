#pragma once

// ===========================================================================
//  i2c_slave.h — I2C slave emulating the 0x4D temperature sensor (SK05)
//
//  Physical modification required:
//    Remove the 0x4D temperature sensor IC from the thermal management board.
//    Solder wires from ESP32 GPIO PIN_I2C_SDA and PIN_I2C_SCL to the
//    SK05 I2C bus pads where 0x4D was mounted.
//    Keep EEPROM (0x54) and 0x41 peripheral on the thermal board.
// ===========================================================================

// Call once from setup(). Starts I2C slave on PIN_I2C_SDA / PIN_I2C_SCL.
void i2c_slave_init();

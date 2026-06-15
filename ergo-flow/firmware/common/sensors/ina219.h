/*
 * ErgoFlow — INA219 Current Sensor Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef INA219_H
#define INA219_H

#include <stdint.h>

#define INA219_ADDR_GND  0x40
#define INA219_ADDR_VDD  0x41
#define INA219_ADDR_SDA  0x42
#define INA219_ADDR_SCL  0x43

int ina219_init(uint8_t i2c_addr);
int ina219_read_current(float *current_ma);
int ina219_read_bus_voltage(float *voltage_v);
int ina219_read_power(float *power_mw);

#endif /* INA219_H */
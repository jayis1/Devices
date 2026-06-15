/*
 * ErgoFlow — I2C Bus Management Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize the I2C bus */
int i2c_bus_init(void);

/* Scan for all I2C devices (returns count found) */
int i2c_bus_scan(void);

/* Write to a register on an I2C device */
int i2c_bus_write_reg(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);

/* Read from a register on an I2C device */
int i2c_bus_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

/* Write a single byte to a register */
int i2c_bus_write_byte(uint8_t dev_addr, uint8_t reg, uint8_t val);

/* Read a single byte from a register */
int i2c_bus_read_byte(uint8_t dev_addr, uint8_t reg, uint8_t *val);

#endif /* I2C_BUS_H */
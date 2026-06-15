/*
 * ErgoFlow — SHT40 Temperature/Humidity Sensor Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef SHT40_H
#define SHT40_H

#include <stdint.h>

#define SHT40_ADDR  0x44   /* SHT40-AD1B default address */

int sht40_init(uint8_t i2c_addr);
int sht40_read(float *temp_c, float *humidity_pct);

#endif /* SHT40_H */
/*
 * ErgoFlow — MMC5603 Magnetometer Driver Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef MMC5603_H
#define MMC5603_H

#include <stdint.h>

#define MMC5603_I2C_ADDR  0x30

typedef struct {
    float x, y, z;  /* Magnetic field in mG */
} mmc5603_mag_t;

int mmc5603_init(uint8_t i2c_addr);
int mmc5603_read(mmc5603_mag_t *mag);

#endif /* MMC5603_H */
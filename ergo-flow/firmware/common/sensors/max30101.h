/*
 * ErgoFlow — MAX30101 Pulse Oximeter Driver Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef MAX30101_H
#define MAX30101_H

#include <stdint.h>

#define MAX30101_I2C_ADDR  0x57

int max30101_init(void);
int max30101_read_hr(float *hr_bpm, float *spo2_pct);
void max30101_shutdown(void);
void max30101_wakeup(void);

#endif /* MAX30101_H */
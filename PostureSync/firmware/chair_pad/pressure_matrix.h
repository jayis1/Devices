#ifndef PRESSURE_MATRIX_H
#define PRESSURE_MATRIX_H

#include <stdint.h>
#include "driver/i2c.h"

typedef struct {
    uint8_t left_pct;
    uint8_t right_pct;
    uint8_t pelvic_tilt;
    uint8_t ischial_contact;
    uint8_t posture_class;
    uint16_t weight_total;
} pressure_analysis_t;

void pressure_scan(uint16_t matrix[16][16], i2c_port_t i2c_port);
void pressure_analyze(const uint16_t matrix[16][16], pressure_analysis_t *out);

#endif
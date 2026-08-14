/*
 * PostureSync Smart Chair Pad — Pressure Matrix Scanner
 * 16×16 FSR array with MCP23017 GPIO expansion
 */

#include "pressure_matrix.h"
#include <string.h>

void pressure_scan(uint16_t matrix[16][16], i2c_port_t i2c_port)
{
    memset(matrix, 0, 16 * 16 * sizeof(uint16_t));

    for (int row = 0; row < 16; row++) {
        /* Select row on MCP23017 #1 */
        uint8_t reg = (row < 8) ? 0x12 : 0x13; /* OLATA or OLATB */
        uint8_t val = (row < 8) ? (1 << row) : (1 << (row - 8));
        uint8_t other_reg = (row < 8) ? 0x13 : 0x12;

        uint8_t buf[2] = {reg, val};
        i2c_master_write_to_device(i2c_port, 0x20, buf, 2, pdMS_TO_TICKS(10));
        buf[0] = other_reg; buf[1] = 0;
        i2c_master_write_to_device(i2c_port, 0x20, buf, 2, pdMS_TO_TICKS(10));

        /* Read columns from MCP23017 #2 */
        uint8_t cols_lo, cols_hi;
        uint8_t reg_a = 0x12;
        i2c_master_write_read_device(i2c_port, 0x21, &reg_a, 1, &cols_lo, 1, pdMS_TO_TICKS(10));
        uint8_t reg_b = 0x13;
        i2c_master_write_read_device(i2c_port, 0x21, &reg_b, 1, &cols_hi, 1, pdMS_TO_TICKS(10));

        /* For each column, analog read via ADC */
        for (int col = 0; col < 16; col++) {
            /* In production: multiplex via 74HC4051 + ADC read */
            matrix[row][col] = 100; /* Placeholder */
        }
    }
}

void pressure_analyze(const uint16_t matrix[16][16], pressure_analysis_t *out)
{
    uint32_t left = 0, right = 0, front = 0, back = 0;
    uint32_t ischial = 0, sacral = 0;

    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            uint16_t v = matrix[r][c];
            if (c < 8) left += v; else right += v;
            if (r < 8) front += v; else back += v;
        }
    }

    /* Ischial region: rows 10-13, cols 5-10 */
    for (int r = 10; r < 14; r++)
        for (int c = 5; c < 11; c++)
            ischial += matrix[r][c];

    /* Sacral region: rows 14-15 */
    for (int r = 14; r < 16; r++)
        for (int c = 0; c < 16; c++)
            sacral += matrix[r][c];

    uint32_t total = left + right;
    out->left_pct = total > 0 ? (uint8_t)(left * 100 / total) : 50;
    out->right_pct = total > 0 ? (uint8_t)(right * 100 / total) : 50;

    int tilt = (front + back > 0) ?
        (int)((front - back) * 100 / (front + back)) : 0;
    out->pelvic_tilt = (uint8_t)(tilt < 0 ? -tilt : tilt);

    out->ischial_contact = (ischial + sacral > 0) ?
        (uint8_t)(ischial * 100 / (ischial + sacral)) : 100;

    /* Posture classification */
    if (out->ischial_contact < 40)
        out->posture_class = 2; /* SLOUCHING */
    else if (out->left_pct < 40 || out->right_pct < 40)
        out->posture_class = 8; /* SCOLIOTIC */
    else if (out->pelvic_tilt > 30)
        out->posture_class = 9; /* ANTERIOR_TILT */
    else
        out->posture_class = 0; /* NEUTRAL */
}
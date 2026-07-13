/*
 * max30102.h — MAX30102 PPG sensor register definitions and driver API
 *
 * The MAX30102 is an integrated pulse oximetry and heart-rate monitor
 * with green (525 nm) and IR (880 nm) LEDs, photodiode, and 18-bit ADC.
 *
 * License: MIT
 */
#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>

/* ── I²C Address ────────────────────────────────────────────── */
#define MAX30102_I2C_ADDR    0x57

/* ── Register Map ────────────────────────────────────────────── */
#define MAX_REG_INTR_STATUS_1   0x00
#define MAX_REG_INTR_STATUS_2   0x01
#define MAX_REG_INTR_ENABLE_1   0x02
#define MAX_REG_INTR_ENABLE_2   0x03
#define MAX_REG_FIFO_WR_PTR     0x04
#define MAX_REG_OVF_COUNTER     0x05
#define MAX_REG_FIFO_RD_PTR     0x06
#define MAX_REG_FIFO_DATA       0x07
#define MAX_REG_FIFO_CONFIG     0x08
#define MAX_REG_MODE_CONFIG     0x09
#define MAX_REG_SPO2_CONFIG     0x0A
#define MAX_REG_LED1_PA         0x0C   /* Red LED pulse amplitude */
#define MAX_REG_LED2_PA         0x0D   /* IR LED pulse amplitude */
#define MAX_REG_PILOT_PA        0x10
#define MAX_REG_MULTI_LED_CTRL1 0x11
#define MAX_REG_MULTI_LED_CTRL2 0x12
#define MAX_REG_TEMP_INTR       0x1E
#define MAX_REG_TEMP_FRAC       0x1F
#define MAX_REG_TEMP_CONFIG     0x21
#define MAX_REG_PROX_INT_THRESH 0x30
#define MAX_REG_PART_ID         0xFF   /* 0x15 = MAX30102 */

/* ── Mode Config ─────────────────────────────────────────────── */
#define MAX_MODE_SHUTDOWN       (1 << 7)
#define MAX_MODE_RESET          (1 << 6)
#define MAX_MODE_HR_ONLY        0x02   /* HR mode (green only) */
#define MAX_MODE_SPO2           0x03   /* SpO2 mode (red + IR) */
#define MAX_MODE_MULTI_LED      0x07

/* ── Sample Data ────────────────────────────────────────────── */
typedef struct {
    uint32_t red;       /* Red LED ADC value (18-bit) */
    uint32_t ir;        /* IR LED ADC value (18-bit) */
    uint32_t green;     /* Green LED ADC value (18-bit, if available) */
} max_sample_t;

/* ── Driver API ──────────────────────────────────────────────── */
void max_init(void);
void max_reset(void);
void max_set_mode(uint8_t mode);
void max_set_sample_rate(uint8_t sr, uint8_t led_pw);
void set_led_amplitude(uint8_t red_pa, uint8_t ir_pa, uint8_t green_pa);
int  max_read_fifo(max_sample_t *samples, int max_count);
uint8_t max_read_intr(void);
void max_clear_intr(void);
float max_read_temperature(void);

#endif /* MAX30102_H */
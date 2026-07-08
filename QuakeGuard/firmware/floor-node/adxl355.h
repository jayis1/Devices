/*
 * adxl355.h — ADXL355 3-axis MEMS accelerometer driver
 *
 * Research-grade accelerometer: ±2g, 20-bit, 1 μg/√Hz noise
 * Used in QuakeGuard Floor Nodes for P-wave detection
 *
 * License: MIT
 */
#ifndef ADXL355_H
#define ADXL355_H

#include <stdint.h>

/* ADXL355 register addresses */
#define ADXL355_DEVID_AD      0x00
#define ADXL355_DEVID_MST     0x01
#define ADXL355_PARTID        0x02
#define ADXL355_REVID         0x03
#define ADXL355_STATUS        0x04
#define ADXL355_FIFO_ENTRIES  0x05
#define ADXL355_TEMP2         0x06
#define ADXL355_TEMP1         0x07
#define ADXL355_XDATA3        0x08
#define ADXL355_XDATA2        0x09
#define ADXL355_XDATA1        0x0A
#define ADXL355_YDATA3        0x0B
#define ADXL355_YDATA2        0x0C
#define ADXL355_YDATA1        0x0D
#define ADXL355_ZDATA3        0x0E
#define ADXL355_ZDATA2        0x0F
#define ADXL355_ZDATA1        0x10
#define ADXL355_FIFO_DATA     0x11
#define ADXL355_OFFSET_X      0x1E
#define ADXL355_OFFSET_Y      0x20
#define ADXL355_OFFSET_Z      0x22
#define ADXL355_ACT_EN        0x24
#define ADXL355_ACT_THRESH    0x25
#define ADXL355_ACT_CNT       0x27
#define ADXL355_FILTER        0x28
#define ADXL355_FIFO_SAMPLES  0x29
#define ADXL355_INT_MAP       0x2A
#define ADXL355_SYNC          0x2B
#define ADXL355_RANGE         0x2C
#define ADXL355_ODR_LPF       0x2D
#define ADXL355_POWER_CTL     0x2D  /* alias */
#define ADXL355_SELF_TEST     0x2E
#define ADXL355_RESET         0x2F

/* ADXL355 commands */
#define ADXL355_READ_CMD      0x01
#define ADXL355_WRITE_CMD     0x00
#define ADXL355_FIFO_READ_CMD 0x03

/* Range settings */
#define ADXL355_RANGE_2G    0x01
#define ADXL355_RANGE_4G    0x02
#define ADXL355_RANGE_8G    0x03

/* ODR / LPF settings */
#define ADXL355_ODR_4000HZ  0x00  /* 4000 Hz, HPF off  */
#define ADXL355_ODR_2000HZ  0x01
#define ADXL355_ODR_1000HZ  0x02  /* 1000 Hz           */
#define ADXL355_ODR_500HZ   0x03
#define ADXL355_ODR_250HZ   0x04
#define ADXL355_ODR_125HZ   0x05
#define ADXL355_ODR_62_5HZ  0x06
#define ADXL355_ODR_31_25HZ 0x07

/* Power modes */
#define ADXL355_MODE_STANDBY  0x00
#define ADXL355_MODE_MEASURE  0x01
#define ADXL355_MODE_ACTIVITY 0x02

/* Status bits */
#define ADXL355_STATUS_DATA_RDY  (1 << 0)
#define ADXL355_STATUS_FIFO_FULL (1 << 1)
#define ADXL355_STATUS_ACT        (1 << 2)

/* Sample structure (3-axis, 20-bit signed) */
typedef struct {
    int32_t x;  /* mg (milli-g), scaled from 20-bit raw */
    int32_t y;
    int32_t z;
} adxl355_sample_t;

/* Configuration */
typedef struct {
    uint8_t range;      /* ADXL355_RANGE_* */
    uint8_t odr;        /* ADXL355_ODR_* */
    uint8_t cs_pin;     /* SPI CS GPIO */
    int32_t threshold_mg; /* Activity threshold in milli-g */
    void (*on_activity)(void);  /* ISR callback for activity interrupt */
} adxl355_config_t;

/* ── Function Prototypes ────────────────────────────────────── */

/**
 * Initialize ADXL355 with given configuration.
 */
int adxl355_init(const adxl355_config_t *cfg);

/**
 * Read a single 3-axis sample (blocking).
 * Returns acceleration in milli-g.
 */
int adxl355_read_sample(adxl355_sample_t *sample);

/**
 * Burst-read FIFO into buffer (up to 96 samples × 3 axes × 3 bytes).
 * Returns number of samples read.
 */
int adxl355_read_fifo(adxl355_sample_t *samples, int max_samples);

/**
 * Set activity detection threshold (in milli-g).
 */
int adxl355_set_activity_threshold(int32_t threshold_mg);

/**
 * Enter standby (0.012 mA) or measurement mode.
 */
int adxl355_set_mode(uint8_t mode);

/**
 * Software reset.
 */
int adxl355_reset(void);

/**
 * Self-test (verifies sensor functionality).
 */
int adxl355_self_test(void);

#endif /* ADXL355_H */
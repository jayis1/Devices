/*
 * BrewSync Common Sensor Drivers
 * Shared I2C/SPI/OneWire driver abstractions
 *
 * Copyright (c) 2025 BrewSync. MIT License.
 */

#ifndef BSMP_SENSORS_H
#define BSMP_SENSORS_H

#include <stdint.h>
#include <stdbool.h>

/* ---- HAL interface (platform implements) ---- */
typedef struct {
    int (*i2c_read)(uint8_t bus, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
    int (*i2c_write)(uint8_t bus, uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
    int (*spi_xfer)(uint8_t bus, uint8_t cs, const uint8_t *tx, uint8_t *rx, uint16_t len);
    void (*delay_ms)(uint32_t ms);
    void (*delay_us)(uint32_t us);
    int (*gpio_set)(uint8_t pin, bool high);
    int (*gpio_get)(uint8_t pin, bool *high);
    int (*adc_read)(uint8_t channel, uint16_t *value);
} bsmp_hal_t;

/* ---- DS18B20 Temperature ---- */
typedef struct {
    float    temp_c;
    bool     valid;
    uint8_t  rom[8];     /* Device ROM code */
    uint8_t  resolution; /* 9-12 bits */
} ds18b20_t;

int ds18b20_init(const bsmp_hal_t *hal, uint8_t data_pin);
int ds18b20_read(const bsmp_hal_t *hal, uint8_t data_pin, ds18b20_t *result);

/* ---- SHT40 Temperature/Humidity ---- */
typedef struct {
    float temp_c;
    float humidity_rh;
    bool  valid;
} sht40_t;

int sht40_init(const bsmp_hal_t *hal, uint8_t i2c_bus);
int sht40_read(const bsmp_hal_t *hal, uint8_t i2c_bus, sht40_t *result);

/* ---- BMP390 Barometric Pressure ---- */
typedef struct {
    float pressure_hpa;
    float temp_c;
    bool  valid;
} bmp390_t;

int bmp390_init(const bsmp_hal_t *hal, uint8_t i2c_bus);
int bmp390_read(const bsmp_hal_t *hal, uint8_t i2c_bus, bmp390_t *result);

/* ---- SCD41 CO2 ---- */
typedef struct {
    float co2_ppm;
    float temp_c;
    float humidity_rh;
    bool  valid;
} scd41_t;

int scd41_init(const bsmp_hal_t *hal, uint8_t i2c_bus);
int scd41_start_periodic(const bsmp_hal_t *hal, uint8_t i2c_bus);
int scd41_read(const bsmp_hal_t *hal, uint8_t i2c_bus, scd41_t *result);
int scd41_stop_periodic(const bsmp_hal_t *hal, uint8_t i2c_bus);

/* ---- ADXL362 Accelerometer (Tilt SG) ---- */
typedef struct {
    float accel_x;  /* g */
    float accel_y;  /* g */
    float accel_z;  /* g */
    float sg;       /* Computed specific gravity */
    bool  valid;
} adxl362_t;

int adxl362_init(const bsmp_hal_t *hal, uint8_t spi_bus, uint8_t cs_pin);
int adxl362_read(const bsmp_hal_t *hal, uint8_t spi_bus, uint8_t cs_pin, adxl362_t *result);

/* Tilt SG computation: SG = 1.0 + (tilt_angle_deg - 0) * cal_slope + cal_offset
 * The accelerometer is mounted in a waterproof tilting float.
 * Tilt angle correlates linearly with liquid density (SG). */

/* ---- MS5837 Pressure ---- */
typedef struct {
    float pressure_bar;
    float temp_c;
    bool  valid;
} ms5837_t;

int ms5837_init(const bsmp_hal_t *hal, uint8_t i2c_bus);
int ms5837_read(const bsmp_hal_t *hal, uint8_t i2c_bus, ms5837_t *result);

/* ---- EZO-pH ---- */
typedef struct {
    float ph;
    bool  valid;
} ezoph_t;

int ezoph_init(const bsmp_hal_t *hal, uint8_t i2c_bus);
int ezoph_read(const bsmp_hal_t *hal, uint8_t i2c_bus, ezoph_t *result);
int ezoph_calibrate(const bsmp_hal_t *hal, uint8_t i2c_bus, float ref_ph);
int ezoph_temp_compensate(const bsmp_hal_t *hal, uint8_t i2c_bus, float temp_c);

/* ---- AS7341 Spectral Sensor (Scanner) ---- */
typedef struct {
    uint16_t ch[11];  /* F1-F8, Clear, NIR, Vis */
    bool     valid;
} as7341_t;

int as7341_init(const bsmp_hal_t *hal, uint8_t i2c_bus);
int as7341_read(const bsmp_hal_t *hal, uint8_t i2c_bus, as7341_t *result);

/* ---- SX1262 Radio ---- */
typedef struct {
    int  rssi_dbm;
    int  snr_db;
    bool crc_ok;
} sx1262_rx_info_t;

int sx1262_init(const bsmp_hal_t *hal, uint8_t spi_bus, uint8_t cs_pin,
                uint8_t busy_pin, uint8_t dio1_pin, uint8_t rst_pin);
int sx1262_config(sx1262_rx_info_t *unused, uint8_t sf, uint8_t bw, uint8_t cr,
                  uint32_t freq_hz, int8_t power_dbm);
int sx1262_send(const bsmp_hal_t *hal, const uint8_t *data, uint8_t len);
int sx1262_receive(const bsmp_hal_t *hal, uint8_t *buf, uint8_t max_len,
                   sx1262_rx_info_t *info, uint32_t timeout_ms);
void sx1262_sleep(const bsmp_hal_t *hal);

/* ---- Utility: Battery voltage ---- */
static inline float battery_voltage_from_mv(uint16_t mv) {
    return mv / 1000.0f;
}

static inline uint8_t battery_pct_from_mv(uint16_t mv) {
    /* Li-Ion 18650 approximate: 4200mV=100%, 3000mV=0% */
    if (mv >= 4200) return 100;
    if (mv <= 3000) return 0;
    return (uint8_t)((mv - 3000) * 100 / 1200);
}

#endif /* BSMP_SENSORS_H */
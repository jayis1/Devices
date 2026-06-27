/*
 * Sensor type definitions shared across nodes
 * sensor_types.h
 */
#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* DS18B20 temperature sensor */
typedef struct {
    float temp_c;
    bool  valid;
} ds18b20_reading_t;

/* Capacitive soil moisture sensor */
typedef struct {
    uint16_t raw_adc;      /* 0-4095 (ESP32 12-bit) or 0-65535 (RP2040) */
    uint8_t  moisture_pct; /* 0-100, calibrated */
    bool     valid;
} moisture_reading_t;

/* Sensirion SCD41 CO2 sensor */
typedef struct {
    uint16_t co2_ppm;
    float    temp_c;
    float    humidity_pct;
    bool     valid;
} scd41_reading_t;

/* MQ-4 methane sensor */
typedef struct {
    uint16_t methane_ppm;
    bool     valid;
} mq4_reading_t;

/* HX711 load cell */
typedef struct {
    int32_t  raw_value;
    uint16_t mass_grams;
    bool     valid;
    bool     tared;
} hx711_reading_t;

/* BME280 weather sensor */
typedef struct {
    float    temp_c;
    float    humidity_pct;
    uint16_t pressure_hpa;
    bool     valid;
} bme280_reading_t;

/* Anemometer */
typedef struct {
    float    wind_speed_ms;
    uint16_t wind_dir_deg;
    bool     valid;
} anemometer_reading_t;

/* Rain gauge */
typedef struct {
    float    rain_mm;       /* since last report */
    float    rain_total_mm; /* cumulative */
    bool     valid;
} rain_gauge_reading_t;

/* pH probe */
typedef struct {
    float    ph;
    bool     valid;
} ph_reading_t;

/* Full bin node reading set */
typedef struct {
    ds18b20_reading_t    temp[3];
    moisture_reading_t   moisture[3];
    scd41_reading_t      co2;
    mq4_reading_t        methane;
    hx711_reading_t      weight;
    uint8_t              battery_pct;
    uint8_t              vent_position;
} bin_sensor_set_t;

/* Full soil probe reading set */
typedef struct {
    ds18b20_reading_t    temp[4];
    moisture_reading_t   moisture[3];
    ph_reading_t         ph;
    scd41_reading_t      co2;
    uint8_t              battery_pct;
} soil_sensor_set_t;

/* Full weather station reading set */
typedef struct {
    bme280_reading_t     bme280;
    anemometer_reading_t anemometer;
    rain_gauge_reading_t rain;
    uint8_t              battery_pct;
} weather_sensor_set_t;

#endif /* SENSOR_TYPES_H */
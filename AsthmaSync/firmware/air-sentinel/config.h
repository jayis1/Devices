/**
 * AsthmaSync — Air Sentinel Configuration
 * ESP32-S3-WROOM-1-N8R2
 *
 * License: MIT
 */

#ifndef AIR_SENTINEL_CONFIG_H
#define AIR_SENTINEL_CONFIG_H

#define AS_SOC           "ESP32-S3-WROOM-1-N8R2"
#define AS_FLASH_SIZE    (8 * 1024 * 1024)
#define AS_PSRAM_SIZE    (2 * 1024 * 1024)

/* I²C Bus (all sensors) */
#define I2C_SCL_PIN      8
#define I2C_SDA_PIN      9
#define I2C_FREQ_HZ      100000  /* 100 kHz (SCD41 needs ≤100 kHz) */

/* Sensor I²C addresses */
#define ADDR_PMSA003I    0x12
#define ADDR_BME688      0x77
#define ADDR_SGP40       0x59
#define ADDR_SCD41       0x62

/* PMSA003I control pins */
#define PMSA_SET_PIN     4   /* standby control (HIGH=normal, LOW=sleep) */
#define PMSA_RST_PIN     5   /* reset (active LOW) */

/* SX1262 Sub-GHz radio */
#define SX1262_CS_PIN    10
#define SX1262_DIO1_PIN  11
#define SX1262_BUSY_PIN  12
#define SX1262_RST_PIN   13
#define SX1262_SCK_PIN   14
#define SX1262_MISO_PIN  15
#define SX1262_MOSI_PIN  16

/* Battery ADC */
#define BAT_ADC_PIN      17
#define BAT_DIVIDER      2.0f   /* voltage divider ratio */
#define BAT_ADC_ATTEN    11     /* 0-3.3 V range (dB attenuation) */

/* Status LED */
#define STATUS_LED_PIN   18

/* Timing */
#define SENSOR_POLL_INTERVAL_MS  30000   /* 30-second sensor read */
#define MESH_TX_INTERVAL_MS      30000   /* 30-second mesh telemetry */
#define BATTERY_CHECK_INTERVAL   300000  /* 5-minute battery check */

/* Sensor warm-up */
#define SCD41_WARMUP_MS     10000   /* SCD41 needs 10s first read */
#define BME688_BURNIN_MS    5000    /* BME688 gas heater needs warmup */

#endif /* AIR_SENTINEL_CONFIG_H */
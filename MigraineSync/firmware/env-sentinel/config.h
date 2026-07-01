/**
 * MigraineSync — Env Sentinel Configuration
 * =========================================
 * ESP32-S3-WROOM-1-N8R2 pin definitions + sensor I²C addresses.
 *
 * License: MIT
 */

#ifndef ENV_SENTINEL_CONFIG_H
#define ENV_SENTINEL_CONFIG_H

#define NODE_ID          0x0002
#define FIRMWARE_VERSION "1.0.0"

/* ── I²C (via TCA9548A mux) ─────────────────────────────── */
#define I2C_SCL_PIN      8
#define I2C_SDA_PIN      9
#define I2C_FREQ_HZ      100000

/* TCA9548A mux address */
#define MUX_ADDR         0x70

/* Sensor addresses (on mux channels) */
#define BMP390_ADDR      0x76   /* ch0 */
#define SPL06_ADDR       0x76   /* ch1 (same addr, different channel) */
#define VEML7700_ADDR    0x10   /* ch2 */
#define SHT45_ADDR       0x44   /* ch2 */
#define BME688_ADDR      0x77   /* ch3 */
#define SCD41_ADDR       0x62   /* ch3 */

/* ── Sub-GHz (SX1262) ───────────────────────────────────── */
#define SUBGHZ_FREQ      868100000
#define SUBGHZ_TX_POWER  22
#define SUBGHZ_SF        7
#define SUBGHZ_CS_PIN    10
#define SUBGHZ_DIO1_PIN  11
#define SUBGHZ_BUSY_PIN  12
#define SUBGHZ_RST_PIN   13
#define SUBGHZ_SCK_PIN   14
#define SUBGHZ_MISO_PIN  15
#define SUBGHZ_MOSI_PIN  16

/* ── Power ──────────────────────────────────────────────── */
#define BATTERY_ADC_PIN  17
#define STATUS_LED_PIN   18

/* ── Sampling ───────────────────────────────────────────── */
#define SENSOR_POLL_INTERVAL_S  1     /* 1 Hz */
#define SUBGHZ_TX_INTERVAL_S    30    /* 30 s → hub */
#define PRESSURE_DELTA_WINDOW_S 10800 /* 3 hours */

#endif /* ENV_SENTINEL_CONFIG_H */
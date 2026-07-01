/**
 * MigraineSync — Aura Band Configuration
 * =======================================
 * nRF52840 QFAA pin definitions + sensor config.
 *
 * License: MIT
 */

#ifndef AURA_BAND_CONFIG_H
#define AURA_BAND_CONFIG_H

#define NODE_ID          0x0003
#define FIRMWARE_VERSION "1.0.0"

/* ── I²C bus ────────────────────────────────────────────── */
#define I2C_SDA_PIN      8   /* P0.08 */
#define I2C_SCL_PIN      9   /* P0.09 */
#define I2C_FREQ_HZ      400000

/* ── Sensor addresses ───────────────────────────────────── */
#define MAX30101_ADDR    0x57   /* PPG: green/red/IR */
#define TMP117_ADDR      0x48   /* Skin temperature */
#define BMP390_ADDR      0x76   /* Barometric pressure */
#define VEML7700_ADDR    0x10   /* Ambient light */
#define LSM6DSO_ADDR     0x6A   /* 6-axis IMU */

/* ── Interrupts ─────────────────────────────────────────── */
#define MAX30101_INT_PIN 6   /* P0.06 */
#define LSM6DSO_INT_PIN  7   /* P0.07 */

/* ── GPIO ───────────────────────────────────────────────── */
#define BUTTON_PIN       15  /* P0.15 — mark prodrome/migraine */
#define VIBRATOR_PIN     16  /* P0.16 — haptic alert */
#define LED_PIN          13  /* P0.13 — status LED */

/* ── Power ──────────────────────────────────────────────── */
#define BATTERY_ADC_PIN  4   /* P0.04 — LiPo voltage divider */
#define BATTERY_MV_FULL  4200
#define BATTERY_MV_EMPTY 3200

/* ── PPG config ─────────────────────────────────────────── */
#define PPG_SAMPLE_RATE_HZ   100
#define PPG_LED_CURRENT_MA   6.4    /* green LED */
#define PPG_DUTY_CYCLE_PCT   25     /* 25% to save battery */

/* ── BLE ────────────────────────────────────────────────── */
#define BLE_SERVICE_UUID     0x6E400001
#define BLE_TX_CHAR_UUID     0x6E400002
#define BLE_RX_CHAR_UUID     0x6E400003
#define BLE_CONN_INTERVAL_MS 30      /* 30 ms for low latency */
#define BLE_TX_INTERVAL_S    5       /* notify every 5 seconds */

/* ── Sampling ───────────────────────────────────────────── */
#define VITALS_INTERVAL_S    5       /* compute HR/HRV every 5s */
#define BARO_INTERVAL_S      1       /* BMP390 at 1 Hz */
#define LIGHT_INTERVAL_S     2       /* VEML7700 at 0.5 Hz */
#define IMU_INTERVAL_S       2       /* LSM6DSO at 0.5 Hz */

#endif /* AURA_BAND_CONFIG_H */
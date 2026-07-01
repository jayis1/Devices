/**
 * MigraineSync — Hydrate Tag Configuration
 * =========================================
 * nRF52840 QFAA pin definitions.
 *
 * License: MIT
 */

#ifndef HYDRATE_TAG_CONFIG_H
#define HYDRATE_TAG_CONFIG_H

#define NODE_ID          0x0004
#define FIRMWARE_VERSION "1.0.0"

/* ── HX711 Load Cell ────────────────────────────────────── */
#define HX711_SCK_PIN    4   /* P0.04 */
#define HX711_DOUT_PIN   5   /* P0.05 */
#define HX711_RATE_PIN   16  /* P0.16 (GND = 10 Hz) */

/* ── LSM6DSO IMU ────────────────────────────────────────── */
#define I2C_SDA_PIN      8   /* P0.08 */
#define I2C_SCL_PIN      9   /* P0.09 */
#define LSM6DSO_ADDR     0x6A
#define LSM6DSO_INT_PIN  6   /* P0.06 — tilt interrupt (wake source) */

/* ── GPIO ───────────────────────────────────────────────── */
#define LED_PIN          11  /* P0.11 — blue LED */
#define BUZZER_PIN       13  /* P0.13 — piezo */
#define BUTTON_PIN       15  /* P0.15 — manual intake mark */

/* ── Power ──────────────────────────────────────────────── */
#define BATTERY_ADC_PIN  4   /* shared with HX711 SCK if LiPo variant */

/* ── HX711 calibration ──────────────────────────────────── */
#define HX711_SCALE_FACTOR   420.0f   /* counts per gram (calibrate per unit) */
#define HX711_TARE_COUNT     8300000  /* zero offset (calibrate) */
#define BOTTLE_EMPTY_G       150.0f   /* empty bottle weight */
#define BOTTLE_CAPACITY_ML   750.0f   /* 750 ml bottle */

/* ── Sip detection ──────────────────────────────────────── */
#define SIP_TILT_THRESHOLD_DEG  35    /* tilt > 35° = drinking gesture */
#define SIP_WEIGHT_DELTA_G      5     /* weight decrease > 5g = real sip */
#define SIP_MIN_INTERVAL_MS     500   /* min 500ms between sips */
#define SLEEP_CURRENT_UA        6     /* nRF52840 system OFF */

/* ── BLE ────────────────────────────────────────────────── */
#define BLE_SERVICE_UUID     0x6E400001
#define BLE_TX_INTERVAL_S    60      /* notify every 60 seconds (low data) */

/* ── Daily hydration goal ───────────────────────────────── */
#define DAILY_GOAL_ML    2000

#endif /* HYDRATE_TAG_CONFIG_H */
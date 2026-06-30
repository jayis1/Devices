/**
 * AsthmaSync — Inhaler Tag Configuration
 * nRF52840 QFAA
 *
 * License: MIT
 */

#ifndef INHALER_TAG_CONFIG_H
#define INHALER_TAG_CONFIG_H

#define IT_SOC  "nRF52840 QFAA"

/* LSM6DSO IMU (I²C) */
#define LSM6DSO_ADDR     0x6A
#define I2C_SDA_PIN      4   /* P0.04 */
#define I2C_SCL_PIN      5   /* P0.05 */
#define LSM6DSO_INT1_PIN 6   /* P0.06 */
#define LSM6DSO_INT2_PIN 7   /* P0.07 */

/* Accel config */
#define ACCEL_ODR_HZ     104    /* 104 Hz output data rate */
#define ACCEL_FS_G        16    /* ±16g full scale (detect drops) */
#define ACCEL_FIFO_SIZE   512   /* 512 samples */

/* Button */
#define BUTTON_PIN       8     /* P0.08 — long press = dose confirm */

/* LED + Buzzer */
#define LED_PIN          11    /* P0.11 */
#define BUZZER_PIN        13    /* P0.13 */

/* Battery */
#define BAT_ADC_PIN      15    /* P0.15 */
#define BAT_CR2032_V     3.0f
#define BAT_LOW_V        2.7f
#define BAT_CRITICAL_V   2.5f

/* BLE */
#define BLE_DEVICE_NAME  "AsthmaSync Inhaler"
#define BLE_CONN_INT_MIN 24   /* 30 ms */
#define BLE_CONN_INT_MAX 24   /* 30 ms */
#define BLE_SLAVE_LATENCY 0
#define BLE_CONN_TIMEOUT 400  /* 4 s */

/* Actuation detection thresholds */
#define ACT_THRESHOLD_G       2.5f   /* peak accel to trigger analysis window */
#define ACT_WINDOW_MS         300    /* analysis window after trigger */
#define ACT_COOLDOWN_MS       10000  /* 10s between actuations (debounce) */
#define ACT_MIN_CONFIDENCE    70     /* 70% confidence to accept */

/* Sleep / power management */
#define SLEEP_TIMEOUT_MS  30000  /* 30s idle → sleep */
#define WAKEUP_SOURCE     NRF_GPIO_PIN_WAKEUP

#endif /* INHALER_TAG_CONFIG_H */
/**
 * AsthmaSync — Wheeze Band Configuration
 * nRF52840 QFAA
 *
 * License: MIT
 */

#ifndef WHEEZE_BAND_CONFIG_H
#define WHEEZE_BAND_CONFIG_H

#define WB_SOC  "nRF52840 QFAA"

/* I²S Microphone (SPH0645LM4H-B) */
#define I2S_SCK_PIN    8   /* P0.08 */
#define I2S_WS_PIN     9   /* P0.09 */
#define I2S_SD_PIN    10   /* P0.10 */
#define I2S_MCK_PIN   11   /* P0.11 */

/* Audio config */
#define AUDIO_SAMPLE_RATE  16000   /* 16 kHz */
#define AUDIO_BITS         24      /* 24-bit I²S data (shifted to 32-bit frame) */
#define AUDIO_FRAME_MS     2000    /* 2-second analysis frame */
#define AUDIO_FRAME_SAMPLES (AUDIO_SAMPLE_RATE * AUDIO_FRAME_MS / 1000)  /* 32000 */
#define MEL_BANDS          40
#define MEL_FRAMES         32      /* 2s ÷ 62.5ms hop = 32 frames */

/* I²C Bus (MAX30101, TMP117, LSM6DSO) */
#define I2C_SDA_PIN   26   /* P0.26 */
#define I2C_SCL_PIN   27   /* P0.27 */
#define I2C_FREQ_HZ   400000  /* 400 kHz Fast-mode */

/* Sensor addresses */
#define ADDR_MAX30101  0x57
#define ADDR_TMP117   0x48
#define ADDR_LSM6DSO  0x6A

/* Sensor interrupt pins */
#define MAX30101_INT_PIN  6   /* P0.06 */
#define LSM6DSO_INT1_PIN  7   /* P0.07 */

/* User interface */
#define BUTTON_PIN    15   /* P0.15 — mark event */
#define MOTOR_PIN     16   /* P0.16 — haptic vibrator */
#define LED_PIN       13   /* P0.13 — status LED */

/* Battery */
#define BAT_ADC_PIN   4    /* P0.04 */
#define BAT_LIPO_MAH  200   /* 200 mAh LiPo */
#define BAT_LOW_PCT   20

/* PPG (MAX30101) config */
#define PPG_SAMPLE_RATE_HZ  100
#define PPG_LED_CURRENT_MA   6.4    /* 6.4 mA drive current */
#define PPG_PULSE_WIDTH_US   411    /* 411 µs → 18-bit ADC */
#define PPG_SPO2_RED_ENABLE  1
#define PPG_DUTY_CYCLE_PCT   25     /* 25% duty to save power */

/* BLE */
#define BLE_DEVICE_NAME  "AsthmaSync Band"
#define BLE_MTU           247

/* Wheeze pre-classifier threshold */
#define WHEEZE_PROB_THRESHOLD  65    /* 65% probability to flag */
#define WHEEZE_COOLDOWN_MS     30000 /* 30s between wheeze events */

/* Power management */
#define SLEEP_TIMEOUT_MS  600000  /* 10 min idle → sleep */
#define DISPLAY_OFF_MS    5000    /* 5s to turn off LED */

#endif /* WHEEZE_BAND_CONFIG_H */
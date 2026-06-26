/*
 * OSMP — common sensor driver abstractions.
 * Each node provides its own backend; these are the shared interfaces.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef OSMP_SENSORS_H
#define OSMP_SENSORS_H

#include <stdint.h>

/* ICM-42688-P 6-axis IMU (Toothbrush) — SPI */
typedef struct {
    int16_t ax, ay, az;   /* LSB = 0.001 g (after scale) */
    int16_t gx, gy, gz;   /* LSB = 0.1 dps */
    uint16_t ts_ms;       /* sample timestamp */
} imu_sample_t;

int  imu_init(void);
void imu_read(imu_sample_t *out);
void imu_wakeup(void);
void imu_sleep(void);

/* FSR 402 pressure (Toothbrush) — ADC */
int     pressure_init(void);
uint16_t pressure_read_cN(void);  /* centinewtons; >350 = over-pressure */

/* Battery monitoring (all battery nodes) */
uint8_t battery_pct(void);        /* 0..100 */
uint8_t battery_charging(void);   /* 0/1 */

/* DS18B20 1-Wire temperature (Saliva) — returns deci-degrees C */
int  ds18b20_init(void);
int16_t ds18b20_read_c10(void);

/* SHT40 ambient T/RH (Hub + Scanner) */
int  sht40_init(void);
void sht40_read(int16_t *t_c10, uint16_t *rh_x100);

/* VL53L1X ToF (Scanner) — mm */
int    vl53l1x_init(void);
uint16_t vl53l1x_read_mm(void);

/* OV5640 camera (Scanner) — DVP */
int  camera_init(void);
int  camera_capture(uint8_t *buf, size_t buf_cap, size_t *out_len, int band);
void camera_set_band(int band);  /* 0=white,1=405,2=470,3=525,4=660,5=850 */

/* Spectral LED ring (Scanner) — per-band PWM enable */
void spectral_set_band(int band);
void spectral_off(void);

/* ISFET pH (Saliva) — returns pH*100 */
int     ph_init(void);
uint16_t ph_read_x100(void);

/* Nitrite amperometry (Saliva) — µM */
int     nitrite_init(void);
uint16_t nitrite_read_um(void);

/* HX711 load cell (Saliva) — grams, for buffer titration micro-volume */
int     hx711_init(void);
int32_t hx711_read_g(void);

/* I2S mic + speaker (Hub) */
int  i2s_mic_init(void);
int  i2s_mic_read(int16_t *samples, size_t n);
int  i2s_spk_init(void);
void i2s_spk_play(const int16_t *samples, size_t n);

/* NeoPixel (Hub) — 16 RGB */
void neopixel_init(void);
void neopixel_set(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void neopixel_show(void);

/* ST7701 5" LCD (Hub) / ST7789 2" LCD (Scanner) — SPI 8-bit 8080 */
void lcd_init(void);
void lcd_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *rgb565);

/* LRA haptics (Toothbrush) */
void haptics_init(void);
void haptics_pulse(uint8_t pattern);  /* 0=short,1=long,2=double */

/* Buzzer (Scanner) */
void buzzer_init(void);
void buzzer_beep(uint16_t ms);

/* RTC (Toothbrush nRF52840 / Saliva STM32L432) */
uint32_t rtc_now_unix(void);
void     rtc_set_unix(uint32_t t);

/* AES-128-CCM (all nodes after pairing) */
void aes_ccm_init(const uint8_t key[16]);
int  aes_ccm_encrypt(uint8_t *buf, uint8_t len, const uint8_t nonce[13], uint8_t tag[8]);
int  aes_ccm_decrypt(uint8_t *buf, uint8_t len, const uint8_t nonce[13], const uint8_t tag[8]);

#endif /* OSMP_SENSORS_H */
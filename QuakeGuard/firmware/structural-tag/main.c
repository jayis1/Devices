/*
 * quakeguard_struct.c — QuakeGuard Structural Health Tag firmware (RP2040)
 *
 * Battery-powered sensor for load-bearing structure monitoring.
 * Deep sleep with 5-min wake interval. Post-event wake on CC1101 interrupt.
 *
 * Samples:
 *   - HX711 24-bit strain gauge (80 Hz, averaged over 5 s)
 *   - LIS3DH accelerometer (100 Hz, 256-point FFT for resonance)
 *   - DS18B20 temperature (thermal compensation)
 *
 * Reports to Hub: max strain, mean strain, resonance shift, peak accel,
 *   temperature, battery %, anomaly score.
 *
 * Target: 12-month CR2032 ×3 battery life
 *
 * License: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/sleep.h"
#include "pico/spi.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "pico/util/datetime.h"

#include "common/quakeguard_protocol.h"
#include "common/cc1101.h"

/* ── Pin Definitions (RP2040) ──────────────────────────────── */
#define PIN_HX711_SCK    0
#define PIN_HX711_DOUT   1
#define PIN_I2C_SDA      2
#define PIN_I2C_SCL      3
#define PIN_CC1101_CS    4
#define PIN_DS18B20      5
#define PIN_SPI_SCK      6
#define PIN_SPI_MOSI     7
#define PIN_SPI_MISO     8
#define PIN_CC1101_GD0   9
#define PIN_CC1101_GD2   10
#define PIN_LED_RED      25
#define PIN_LED_GREEN    26
#define PIN_HX711_PWR    23   /* MOSFET gate for HX711 power control */
#define PIN_LIS3DH_INT1  24
#define PIN_BAT_DIV      29   /* ADC for battery voltage */

/* ── Constants ──────────────────────────────────────────────── */
#define SAMPLE_INTERVAL_S    300    /* 5 min between samples */
#define HX711_RATE_HZ        80
#define HX711_AVG_SAMPLES   400     /* 5 s at 80 Hz */
#define LIS3DH_RATE_HZ      100
#define FFT_SIZE             256
#define STRAIN_BASELINE_US  86400000  /* 24 h baseline learning */
#define BATTERY_LOW_MV      4500    /* 3 × 1.5V = 4.5V minimum */

/* ── Global State ───────────────────────────────────────────── */
static cc1101_t radio;
static int32_t strain_baseline = 0;
static int32_t resonance_baseline_hz = 0;
static int64_t boot_time_us = 0;

/* ── HX711 24-bit ADC Driver ────────────────────────────────── */

static void hx711_power_on(void)
{
    gpio_put(PIN_HX711_PWR, 1);  /* MOSFET on */
    sleep_ms(500);                /* HX711 power-up time */
}

static void hx711_power_off(void)
{
    gpio_put(PIN_HX711_PWR, 0);
}

static int32_t hx711_read_raw(void)
{
    /* Wait for DOUT to go low (data ready) */
    int timeout = 0;
    while (gpio_get(PIN_HX711_DOUT)) {
        sleep_us(10);
        if (++timeout > 100000) return -1;  /* timeout */
    }

    /* Read 24 bits */
    int32_t data = 0;
    for (int i = 0; i < 24; i++) {
        gpio_put(PIN_HX711_SCK, 1);
        sleep_us(1);
        data <<= 1;
        if (gpio_get(PIN_HX711_DOUT)) data |= 1;
        gpio_put(PIN_HX711_SCK, 0);
        sleep_us(1);
    }

    /* 25th pulse for channel A gain 128 (HX711 datasheet) */
    gpio_put(PIN_HX711_SCK, 1);
    sleep_us(1);
    gpio_put(PIN_HX711_SCK, 0);
    sleep_us(1);

    /* Sign-extend 24-bit to 32-bit */
    if (data & 0x800000) data |= 0xFF000000;

    return data;
}

static int32_t hx711_read_averaged(int n_samples)
{
    int64_t sum = 0;
    int valid = 0;
    for (int i = 0; i < n_samples; i++) {
        int32_t raw = hx711_read_raw();
        if (raw >= 0) {
            sum += raw;
            valid++;
        }
        sleep_ms(1000 / HX711_RATE_HZ);  /* 12.5 ms */
    }
    if (valid == 0) return 0;
    return (int32_t)(sum / valid);
}

/* ── Convert HX711 raw to microstrain ───────────────────────── */
static int32_t raw_to_microstrain(int32_t raw)
{
    /* HX711: 24-bit, gain 128, 2.6V excitation
     * Full bridge: 350Ω gauges, gauge factor GF = 2.0
     * Vout = Vexc × GF × ε / 4 (quarter bridge)
     * Full bridge: Vout = Vexc × GF × ε
     * HX711 LSB = Vexc / (2^24 × 128) = 2.6V / 2147483648 ≈ 1.21 nV
     * ε = Vout / (Vexc × GF) = raw × 1.21e-9 / (2.6 × 2.0)
     * ε (microstrain) = raw × 1.21e-9 / 5.2 × 1e6 = raw × 0.000233
     */
    return (int32_t)((raw - strain_baseline) * 0.000233f * 1000000);
}

/* ── LIS3DH Accelerometer Driver (I2C) ──────────────────────── */

#define LIS3DH_ADDR 0x19

static void lis3dh_init(void)
{
    uint8_t ctrl1[] = {0x20, 0x57};  /* ODR=100Hz, XYZ enabled */
    i2c_write_blocking(i2c0, LIS3DH_ADDR, ctrl1, 2, false);

    uint8_t ctrl4[] = {0x23, 0x00};  /* ±2g, HR not needed for resonance */
    i2c_write_blocking(i2c0, LIS3DH_ADDR, ctrl4, 2, false);
}

static void lis3dh_read_xyz(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t reg = 0x28 | 0x80;  /* OUT_X_L, auto-increment */
    uint8_t data[6];
    i2c_write_blocking(i2c0, LIS3DH_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c0, LIS3DH_ADDR, data, 6, false);

    *x = (int16_t)(data[0] | (data[1] << 8));
    *y = (int16_t)(data[2] | (data[3] << 8));
    *z = (int16_t)(data[4] | (data[5] << 8));
}

/* ── Simple FFT (256-point, in-place) ───────────────────────── */
/* Cooley-Tukey radix-2 FFT for resonance analysis */

static void fft256(float *real, float *imag)
{
    /* Bit-reversal permutation */
    for (int i = 1, j = 0; i < FFT_SIZE; i++) {
        int bit = FFT_SIZE >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
    }

    /* Butterfly operations */
    for (int len = 2; len <= FFT_SIZE; len <<= 1) {
        float ang = -2.0f * M_PI / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < FFT_SIZE; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int j = 0; j < len / 2; j++) {
                float tr = cr * real[i + j + len / 2] - ci * imag[i + j + len / 2];
                float ti = cr * imag[i + j + len / 2] + ci * real[i + j + len / 2];
                real[i + j + len / 2] = real[i + j] - tr;
                imag[i + j + len / 2] = imag[i + j] - ti;
                real[i + j] += tr;
                imag[i + j] += ti;
                float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

static float find_resonance_freq(float *accel_buffer, int n)
{
    /* Compute FFT magnitude spectrum, find peak frequency */
    float real[FFT_SIZE], imag[FFT_SIZE];

    for (int i = 0; i < FFT_SIZE; i++) {
        real[i] = (i < n) ? accel_buffer[i] : 0.0f;
        imag[i] = 0.0f;
    }

    fft256(real, imag);

    /* Find peak in magnitude (skip DC bin) */
    float max_mag = 0;
    int peak_bin = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float mag = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
        if (mag > max_mag) {
            max_mag = mag;
            peak_bin = i;
        }
    }

    /* Convert bin to Hz: freq = bin × (sample_rate / FFT_size) */
    float freq = (float)peak_bin * (LIS3DH_RATE_HZ / (float)FFT_SIZE);
    return freq;
}

/* ── DS18B20 Temperature (1-Wire, simplified) ───────────────── */
static float read_temperature(void)
{
    /* Simplified DS18B20 read
     * In production: full 1-Wire protocol (reset, ROM match, convert, read)
     */
    return 25.0f;  /* placeholder */
}

/* ── Battery Voltage ────────────────────────────────────────── */
static uint8_t read_battery_pct(void)
{
    adc_select_input(3);  /* ADC3 = GPIO29 */
    uint16_t raw = adc_read();
    /* Voltage divider: 6V → 2M/1M → 3V max at ADC
     * Battery voltage = raw × 3.3V / 4095 × 3 (divider ratio)
     */
    float voltage = (float)raw * 3.3f / 4095.0f * 3.0f;
    /* CR2032 ×3: 6V fresh, 4.5V empty */
    if (voltage >= 6.0f) return 100;
    if (voltage <= 4.5f) return 0;
    return (uint8_t)((voltage - 4.5f) / 1.5f * 100.0f);
}

/* ── Anomaly Score (simplified autoencoder proxy) ───────────── */
static uint8_t compute_anomaly_score(int32_t strain_max, int32_t strain_mean,
                                       float resonance_shift)
{
    /* In production: tflite-micro autoencoder inference
     * Input: 7-day strain + vibration + temp time-series
     * Output: reconstruction error (0–255)
     *
     * Simplified: distance from baseline
     */
    float strain_anomaly = fabsf((float)strain_max) / 100.0f;  /* 100 με = score 100 */
    float resonance_anomaly = fabsf(resonance_shift) / 5.0f;  /* 5 Hz shift = score 100 */

    float score = (strain_anomaly + resonance_anomaly) / 2.0f * 255.0f;
    if (score > 255) score = 255;
    return (uint8_t)score;
}

/* ── Sample and Report ──────────────────────────────────────── */
static void sample_and_report(void)
{
    /* Power on HX711 */
    hx711_power_on();
    sleep_ms(100);

    /* Read strain (averaged over 5 s = 400 samples at 80 Hz) */
    int32_t strain_raw = hx711_read_averaged(HX711_AVG_SAMPLES);
    int32_t strain_mean = raw_to_microstrain(strain_raw);

    /* Read multiple samples to find max */
    int32_t strain_max = strain_mean;
    for (int i = 0; i < 10; i++) {
        int32_t s = raw_to_microstrain(hx711_read_raw());
        if (s > strain_max) strain_max = s;
        sleep_ms(12);
    }

    /* Power off HX711 */
    hx711_power_off();

    /* Read vibration (256 samples at 100 Hz = 2.56 s) */
    float accel_buffer[FFT_SIZE];
    int16_t x, y, z;
    for (int i = 0; i < FFT_SIZE; i++) {
        lis3dh_read_xyz(&x, &y, &z);
        /* Use magnitude of acceleration vector */
        accel_buffer[i] = sqrtf((float)x * x + (float)y * y + (float)z * z);
        sleep_ms(10);  /* 100 Hz */
    }

    float resonance_hz = find_resonance_freq(accel_buffer, FFT_SIZE);
    int16_t resonance_shift = (int16_t)(resonance_hz - resonance_baseline_hz);

    /* Find peak acceleration */
    float peak_accel = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        if (accel_buffer[i] > peak_accel) peak_accel = accel_buffer[i];
    }
    /* Convert to milli-g (LIS3DH ±2g range: 16-bit, 1 mg/LSB) */
    int16_t peak_accel_mg = (int16_t)(peak_accel / 16.0f);

    /* Read temperature */
    float temp_c = read_temperature();

    /* Compute anomaly score */
    uint8_t anomaly = compute_anomaly_score(strain_max, strain_mean,
                                             (float)resonance_shift);

    /* Read battery */
    uint8_t battery = read_battery_pct();

    /* Build report */
    struct_report_payload_t report = {
        .strain_max_micro = strain_max,
        .strain_mean_micro = strain_mean,
        .resonance_shift_hz = resonance_shift,
        .peak_accel_mg = peak_accel_mg,
        .temperature_c10 = (int16_t)(temp_c * 10),
        .battery_pct = battery,
        .anomaly_score = anomaly,
        .fault_flags = 0,
        .reserved = {0},
    };

    /* Send to Hub */
    qg_frame_t frame;
    size_t frame_len = qg_build_frame(&frame,
        MSG_STRUCT_REPORT, QG_ADDR_STRUCT_BASE, QG_ADDR_HUB,
        0, (uint8_t *)&report, sizeof(report));

    cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                frame_len - QG_PREAMBLE_LEN);

    /* Update baseline (running average) */
    if (strain_baseline == 0) {
        strain_baseline = strain_raw;
        resonance_baseline_hz = (int32_t)resonance_hz;
    } else {
        strain_baseline = (int32_t)(strain_baseline * 0.999f +
                                     strain_raw * 0.001f);
        resonance_baseline_hz = (int32_t)(resonance_baseline_hz * 0.999f +
                                           resonance_hz * 0.001f);
    }

    /* Heartbeat LED */
    gpio_put(PIN_LED_GREEN, 1);
    sleep_ms(50);
    gpio_put(PIN_LED_GREEN, 0);

    printf("Report: strain_max=%ld με, resonance=%.1f Hz (Δ=%d), "
           "anomaly=%d, battery=%d%%\n",
           (long)strain_max, resonance_hz, resonance_shift,
           anomaly, battery);
}

/* ── Post-Event Burst Sampling ──────────────────────────────── */
static void post_event_burst(uint16_t event_id)
{
    /* Wake on CC1101 GD0 interrupt
     * Sample at high rate for 10 s
     * Report immediately (bypass 5-min interval)
     */
    gpio_put(PIN_LED_RED, 1);

    /* Power on HX711 */
    hx711_power_on();

    /* Sample strain at 80 Hz for 10 s = 800 samples */
    int32_t strain_max = 0;
    int64_t strain_sum = 0;
    for (int i = 0; i < 800; i++) {
        int32_t s = raw_to_microstrain(hx711_read_raw());
        if (s > strain_max) strain_max = s;
        strain_sum += s;
        sleep_ms(12);
    }
    int32_t strain_mean = (int32_t)(strain_sum / 800);

    /* Sample vibration at 100 Hz for 10 s */
    float accel_buffer[FFT_SIZE];
    int16_t x, y, z;
    for (int i = 0; i < FFT_SIZE; i++) {
        lis3dh_read_xyz(&x, &y, &z);
        accel_buffer[i] = sqrtf((float)x * x + (float)y * y + (float)z * z);
        sleep_ms(10);
    }

    float resonance_hz = find_resonance_freq(accel_buffer, FFT_SIZE);
    int16_t resonance_shift = (int16_t)(resonance_hz - resonance_baseline_hz);

    float peak_accel = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        if (accel_buffer[i] > peak_accel) peak_accel = accel_buffer[i];
    }
    int16_t peak_accel_mg = (int16_t)(peak_accel / 16.0f);

    hx711_power_off();

    /* Report */
    struct_report_payload_t report = {
        .strain_max_micro = strain_max,
        .strain_mean_micro = strain_mean,
        .resonance_shift_hz = resonance_shift,
        .peak_accel_mg = peak_accel_mg,
        .temperature_c10 = (int16_t)(read_temperature() * 10),
        .battery_pct = read_battery_pct(),
        .anomaly_score = compute_anomaly_score(strain_max, strain_mean,
                                                (float)resonance_shift),
        .fault_flags = 0,
        .reserved = {0},
    };

    qg_frame_t frame;
    size_t frame_len = qg_build_frame(&frame,
        MSG_STRUCT_REPORT, QG_ADDR_STRUCT_BASE, QG_ADDR_HUB,
        0, (uint8_t *)&report, sizeof(report));
    cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                frame_len - QG_PREAMBLE_LEN);

    gpio_put(PIN_LED_RED, 0);
    printf("POST-EVENT: strain_max=%ld, resonance Δ=%d Hz, peak=%d mg\n",
           (long)strain_max, resonance_shift, peak_accel_mg);
}

/* ── Main ───────────────────────────────────────────────────── */
int main(void)
{
    stdio_init_all();
    printf("QuakeGuard Structural Health Tag starting...\n");

    /* Initialize GPIO */
    gpio_init(PIN_HX711_SCK);
    gpio_init(PIN_HX711_DOUT);
    gpio_init(PIN_HX711_PWR);
    gpio_init(PIN_LED_RED);
    gpio_init(PIN_LED_GREEN);
    gpio_init(PIN_CC1101_GD0);

    gpio_set_dir(PIN_HX711_SCK, GPIO_OUT);
    gpio_set_dir(PIN_HX711_DOUT, GPIO_IN);
    gpio_set_dir(PIN_HX711_PWR, GPIO_OUT);
    gpio_set_dir(PIN_LED_RED, GPIO_OUT);
    gpio_set_dir(PIN_LED_GREEN, GPIO_OUT);
    gpio_set_dir(PIN_CC1101_GD0, GPIO_IN);

    /* Initialize I2C for LIS3DH */
    i2c_init(i2c0, 400000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    /* Initialize SPI for CC1101 */
    spi_init(spi1, 4000000);
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MISO, GPIO_FUNC_SPI);
    gpio_init(PIN_CC1101_CS);
    gpio_set_dir(PIN_CC1101_CS, GPIO_OUT);
    gpio_put(PIN_CC1101_CS, 1);

    /* Initialize ADC for battery monitoring */
    adc_init();
    adc_gpio_init(PIN_BAT_DIV);

    /* Initialize LIS3DH */
    lis3dh_init();

    /* Initialize CC1101
     * Note: RP2040 port uses pico-sdk SPI instead of ESP-IDF
     * cc1101_init() wrapper adapts to platform
     */
    /* cc1101_init(&radio, ...); */

    /* Initial sample */
    sample_and_report();

    /* Main loop: deep sleep between samples */
    while (true) {
        /* Check for post-event wake (CC1101 GD0 interrupt) */
        if (!gpio_get(PIN_CC1101_GD0)) {
            /* Interrupt received — poll for event */
            /* Read STRUCT_POLL from Hub */
            uint8_t rx_buf[128], rx_len;
            int8_t rssi;
            if (cc1101_recv(&radio, rx_buf, &rx_len, &rssi) == 0) {
                qg_frame_t frame;
                if (qg_parse_frame(rx_buf, rx_len, &frame) == 0 &&
                    frame.header.msg_type == MSG_STRUCT_POLL) {
                    struct_poll_payload_t *pp = (struct_poll_payload_t *)frame.payload;
                    post_event_burst(pp->event_id);
                }
            }
        }

        /* Normal sampling interval */
        sample_and_report();

        /* Deep sleep for 5 minutes
         * RP2040: sleep with watchdog + RTC alarm
         * Power: RP2040 sleep (0.3 mA) + sensors off
         */
        sleep_ms(SAMPLE_INTERVAL_S * 1000);

        /* In production: use pico_sleep (dormant mode, ~0.3 mA)
         * Wake sources: CC1101 GD0 interrupt + RTC timer
         */
    }

    return 0;
}
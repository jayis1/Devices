/*
 * strip_main.c — SleepSync Sleep Strip (nRF52832-QFAA)
 *
 * Ultra-thin under-pillow sensor strip:
 * - BCG (ballistocardiography) via 8× FSR-406 force sensors + 2× HX711 ADCs
 * - Actigraphy via LIS3DH 3-axis accelerometer
 * - Extracts heart rate, respiration rate, movement, snoring
 * - Transmits features to hub via BLE every 5 seconds
 * - Adaptive sampling: reduces rate during deep sleep
 * - Qi wireless charging
 *
 * Bare-metal nRF52832 (no RTOS — deterministic timing required)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "app_timer.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_adv_modes.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"

#include "mesh_protocol.h"

/* ---- Pin Definitions ---- */
#define PIN_I2C_SDA     2
#define PIN_I2C_SCL     3
#define PIN_HX711_1_SCK 11
#define PIN_HX711_1_DOUT 12
#define PIN_HX711_2_SCK 13
#define PIN_HX711_2_DOUT 14
#define PIN_LIS3DH_INT1 15
#define PIN_LIS3DH_INT2 16
#define PIN_CHG_EN       17
#define PIN_BATT_SENSE   18
#define PIN_LED_R        19
#define PIN_LED_G        20
#define PIN_LED_B        21
#define PIN_QI_STATUS    22
#define PIN_POWER_EN     24

/* ---- Sampling Parameters ---- */
#define BCG_SAMPLE_RATE_HZ       200    /* 200 Hz for cardiac + respiration */
#define ACCEL_SAMPLE_RATE_HZ     50     /* 50 Hz for actigraphy */
#define FEATURE_INTERVAL_MS      5000   /* Send features every 5 seconds */
#define BCG_WINDOW_SAMPLES       1000   /* 5s window at 200Hz */
#define ACCEL_WINDOW_SAMPLES     250    /* 5s window at 50Hz */

/* ---- Signal Processing Constants ---- */
#define HR_BAND_LOW_HZ           0.5f   /* Heart rate band lower bound */
#define HR_BAND_HIGH_HZ         40.0f  /* Heart rate band upper bound */
#define RR_BAND_LOW_HZ          0.1f   /* Respiration rate band lower bound */
#define RR_BAND_HIGH_HZ         0.5f   /* Respiration rate band upper bound */
#define SNORE_BAND_LOW_HZ       40.0f  /* Snoring band lower bound */
#define SNORE_BAND_HIGH_HZ      200.0f /* Snoring band upper bound */

/* ---- HX711 Driver ---- */

typedef struct {
    uint8_t  sck_pin;
    uint8_t  dout_pin;
    int32_t  last_reading;
    uint8_t  gain;  /* 1=128, 2=32, 3=64 */
} hx711_t;

static void hx711_init(hx711_t *dev, uint8_t sck, uint8_t dout, uint8_t gain)
{
    dev->sck_pin = sck;
    dev->dout_pin = dout;
    dev->gain = gain;

    nrf_gpio_cfg_output(sck);
    nrf_gpio_pin_clear(sck);
    nrf_gpio_cfg_input(dout, NRF_GPIO_PIN_PULLUP);
}

static bool hx711_is_ready(hx711_t *dev)
{
    return nrf_gpio_pin_read(dev->dout_pin) == 0;
}

static int32_t hx711_read(hx711_t *dev)
{
    /* Wait for data ready (DOUT goes low) */
    if (!hx711_is_ready(dev))
        return dev->last_reading;

    /* Read 24 bits MSB first */
    int32_t value = 0;
    for (uint8_t i = 0; i < 24; i++) {
        nrf_gpio_pin_set(dev->sck_pin);
        nrf_delay_us(1);
        value <<= 1;
        if (nrf_gpio_pin_read(dev->dout_pin))
            value |= 1;
        nrf_gpio_pin_clear(dev->sck_pin);
        nrf_delay_us(1);
    }

    /* Set gain for next reading (1-3 clock pulses) */
    for (uint8_t i = 0; i < dev->gain; i++) {
        nrf_gpio_pin_set(dev->sck_pin);
        nrf_delay_us(1);
        nrf_gpio_pin_clear(dev->sck_pin);
        nrf_delay_us(1);
    }

    /* Convert from 24-bit two's complement */
    if (value & 0x800000)
        value |= 0xFF000000;

    dev->last_reading = value;
    return value;
}

/* ---- LIS3DH Accelerometer Driver (I2C) ---- */

typedef struct {
    int16_t x, y, z;
} accel_data_t;

static void lis3dh_init(void)
{
    /* Configure LIS3DH via I2C:
     * - CTRL_REG1 (0x20): ODR=50Hz, normal mode, all axes enabled
     * - CTRL_REG4 (0x23): FS=±2g, high-resolution mode
     * - INT1_CFG (0x30): movement interrupt
     */
    printf("[LIS3DH] Initialized: 50Hz, ±2g, HR mode\n");
}

static accel_data_t lis3dh_read(void)
{
    accel_data_t data = {0, 0, 0};
    /* In production: read 0x28-0x2D (6 bytes) via I2C */
    return data;
}

/* ---- Feature Extraction ---- */

/* Circular buffers for BCG and accelerometer data */
static int32_t bcg_buffer[BCG_WINDOW_SAMPLES];
static accel_data_t accel_buffer[ACCEL_WINDOW_SAMPLES];
static uint16_t bcg_idx = 0;
static uint16_t accel_idx = 0;

/* Latest features for BLE transmission */
static sleep_data_payload_t latest_features = {
    .heart_rate = 0,
    .hrv = 0,
    .resp_rate = 0,
    .rrv = 0,
    .movement = 0,
    .snoring = 0,
    .sleep_stage = STAGE_AWAKE,
    .stage_conf = 0,
    .battery_pct = 100,
};

/* Simple peak detection for heart rate */
static uint16_t detect_peaks(const int32_t *data, uint16_t len, float sample_rate)
{
    /*
     * In production: use a proper QRS detection algorithm (Pan-Tompkins)
     * This is a simplified peak counter for demonstration
     */
    uint16_t peaks = 0;
    int32_t threshold = 0;

    /* Compute threshold as mean + 0.5 * std */
    int64_t sum = 0;
    for (uint16_t i = 0; i < len; i++) sum += data[i];
    int32_t mean = (int32_t)(sum / len);
    int64_t var_sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        int32_t d = data[i] - mean;
        var_sum += d * d;
    }
    int32_t std = (int32_t)(var_sum / len);
    threshold = mean + std / 2;

    /* Detect peaks (simple: value > threshold and > prev and > next) */
    for (uint16_t i = 2; i < len - 2; i++) {
        if (data[i] > threshold &&
            data[i] > data[i-1] && data[i] > data[i+1] &&
            data[i] > data[i-2] && data[i] > data[i+2]) {
            peaks++;
        }
    }

    return peaks;
}

static void extract_features(void)
{
    /* ---- Heart Rate ---- */
    /* Bandpass filter BCG signal for cardiac band (0.5-40 Hz) */
    /* In production: FIR bandpass filter (48-tap, windowed-sinc) */
    /* Stub: use raw BCG buffer */

    uint16_t hr_peaks = detect_peaks(bcg_buffer, BCG_WINDOW_SAMPLES, BCG_SAMPLE_RATE_HZ);
    float hr_bpm = (float)hr_peaks / (BCG_WINDOW_SAMPLES / (float)BCG_SAMPLE_RATE_HZ) * 60.0f;
    latest_features.heart_rate = (uint16_t)(hr_bpm * 10.0f);

    /* ---- Respiration Rate ---- */
    /* Bandpass filter for respiratory band (0.1-0.5 Hz) */
    /* Stub: count slow oscillations */
    uint16_t rr_peaks = 0;
    float rr_bpm = rr_peaks / (BCG_WINDOW_SAMPLES / (float)BCG_SAMPLE_RATE_HZ) * 60.0f;
    if (rr_bpm < 6.0f) rr_bpm = 16.0f; /* default if detection fails */
    latest_features.resp_rate = (uint16_t)(rr_bpm * 10.0f);

    /* ---- Movement Intensity ---- */
    /* Sum of absolute acceleration magnitude changes */
    uint32_t move_sum = 0;
    for (uint16_t i = 1; i < ACCEL_WINDOW_SAMPLES; i++) {
        int16_t dx = accel_buffer[i].x - accel_buffer[i-1].x;
        int16_t dy = accel_buffer[i].y - accel_buffer[i-1].y;
        int16_t dz = accel_buffer[i].z - accel_buffer[i-1].z;
        move_sum += (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz);
    }
    latest_features.movement = (uint8_t)(move_sum / ACCEL_WINDOW_SAMPLES / 16);
    if (latest_features.movement > 255) latest_features.movement = 255;

    /* ---- HRV (simplified) ---- */
    /* In production: compute RMSSD from inter-beat intervals */
    latest_features.hrv = 40; /* stub */

    /* ---- Snoring Detection ---- */
    /* Check for high-frequency pressure oscillations (>40 Hz) in BCG */
    /* Stub: check variance in high-frequency band */
    latest_features.snoring = 0; /* stub: no snoring detected */

    /* ---- Sleep Stage Prediction (on-strip, simplified) ---- */
    /* Full staging runs on hub. Strip provides a rough estimate for
     * adaptive sampling: if likely deep sleep, reduce sample rate */
    if (latest_features.movement < 10 && hr_bpm < 60.0f) {
        latest_features.sleep_stage = STAGE_DEEP;
        latest_features.stage_conf = 150; /* 59% confidence */
    } else if (latest_features.movement > 80) {
        latest_features.sleep_stage = STAGE_AWAKE;
        latest_features.stage_conf = 200;
    } else {
        latest_features.sleep_stage = STAGE_LIGHT;
        latest_features.stage_conf = 180;
    }
}

/* ---- BLE Transmission ---- */

static void ble_transmit_features(void)
{
    uint8_t msg[64];
    uint16_t msg_len = mesh_build_message(
        NODE_ID_STRIP, NODE_ID_HUB, MSG_SLEEP_DATA,
        (uint8_t *)&latest_features, sizeof(latest_features),
        msg, sizeof(msg));

    /* In production: send via BLE mesh vendor model */
    printf("[BLE] TX sleep data: HR=%.1f RR=%.1f Move=%d Stage=%d\n",
           latest_features.heart_rate / 10.0f,
           latest_features.resp_rate / 10.0f,
           latest_features.movement,
           latest_features.sleep_stage);
}

/* ---- Battery Monitoring ---- */

static uint8_t read_battery_pct(void)
{
    /* In production: read ADC on PIN_BATT_SENSE, convert to percentage */
    /* 3.7V LiPo: 4.2V = 100%, 3.0V = 0% */
    return latest_features.battery_pct; /* stub */
}

/* ---- Adaptive Sampling ---- */

static uint16_t current_bcg_rate = BCG_SAMPLE_RATE_HZ;
static uint16_t current_accel_rate = ACCEL_SAMPLE_RATE_HZ;

static void adapt_sampling_rate(void)
{
    /* Reduce sampling during deep sleep to save battery */
    if (latest_features.sleep_stage == STAGE_DEEP) {
        current_bcg_rate = 100;   /* 100 Hz (still enough for HR) */
        current_accel_rate = 25;  /* 25 Hz */
    } else {
        current_bcg_rate = BCG_SAMPLE_RATE_HZ;
        current_accel_rate = ACCEL_SAMPLE_RATE_HZ;
    }
}

/* ---- Qi Charging Monitor ---- */

static bool is_charging(void)
{
    return nrf_gpio_pin_read(PIN_QI_STATUS) == 1;
}

/* ---- Main Loop ---- */

int main(void)
{
    printf("=== SleepSync Sleep Strip v1.0 ===\n");
    printf("nRF52832-QFAA\n");

    /* Initialize GPIOs */
    nrf_gpio_cfg_output(PIN_LED_R);
    nrf_gpio_cfg_output(PIN_LED_G);
    nrf_gpio_cfg_output(PIN_LED_B);
    nrf_gpio_cfg_output(PIN_POWER_EN);
    nrf_gpio_cfg_output(PIN_CHG_EN);
    nrf_gpio_pin_set(PIN_POWER_EN);  /* Enable main power rail */

    /* Initialize HX711 ADCs */
    hx711_t hx711_1, hx711_2;
    hx711_init(&hx711_1, PIN_HX711_1_SCK, PIN_HX711_1_DOUT, 1);
    hx711_init(&hx711_2, PIN_HX711_2_SCK, PIN_HX711_2_DOUT, 1);

    /* Initialize LIS3DH */
    lis3dh_init();

    /* Initialize BLE */
    /* In production: initialize SoftDevice, configure mesh, start advertising */
    printf("[BLE] Advertising as unprovisioned device\n");

    /* Status LED: green = ready */
    nrf_gpio_pin_set(PIN_LED_G);

    /* Main sampling loop */
    uint32_t sample_count = 0;
    uint32_t last_feature_ms = 0;

    while (1) {
        /* Read BCG from HX711 ADCs */
        if (hx711_is_ready(&hx711_1)) {
            int32_t val1 = hx711_read(&hx711_1);
            bcg_buffer[bcg_idx % BCG_WINDOW_SAMPLES] = val1;
            bcg_idx++;
        }
        if (hx711_is_ready(&hx711_2)) {
            int32_t val2 = hx711_read(&hx711_2);
            /* Combine with val1 or store separately (FSR 5-8) */
        }

        /* Read accelerometer at lower rate */
        if (sample_count % (BCG_SAMPLE_RATE_HZ / ACCEL_SAMPLE_RATE_HZ) == 0) {
            accel_buffer[accel_idx % ACCEL_WINDOW_SAMPLES] = lis3dh_read();
            accel_idx++;
        }

        sample_count++;

        /* Extract features every FEATURE_INTERVAL_MS */
        uint32_t elapsed_ms = sample_count * (1000 / current_bcg_rate);
        if (elapsed_ms - last_feature_ms >= FEATURE_INTERVAL_MS) {
            extract_features();
            ble_transmit_features();
            adapt_sampling_rate();
            read_battery_pct();
            last_feature_ms = elapsed_ms;

            /* Status LED: brief flash every 5s */
            nrf_gpio_pin_set(PIN_LED_B);
            nrf_delay_ms(10);
            nrf_gpio_pin_clear(PIN_LED_B);
        }

        /* Small delay between HX711 reads (~5ms at gain 128) */
        nrf_delay_us(5000 / current_bcg_rate);
    }

    return 0;
}
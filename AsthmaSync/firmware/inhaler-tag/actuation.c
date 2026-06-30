/**
 * AsthmaSync — Inhaler Tag Actuation Detection
 * ==============================================
 * Detects MDI actuation events from LSM6DSO accelerometer data.
 *
 * Algorithm:
 *   1. IMU interrupt triggers on acceleration > 2.5g (WAKE_UP feature)
 *   2. Capture 300ms window of 3-axis accel at 104 Hz
 *   3. Extract features: peak accel, duration, jerk, spectral entropy
 *   4. Run Random Forest classifier (4 classes: actuation/shake/drop/static)
 *   5. If class=actuation with confidence > 70% → log dose + notify hub
 *
 * License: MIT
 */

#include "config.h"
#include "actuation.h"
#include "../common/protocol.h"
#include <string.h>
#include <math.h>

/* ── nRF SDK includes ──────────────────────────────────── */
#include "nrf_drv_twi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_log.h"

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);

/* ── State ─────────────────────────────────────────────── */
static uint16_t s_dose_count = 0;
static uint32_t s_last_actuation_ms = 0;
static actuation_t s_last_actuation;
static actuation_callback_t s_callback = NULL;

/* ── I²C Helper ───────────────────────────────────────── */
static int lsm6dso_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, buf, 2, false) == NRF_SUCCESS ? 0 : -1;
}

static int lsm6dso_read(uint8_t reg, uint8_t *data, size_t len)
{
    if (nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, &reg, 1, true) != NRF_SUCCESS)
        return -1;
    return nrf_drv_twi_rx(&m_twi, LSM6DSO_ADDR, data, len) == NRF_SUCCESS ? 0 : -1;
}

/* ── LSM6DSO Initialization ───────────────────────────── */
int lsm6dso_init(void)
{
    /* Software reset */
    lsm6dso_write(0x12, 0x01);  /* CTRL3_C: BDU=1, IF_INC=1 */
    nrf_delay_ms(10);

    /* Read WHO_AM_I */
    uint8_t whoami;
    lsm6dso_read(0x0F, &whoami, 1);
    if (whoami != 0x6C) {
        NRF_LOG_ERROR("LSM6DSO WHO_AM_I mismatch: 0x%02X", whoami);
        return -1;
    }

    /* Configure accelerometer:
       CTRL1_XL: ODR=104Hz (0x40<<4) | FS=±16g (0x0C) | LPF2 */
    lsm6dso_write(0x10, 0x4C);  /* ODR 104 Hz, ±16g */

    /* Configure wake-up interrupt:
       WAKE_UP_DUR: threshold duration = 1 (2 LSB)
       WAKE_UP_THS: threshold = 2 (2g threshold when FS=16g: 2/2 = 1g/bit × 2 = 2g)
       ... but we want 2.5g threshold. With FS=16g: 1 LSB = 16/2^6 = 0.25g → 2.5g = 10 */
    lsm6dso_write(0x5C, 0x00);  /* WAKE_UP_DUR: duration=0 (1 LSB = 1/ODR) */
    lsm6dso_write(0x5B, 0x0A);  /* WAKE_UP_THS: threshold = 10 (= 2.5g) */

    /* Route wake-up to INT1 */
    lsm6dso_write(0x0D, 0x20);  /* MD1_CFG: wake-up on INT1 */

    /* Enable interrupt latching */
    lsm6dso_write(0x12, 0x44);  /* CTRL3_C: BDU=1, IF_INC=1, H_LACTIVE=0 */

    /* Clear any pending interrupts */
    uint8_t dummy;
    lsm6dso_read(0x1B, &dummy, 1);  /* WAKE_UP_SRC */

    NRF_LOG_INFO("LSM6DSO initialized (WHO_AM_I=0x%02X)", whoami);
    return 0;
}

/* ── Read Acceleration ─────────────────────────────────── */
int lsm6dso_read_accel(float *x_g, float *y_g, float *z_g)
{
    uint8_t data[6];
    if (lsm6dso_read(0x28, data, 6) != 0)  /* OUTX_L_A */
        return -1;

    int16_t raw_x = (int16_t)((data[1] << 8) | data[0]);
    int16_t raw_y = (int16_t)((data[3] << 8) | data[2]);
    int16_t raw_z = (int16_t)((data[5] << 8) | data[4]);

    /* ±16g full scale → sensitivity = 0.488 mg/LSB → divide by 2048 */
    const float sensitivity = 16.0f / 32768.0f;
    *x_g = raw_x * sensitivity;
    *y_g = raw_y * sensitivity;
    *z_g = raw_z * sensitivity;

    return 0;
}

/* ── Capture analysis window ───────────────────────────── */
static int capture_window(accel_sample_t *samples, int max_samples, int *out_count)
{
    int count = 0;
    uint32_t start = app_timer_cnt_get();
    uint32_t window_ticks = APP_TIMER_TICKS(ACT_WINDOW_MS);

    while (count < max_samples && (app_timer_cnt_get() - start) < window_ticks) {
        float x, y, z;
        if (lsm6dso_read_accel(&x, &y, &z) == 0) {
            samples[count].x = x;
            samples[count].y = y;
            samples[count].z = z;
            /* Calculate magnitude minus gravity (1g) */
            float mag = sqrtf(x*x + y*y + z*z);
            samples[count].magnitude = mag;
            samples[count].jerk = (count > 0) ?
                fabsf(mag - samples[count-1].magnitude) : 0.0f;
            count++;
        }
        nrf_delay_us(9615);  /* 104 Hz → 9.615 ms per sample */
    }

    *out_count = count;
    return 0;
}

/* ── Feature Extraction ────────────────────────────────── */
static int extract_features(const accel_sample_t *samples, int count,
                            float *features, int max_features)
{
    if (count == 0 || max_features < 12)
        return -1;

    /* 12 features: peak_mag, mean_mag, std_mag, peak_jerk, mean_jerk,
       std_jerk, spectral_centroid, spectral_entropy, duration,
       x_range, y_range, z_range */
    float peak_mag = 0, mean_mag = 0, std_mag = 0;
    float peak_jerk = 0, mean_jerk = 0, std_jerk = 0;
    float x_min=1e9, x_max=-1e9, y_min=1e9, y_max=-1e9, z_min=1e9, z_max=-1e9;

    for (int i = 0; i < count; i++) {
        if (samples[i].magnitude > peak_mag) peak_mag = samples[i].magnitude;
        mean_mag += samples[i].magnitude;
        mean_jerk += samples[i].jerk;
        if (samples[i].jerk > peak_jerk) peak_jerk = samples[i].jerk;
        if (samples[i].x < x_min) x_min = samples[i].x;
        if (samples[i].x > x_max) x_max = samples[i].x;
        if (samples[i].y < y_min) y_min = samples[i].y;
        if (samples[i].y > y_max) y_max = samples[i].y;
        if (samples[i].z < z_min) z_min = samples[i].z;
        if (samples[i].z > z_max) z_max = samples[i].z;
    }
    mean_mag /= count;
    mean_jerk /= count;

    for (int i = 0; i < count; i++) {
        std_mag += (samples[i].magnitude - mean_mag) * (samples[i].magnitude - mean_mag);
        std_jerk += (samples[i].jerk - mean_jerk) * (samples[i].jerk - mean_jerk);
    }
    std_mag = sqrtf(std_mag / count);
    std_jerk = sqrtf(std_jerk / count);

    /* Spectral features (simplified — real implementation uses FFT) */
    float spectral_centroid = peak_mag / (count * 0.01f);  /* simplified */
    float spectral_entropy = std_mag / (mean_mag + 0.001f); /* simplified */

    float duration_s = (float)count / 104.0f;

    features[0]  = peak_mag;
    features[1]  = mean_mag;
    features[2]  = std_mag;
    features[3]  = peak_jerk;
    features[4]  = mean_jerk;
    features[5]  = std_jerk;
    features[6]  = spectral_centroid;
    features[7]  = spectral_entropy;
    features[8]  = duration_s;
    features[9]  = x_max - x_min;
    features[10] = y_max - y_min;
    features[11] = z_max - z_min;

    return 12;
}

/* ── Actuation Classification (simplified RF inference) ── */
/* In production: load tflite-micro model or use fixed threshold rules.
   Here we use a simple decision tree that captures the essential
   signature of an MDI actuation: sharp impulse + quick decay. */
static int classify_actuation(const float *features, int feat_len,
                              float *out_confidence)
{
    float peak_mag   = features[0];
    float std_mag    = features[2];
    float peak_jerk  = features[3];
    float duration   = features[8];
    float z_range    = features[11];

    /* MDI actuation signature:
       - Peak 3-8g (hard press downward)
       - Duration < 200ms
       - High jerk (sharp impulse)
       - Z-axis dominant (vertical press) */
    if (peak_mag > 3.0f && peak_mag < 10.0f &&
        duration < 0.25f &&
        peak_jerk > 5.0f &&
        z_range > peak_mag * 0.4f) {
        *out_confidence = 0.85f;
        return ACT_CLASS_ACTUATION;
    }

    /* Pocket shake: moderate peak, longer duration, multi-axis */
    if (peak_mag > 2.0f && peak_mag < 4.0f && duration > 0.15f) {
        *out_confidence = 0.72f;
        return ACT_CLASS_POCKET_SHAKE;
    }

    /* Drop: very high peak, broadband, short duration */
    if (peak_mag > 10.0f && duration < 0.1f) {
        *out_confidence = 0.80f;
        return ACT_CLASS_DROP;
    }

    /* Static */
    *out_confidence = 0.90f;
    return ACT_CLASS_STATIC;
}

/* ── Actuation Event Handler ───────────────────────────── */
void actuation_on_interrupt(void)
{
    uint32_t now = app_timer_cnt_get();
    uint32_t elapsed_ms = (now - s_last_actuation_ms) * 1000 / 32768;

    /* Cooldown check */
    if (elapsed_ms < ACT_COOLDOWN_MS) {
        NRF_LOG_DEBUG("Actuation cooldown (%d ms < %d ms)", elapsed_ms, ACT_COOLDOWN_MS);
        /* Clear interrupt */
        uint8_t src;
        lsm6dso_read(0x1B, &src, 1);
        return;
    }

    NRF_LOG_INFO("Wake-up interrupt — capturing analysis window...");

    /* Capture 300ms window */
    static accel_sample_t samples[32];  /* 300ms @ 104Hz ≈ 31 samples */
    int count = 0;
    capture_window(samples, 32, &count);

    /* Extract features */
    float features[12];
    int feat_count = extract_features(samples, count, features, 12);
    if (feat_count < 0) {
        NRF_LOG_ERROR("Feature extraction failed");
        return;
    }

    /* Classify */
    float confidence = 0;
    int class_id = classify_actuation(features, feat_count, &confidence);

    NRF_LOG_INFO("Actuation analysis: class=%d conf=%.2f peak=%.1fg dur=%.0fms",
                 class_id, confidence, features[0], features[8] * 1000);

    /* Clear interrupt */
    uint8_t src;
    lsm6dso_read(0x1B, &src, 1);

    /* Accept if actuation with sufficient confidence */
    if (class_id == ACT_CLASS_ACTUATION && confidence > (ACT_MIN_CONFIDENCE / 100.0f)) {
        s_dose_count++;
        s_last_actuation_ms = now;

        /* Build actuation payload */
        memset(&s_last_actuation, 0, sizeof(s_last_actuation));
        s_last_actuation.actuation_type = 0;  /* MDI */
        s_last_actuation.confidence = (uint8_t)(confidence * 100);
        s_last_actuation.peak_accel_x1000 = (int16_t)(features[0] * 1000);
        s_last_actuation.duration_ms = (uint16_t)(features[8] * 1000);
        s_last_actuation.battery_pct = 100;  /* TODO: read battery */

        NRF_LOG_INFO("Actuation confirmed! Dose #%u (conf=%u%%)",
                     s_dose_count, s_last_actuation.confidence);

        /* Notify callback (BLE send) */
        if (s_callback) {
            s_callback(&s_last_actuation);
        }
    }
}

/* ── Register callback ─────────────────────────────────── */
void actuation_set_callback(actuation_callback_t cb)
{
    s_callback = cb;
}

/* ── Get dose count ────────────────────────────────────── */
uint16_t actuation_get_dose_count(void)
{
    return s_dose_count;
}

/* ── Get last actuation ────────────────────────────────── */
const actuation_t* actuation_get_last(void)
{
    return &s_last_actuation;
}

/* ── Reset dose count (new inhaler) ────────────────────── */
void actuation_reset_dose_count(void)
{
    s_dose_count = 0;
    NRF_LOG_INFO("Dose count reset (new inhaler)");
}
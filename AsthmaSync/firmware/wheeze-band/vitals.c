/**
 * AsthmaSync — Wheeze Band Vitals (PPG Processing)
 * =================================================
 * Reads MAX30101 PPG sensor for heart rate (HR), heart rate
 * variability (HRV/rmSSD), and SpO₂ estimation.
 * Also reads TMP117 skin temperature.
 *
 * License: MIT
 */

#include "config.h"
#include "vitals.h"
#include "../common/protocol.h"
#include <string.h>
#include <math.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(asthmasync_vitals, LOG_LEVEL_INF);

static const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

/* ── State ─────────────────────────────────────────────── */
static uint8_t s_hr = 0;
uint8_t s_spo2 = 98;
static uint16_t s_hrv_rmssd = 300;  /* 30.0 ms × 10 */
static float s_skin_temp = 33.0f;
static int32_t s_ir_buffer[100];    /* IR samples for SpO₂ */
static int32_t s_red_buffer[100];   /* Red samples for SpO₂ */
static uint8_t s_sample_idx = 0;
static float s_hr_baseline = 70.0f;
static float s_hrv_baseline = 40.0f;
static vitals_callback_t s_callback = NULL;

/* ── I²C Helpers ───────────────────────────────────────── */
static int max30101_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_write(i2c_dev, buf, 2, ADDR_MAX30101);
}

static int max30101_read(uint8_t reg, uint8_t *data, size_t len)
{
    if (i2c_write(i2c_dev, &reg, 1, ADDR_MAX30101) != 0)
        return -1;
    return i2c_read(i2c_dev, data, len, ADDR_MAX30101);
}

static int tmp117_write(uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = { reg, (val >> 8) & 0xFF, val & 0xFF };
    return i2c_write(i2c_dev, buf, 3, ADDR_TMP117);
}

static int tmp117_read(uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    if (i2c_write(i2c_dev, &reg, 1, ADDR_TMP117) != 0)
        return -1;
    if (i2c_read(i2c_dev, buf, 2, ADDR_TMP117) != 0)
        return -1;
    *val = (buf[0] << 8) | buf[1];
    return 0;
}

/* ── MAX30101 Initialization ───────────────────────────── */
int max30101_init(void)
{
    /* Reset */
    max30101_write(0x09, 0x40);  /* MODE_CONFIG: RESET */
    k_msleep(10);

    /* Mode: SpO₂ mode (Red + IR LEDs) */
    max30101_write(0x09, 0x03);  /* SPO2_MODE */

    /* SPO2_CONFIG: sample rate 100 Hz, pulse width 411µs (18-bit), range 4096 */
    /* 0x03 = SPO2_SR (100Hz) | 0x05 << 2 (411µs LED_PW) | 0x01 << 5 (4096nA) */
    max30101_write(0x0A, 0x27);  /* SPO2_CONGIG */

    /* LED pulse amplitude: Red 6.4mA, IR 6.4mA */
    /* 0x24 = ~6.4 mA (0x24 = 36 decimal, 0.2mA steps) */
    max30101_write(0x0C, 0x24);  /* LED1_PA (Red) */
    max30101_write(0x0D, 0x24);  /* LED2_PA (IR) */

    /* Proximity threshold for LED power saving (not used in SpO2 mode) */
    max30101_write(0x10, 0x7F);  /* PROX_INT_THRESH */

    /* Clear interrupt status */
    uint8_t status;
    max30101_read(0x00, &status, 1);

    LOG_INF("MAX30101 initialized (SpO2 mode, 100 Hz, 6.4 mA)");
    return 0;
}

/* ── Read PPG samples ──────────────────────────────────── */
int max30101_read_samples(int32_t *red, int32_t *ir)
{
    /* Check if data is ready */
    uint8_t status;
    if (max30101_read(0x00, &status, 1) != 0)
        return -1;
    if (!(status & 0x40))  /* PPG_RDY */
        return 0;  /* no data */

    /* Read FIFO: 6 bytes (3 Red + 3 IR, 18-bit each) */
    uint8_t data[6];
    if (max30101_read(0x07, data, 6) != 0)
        return -1;

    /* Red LED: bytes 0-2 (18-bit) */
    *red = ((int32_t)(data[0] << 16 | data[1] << 8 | data[2]) >> 8) & 0x3FFFF;
    /* IR LED: bytes 3-5 (18-bit) */
    *ir  = ((int32_t)(data[3] << 16 | data[4] << 8 | data[5]) >> 8) & 0x3FFFF;

    return 1;
}

/* ── Heart Rate Calculation (autocorrelation) ─────────── */
static uint8_t calculate_hr(const int32_t *samples, int count)
{
    if (count < 30)
        return 0;

    /* Autocorrelation method:
       - Remove DC (mean subtraction)
       - Autocorrelate at lags 15-150 (40-400 BPM → 0.25-2.5s at 100Hz)
       - Find peak → HR = 100 * 60 / best_lag */

    /* DC removal */
    float mean = 0;
    for (int i = 0; i < count; i++) mean += samples[i];
    mean /= count;

    float acf[150];
    for (int lag = 15; lag < 150 && lag < count; lag++) {
        float sum = 0;
        for (int i = 0; i < count - lag; i++) {
            float a = samples[i] - mean;
            float b = samples[i + lag] - mean;
            sum += a * b;
        }
        acf[lag - 15] = sum;
    }

    /* Find peak (excluding lag 0) */
    int best_lag = 0;
    float best_val = 0;
    for (int i = 0; i < 135; i++) {
        if (acf[i] > best_val) {
            best_val = acf[i];
            best_lag = i + 15;
        }
    }

    if (best_lag == 0 || best_val < 0)
        return 0;

    /* HR = sample_rate × 60 / lag = 100 × 60 / lag */
    uint8_t hr = (uint8_t)(PPG_SAMPLE_RATE_HZ * 60 / best_lag);
    if (hr < 30 || hr > 200)
        return 0;

    return hr;
}

/* ── SpO₂ Calculation (ratio of ratios) ────────────────── */
static uint8_t calculate_spo2(const int32_t *red, const int32_t *ir, int count)
{
    if (count < 30)
        return 0;

    /* Compute AC and DC components */
    float red_dc = 0, ir_dc = 0;
    float red_ac = 0, ir_ac = 0;

    for (int i = 0; i < count; i++) {
        red_dc += red[i];
        ir_dc  += ir[i];
    }
    red_dc /= count;
    ir_dc  /= count;

    /* AC = max - min */
    int32_t red_max = 0, red_min = 0x3FFFF;
    int32_t ir_max  = 0, ir_min  = 0x3FFFF;
    for (int i = 0; i < count; i++) {
        if (red[i] > red_max) red_max = red[i];
        if (red[i] < red_min) red_min = red[i];
        if (ir[i]  > ir_max)  ir_max  = ir[i];
        if (ir[i]  < ir_min)  ir_min  = ir[i];
    }
    red_ac = red_max - red_min;
    ir_ac  = ir_max  - ir_min;

    if (ir_dc == 0 || ir_ac == 0)
        return 0;

    /* R = (red_ac/red_dc) / (ir_ac/ir_dc) */
    float R = (red_ac / red_dc) / (ir_ac / ir_dc);

    /* SpO₂ = 110 - 25 × R (empirical calibration)
       (Typical calibration curve for MAX30101) */
    float spo2 = 110.0f - 25.0f * R;

    /* Clamp to valid range */
    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 70.0f)  spo2 = 70.0f;

    return (uint8_t)spo2;
}

/* ── HRV Calculation (rmSSD) ───────────────────────────── */
static uint16_t calculate_hrv(const int32_t *samples, int count, uint8_t hr)
{
    if (count < 60 || hr == 0)
        return 0;

    /* Detect peaks (simplified: threshold at 0.7 × max) */
    float mean = 0;
    for (int i = 0; i < count; i++) mean += samples[i];
    mean /= count;

    float max_val = 0;
    for (int i = 0; i < count; i++) {
        float v = samples[i] - mean;
        if (v > max_val) max_val = v;
    }

    float threshold = 0.6f * max_val;
    uint32_t peak_times[20];
    int peak_count = 0;
    bool above = false;

    for (int i = 0; i < count && peak_count < 20; i++) {
        float v = samples[i] - mean;
        if (v > threshold && !above) {
            above = true;
            peak_times[peak_count++] = i;
        } else if (v < threshold * 0.5f) {
            above = false;
        }
    }

    if (peak_count < 3)
        return 0;

    /* Compute RR intervals (in ms, at 100 Hz sample rate) */
    float rr[20];
    for (int i = 1; i < peak_count; i++) {
        rr[i - 1] = (peak_times[i] - peak_times[i - 1]) * 1000.0f / PPG_SAMPLE_RATE_HZ;
    }

    /* rmSSD = sqrt(mean(diff(RR)²)) */
    float sum_sq = 0;
    for (int i = 1; i < peak_count - 1; i++) {
        float diff = rr[i] - rr[i - 1];
        sum_sq += diff * diff;
    }
    float rmssd = sqrtf(sum_sq / (peak_count - 2));

    return (uint16_t)(rmssd * 10);  /* × 10 for fixed-point */
}

/* ── Process PPG samples (called at 100 Hz) ────────────── */
void vitals_on_sample(int32_t red, int32_t ir)
{
    s_red_buffer[s_sample_idx] = red;
    s_ir_buffer[s_sample_idx]  = ir;
    s_sample_idx++;

    if (s_sample_idx >= 100) {
        /* Process 1-second window */
        s_hr = calculate_hr(s_ir_buffer, 100);
        s_spo2 = calculate_spo2(s_red_buffer, s_ir_buffer, 100);
        s_hrv_rmssd = calculate_hrv(s_ir_buffer, 100, s_hr);

        /* Update baseline (exponential moving average) */
        if (s_hr > 0) {
            s_hr_baseline = 0.95f * s_hr_baseline + 0.05f * s_hr;
        }
        if (s_hrv_rmssd > 0) {
            s_hrv_baseline = 0.95f * s_hrv_baseline + 0.05f * (s_hrv_rmssd / 10.0f);
        }

        s_sample_idx = 0;

        /* Check for low SpO₂ */
        if (s_spo2 < THRESH_SPO2_RED && s_spo2 > 0) {
            LOG_WRN("SpO₂ low: %u%%", s_spo2);
            if (s_callback) {
                vitals_t v = {
                    .hr = s_hr,
                    .spo2 = s_spo2,
                    .hrv_rmssd_x10 = s_hrv_rmssd,
                    .skin_temp_x10 = (int16_t)(s_skin_temp * 10),
                };
                s_callback(VITALS_EVENT_SPO2_LOW, &v);
            }
        }

        /* Check for HRV drop */
        if (s_hrv_rmssd > 0) {
            float hrv = s_hrv_rmssd / 10.0f;
            if (hrv < s_hrv_baseline * (1.0f - THRESH_HRV_DROP_PCT / 100.0f)) {
                LOG_WRN("HRV drop: %.1f ms (baseline: %.1f ms)", hrv, s_hrv_baseline);
                if (s_callback) {
                    vitals_t v = {
                        .hr = s_hr,
                        .spo2 = s_spo2,
                        .hrv_rmssd_x10 = s_hrv_rmssd,
                        .skin_temp_x10 = (int16_t)(s_skin_temp * 10),
                    };
                    s_callback(VITALS_EVENT_HRV_DROP, &v);
                }
            }
        }
    }
}

/* ── Read skin temperature (TMP117) ────────────────────── */
int tmp117_read_temp(float *temp_c)
{
    uint16_t raw;
    if (tmp117_read(0x00, &raw) != 0)
        return -1;

    /* TMP117: temperature = raw × 0.0078125 °C */
    int16_t temp_raw = (int16_t)raw;
    *temp_c = temp_raw * 0.0078125f;
    s_skin_temp = *temp_c;

    return 0;
}

/* ── Pack vitals into protocol struct ──────────────────── */
int vitals_pack(vitals_t *out)
{
    out->hr = s_hr;
    out->spo2 = s_spo2;
    out->hrv_rmssd_x10 = s_hrv_rmssd;
    out->skin_temp_x10 = (int16_t)(s_skin_temp * 10);
    out->activity = 0;  /* TODO: read from LSM6DSO */
    memset(out->reserved, 0, sizeof(out->reserved));
    return 0;
}

/* ── Initialize vitals ──────────────────────────────────── */
int vitals_init(void)
{
    int ret = max30101_init();
    if (ret != 0) {
        LOG_ERR("MAX30101 init failed: %d", ret);
        return -1;
    }

    /* TMP117 config: continuous conversion, 10 Hz */
    tmp117_write(0x01, 0x0220);  /* CONFIG: 10 Hz, averaging=8 */

    s_hr_baseline = 70.0f;
    s_hrv_baseline = 40.0f;
    s_sample_idx = 0;

    LOG_INF("Vitals initialized (MAX30101 + TMP117)");
    return 0;
}

/* ── Register callback ──────────────────────────────────── */
void vitals_set_callback(vitals_callback_t cb)
{
    s_callback = cb;
}
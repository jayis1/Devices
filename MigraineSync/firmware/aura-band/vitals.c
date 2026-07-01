/**
 * MigraineSync — Aura Band Vitals
 * ===============================
 * MAX30101 PPG processing for HR, HRV (RMSSD), SpO₂.
 * TMP117 skin temperature. LSM6DSO activity classification.
 *
 * nRF52840 Zephyr RTOS.
 *
 * License: MIT
 */

#include "vitals.h"
#include "config.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/util.h>
#include <math.h>
#include <string.h>

/* ── MAX30101 Registers ─────────────────────────────────── */
#define MAX30101_REG_INT_STATUS_1  0x00
#define MAX30101_REG_INT_STATUS_2  0x01
#define MAX30101_REG_INT_ENABLE_1  0x02
#define MAX30101_REG_FIFO_WR_PTR   0x04
#define MAX30101_REG_FIFO_RD_PTR   0x06
#define MAX30101_REG_FIFO_DATA     0x07
#define MAX30101_REG_FIFO_CONFIG   0x08
#define MAX30101_REG_MODE_CONFIG   0x09
#define MAX30101_REG_SPO2_CONFIG   0x0A
#define MAX30101_REG_LED1_PA       0x0C   /* red */
#define MAX30101_REG_LED2_PA       0x0D   /* IR */
#define MAX30101_REG_LED3_PA       0x10   /* green */
#define MAX30101_REG_PROX_INT      0x11

#define MAX30101_MODE_SPO2         0x03   /* red + IR */
#define MAX30101_MODE_MULTI        0x07   /* red + IR + green */

#define PPG_BUFFER_SIZE  1500   /* 15 seconds at 100 Hz */

static uint16_t s_green_buf[PPG_BUFFER_SIZE];
static uint16_t s_red_buf[PPG_BUFFER_SIZE];
static uint16_t s_ir_buf[PPG_BUFFER_SIZE];
static int s_sample_count = 0;

/* Skin temp history for slope calculation */
#define SKIN_TEMP_HISTORY_LEN  360   /* 6 hours at 1/min */
static float s_skin_temp_history[SKIN_TEMP_HISTORY_LEN];
static int s_skin_temp_idx = 0;

/* ── I²C helpers (Zephyr) ───────────────────────────────── */
static const struct device *i2c_dev;

static int i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_write(i2c_dev, buf, 2, addr);
}

static int i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_write_read(i2c_dev, addr, &reg, 1, buf, len);
}

/* ── MAX30101 Init ──────────────────────────────────────── */
int vitals_init(void)
{
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev))
        return -1;

    /* Reset */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_MODE_CONFIG, 0x40);
    k_msleep(10);

    /* Mode: multi-LED (red + IR + green) */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_MODE_CONFIG, MAX30101_MODE_MULTI);

    /* SPO2 config: ADC range 4096, sample rate 100 Hz, pulse width 411 µs */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_SPO2_CONFIG, 0x27);

    /* FIFO config: avg 4 samples, almost full at 15 */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_FIFO_CONFIG, 0x4F);

    /* LED currents: ~6.4 mA each */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_LED1_PA, 0x24);  /* red */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_LED2_PA, 0x24);  /* IR */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_LED3_PA, 0x24);  /* green */

    /* Clear FIFO */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_FIFO_WR_PTR, 0x00);
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_FIFO_RD_PTR, 0x00);

    /* Enable PPG ready interrupt */
    i2c_write_reg(MAX30101_ADDR, MAX30101_REG_INT_ENABLE_1, 0x80);

    memset(s_green_buf, 0, sizeof(s_green_buf));
    memset(s_red_buf, 0, sizeof(s_red_buf));
    memset(s_ir_buf, 0, sizeof(s_ir_buf));
    s_sample_count = 0;

    return 0;
}

/* ── Read FIFO samples ──────────────────────────────────── */
static int read_fifo_samples(uint16_t *green, uint16_t *red, uint16_t *ir, int max_samples)
{
    uint8_t wr_ptr, rd_ptr;
    i2c_read_regs(MAX30101_ADDR, MAX30101_REG_FIFO_WR_PTR, &wr_ptr, 1);
    i2c_read_regs(MAX30101_ADDR, MAX30101_REG_FIFO_RD_PTR, &rd_ptr, 1);

    int num_samples = (wr_ptr - rd_ptr) & 0x1F;
    if (num_samples > max_samples)
        num_samples = max_samples;

    for (int i = 0; i < num_samples; i++) {
        uint8_t data[9];  /* 3 samples × 3 bytes (18-bit each) */
        i2c_read_regs(MAX30101_ADDR, MAX30101_REG_FIFO_DATA, data, 9);

        /* Green = bytes 0-2, Red = bytes 3-5, IR = bytes 6-8 */
        green[i] = ((uint16_t)(data[0] & 0x03) << 16) | ((uint16_t)data[1] << 8) | data[2];
        red[i]   = ((uint16_t)(data[3] & 0x03) << 16) | ((uint16_t)data[4] << 8) | data[5];
        ir[i]    = ((uint16_t)(data[6] & 0x03) << 16) | ((uint16_t)data[7] << 8) | data[8];
    }

    return num_samples;
}

/* ── DC removal filter ──────────────────────────────────── */
static void dc_remove(uint16_t *input, float *output, int n, float alpha)
{
    float prev_w = 0.0f;
    for (int i = 0; i < n; i++) {
        float w = (float)input[i] + alpha * prev_w;
        output[i] = prev_w - w;
        prev_w = w;
    }
}

/* ── Simple peak detection for HR ───────────────────────── */
static int detect_peaks(float *signal, int n, int *peaks, int max_peaks)
{
    int count = 0;
    float threshold = 0.0f;

    /* Compute RMS as threshold reference */
    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++)
        sum_sq += signal[i] * signal[i];
    threshold = 0.6f * sqrtf(sum_sq / n);

    for (int i = 2; i < n - 2 && count < max_peaks; i++) {
        if (signal[i] > threshold &&
            signal[i] > signal[i-1] && signal[i] > signal[i+1] &&
            signal[i] > signal[i-2] && signal[i] > signal[i+2]) {
            peaks[count++] = i;
        }
    }
    return count;
}

/* ── Compute HR from peak intervals ─────────────────────── */
static uint8_t compute_hr(int *peaks, int n_peaks, float sample_rate)
{
    if (n_peaks < 2)
        return 0;

    float total_interval = 0.0f;
    int count = 0;
    for (int i = 1; i < n_peaks; i++) {
        int interval = peaks[i] - peaks[i-1];
        if (interval > 0) {
            total_interval += interval;
            count++;
        }
    }

    if (count == 0)
        return 0;

    float avg_interval = total_interval / count;
    float hr = (sample_rate * 60.0f) / avg_interval;
    if (hr < 30 || hr > 220)
        return 0;
    return (uint8_t)hr;
}

/* ── Compute HRV (RMSSD) from peak intervals ────────────── */
static float compute_hrv_rmssd(int *peaks, int n_peaks, float sample_rate)
{
    if (n_peaks < 3)
        return 0.0f;

    float sum_sq_diff = 0.0f;
    int count = 0;
    for (int i = 2; i < n_peaks; i++) {
        float rr1 = (peaks[i-1] - peaks[i-2]) / sample_rate * 1000.0f;  /* ms */
        float rr2 = (peaks[i] - peaks[i-1]) / sample_rate * 1000.0f;
        float diff = rr2 - rr1;
        sum_sq_diff += diff * diff;
        count++;
    }

    if (count == 0)
        return 0.0f;

    return sqrtf(sum_sq_diff / count);
}

/* ── Compute SpO₂ from red/IR ratio ─────────────────────── */
static uint8_t compute_spo2(uint16_t *red, uint16_t *ir, int n)
{
    float red_sum = 0, ir_sum = 0;
    for (int i = 0; i < n; i++) {
        red_sum += red[i];
        ir_sum += ir[i];
    }
    float red_avg = red_sum / n;
    float ir_avg = ir_sum / n;

    if (ir_avg == 0 || red_avg == 0)
        return 0;

    /* Compute AC components (std dev as proxy) */
    float red_ac = 0, ir_ac = 0;
    for (int i = 0; i < n; i++) {
        red_ac += fabsf(red[i] - red_avg);
        ir_ac += fabsf(ir[i] - ir_avg);
    }
    red_ac /= n;
    ir_ac /= n;

    if (ir_ac == 0 || red_avg == 0)
        return 0;

    float ratio = (red_ac / red_avg) / (ir_ac / ir_avg);
    float spo2 = 110.0f - 25.0f * ratio;

    if (spo2 > 100) spo2 = 100;
    if (spo2 < 70) spo2 = 70;
    return (uint8_t)spo2;
}

/* ── Read vitals ────────────────────────────────────────── */
int vitals_read(vitals_t *vitals)
{
    if (!vitals)
        return -1;

    memset(vitals, 0, sizeof(*vitals));

    /* Read FIFO samples */
    uint16_t green[32], red[32], ir[32];
    int n = read_fifo_samples(green, red, ir, 32);

    if (n < 10) {
        /* Not enough samples — return last known or zero */
        return -1;
    }

    /* Accumulate into buffer */
    for (int i = 0; i < n && s_sample_count < PPG_BUFFER_SIZE; i++) {
        s_green_buf[s_sample_count] = green[i];
        s_red_buf[s_sample_count] = red[i];
        s_ir_buf[s_sample_count] = ir[i];
        s_sample_count++;
    }

    /* Process when buffer is full (15 seconds of data) */
    if (s_sample_count >= PPG_BUFFER_SIZE) {
        float filtered[PPG_BUFFER_SIZE];
        dc_remove(s_green_buf, filtered, PPG_BUFFER_SIZE, 0.95f);

        int peaks[128];
        int n_peaks = detect_peaks(filtered, PPG_BUFFER_SIZE, peaks, 128);

        vitals->hr_bpm = compute_hr(peaks, n_peaks, PPG_SAMPLE_RATE_HZ);
        vitals->hrv_rmssd = compute_hrv_rmssd(peaks, n_peaks, PPG_SAMPLE_RATE_HZ);
        vitals->spo2_pct = compute_spo2(s_red_buf, s_ir_buf, PPG_BUFFER_SIZE);

        /* Reset buffer */
        s_sample_count = 0;
    }

    /* Read skin temperature from TMP117 */
    float temp;
    if (skin_temp_read(&temp) == 0) {
        vitals->skin_temp_c = temp;

        /* Update history + compute 6-hour slope */
        s_skin_temp_history[s_skin_temp_idx] = temp;
        s_skin_temp_idx = (s_skin_temp_idx + 1) % SKIN_TEMP_HISTORY_LEN;

        if (s_skin_temp_idx > 10) {
            int oldest = (s_skin_temp_idx + 1) % SKIN_TEMP_HISTORY_LEN;
            int newest = (s_skin_temp_idx - 1 + SKIN_TEMP_HISTORY_LEN) % SKIN_TEMP_HISTORY_LEN;
            float dt_hours = SKIN_TEMP_HISTORY_LEN / 60.0f;  /* approx */
            vitals->skin_temp_slope =
                (s_skin_temp_history[newest] - s_skin_temp_history[oldest]) / dt_hours;
        }
    }

    vitals->activity = activity_get_level();
    return 0;
}

/* ── TMP117 skin temperature ────────────────────────────── */
int skin_temp_read(float *temp_c)
{
    uint8_t data[2];
    int ret = i2c_read_regs(TMP117_ADDR, 0x00, data, 2);  /* TEMP_RESULT reg */
    if (ret != 0)
        return -1;

    int16_t raw = (int16_t)((data[0] << 8) | data[1]);
    /* TMP117: 0.0078125 °C per LSB, no calibration needed (±0.1°C factory) */
    *temp_c = raw * 0.0078125f;
    return 0;
}

/* ── LSM6DSO activity classification ────────────────────── */
uint8_t activity_get_level(void)
{
    /* In production: read LSM6DSO accelerometer, compute magnitude,
     * classify based on thresholds:
     *   < 1.1 m/s²  → 0 (sleep)
     *   < 1.5 m/s²  → 1 (sedentary)
     *   < 3.0 m/s²  → 2 (light)
     *   < 6.0 m/s²  → 3 (moderate)
     *   >= 6.0 m/s² → 4 (vigorous)
     */

    uint8_t data[6];
    int ret = i2c_read_regs(LSM6DSO_ADDR, 0x28, data, 6);  /* OUTX_L_A */
    if (ret != 0)
        return 1;  /* default sedentary */

    int16_t ax = (int16_t)((data[0] << 8) | data[1]);
    int16_t ay = (int16_t)((data[2] << 8) | data[3]);
    int16_t az = (int16_t)((data[4] << 8) | data[5]);

    /* Convert to m/s² (±2g range, 0.061 mg/LSB) */
    float x = ax * 0.061f / 1000.0f * 9.81f;
    float y = ay * 0.061f / 1000.0f * 9.81f;
    float z = az * 0.061f / 1000.0f * 9.81f;
    float mag = sqrtf(x*x + y*y + z*z);

    /* Subtract gravity (~9.81) for dynamic acceleration */
    float dynamic = fabsf(mag - 9.81f);

    if (dynamic < 0.3f) return 0;  /* sleep */
    if (dynamic < 1.0f) return 1;  /* sedentary */
    if (dynamic < 3.0f) return 2;  /* light */
    if (dynamic < 6.0f) return 3;  /* moderate */
    return 4;                       /* vigorous */
}
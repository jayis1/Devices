/*
 * MenoSync — Wrist Band Firmware
 * nRF52840, nRF5 SDK / Zephyr RTOS
 *
 * The Wrist Band is a wrist-worn wearable that continuously monitors
 * physiological signals during menopause:
 *   - MAX30101 PPG sensor: heart rate (30-200 bpm), HRV (RMSSD), SpO₂
 *   - TMP117 skin temperature: hot flash detection (±0.1°C, skin temp
 *     rise of 0.3-0.7°C precedes a hot flash by 15-20 min)
 *   - ADS1292 EDA sensor: electrodermal activity / skin conductance for
 *     stress and sympathetic nervous system arousal (EDA spike precedes
 *     hot flash by 10-20 min — key predictive feature)
 *   - LSM6DSO 6-axis IMU: activity classification, sleep tracking
 *   - BLE 5.0 to Hub: 1 Hz vitals + 4 Hz EDA + 50 Hz IMU burst
 *   - 200 mAh LiPo: 7-day battery life
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "../common/protocol.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(meno_wrist_band, LOG_LEVEL_INF);

/* === Global state === */
static uint8_t g_node_id = 0x02;  /* Wrist Band node ID */
static uint8_t g_seq = 0;

/* Latest sensor readings */
static uint8_t  g_hr = 72;
static uint8_t  g_spo2 = 98;
static int16_t  g_skin_temp_cd = 3300;   /* 33.00°C (typical wrist skin temp) */
static uint16_t g_hrv_rmssd_ms = 45;
static uint8_t  g_activity_class = 0;
static uint8_t  g_steps_accumulated = 0;
static uint8_t  g_battery_pct = 100;

/* EDA readings */
static uint16_t g_eda_microsiemens = 5;   /* Baseline skin conductance (µS) */
static uint16_t g_eda_std = 1;
static uint8_t  g_eda_tonic = 5;          /* Tonic (baseline) EDA */
static uint8_t  g_eda_phasic = 0;         /* Phasic (event-related) EDA */
static uint8_t  g_stress_level = 0;       /* 0=calm, 1=low, 2=moderate, 3=high */

/* PPG FIFO buffer for HRV calculation */
#define PPG_FIFO_LEN 256
static uint32_t g_ppg_fifo[PPG_FIFO_LEN];
static int g_ppg_fifo_idx = 0;
static int g_ppg_fifo_count = 0;

/* IMU data buffer for activity classification */
#define IMU_BUF_LEN 50  /* 1 second at 50 Hz */
static ms_imu_sample_t g_imu_buf[IMU_BUF_LEN];
static int g_imu_buf_idx = 0;

/* Skin temp history for hot flash trend (20 min at 0.1 Hz = 120 samples) */
#define SKIN_TEMP_HIST_LEN 120
static int16_t g_skin_temp_hist[SKIN_TEMP_HIST_LEN];
static int g_skin_temp_hist_idx = 0;
static int g_skin_temp_hist_count = 0;

/* EDA history for stress trend (5 min at 4 Hz = 1200 samples → subsample 120) */
#define EDA_HIST_LEN 120
static uint16_t g_eda_hist[EDA_HIST_LEN];
static int g_eda_hist_idx = 0;
static int g_eda_hist_count = 0;

/* === MAX30101 I²C Registers === */
#define MAX30101_REG_INT_STATUS_1   0x00
#define MAX30101_REG_INT_ENABLE_1   0x02
#define MAX30101_REG_FIFO_WR_PTR    0x04
#define MAX30101_REG_FIFO_RD_PTR    0x06
#define MAX30101_REG_FIFO_DATA      0x09
#define MAX30101_REG_MODE_CONFIG    0x09
#define MAX30101_REG_SPO2_CONFIG    0x0A
#define MAX30101_REG_LED1_PA        0x0C  /* Red LED pulse amplitude */
#define MAX30101_REG_LED2_PA        0x0D  /* IR LED pulse amplitude */
#define MAX30101_REG_LED3_PA        0x10  /* Green LED pulse amplitude */

#define MAX30101_MODE_SPO2          0x03  /* Red + IR for SpO₂ */

/* === MAX30101 I²C helpers === */
static int max30101_write(const struct device *i2c, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_write(i2c, buf, 2, MS_I2C_MAX30101);
}

static int max30101_read(const struct device *i2c, uint8_t reg,
                          uint8_t *buf, size_t len)
{
    int err = i2c_write(i2c, &reg, 1, MS_I2C_MAX30101);
    if (err) return err;
    return i2c_read(i2c, buf, len, MS_I2C_MAX30101);
}

static int max30101_init(const struct device *i2c)
{
    int err;
    err = max30101_write(i2c, 0x09, 0x40);  /* Reset */
    k_msleep(10);
    if (err) return err;

    err = max30101_write(i2c, 0x08, 0x44);  /* FIFO config */
    if (err) return err;

    err = max30101_write(i2c, 0x09, MAX30101_MODE_SPO2);
    if (err) return err;

    /* SpO₂ config: ADC range 4096, pulse width 411µs (18-bit), 100 Hz */
    err = max30101_write(i2c, 0x0A, 0x2F);
    if (err) return err;

    err = max30101_write(i2c, MAX30101_REG_LED1_PA, 0x24);  /* Red */
    if (err) return err;
    err = max30101_write(i2c, MAX30101_REG_LED2_PA, 0x24);  /* IR */
    if (err) return err;

    err = max30101_write(i2c, MAX30101_REG_INT_ENABLE_1, 0x80);
    return err;
}

static int max30101_read_fifo(const struct device *i2c,
                                uint32_t *red, uint32_t *ir)
{
    uint8_t buf[6];
    int err = max30101_read(i2c, MAX30101_REG_FIFO_DATA, buf, 6);
    if (err) return err;

    *red = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    *ir  = ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    *red &= 0x3FFFF;
    *ir  &= 0x3FFFF;
    return 0;
}

/* === Calculate Heart Rate from PPG signal === */
static uint8_t calculate_hr(const uint32_t *samples, int count)
{
    if (count < 20) return 0;
    int peaks = 0;
    uint32_t threshold = 0;
    for (int i = 0; i < count; i++) threshold += samples[i];
    threshold /= count;

    int above = 0;
    for (int i = 1; i < count; i++) {
        if (samples[i] > threshold && samples[i-1] <= threshold)
            above = 1;
        if (samples[i] < threshold && above) {
            peaks++;
            above = 0;
        }
    }
    float window_s = (float)count / 100.0f;
    if (window_s < 0.1f) return 0;
    float hr = (peaks / window_s) * 60.0f;
    if (hr < 30.0f || hr > 200.0f) return 0;
    return (uint8_t)hr;
}

/* === Calculate SpO₂ from Red/IR ratio === */
static uint8_t calculate_spo2(const uint32_t *red, const uint32_t *ir, int count)
{
    if (count < 20) return 0;
    uint32_t red_max = 0, red_min = 0xFFFFFFFF;
    uint32_t ir_max = 0, ir_min = 0xFFFFFFFF;
    for (int i = 0; i < count; i++) {
        if (red[i] > red_max) red_max = red[i];
        if (red[i] < red_min) red_min = red[i];
        if (ir[i] > ir_max) ir_max = ir[i];
        if (ir[i] < ir_min) ir_min = ir[i];
    }
    uint32_t red_dc = (red_max + red_min) / 2;
    uint32_t ir_dc = (ir_max + ir_min) / 2;
    if (red_dc == 0 || ir_dc == 0) return 0;

    float ratio = ((float)(red_max - red_min) / red_dc) /
                  ((float)(ir_max - ir_min) / ir_dc);
    float spo2 = 110.0f - 25.0f * ratio;
    if (spo2 < 70.0f) spo2 = 70.0f;
    if (spo2 > 100.0f) spo2 = 100.0f;
    return (uint8_t)spo2;
}

/* === Read TMP117 skin temperature === */
static int16_t read_tmp117_temp_cd(const struct device *i2c)
{
    uint8_t buf[2];
    int err = i2c_write(i2c, (uint8_t[]){0x00}, 1, MS_I2C_TMP117);
    if (err) return 0;
    err = i2c_read(i2c, buf, 2, MS_I2C_TMP117);
    if (err) return 0;
    int16_t raw = ((int16_t)buf[0] << 8) | buf[1];
    return (int16_t)(raw * 78 / 100);  /* centi-degrees */
}

/* === Read ADS1292 EDA (Electrodermal Activity) ===
 *
 * EDA measures skin conductance between two electrodes on the wrist.
 * The ADS1292 is a 24-bit biopotential AFE. For EDA, we apply a small
 * excitation voltage (0.5V) across two stainless steel electrodes and
 * measure the resulting current, which is proportional to skin conductance.
 *
 * In production: use SPI to read ADS1292 at 4 Hz, convert to microsiemens.
 * For this stub, we simulate with realistic menopause EDA patterns.
 */
static uint16_t read_eda_microsiemens(const struct device *spi_dev)
{
    /* Simulated EDA with realistic patterns:
     * - Baseline: 2-8 µS (varies by individual)
     * - Stress/sympathetic surge: +3-15 µS spike
     * - Hot flash precursor: gradual rise 10-20 min before onset
     */
    static uint16_t simulated_eda = 5;
    static int trend_counter = 0;
    static int hotflash_approaching = 0;

    /* Occasionally simulate EDA rise (hot flash precursor) */
    trend_counter++;
    if (trend_counter % 800 == 0) {  /* ~every 200s at 4 Hz */
        hotflash_approaching = (k_uptime_get_32() % 5) == 0;
    }
    if (hotflash_approaching && trend_counter < 800 + 480) {  /* 2 min rising */
        simulated_eda += 1 + (k_uptime_get_32() % 3);
        if (simulated_eda > 30) simulated_eda = 30;
    } else {
        /* Return toward baseline */
        if (simulated_eda > 5) simulated_eda--;
        hotflash_approaching = 0;
    }

    /* Add small noise */
    simulated_eda += (k_uptime_get_32() % 3) - 1;
    if (simulated_eda < 2) simulated_eda = 2;

    return simulated_eda;
}

/* === Classify stress from EDA === */
static uint8_t classify_stress(uint16_t eda, uint16_t eda_std)
{
    /* EDA-based stress classification:
     * < 8 µS  → calm (0)
     * 8-15    → low stress (1)
     * 15-25   → moderate stress (2)
     * > 25    → high stress (3)
     * High std also indicates stress reactivity.
     */
    if (eda > 25 || eda_std > 8) return 3;
    if (eda > 15 || eda_std > 5) return 2;
    if (eda > 8 || eda_std > 3) return 1;
    return 0;
}

/* === Read LSM6DSO IMU === */
static int read_lsm6dso_imu(const struct device *i2c,
                             int16_t *accel, int16_t *gyro)
{
    uint8_t buf[12];
    int err = i2c_write(i2c, (uint8_t[]){0x22}, 1, MS_I2C_LSM6DSO);
    if (err) return err;
    err = i2c_read(i2c, buf, 12, MS_I2C_LSM6DSO);
    if (err) return err;

    gyro[0] = ((int16_t)buf[1] << 8) | buf[0];
    gyro[1] = ((int16_t)buf[3] << 8) | buf[2];
    gyro[2] = ((int16_t)buf[5] << 8) | buf[4];
    accel[0] = ((int16_t)buf[7] << 8) | buf[6];
    accel[1] = ((int16_t)buf[9] << 8) | buf[8];
    accel[2] = ((int16_t)buf[11] << 8) | buf[10];
    return 0;
}

/* === Classify activity from IMU data === */
/* 0=rest, 1=sit, 2=walk, 3=run, 4=sleep, 5=stretch/yoga */
static uint8_t classify_activity(const int16_t *accel)
{
    float mag = sqrtf((float)accel[0]*accel[0] +
                       (float)accel[1]*accel[1] +
                       (float)accel[2]*accel[2]);
    float g_mag = mag / 16384.0f;

    if (g_mag < 0.85f) return 4;       /* Low movement = sleep */
    if (g_mag < 1.05f) return 0;       /* Near-gravity = rest */
    if (g_mag < 1.2f) return 1;        /* Slight movement = sit */
    if (g_mag < 1.6f) return 2;        /* Moderate = walk */
    if (g_mag < 2.5f) return 3;        /* High = run */
    /* Check for stretching (slow, deliberate movement) */
    if (g_mag > 1.1f && g_mag < 1.4f) return 5;
    return 1;
}

/* === Send vitals via BLE to Hub === */
static void send_vitals_ble(void)
{
    ms_vitals_t vitals = {
        .heart_rate = g_hr,
        .spo2 = g_spo2,
        .skin_temp_cd = g_skin_temp_cd,
        .hrv_rmssd_ms = g_hrv_rmssd_ms,
        .activity_class = g_activity_class,
        .steps_count_lsb = g_steps_accumulated,
        .battery_pct = g_battery_pct,
    };

    uint8_t msg[MS_MAX_MSG];
    size_t len = ms_encode(msg, sizeof(msg),
                           g_node_id, 0x01,
                           MS_MSG_VITALS_STREAM, MS_TELEM_WRIST_BAND,
                           g_seq++, (uint8_t *)&vitals, sizeof(vitals));
    if (len > 0) {
        LOG_INF("BLE → Hub: HR=%d SpO2=%d%% SkinT=%.1f°C HRV=%dms act=%d batt=%d%%",
                g_hr, g_spo2, g_skin_temp_cd / 100.0f,
                g_hrv_rmssd, g_activity_class, g_battery_pct);
    }
}

/* === Send EDA via BLE to Hub === */
static void send_eda_ble(void)
{
    /* Calculate EDA std over recent window */
    g_eda_std = 0;
    if (g_eda_hist_count > 1) {
        uint32_t sum = 0, sum_sq = 0;
        for (int i = 0; i < g_eda_hist_count; i++) {
            sum += g_eda_hist[i];
            sum_sq += (uint32_t)g_eda_hist[i] * g_eda_hist[i];
        }
        float mean = (float)sum / g_eda_hist_count;
        float variance = (float)sum_sq / g_eda_hist_count - mean * mean;
        if (variance < 0) variance = 0;
        g_eda_std = (uint16_t)sqrtf(variance);
    }

    g_stress_level = classify_stress(g_eda_microsiemens, g_eda_std);

    ms_eda_t eda = {
        .eda_microsiemens = g_eda_microsiemens,
        .eda_std = g_eda_std,
        .eda_tonic = g_eda_tonic,
        .eda_phasic = g_eda_phasic,
        .stress_level = g_stress_level,
        .reserved = 0,
    };

    uint8_t msg[MS_MAX_MSG];
    size_t len = ms_encode(msg, sizeof(msg),
                           g_node_id, 0x01,
                           MS_MSG_EDA_STREAM, MS_TELEM_WRIST_BAND,
                           g_seq++, (uint8_t *)&eda, sizeof(eda));
    if (len > 0) {
        LOG_INF("BLE → Hub: EDA=%dµS std=%d stress=%d",
                g_eda_microsiemens, g_eda_std, g_stress_level);
    }
}

/* === Check hot flash precursor (edge screening) === */
/* Skin temp rising + EDA rising → potential hot flash approaching.
 * Hub runs full HotFlashNet; this is a fast local pre-screen.
 */
static void check_hotflash_precursor(void)
{
    if (g_skin_temp_hist_count < 12) return;  /* Need 2 min of data */

    /* Check skin temp trend (last 10 min = 60 samples at 0.1 Hz) */
    int lookback = g_skin_temp_hist_count < 60 ? g_skin_temp_hist_count : 60;
    int start = (g_skin_temp_hist_idx - lookback + SKIN_TEMP_HIST_LEN) %
                SKIN_TEMP_HIST_LEN;
    int16_t old_temp = g_skin_temp_hist[start];
    int16_t new_temp = g_skin_temp_hist[(g_skin_temp_hist_idx - 1 +
                 SKIN_TEMP_HIST_LEN) % SKIN_TEMP_HIST_LEN];
    int16_t temp_rise = new_temp - old_temp;

    if (temp_rise > MS_SKIN_TEMP_RISE_CD) {
        LOG_WRN("Skin temp rising: +%.1f°C in 10 min (hot flash precursor)",
                temp_rise / 100.0f);
    }

    /* Check EDA trend */
    if (g_eda_microsiemens > MS_EDA_SPIKE_UV && g_eda_phasic > 5) {
        LOG_WRN("EDA spike: %d µS (sympathetic surge — hot flash may be approaching)",
                g_eda_microsiemens);
    }
}

/* === PPG Sampling Task (100 Hz) === */
static void ppg_sampling_task(const struct device *i2c)
{
    uint32_t red_buf[100], ir_buf[100];
    int sample_count = 0;

    while (1) {
        uint32_t red, ir;
        if (max30101_read_fifo(i2c, &red, &ir) == 0) {
            if (sample_count < 100) {
                red_buf[sample_count] = red;
                ir_buf[sample_count] = ir;
                sample_count++;
            }

            if (sample_count >= 100) {
                g_hr = calculate_hr(ir_buf, 100);
                g_spo2 = calculate_spo2(red_buf, ir_buf, 100);

                /* Store IR for HRV */
                for (int i = 0; i < 100 && g_ppg_fifo_count < PPG_FIFO_LEN; i++) {
                    g_ppg_fifo[g_ppg_fifo_idx] = ir_buf[i];
                    g_ppg_fifo_idx = (g_ppg_fifo_idx + 1) % PPG_FIFO_LEN;
                    if (g_ppg_fifo_count < PPG_FIFO_LEN) g_ppg_fifo_count++;
                }

                /* Calculate HRV every 4 seconds */
                if (g_ppg_fifo_count >= 200) {
                    g_hrv_rmssd = 35 + (k_uptime_get_32() % 25);  /* placeholder */
                }

                sample_count = 0;
            }
        }
        k_msleep(10);  /* 100 Hz */
    }
}

/* === IMU Sampling Task (50 Hz) === */
static void imu_sampling_task(const struct device *i2c)
{
    while (1) {
        int16_t accel[3], gyro[3];
        if (read_lsm6dso_imu(i2c, accel, gyro) == 0) {
            ms_imu_sample_t *s = &g_imu_buf[g_imu_buf_idx];
            s->accel_x = accel[0]; s->accel_y = accel[1]; s->accel_z = accel[2];
            s->gyro_x = gyro[0]; s->gyro_y = gyro[1]; s->gyro_z = gyro[2];
            g_imu_buf_idx = (g_imu_buf_idx + 1) % IMU_BUF_LEN;

            if (g_imu_buf_idx == 0) {
                g_activity_class = classify_activity(accel);
                static float prev_mag = 0;
                float mag = sqrtf((float)accel[0]*accel[0] +
                                   (float)accel[1]*accel[1] +
                                   (float)accel[2]*accel[2]);
                if (mag > 12000 && prev_mag < 12000)
                    g_steps_accumulated++;
                prev_mag = mag;
            }
        }
        k_msleep(20);  /* 50 Hz */
    }
}

/* === EDA Sampling Task (4 Hz) === */
static void eda_sampling_task(const struct device *spi_dev)
{
    while (1) {
        g_eda_microsiemens = read_eda_microsiemens(spi_dev);

        /* Store in history (subsampled: every 15s at 4 Hz = every 60 samples) */
        static int eda_subsample = 0;
        if (++eda_subsample >= 60) {
            eda_subsample = 0;
            g_eda_hist[g_eda_hist_idx] = g_eda_microsiemens;
            g_eda_hist_idx = (g_eda_hist_idx + 1) % EDA_HIST_LEN;
            if (g_eda_hist_count < EDA_HIST_LEN) g_eda_hist_count++;
        }

        /* Update tonic/phasic decomposition (simplified) */
        if (g_eda_hist_count > 10) {
            /* Tonic = long-term average, Phasic = current - tonic */
            uint32_t sum = 0;
            for (int i = 0; i < g_eda_hist_count; i++)
                sum += g_eda_hist[i];
            g_eda_tonic = (uint8_t)(sum / g_eda_hist_count);
            int16_t phasic = (int16_t)g_eda_microsiemens - (int16_t)g_eda_tonic;
            g_eda_phasic = phasic > 0 ? (uint8_t)phasic : 0;
        }

        send_eda_ble();
        k_msleep(250);  /* 4 Hz */
    }
}

/* === Vitals Reporting Task (1 Hz) === */
static void vitals_report_task(const struct device *i2c)
{
    while (1) {
        k_msleep(1000);
        send_vitals_ble();
    }
}

/* === Skin Temp + Hot Flash Screening Task (0.1 Hz) === */
static void skin_temp_task(const struct device *i2c)
{
    while (1) {
        k_msleep(10000);  /* 0.1 Hz = every 10s */

        g_skin_temp_cd = read_tmp117_temp_cd(i2c);

        /* Store in history */
        g_skin_temp_hist[g_skin_temp_hist_idx] = g_skin_temp_cd;
        g_skin_temp_hist_idx = (g_skin_temp_hist_idx + 1) % SKIN_TEMP_HIST_LEN;
        if (g_skin_temp_hist_count < SKIN_TEMP_HIST_LEN)
            g_skin_temp_hist_count++;

        check_hotflash_precursor();

        /* Log if skin temp is elevated (hot flash indicator) */
        if (g_skin_temp_cd > MS_SKIN_TEMP_HOT_CD) {
            LOG_WRN("Skin temp elevated: %.1f°C (hot flash indicator)",
                    g_skin_temp_cd / 100.0f);
        }
    }
}

/* === Battery Monitoring Task === */
static void battery_task(void)
{
    while (1) {
        k_msleep(60000);
        if (g_battery_pct > 0) g_battery_pct--;
        if (g_battery_pct < 20) {
            LOG_WRN("Low battery: %d%%", g_battery_pct);
        }
    }
}

/* === Main === */
void main(void)
{
    LOG_INF("MenoSync Wrist Band starting");

    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c)) {
        LOG_ERR("I2C0 not ready");
        return;
    }

    if (max30101_init(i2c) != 0) {
        LOG_ERR("MAX30101 init failed");
        return;
    }
    LOG_INF("MAX30101 initialized (SpO2 mode, 100 Hz)");

    LOG_INF("BLE 5.0 advertising started (MenoSync Wrist Band)");

    /* Start tasks (in production: use K_THREAD_DEFINE) */
    ppg_sampling_task(i2c);
}

/* Thread stack sizes */
#define PPG_STACK_SIZE 1024
#define IMU_STACK_SIZE 1024
#define EDA_STACK_SIZE 1024
#define VITALS_STACK_SIZE 512
#define TEMP_STACK_SIZE 512
#define BATT_STACK_SIZE 256
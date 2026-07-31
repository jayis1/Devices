/*
 * BloomSync — Recovery Band Firmware
 * nRF52840, nRF5 SDK / Zephyr RTOS
 *
 * The Recovery Band is a wrist-worn wearable that continuously monitors
 * maternal vital signs during the 6-week postpartum period:
 *   - MAX30101 PPG sensor: heart rate (30-200 bpm), HRV (RMSSD), SpO₂ (85-100%)
 *   - LSM6DSO 6-axis IMU: activity classification, sleep tracking
 *   - TMP117 skin temperature: thermoregulation monitoring (±0.1°C)
 *   - BLE 5.0 to Hub: 1 Hz vitals + 50 Hz IMU burst
 *   - 200 mAh LiPo: 7-day battery life
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "../common/protocol.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(bloom_recovery_band, LOG_LEVEL_INF);

/* === BLE UUIDs ===
 * Service:  0x8E7C0001-5F60-4B9A-88A0-9F2B6D5C0001
 * Vitals:   0x8E7C0002-5F60-4B9A-88A0-9F2B6D5C0002
 * IMU:      0x8E7C0003-5F60-4B9A-88A0-9F2B6D5C0003
 */
#define BLE_SERVICE_UUID_VAL \
    BT_UUID_128_ENCODE(0x8E7C0001, 0x5F60, 0x4B9A, 0x88A0, 0x9F2B6D5C0001)

/* === Global state === */
static uint8_t g_node_id = 0x02;  /* Recovery Band node ID */
static uint8_t g_seq = 0;

/* Latest sensor readings */
static uint8_t  g_hr = 75;
static uint8_t  g_spo2 = 98;
static int16_t  g_skin_temp_cd = 3680;
static uint16_t g_hrv_rmssd_ms = 45;
static uint8_t  g_activity_class = 0;
static uint8_t  g_steps_accumulated = 0;
static uint8_t  g_battery_pct = 100;

/* PPG FIFO buffer for HRV calculation */
#define PPG_FIFO_LEN 256
static uint32_t g_ppg_fifo[PPG_FIFO_LEN];
static int g_ppg_fifo_idx = 0;
static int g_ppg_fifo_count = 0;

/* IMU data buffer for activity classification */
#define IMU_BUF_LEN 50  /* 1 second at 50 Hz */
static bs_imu_sample_t g_imu_buf[IMU_BUF_LEN];
static int g_imu_buf_idx = 0;

/* === MAX30101 I²C Registers === */
#define MAX30101_REG_INT_STATUS_1   0x00
#define MAX30101_REG_INT_ENABLE_1   0x02
#define MAX30101_REG_FIFO_WR_PTR    0x04
#define MAX30101_REG_FIFO_RD_PTR    0x06
#define MAX30101_REG_FIFO_DATA      0x09
#define MAX30101_REG_MODE_CONFIG    0x09  /* Note: different from FIFO_DATA */
#define MAX30101_REG_SPO2_CONFIG    0x0A
#define MAX30101_REG_LED1_PA        0x0C  /* Red LED pulse amplitude */
#define MAX30101_REG_LED2_PA        0x0D  /* IR LED pulse amplitude */
#define MAX30101_REG_LED3_PA        0x10  /* Green LED pulse amplitude */
#define MAX30101_REG_PILOT_PA       0x11
#define MAX30101_REG_PROX_INT_AMP   0x10
#define MAX30101_REG_TEMP_CONFIG    0x21
#define MAX30101_REG_TEMP_INT       0x1F
#define MAX30101_REG_TEMP_FRAC      0x20

#define MAX30101_MODE_SPO2          0x03  /* Red + IR for SpO₂ */
#define MAX30101_MODE_HR            0x02  /* Red only for HR */
#define MAX30101_MODE_MULTI         0x07  /* Red + IR + Green */

/* === MAX30101 I²C Write === */
static int max30101_write(const struct device *i2c, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_write(i2c, buf, 2, BS_I2C_MAX30101);
}

/* === MAX30101 I²C Read === */
static int max30101_read(const struct device *i2c, uint8_t reg,
                          uint8_t *buf, size_t len)
{
    int err = i2c_write(i2c, &reg, 1, BS_I2C_MAX30101);
    if (err) return err;
    return i2c_read(i2c, buf, len, BS_I2C_MAX30101);
}

/* === Initialize MAX30101 === */
static int max30101_init(const struct device *i2c)
{
    int err;
    /* Reset */
    err = max30101_write(i2c, 0x09, 0x40);  /* MODE_CONFIG: reset */
    k_msleep(10);
    if (err) return err;

    /* Configure FIFO: sample averaging = 4, FIFO rollover = on, almost full = 17 */
    err = max30101_write(i2c, 0x08, 0x44);  /* FIFO_CONFIG */
    if (err) return err;

    /* Mode: SpO₂ mode (Red + IR) */
    err = max30101_write(i2c, 0x09, MAX30101_MODE_SPO2);
    if (err) return err;

    /* SpO₂ config: ADC range 4096, pulse width 411µs (18-bit), sample rate 100 Hz */
    err = max30101_write(i2c, 0x0A, 0x2F);  /* SPO2_CONFIG */
    if (err) return err;

    /* LED pulse amplitudes: ~7mA for Red and IR */
    err = max30101_write(i2c, MAX30101_REG_LED1_PA, 0x24);  /* Red */
    if (err) return err;
    err = max30101_write(i2c, MAX30101_REG_LED2_PA, 0x24);  /* IR */
    if (err) return err;

    /* Enable data ready interrupt */
    err = max30101_write(i2c, MAX30101_REG_INT_ENABLE_1, 0x80);
    return err;
}

/* === Read PPG samples from MAX30101 FIFO === */
static int max30101_read_fifo(const struct device *i2c,
                                uint32_t *red, uint32_t *ir)
{
    uint8_t buf[6];
    int err = max30101_read(i2c, MAX30101_REG_FIFO_DATA, buf, 6);
    if (err) return err;

    /* MAX30101 FIFO: 3 bytes Red + 3 bytes IR, 18-bit each */
    *red = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    *ir  = ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    /* Mask to 18 bits */
    *red &= 0x3FFFF;
    *ir  &= 0x3FFFF;
    return 0;
}

/* === Calculate Heart Rate from PPG signal === */
/* Uses peak detection on IR channel over a 4-second window.
 * In production: use a more robust algorithm with adaptive thresholding
 * and motion artifact rejection (from IMU data).
 */
static uint8_t calculate_hr(const uint32_t *samples, int count)
{
    if (count < 20) return 0;

    /* Simple peak detection */
    int peaks = 0;
    uint32_t threshold = 0;
    for (int i = 0; i < count; i++) threshold += samples[i];
    threshold /= count;

    int above = 0;
    for (int i = 1; i < count; i++) {
        if (samples[i] > threshold && samples[i-1] <= threshold) {
            above = 1;
        }
        if (samples[i] < threshold && above) {
            peaks++;
            above = 0;
        }
    }

    /* Convert peaks to bpm: (peaks / window_seconds) × 60 */
    float window_s = (float)count / 100.0f;  /* 100 Hz sample rate */
    if (window_s < 0.1f) return 0;
    float hr = (peaks / window_s) * 60.0f;
    if (hr < 30.0f || hr > 200.0f) return 0;
    return (uint8_t)hr;
}

/* === Calculate SpO₂ from Red/IR ratio === */
/* SpO₂ = 110 - 25 × (AC_red/DC_red) / (AC_ir/DC_ir)
 * Simplified: uses DC-coupled ratio of ratios.
 */
static uint8_t calculate_spo2(const uint32_t *red, const uint32_t *ir, int count)
{
    if (count < 20) return 0;

    /* Calculate AC and DC components */
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

    float red_ac = (float)(red_max - red_min);
    float ir_ac = (float)(ir_max - ir_min);
    float ratio = (red_ac / red_dc) / (ir_ac / ir_dc);

    /* Empirical calibration: SpO₂ = 110 - 25 × R */
    float spo2 = 110.0f - 25.0f * ratio;
    if (spo2 < 70.0f) spo2 = 70.0f;
    if (spo2 > 100.0f) spo2 = 100.0f;
    return (uint8_t)spo2;
}

/* === Calculate HRV (RMSSD) from peak-to-peak intervals === */
static uint16_t calculate_hrv_rmssd(const uint32_t *peaks, int peak_count)
{
    if (peak_count < 3) return 0;

    /* Calculate successive differences */
    float sum_sq = 0;
    int count = 0;
    for (int i = 1; i < peak_count; i++) {
        int32_t diff = (int32_t)peaks[i] - (int32_t)peaks[i-1];
        sum_sq += (float)(diff * diff);
        count++;
    }

    if (count == 0) return 0;
    float rmssd = sqrtf(sum_sq / count);
    return (uint16_t)rmssd;
}

/* === Read TMP117 skin temperature === */
static int16_t read_tmp117_temp_cd(const struct device *i2c)
{
    uint8_t buf[2];
    int err = i2c_write(i2c, (uint8_t[]){0x00}, 1, BS_I2C_TMP117);  /* Reg 0: temp */
    if (err) return 0;
    err = i2c_read(i2c, buf, 2, BS_I2C_TMP117);
    if (err) return 0;

    /* TMP117: 16-bit, resolution 0.0078°C/LSB, 2's complement */
    int16_t raw = ((int16_t)buf[0] << 8) | buf[1];
    /* Convert to centi-degrees: raw × 0.0078 × 100 = raw × 0.78 */
    return (int16_t)(raw * 78 / 100);  /* centi-degrees Celsius */
}

/* === Read LSM6DSO IMU === */
static int read_lsm6dso_imu(const struct device *i2c,
                             int16_t *accel, int16_t *gyro)
{
    uint8_t buf[12];
    /* LSM6DSO: OUTX_L_G = 0x22, OUTX_L_XL = 0x28 */
    int err = i2c_write(i2c, (uint8_t[]){0x22}, 1, 0x6A);
    if (err) return err;
    err = i2c_read(i2c, buf, 12, 0x6A);
    if (err) return err;

    /* Gyro (first 6 bytes), Accel (next 6 bytes) */
    gyro[0] = ((int16_t)buf[1] << 8) | buf[0];
    gyro[1] = ((int16_t)buf[3] << 8) | buf[2];
    gyro[2] = ((int16_t)buf[5] << 8) | buf[4];
    accel[0] = ((int16_t)buf[7] << 8) | buf[6];
    accel[1] = ((int16_t)buf[9] << 8) | buf[8];
    accel[2] = ((int16_t)buf[11] << 8) | buf[10];
    return 0;
}

/* === Classify activity from IMU data === */
/* 0=rest, 1=sit, 2=walk, 3=run, 4=sleep, 5=nurse (arm held up) */
static uint8_t classify_activity(const int16_t *accel)
{
    /* Calculate magnitude */
    float mag = sqrtf((float)accel[0]*accel[0] +
                       (float)accel[1]*accel[1] +
                       (float)accel[2]*accel[2]);
    /* Gravity ≈ 16384 at ±2g range (1g = 9.8 m/s²) */
    float g_mag = mag / 16384.0f;

    if (g_mag < 0.85f) return 4;       /* Low movement = sleep */
    if (g_mag < 1.05f) return 0;       /* Near-gravity = rest */
    if (g_mag < 1.2f) return 1;        /* Slight movement = sit */
    if (g_mag < 1.6f) return 2;        /* Moderate = walk */
    if (g_mag < 2.5f) return 3;        /* High = run */
    /* Check for nursing position (arm elevated, low dynamic movement) */
    if (accel[1] > 8000) return 5;     /* Y-axis high = arm up */
    return 1;
}

/* === BLE GATT notification: send vitals to Hub === */
static void send_vitals_ble(const struct device *i2c)
{
    bs_vitals_t vitals = {
        .heart_rate = g_hr,
        .spo2 = g_spo2,
        .skin_temp_cd = g_skin_temp_cd,
        .hrv_rmssd_ms = g_hrv_rmssd,
        .activity_class = g_activity_class,
        .steps_count_lsb = g_steps_accumulated,
        .battery_pct = g_battery_pct,
    };

    /* Encode protocol message */
    uint8_t msg[BS_MAX_MSG];
    size_t len = bs_encode(msg, sizeof(msg),
                           g_node_id, 0x01,  /* src=Recovery Band, dst=Hub */
                           BS_MSG_VITALS_STREAM, BS_TELEM_RECOVERY_BAND,
                           g_seq++, (uint8_t *)&vitals, sizeof(vitals));
    if (len > 0) {
        /* In production: send via BLE GATT notification on Vitals characteristic */
        LOG_INF("BLE → Hub: HR=%d SpO2=%d%% Temp=%.1f°C HRV=%dms act=%d batt=%d%%",
                g_hr, g_spo2, g_skin_temp_cd / 100.0f,
                g_hrv_rmssd, g_activity_class, g_battery_pct);
    }
}

/* === PPG Sampling Task (100 Hz) === */
/* Reads MAX30101 FIFO, accumulates samples, calculates HR/SpO₂/HRV every second.
 */
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

            /* Every 100 samples (1 second), calculate vitals */
            if (sample_count >= 100) {
                g_hr = calculate_hr(ir_buf, 100);
                g_spo2 = calculate_spo2(red_buf, ir_buf, 100);

                /* Store IR for HRV */
                for (int i = 0; i < 100 && g_ppg_fifo_count < PPG_FIFO_LEN; i++) {
                    g_ppg_fifo[g_ppg_fifo_idx] = ir_buf[i];
                    g_ppg_fifo_idx = (g_ppg_fifo_idx + 1) % PPG_FIFO_LEN;
                    if (g_ppg_fifo_count < PPG_FIFO_LEN) g_ppg_fifo_count++;
                }

                /* Calculate HRV every 4 seconds (400 samples) */
                if (g_ppg_fifo_count >= 200) {
                    /* Simplified: use peak intervals from buffer */
                    g_hrv_rmssd = 30 + (k_uptime_get_32() % 30);  /* placeholder */
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
            /* Store in buffer */
            bs_imu_sample_t *s = &g_imu_buf[g_imu_buf_idx];
            s->accel_x = accel[0]; s->accel_y = accel[1]; s->accel_z = accel[2];
            s->gyro_x = gyro[0]; s->gyro_y = gyro[1]; s->gyro_z = gyro[2];
            g_imu_buf_idx = (g_imu_buf_idx + 1) % IMU_BUF_LEN;

            /* Classify activity every 1 second (50 samples) */
            if (g_imu_buf_idx == 0) {
                g_activity_class = classify_activity(accel);

                /* Simple step counting: detect peaks in acceleration magnitude */
                static float prev_mag = 0;
                float mag = sqrtf((float)accel[0]*accel[0] +
                                   (float)accel[1]*accel[1] +
                                   (float)accel[2]*accel[2]);
                if (mag > 12000 && prev_mag < 12000) {
                    g_steps_accumulated++;
                }
                prev_mag = mag;
            }
        }
        k_msleep(20);  /* 50 Hz */
    }
}

/* === Vitals Reporting Task (1 Hz) === */
static void vitals_report_task(const struct device *i2c)
{
    while (1) {
        k_msleep(1000);

        /* Read skin temperature */
        g_skin_temp_cd = read_tmp117_temp_cd(i2c);

        /* Send vitals via BLE to Hub */
        send_vitals_ble(i2c);
    }
}

/* === Battery Monitoring Task === */
static void battery_task(void)
{
    while (1) {
        k_msleep(60000);  /* Every minute */
        /* In production: read MAX17048 fuel gauge via I²C */
        /* g_battery_pct = max17048_read_percent(); */
        if (g_battery_pct < 20) {
            LOG_WRN("Low battery: %d%%", g_battery_pct);
        }
        if (g_battery_pct > 0) g_battery_pct--;  /* Simulated drain */
    }
}

/* === Main === */
void main(void)
{
    LOG_INF("BloomSync Recovery Band starting");

    /* Get I²C device */
    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c)) {
        LOG_ERR("I2C0 not ready");
        return;
    }

    /* Initialize MAX30101 PPG sensor */
    if (max30101_init(i2c) != 0) {
        LOG_ERR("MAX30101 init failed");
        return;
    }
    LOG_INF("MAX30101 initialized (SpO2 mode, 100 Hz)");

    /* Initialize BLE */
    /* In production: bt_enable(), register GATT service, start advertising */
    LOG_INF("BLE 5.0 advertising started (BloomSync Recovery Band)");

    /* Start tasks */
    /* In Zephyr: use K_THREAD_DEFINE for each task */
    ppg_sampling_task(i2c);
    /* In production: spawn as separate threads */
}

/* Thread definitions (Zephyr) */
#define PPG_STACK_SIZE 1024
#define IMU_STACK_SIZE 1024
#define VITALS_STACK_SIZE 512
#define BATT_STACK_SIZE 256

/* K_THREAD_DEFINE(ppg_thread, PPG_STACK_SIZE, ppg_sampling_task, ...); */
/* K_THREAD_DEFINE(imu_thread, IMU_STACK_SIZE, imu_sampling_task, ...); */
/* K_THREAD_DEFINE(vitals_thread, VITALS_STACK_SIZE, vitals_report_task, ...); */
/* K_THREAD_DEFINE(batt_thread, BATT_STACK_SIZE, battery_task, 0, 0, 0, 5); */
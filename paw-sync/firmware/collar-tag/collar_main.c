/*
 * collar_main.c — PawSync Collar Tag firmware (nRF52840, nRF5 SDK)
 *
 * Samples PPG (MAX30101) for heart rate/HRV, IMU (LSM6DSO32) for activity
 * classification and gait analysis, and skin temperature. Runs an on-device
 * TFLite Micro CNN for activity classification every 5s. Transmits vitals
 * to the hub over BLE mesh every 60s. Stores 4 hours of data locally when
 * out of mesh range (walks) and syncs when back.
 *
 * Power: 180mAh LiPo, 5-7 days per charge, Qi wireless charging.
 * Duty cycle: PPG 20s/min @ 100Hz, IMU continuous @ 50Hz, BLE TX 1/min.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_gpiote.h"
#include "app_timer.h"
#include "nrf_pwr_mgmt.h"
#include "ble_srv_common.h"
#include "paw_protocol.h"

/* ---- Pin definitions (nRF52840 custom board) ---- */
#define PIN_I2C_PPG_SDA   2
#define PIN_I2C_PPG_SCL   3
#define PIN_I2C_IMU_SDA   4
#define PIN_I2C_IMU_SCL   5
#define PIN_LED_GREEN     10
#define PIN_LED_RED       11
#define PIN_QI_RX_EN     12
#define PIN_VBAT_SENSE   13  /* analog via SAADC */
#define PIN_PPG_INT      15
#define PIN_IMU_INT1     20

/* ---- Sampling parameters ---- */
#define PPG_SAMPLE_HZ     100
#define PPG_SAMPLE_DUR_S  20    /* 20s per minute */
#define PPG_WINDOW        (PPG_SAMPLE_HZ * PPG_SAMPLE_DUR_S)  /* 2000 samples */
#define IMU_SAMPLE_HZ     50
#define IMU_WINDOW        (IMU_SAMPLE_HZ * 2)   /* 2s window for activity CNN */
#define REPORT_INTERVAL_S 60    /* BLE mesh report every 60s */
#define OFFLINE_BUF_SLOTS 240    /* 4 hours @ 1/min */

/* ---- MAX30101 registers ---- */
#define MAX30101_I2C_ADDR     0x57
#define MAX30101_REG_INT_EN1 0x02
#define MAX30101_REG_INT_EN2 0x03
#define MAX30101_REG_FIFO_WR 0x04
#define MAX30101_REG_FIFO_RD 0x06
#define MAX30101_REG_FIFO_CFG 0x08
#define MAX30101_REG_MODE_CFG 0x09
#define MAX30101_REG_SPO2_CFG 0x0A
#define MAX30101_REG_LED1_PA  0x0C  /* Red LED */
#define MAX30101_REG_LED2_PA  0x0D  /* IR LED */
#define MAX30101_REG_TEMP_INT 0x1F  /* Die temperature */
#define MAX30101_REG_TEMP_FRAC 0x20

/* ---- LSM6DSO32 registers ---- */
#define LSM6DSO32_I2C_ADDR   0x6B
#define LSM6DSO32_REG_CTRL1_XL 0x10  /* Accel ODR + FS */
#define LSM6DSO32_REG_CTRL2_G  0x11  /* Gyro ODR + FS */
#define LSM6DSO32_REG_CTRL3_C  0x12  /* BDU + IF_INC */
#define LSM6DSO32_REG_OUTX_L_G 0x22  /* Gyro X */
#define LSM6DSO32_REG_OUTX_L_XL 0x28 /* Accel X */
#define LSM6DSO32_REG_INT1_CTRL 0x0D

/* ---- Globals ---- */
static nrf_drv_twi_t twi_ppg = NRF_DRV_TWI_INSTANCE(0);
static nrf_drv_twi_t twi_imu = NRF_DRV_TWI_INSTANCE(1);
static app_timer_id_t report_timer;
static app_timer_id_t ppg_timer;

/* Latest computed vitals */
static uint8_t  g_hr_bpm = 0;
static uint16_t g_hrv_rmssd = 0;    /* centi-ms */
static int16_t  g_skin_temp = 0;    /* centi-degC */
static uint8_t  g_activity_class = 0;
static uint8_t  g_activity_confidence = 0;
static int16_t  g_gait[PAW_GAIT_FEATURES] = {0};
static uint8_t  g_battery_pct = 100;
static uint8_t  g_flags = 0;
static uint16_t g_scratch_count = 0;
static uint16_t g_headshake_count = 0;
static uint16_t g_activity_count = 0;

/* Offline buffer (for when out of mesh range) */
typedef struct {
    paw_vitals_payload_t vitals;
    paw_activity_payload_t activity;
} offline_slot_t;

static offline_slot_t offline_buf[OFFLINE_BUF_SLOTS];
static volatile int offline_head = 0;
static volatile int offline_count = 0;
static volatile bool mesh_connected = false;

/* ---- TWI helpers ---- */
static uint8_t twi_read_reg(nrf_drv_twi_t *twi, uint8_t addr, uint8_t reg)
{
    uint8_t val;
    nrf_drv_twi_tx(twi, addr, &reg, 1, true);
    nrf_drv_twi_rx(twi, addr, &val, 1);
    return val;
}

static void twi_write_reg(nrf_drv_twi_t *twi, uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    nrf_drv_twi_tx(twi, addr, buf, 2, false);
}

static void twi_read_burst(nrf_drv_twi_t *twi, uint8_t addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    nrf_drv_twi_tx(twi, addr, &reg, 1, true);
    nrf_drv_twi_rx(twi, addr, buf, len);
}

/* ---- MAX30101 PPG driver ---- */
static void max30101_init(void)
{
    /* Reset */
    twi_write_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_MODE_CFG, 0x40);
    nrf_delay_ms(10);
    /* Mode: HR (red+IR) */
    twi_write_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_MODE_CFG, 0x07);
    /* SPO2 config: 100Hz, 411us pulse, 4096 samples */
    twi_write_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_SPO2_CFG, 0x27);
    /* LED power: ~7mA each */
    twi_write_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_LED1_PA, 0x24);
    twi_write_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_LED2_PA, 0x24);
    /* FIFO: avg 4 samples, no rollover */
    twi_write_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_FIFO_CFG, 0x4F);
}

/* Read PPG samples (IR channel) */
static int max30101_read_ir(uint16_t *ir_buf, int max_samples)
{
    int count = 0;
    for (int i = 0; i < max_samples; i++) {
        uint8_t raw[3];
        twi_read_burst(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_FIFO_RD, raw, 3);
        /* 18-bit value, shift to 16-bit */
        ir_buf[i] = ((raw[0] << 8) | (raw[1] << 2) | (raw[2] >> 6)) >> 2;
        count++;
    }
    return count;
}

/* ---- LSM6DSO32 IMU driver ---- */
static void lsm6dso32_init(void)
{
    /* Accel: 50Hz, ±4g */
    twi_write_reg(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_CTRL1_XL, 0x38);
    /* Gyro: 50Hz, ±500dps */
    twi_write_reg(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_CTRL2_G, 0x34);
    /* BDU=1, IF_INC=1 */
    twi_write_reg(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_CTRL3_C, 0x44);
    /* INT1: activity detection */
    twi_write_reg(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_INT1_CTRL, 0x08);
}

static void lsm6dso32_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];
    twi_read_burst(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_OUTX_L_XL, buf, 6);
    *ax = (int16_t)((buf[1] << 8) | buf[0]);
    *ay = (int16_t)((buf[3] << 8) | buf[2]);
    *az = (int16_t)((buf[5] << 8) | buf[4]);
}

static void lsm6dso32_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[6];
    twi_read_burst(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_OUTX_L_G, buf, 6);
    *gx = (int16_t)((buf[1] << 8) | buf[0]);
    *gy = (int16_t)((buf[3] << 8) | buf[2]);
    *gz = (int16_t)((buf[5] << 8) | buf[4]);
}

/* ---- Battery monitoring (SAADC) ---- */
static void battery_read(void)
{
    nrf_drv_saadc_sample_convert(0, (nrf_saadc_value_t *)&g_battery_pct);
    /* Convert ADC value to percentage (rough) */
    if (g_battery_pct > 255) g_battery_pct = 100;
    else g_battery_pct = (g_battery_pct * 100) / 255;
}

/* ---- Skin temperature (MAX30101 die temp) ---- */
static int16_t read_skin_temp(void)
{
    uint8_t intg = twi_read_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_TEMP_INT);
    uint8_t frac = twi_read_reg(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_TEMP_FRAC);
    /* Die temperature ≈ skin temp (attached to neck) */
    /* Convert to centi-degC: (intg + frac/16) * 100 */
    return (int16_t)(intg * 100 + (frac & 0x0F) * 6);
}

/* ---- PPG to HR + HRV (declared in ppg_hr.c) ---- */
extern void ppg_compute_hr_hrv(const uint16_t *ir, int n,
                               uint8_t *hr, uint16_t *hrv);

/* ---- Activity CNN (declared in activity_cnn.c) ---- */
extern void activity_classify(const int16_t *accel_x, const int16_t *accel_y,
                              const int16_t *accel_z, const int16_t *gyro_x,
                              const int16_t *gyro_y, const int16_t *gyro_z,
                              int n, uint8_t *class_out, uint8_t *conf_out);

/* ---- Gait analysis (declared in gait_analysis.c) ---- */
extern void gait_compute_features(const int16_t *accel_z, const int16_t *gyro_z,
                                  int n, int16_t gait[PAW_GAIT_FEATURES]);

/* ---- Scratch detection (declared in scratch_detect.c) ---- */
extern int scratch_detect_from_imu(const int16_t *accel_z, int n);
extern int head_shake_detect_from_imu(const int16_t *gyro_z, int n);

/* ---- BLE mesh send (stub — in production: SoftDevice mesh) ---- */
static void mesh_send_vitals(void)
{
    paw_vitals_payload_t vp = {0};
    vp.type            = PAW_MSG_VITALS;
    vp.node_id         = PAW_NODE_ID_COLLAR;
    vp.flags           = g_flags;
    vp.hr_bpm          = g_hr_bpm;
    vp.hrv_rmssd       = g_hrv_rmssd;
    vp.skin_temp_centic = g_skin_temp;
    memcpy(vp.gait, g_gait, sizeof(g_gait));
    vp.battery_pct     = g_battery_pct;
    paw_pack_crc(&vp, sizeof(vp) - 2);

    if (mesh_connected) {
        /* ble_mesh_send(&vp, sizeof(vp)); */
    } else {
        /* Buffer offline */
        if (offline_count < OFFLINE_BUF_SLOTS) {
            memcpy(&offline_buf[offline_head].vitals, &vp, sizeof(vp));
            offline_head = (offline_head + 1) % OFFLINE_BUF_SLOTS;
            if (offline_count < OFFLINE_BUF_SLOTS) offline_count++;
        }
    }
}

static void mesh_send_activity(void)
{
    paw_activity_payload_t ap = {0};
    ap.type            = PAW_MSG_ACTIVITY;
    ap.node_id         = PAW_NODE_ID_COLLAR;
    ap.flags           = g_flags;
    ap.activity_class  = g_activity_class;
    ap.confidence      = g_activity_confidence;
    ap.duration_s      = REPORT_INTERVAL_S;
    memcpy(ap.gait, g_gait, sizeof(ap.gait));
    paw_pack_crc(&ap, sizeof(ap) - 2);

    if (mesh_connected) {
        /* ble_mesh_send(&ap, sizeof(ap)); */
    } else {
        if (offline_count < OFFLINE_BUF_SLOTS) {
            memcpy(&offline_buf[offline_head].activity, &ap, sizeof(ap));
        }
    }
}

/* ---- Sync offline buffer when mesh reconnects ---- */
static void sync_offline_buffer(void)
{
    if (!mesh_connected || offline_count == 0) return;
    for (int i = 0; i < offline_count; i++) {
        int idx = (offline_head - offline_count + i + OFFLINE_BUF_SLOTS) % OFFLINE_BUF_SLOTS;
        /* ble_mesh_send(&offline_buf[idx].vitals, sizeof(paw_vitals_payload_t)); */
        nrf_delay_ms(50);  /* rate limit */
    }
    offline_count = 0;
    offline_head = 0;
}

/* ---- Report timer callback (every 60s) ---- */
static void report_timer_handler(void *ctx)
{
    (void)ctx;
    mesh_send_vitals();
    mesh_send_activity();
    battery_read();

    /* Check alert conditions */
    if (g_scratch_count > 30)  /* >3x normal baseline (~10) */
        g_flags |= PAW_ALERT_SCRATCHING;
    if (g_battery_pct < 15)
        g_flags |= PAW_ALERT_LOW_BATT;

    /* Reset per-cycle counters */
    g_scratch_count = 0;
    g_headshake_count = 0;
    g_activity_count = 0;
    g_flags = 0;

    /* Sync offline data if reconnected */
    sync_offline_buffer();
}

/* ---- PPG sampling timer (fires once per minute for 20s) ---- */
static void ppg_sample_cycle(void)
{
    static uint16_t ir_buf[PPG_WINDOW];
    int n = max30101_read_ir(ir_buf, PPG_WINDOW);
    if (n > 100) {
        ppg_compute_hr_hrv(ir_buf, n, &g_hr_bpm, &g_hrv_rmssd);
    }
    g_skin_temp = read_skin_temp();
}

/* ---- IMU sampling + activity classification (continuous in main loop) ---- */
static void imu_sample_cycle(void)
{
    static int16_t ax[IMU_WINDOW], ay[IMU_WINDOW], az[IMU_WINDOW];
    static int16_t gx[IMU_WINDOW], gy[IMU_WINDOW], gz[IMU_WINDOW];
    static int sample_idx = 0;

    lsm6dso32_read_accel(&ax[sample_idx], &ay[sample_idx], &az[sample_idx]);
    lsm6dso32_read_gyro(&gx[sample_idx], &gy[sample_idx], &gz[sample_idx]);

    sample_idx++;
    if (sample_idx >= IMU_WINDOW) {
        /* Run activity CNN */
        activity_classify(ax, ay, az, gx, gy, gz, IMU_WINDOW,
                          &g_activity_class, &g_activity_confidence);
        /* Compute gait features if walking/running */
        if (g_activity_class == 1 || g_activity_class == 2) {
            gait_compute_features(az, gz, IMU_WINDOW, g_gait);
            g_activity_count++;
        }
        /* Detect scratching */
        if (scratch_detect_from_imu(az, IMU_WINDOW))
            g_scratch_count++;
        /* Detect head-shaking */
        if (head_shake_detect_from_imu(gz, IMU_WINDOW))
            g_headshake_count++;

        sample_idx = 0;
    }
}

/* ---- Main ---- */
int main(void)
{
    /* Init TWI buses */
    nrf_drv_twi_config_t twi_cfg = {
        .scl = PIN_I2C_PPG_SCL,
        .sda = PIN_I2C_PPG_SDA,
        .frequency = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH
    };
    nrf_drv_twi_init(&twi_ppg, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&twi_ppg);

    twi_cfg.scl = PIN_I2C_IMU_SCL;
    twi_cfg.sda = PIN_I2C_IMU_SDA;
    nrf_drv_twi_init(&twi_imu, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&twi_imu);

    /* Init sensors */
    max30101_init();
    lsm6dso32_init();

    /* Init GPIO for LEDs */
    nrf_gpio_cfg_output(PIN_LED_GREEN);
    nrf_gpio_cfg_output(PIN_LED_RED);
    nrf_gpio_pin_set(PIN_LED_GREEN);  /* power on indicator */

    /* Init timers */
    app_timer_init();
    app_timer_create(&report_timer, APP_TIMER_MODE_REPEATED, report_timer_handler);
    app_timer_start(report_timer, APP_TIMER_TICKS(REPORT_INTERVAL_S * 1000), NULL);

    printf("PawSync Collar Tag starting\n");

    /* Main loop: sample IMU at 50Hz, PPG once per minute */
    while (1) {
        imu_sample_cycle();
        ppg_sample_cycle();
        nrf_delay_us(20000);  /* 50Hz = 20ms */
        nrf_pwr_mgmt_run();    /* sleep between samples */
    }
    return 0;
}
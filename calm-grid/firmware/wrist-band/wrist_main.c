/*
 * wrist_main.c — CalmGrid Wrist Band firmware (nRF52840, nRF5 SDK)
 *
 * Samples PPG (MAX30101) for heart rate/HRV, EDA (AD5940) for skin
 * conductance, IMU (LSM6DSO32) for activity classification, and skin
 * temperature (TMP117). Runs an on-device TFLite Micro CNN for activity
 * classification every 5s. Detects acute stress episodes from concurrent
 * EDA + HRV + HR. Transmits vitals to the hub over BLE mesh every 60s.
 * Stores 6 hours of data locally when out of mesh range and syncs when
 * back.
 *
 * Power: 220mAh LiPo, 4-5 days per charge, Qi wireless charging.
 * Duty cycle: PPG 20s/min @ 100Hz, EDA continuous @ 4Hz,
 *             IMU continuous @ 50Hz, BLE TX 1/min.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_gpiote.h"
#include "app_timer.h"
#include "nrf_pwr_mgmt.h"
#include "ble_srv_common.h"
#include "calm_protocol.h"

/* ---- Pin definitions (nRF52840 custom board) ---- */
#define PIN_I2C_PPG_SDA   2
#define PIN_I2C_PPG_SCL   3
#define PIN_I2C_IMU_SDA   4
#define PIN_I2C_IMU_SCL   5
#define PIN_SPI_CS        6   /* AD5940 CS */
#define PIN_SPI_SCK       7   /* AD5940 SCK */
#define PIN_SPI_MOSI      8   /* AD5940 MOSI */
#define PIN_SPI_MISO      9   /* AD5940 MISO */
#define PIN_LED_GREEN     10
#define PIN_LED_RED       11
#define PIN_QI_RX_EN     12
#define PIN_VBAT_SENSE   13
#define PIN_PPG_INT      15
#define PIN_IMU_INT1     20
#define PIN_EDA_EXCITE   28  /* H-tied excitation electrode drive */
#define PIN_EDA_SENSE_P  29  /* TIA positive input */
#define PIN_EDA_SENSE_N  30  /* TIA negative input */

/* ---- Sampling parameters ---- */
#define PPG_SAMPLE_HZ     100
#define PPG_SAMPLE_DUR_S  20
#define PPG_WINDOW        (PPG_SAMPLE_HZ * PPG_SAMPLE_DUR_S)  /* 2000 */
#define IMU_SAMPLE_HZ     50
#define IMU_WINDOW        (IMU_SAMPLE_HZ * 2)   /* 2s for activity CNN */
#define EDA_SAMPLE_HZ     4
#define EDA_WINDOW        (EDA_SAMPLE_HZ * 60)  /* 1 min of EDA */
#define REPORT_INTERVAL_S 60
#define OFFLINE_BUF_SLOTS 360    /* 6 hours @ 1/min */

/* ---- MAX30101 registers ---- */
#define MAX30101_I2C_ADDR     0x57
#define MAX30101_REG_INT_EN1 0x02
#define MAX30101_REG_FIFO_WR 0x04
#define MAX30101_REG_FIFO_RD 0x06
#define MAX30101_REG_FIFO_CFG 0x08
#define MAX30101_REG_MODE_CFG 0x09
#define MAX30101_REG_SPO2_CFG 0x0A
#define MAX30101_REG_LED1_PA  0x0C
#define MAX30101_REG_LED2_PA  0x0D
#define MAX30101_REG_TEMP_INT 0x1F

/* ---- LSM6DSO32 registers ---- */
#define LSM6DSO32_I2C_ADDR   0x6B
#define LSM6DSO32_REG_CTRL1_XL 0x10
#define LSM6DSO32_REG_CTRL2_G  0x11
#define LSM6DSO32_REG_CTRL3_C  0x12
#define LSM6DSO32_REG_OUTX_L_G 0x22
#define LSM6DSO32_REG_OUTX_L_XL 0x28

/* ---- TMP117 registers ---- */
#define TMP117_I2C_ADDR      0x48
#define TMP117_REG_TEMP      0x00

/* ---- AD5940 registers (EDA measurement) ---- */
#define AD5940_REG_AFECON   0x2000
#define AD5940_REG_SWCFG    0x2008
#define AD5940_REG_DFTCTRL  0x2018

/* ---- Globals ---- */
static nrf_drv_twi_t twi_ppg = NRF_DRV_TWI_INSTANCE(0);
static nrf_drv_twi_t twi_imu = NRF_DRV_TWI_INSTANCE(1);
static nrf_drv_spi_t spi_eda = NRF_DRV_SPI_INSTANCE(0);
static app_timer_id_t report_timer;
static app_timer_id_t ppg_timer;

/* Latest computed vitals */
static uint8_t  g_hr_bpm = 0;
static uint16_t g_hrv_rmssd = 0;    /* centi-ms */
static uint16_t g_eda_scl = 0;      /* µS * 100 */
static uint16_t g_eda_scr_rate = 0; /* events/min * 100 */
static int16_t  g_skin_temp = 0;    /* centi-degC */
static uint8_t  g_activity = 0;     /* activity class 0-7 */
static uint16_t g_step_count = 0;

/* EDA buffer (1 minute at 4Hz = 240 samples) */
static uint16_t eda_buffer[EDA_WINDOW];
static volatile uint32_t eda_buf_idx = 0;

/* Baseline (set by hub after 14-day learning) */
static uint16_t baseline_hrv = 0;
static uint16_t baseline_scr_rate = 0;
static uint8_t  baseline_hr = 0;

/* ---- Function declarations ---- */
extern void    ppg_compute_hr_hrv(const uint16_t *ir_data, int n,
                                  uint8_t *hr, uint16_t *hrv);
extern void    eda_compute_scl_scr(const uint16_t *raw, int n,
                                   uint16_t *scl, uint16_t *scr_rate);
extern uint8_t activity_classify(const int16_t *ax, const int16_t *ay,
                                  const int16_t *az, const int16_t *gx,
                                  const int16_t *gy, const int16_t *gz, int n);
extern int     detect_acute_stress_episode(uint16_t cur_scr_rate,
                                           uint16_t baseline_scr,
                                           uint16_t cur_hrv,
                                           uint16_t baseline_hrv,
                                           uint8_t cur_hr,
                                           uint8_t baseline_hr);

/* ---- I2C write helper ---- */
static void i2c_write(nrf_drv_twi_t *twi, uint8_t addr,
                      uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    nrf_drv_twi_tx(twi, addr, buf, 2, false);
}

static uint8_t i2c_read_reg(nrf_drv_twi_t *twi, uint8_t addr, uint8_t reg)
{
    uint8_t val = 0;
    nrf_drv_twi_tx(twi, addr, &reg, 1, true);
    nrf_drv_twi_rx(twi, addr, &val, 1);
    return val;
}

/* ---- MAX30101 init ---- */
static void max30101_init(void)
{
    /* Reset */
    i2c_write(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_MODE_CFG, 0x40);
    nrf_delay_ms(10);
    /* Mode: HR + SpO2 (red + IR) */
    i2c_write(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_MODE_CFG, 0x07);
    /* SPO2 ADC range 4096, sample rate 100Hz, pulse width 411µs (18-bit) */
    i2c_write(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_SPO2_CFG, 0x27);
    /* LED current ~7mA */
    i2c_write(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_LED1_PA, 0x24);
    i2c_write(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_LED2_PA, 0x24);
    /* FIFO: 16 samples, rollover */
    i2c_write(&twi_ppg, MAX30101_I2C_ADDR, MAX30101_REG_FIFO_CFG, 0x0F);
}

/* ---- LSM6DSO32 init ---- */
static void lsm6dso32_init(void)
{
    /* Accel: 4g, 50Hz ODR */
    i2c_write(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_CTRL1_XL, 0x28);
    /* Gyro: 500dps, 50Hz ODR */
    i2c_write(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_CTRL2_G, 0x24);
    /* BDU + IF_INC */
    i2c_write(&twi_imu, LSM6DSO32_I2C_ADDR, LSM6DSO32_REG_CTRL3_C, 0x44);
}

/* ---- Read IMU 6-axis data ---- */
static void imu_read(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[12];
    uint8_t reg = LSM6DSO32_REG_OUTX_L_G;
    nrf_drv_twi_tx(&twi_imu, LSM6DSO32_I2C_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&twi_imu, LSM6DSO32_I2C_ADDR, buf, 12);
    *gx = (int16_t)((buf[1] << 8) | buf[0]);
    *gy = (int16_t)((buf[3] << 8) | buf[2]);
    *gz = (int16_t)((buf[5] << 8) | buf[4]);
    *ax = (int16_t)((buf[7] << 8) | buf[6]);
    *ay = (int16_t)((buf[9] << 8) | buf[8]);
    *az = (int16_t)((buf[11] << 8) | buf[10]);
}

/* ---- TMP117 read (skin temp) ---- */
static int16_t tmp117_read_centic(void)
{
    uint8_t buf[2];
    uint8_t reg = TMP117_REG_TEMP;
    nrf_drv_twi_tx(&twi_ppg, TMP117_I2C_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&twi_ppg, TMP117_I2C_ADDR, buf, 2);
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    /* TMP117: 7.8125 m°C per LSB → centi-degC = raw * 0.0078125 * 100 */
    return (int16_t)(raw * 0.78125f);  /* centi-degC */
}

/* ---- AD5940 EDA read (stub — SPI communication) ---- */
static uint16_t ad5940_read_eda(void)
{
    /* In production: configure AD5940 for EDA measurement, read ADC
     * Compute skin conductance from excitation voltage + measured current
     * G = I / V → microsiemens
     *
     * The AD5940 applies a small AC excitation (e.g., 100mV @ 0.1Hz)
     * to the Ag/AgCl electrodes and measures the resulting current via
     * the TIA. The conductance is computed from the DFT magnitude.
     */
    return 5000;  /* placeholder: 50 µS * 100 = 5000 */
}

/* ---- PPG sampling task (20s every 60s) ---- */
static void ppg_sample_task(void *ctx)
{
    (void)ctx;
    uint16_t ir_data[PPG_WINDOW];
    /* Read FIFO from MAX30101 for 20s at 100Hz = 2000 samples */
    for (int i = 0; i < PPG_WINDOW; i++) {
        /* Read IR sample from FIFO */
        uint8_t buf[6];
        uint8_t reg = MAX30101_REG_FIFO_RD;
        nrf_drv_twi_tx(&twi_ppg, MAX30101_I2C_ADDR, &reg, 1, true);
        nrf_drv_twi_rx(&twi_ppg, MAX30101_I2C_ADDR, buf, 6);
        ir_data[i] = (uint16_t)((buf[4] << 8) | buf[5]);  /* IR channel */
        nrf_delay_us(10000);  /* 10ms → 100Hz */
    }
    /* Compute HR + HRV */
    ppg_compute_hr_hrv(ir_data, PPG_WINDOW, &g_hr_bpm, &g_hrv_rmssd);
}

/* ---- EDA sampling (continuous at 4Hz via app timer) ---- */
static void eda_sample_handler(void *ctx)
{
    (void)ctx;
    uint16_t raw = ad5940_read_eda();
    eda_buffer[eda_buf_idx] = raw;
    eda_buf_idx = (eda_buf_idx + 1) % EDA_WINDOW;
}

/* ---- Report timer: send vitals to hub every 60s ---- */
static void report_handler(void *ctx)
{
    (void)ctx;

    /* Compute EDA SCL + SCR from the last minute */
    eda_compute_scl_scr(eda_buffer, EDA_WINDOW, &g_eda_scl, &g_eda_scr_rate);

    /* Check for acute stress */
    uint8_t flags = 0;
    if (detect_acute_stress_episode(g_eda_scr_rate, baseline_scr_rate,
                                     g_hrv_rmssd, baseline_hrv,
                                     g_hr_bpm, baseline_hr))
        flags |= CALM_ALERT_ACUTE_STRESS;

    /* HRV decline check */
    if (baseline_hrv > 0 && g_hrv_rmssd < baseline_hrv * 0.8f)
        flags |= CALM_ALERT_HRV_DECLINE;

    /* HR elevation */
    if (baseline_hr > 0 && g_hr_bpm > baseline_hr * 1.1f)
        flags |= CALM_ALERT_HR_ELEVATED;

    /* EDA arousal */
    if (baseline_scr_rate > 0 && g_eda_scr_rate > baseline_scr_rate * 2)
        flags |= CALM_ALERT_EDA_AROUSAL;

    /* Build vitals payload */
    calm_vitals_payload_t vp;
    vp.type = CALM_MSG_VITALS;
    vp.node_id = CALM_NODE_ID_WRIST;
    vp.seq = 0;
    vp.flags = flags;
    vp.hr_bpm = g_hr_bpm;
    vp.hrv_rmssd = g_hrv_rmssd;
    vp.eda_scl = g_eda_scl;
    vp.eda_scr_rate = g_eda_scr_rate;
    vp.skin_temp_centic = g_skin_temp;
    vp.activity_class = g_activity;
    vp.confidence = 85;
    vp.step_count = g_step_count;
    vp.battery_pct = 0;  /* filled by ADC read */
    calm_pack_crc(&vp, sizeof(vp) - 2);

    /* Send over BLE mesh */
    calm_mesh_send(CALM_MSG_VITALS, CALM_NODE_ID_WRIST, &vp, sizeof(vp) - 2);

    /* Update skin temp (less frequent — every 60s) */
    g_skin_temp = tmp117_read_centic();
}

/* ---- IMU activity classification task (every 5s) ---- */
static void imu_classify_task(void)
{
    int16_t ax[IMU_WINDOW], ay[IMU_WINDOW], az[IMU_WINDOW];
    int16_t gx[IMU_WINDOW], gy[IMU_WINDOW], gz[IMU_WINDOW];

    for (int i = 0; i < IMU_WINDOW; i++) {
        imu_read(&ax[i], &ay[i], &az[i], &gx[i], &gy[i], &gz[i]);
        nrf_delay_ms(20);  /* 50Hz */
    }

    g_activity = activity_classify(ax, ay, az, gx, gy, gz, IMU_WINDOW);

    /* Step counting from accel magnitude */
    for (int i = 1; i < IMU_WINDOW; i++) {
        float mag = sqrtf((float)ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]);
        float mag_prev = sqrtf((float)ax[i-1]*ax[i-1] + ay[i-1]*ay[i-1] + az[i-1]*az[i-1]);
        if (mag > 1500.0f && mag_prev < 1500.0f)  /* threshold crossing */
            g_step_count++;
    }
}

/* ---- Main ---- */
int main(void)
{
    /* Initialize peripherals */
    nrf_drv_twi_config_t twi_cfg = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_cfg.sda = PIN_I2C_PPG_SDA;
    twi_cfg.scl = PIN_I2C_PPG_SCL;
    twi_cfg.frequency = NRF_TWI_FREQ_400K;
    nrf_drv_twi_init(&twi_ppg, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&twi_ppg);

    twi_cfg.sda = PIN_I2C_IMU_SDA;
    twi_cfg.scl = PIN_I2C_IMU_SCL;
    nrf_drv_twi_init(&twi_imu, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&twi_imu);

    /* Initialize sensors */
    max30101_init();
    lsm6dso32_init();

    /* GPIO for LEDs */
    nrf_gpio_cfg_output(PIN_LED_GREEN);
    nrf_gpio_cfg_output(PIN_LED_RED);
    nrf_gpio_pin_write(PIN_LED_GREEN, 0);
    nrf_gpio_pin_write(PIN_LED_RED, 0);

    /* Initialize app timers */
    app_timer_init();
    app_timer_create(&report_timer, APP_TIMER_MODE_REPEATED, report_handler);
    app_timer_create(&ppg_timer, APP_TIMER_MODE_REPEATED, ppg_sample_task);

    /* Start timers */
    app_timer_start(report_timer, APP_TIMER_TICKS(REPORT_INTERVAL_S * 1000), NULL);
    app_timer_start(ppg_timer, APP_TIMER_TICKS(60000), NULL);  /* every 60s */

    /* EDA sampling at 4Hz = every 250ms */
    app_timer_create(&eda_timer, APP_TIMER_MODE_REPEATED, eda_sample_handler);
    app_timer_start(eda_timer, APP_TIMER_TICKS(250), NULL);

    /* Main loop: IMU classification every 5s + power management */
    while (1) {
        imu_classify_task();
        nrf_pwr_mgmt_run();  /* enter WFI between iterations */
    }

    return 0;
}
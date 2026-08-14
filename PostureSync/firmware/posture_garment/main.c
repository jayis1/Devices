/*
 * PostureSync Posture Garment Firmware
 * Target: nRF52840 QFAA + ADS1298 EMG frontend
 *
 * Smart shirt with 8-channel surface EMG for bilateral muscle
 * imbalance detection + 3-segment IMU spinal curvature tracking.
 *
 * Electrodes: Ag/AgCl textile (MedTex 180), washable
 * Channels: L/R Upper Trap, L/R Erector Spinae, L/R SCM, L/R Rectus Abd
 * IMUs: 3× ICM-42688-P (cervical, thoracic, lumbar)
 *
 * Battery: 502035 LiPo 500mAh, 3-day life
 * BLE 5.0 to Hub
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nrf.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "app_timer.h"
#include "app_error.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "ble.h"

#include "../common/protocol.h"

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA       NRF_GPIO_PIN_MAP(0, 2)
#define PIN_I2C_SCL       NRF_GPIO_PIN_MAP(0, 3)
#define PIN_SPI_CS_ADS    NRF_GPIO_PIN_MAP(0, 4)
#define PIN_SPI_SCK       NRF_GPIO_PIN_MAP(0, 5)
#define PIN_SPI_MISO      NRF_GPIO_PIN_MAP(0, 6)
#define PIN_SPI_MOSI      NRF_GPIO_PIN_MAP(0, 7)
#define PIN_SPI_CS_IMU1   NRF_GPIO_PIN_MAP(0, 8)
#define PIN_SPI_CS_IMU2   NRF_GPIO_PIN_MAP(0, 9)
#define PIN_SPI_CS_IMU3   NRF_GPIO_PIN_MAP(0, 10)
#define PIN_ADS_DRDY      NRF_GPIO_PIN_MAP(0, 11)
#define PIN_ADS_START     NRF_GPIO_PIN_MAP(0, 12)
#define PIN_ADS_RESET     NRF_GPIO_PIN_MAP(0, 13)
#define PIN_IMU1_INT      NRF_GPIO_PIN_MAP(0, 14)
#define PIN_IMU2_INT      NRF_GPIO_PIN_MAP(0, 15)
#define PIN_IMU3_INT      NRF_GPIO_PIN_MAP(0, 16)
#define PIN_BAT_SENSE     NRF_GPIO_PIN_MAP(0, 17)
#define PIN_CHG_STAT      NRF_GPIO_PIN_MAP(0, 18)
#define PIN_BTN_PAIR      NRF_GPIO_PIN_MAP(0, 19)
#define PIN_LED_R         NRF_GPIO_PIN_MAP(0, 20)
#define PIN_LED_G         NRF_GPIO_PIN_MAP(0, 21)
#define PIN_LED_B         NRF_GPIO_PIN_MAP(0, 22)

/* SPI */
#define SPI_INSTANCE 0
static nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);

/* I2C */
#define TWI_INSTANCE 0
static nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE);

/* ADS1298 registers */
#define ADS_REG_ID     0x00
#define ADS_REG_CONFIG1 0x01
#define ADS_REG_CONFIG2 0x02
#define ADS_REG_CONFIG3 0x03
#define ADS_REG_CH1SET 0x05
#define ADS_REG_CH8SET 0x0C
#define ADS_REG_CMD    0x00

/* Sampling */
#define EMG_SAMPLE_RATE_HZ 2000
#define EMG_RMS_WINDOW     200  /* 100ms at 2000Hz */

/* 3 IMU Madgwick states (cervical, thoracic, lumbar) */
#include "../spine_band/madgwick_ahrs.h"
static madgwick_state_t g_ahrs[3];

/* EMG data buffers */
static int32_t g_emg_buffer[8][EMG_RMS_WINDOW];
static uint16_t g_emg_buf_idx = 0;
static uint16_t g_emg_rms[8] = {0};
static uint8_t  g_asymmetry_pct = 0;
static uint8_t  g_fatigue_idx = 0;

/* Calibration MVC (Maximum Voluntary Contraction) */
static float g_mvc[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static bool g_calibrated = false;

/* BLE */
#define BLE_CONN_CFG_TAG 1
static uint16_t g_conn_handle = BLE_CONN_HANDLE_INVALID;
static bool g_ble_connected = false;

/* ---- ADS1298 SPI ---- */
static void ads_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[3] = {0x40 | reg, 0x00, val}; /* WREG command */
    nrf_drv_spi_transfer(&spi, tx, 3, NULL, 0);
}

static uint8_t ads_read_reg(uint8_t reg)
{
    uint8_t tx[3] = {0x20 | reg, 0x00, 0x00}; /* RREG command */
    uint8_t rx[3] = {0};
    nrf_drv_spi_transfer(&spi, tx, 3, rx, 3);
    return rx[2];
}

static void ads_read_channels(int32_t *ch)
{
    /* Read 8 channels × 24-bit = 24 bytes + 3 status = 27 bytes */
    uint8_t tx[27] = {0};
    uint8_t rx[27] = {0};
    nrf_drv_spi_transfer(&spi, tx, 27, rx, 27);

    /* Parse 8 channels (skip 3-byte status prefix) */
    for (int i = 0; i < 8; i++) {
        int offset = 3 + i * 3;
        int32_t val = (rx[offset] << 16) | (rx[offset + 1] << 8) | rx[offset + 2];
        /* Sign extend 24-bit */
        if (val & 0x800000) val |= 0xFF000000;
        ch[i] = val;
    }
}

static void ads_init(void)
{
    /* Hardware reset */
    nrf_gpio_pin_write(PIN_ADS_RESET, 0);
    nrf_delay_ms(2);
    nrf_gpio_pin_write(PIN_ADS_RESET, 1);
    nrf_delay_ms(2);

    /* Stop continuous */
    ads_write_reg(ADS_REG_CMD, 0x0A); /* SDATAC */

    /* Config1: 2kHz, HR mode */
    ads_write_reg(ADS_REG_CONFIG1, 0x70);

    /* Config2: internal reference enabled */
    ads_write_reg(ADS_REG_CONFIG2, 0x10);

    /* Config3: internal reference, 2.4V reference */
    ads_write_reg(ADS_REG_CONFIG3, 0xEC);

    /* All channels: normal input, gain 6, no SRB2 */
    for (int ch = 0; ch < 8; ch++) {
        ads_write_reg(ADS_REG_CH1SET + ch, 0x00); /* Normal input, gain 6 */
    }

    /* Start conversions */
    nrf_gpio_pin_write(PIN_ADS_START, 1);
    ads_write_reg(ADS_REG_CMD, 0x08); /* RDATAC */
}

/* ---- IMU (3× ICM-42688-P) ---- */
static void imu_write_reg(uint8_t cs_pin, uint8_t reg, uint8_t val)
{
    nrf_gpio_pin_write(cs_pin, 0);
    uint8_t tx[2] = {reg & 0x7F, val};
    nrf_drv_spi_transfer(&spi, tx, 2, NULL, 0);
    nrf_gpio_pin_write(cs_pin, 1);
}

static uint8_t imu_read_reg(uint8_t cs_pin, uint8_t reg)
{
    nrf_gpio_pin_write(cs_pin, 0);
    uint8_t tx[2] = {reg | 0x80, 0};
    uint8_t rx[2] = {0};
    nrf_drv_spi_transfer(&spi, tx, 2, rx, 2);
    nrf_gpio_pin_write(cs_pin, 1);
    return rx[1];
}

static void imu_read_xyz(uint8_t cs_pin, float *ax, float *ay, float *az,
                         float *gx, float *gy, float *gz)
{
    nrf_gpio_pin_write(cs_pin, 0);
    uint8_t tx[13] = {0x0B | 0x80, 0};
    uint8_t rx[13] = {0};
    nrf_drv_spi_transfer(&spi, tx, 13, rx, 13);
    nrf_gpio_pin_write(cs_pin, 1);

    int16_t ax_raw = (rx[1] << 8) | rx[2];
    int16_t ay_raw = (rx[3] << 8) | rx[4];
    int16_t az_raw = (rx[5] << 8) | rx[6];
    int16_t gx_raw = (rx[7] << 8) | rx[8];
    int16_t gy_raw = (rx[9] << 8) | rx[10];
    int16_t gz_raw = (rx[11] << 8) | rx[12];

    *ax = ax_raw / 8192.0f;
    *ay = ay_raw / 8192.0f;
    *az = az_raw / 8192.0f;
    *gx = gx_raw / 16.4f;
    *gy = gy_raw / 16.4f;
    *gz = gz_raw / 16.4f;
}

static void imu_init(uint8_t cs_pin)
{
    nrf_gpio_cfg_output(cs_pin);
    nrf_gpio_pin_write(cs_pin, 1);

    imu_write_reg(cs_pin, 0x4E, 0x80); /* Reset */
    nrf_delay_ms(10);
    imu_write_reg(cs_pin, 0x4F, 0x0C); /* Gyro: ±2000 dps, 200Hz */
    imu_write_reg(cs_pin, 0x50, 0x0C); /* Accel: ±4g, 200Hz */
    imu_write_reg(cs_pin, 0x4E, 0x0F); /* LN mode */
    nrf_delay_ms(10);
}

/* ---- EMG processing ---- */
static void emg_compute_rms(void)
{
    for (int ch = 0; ch < 8; ch++) {
        float sum_sq = 0;
        for (int i = 0; i < EMG_RMS_WINDOW; i++) {
            float v = (float)g_emg_buffer[ch][i] / 1000.0f; /* uV */
            sum_sq += v * v;
        }
        float rms = sqrtf(sum_sq / EMG_RMS_WINDOW);
        /* Normalize to MVC */
        float normalized = rms / g_mvc[ch];
        if (normalized > 65.535f) normalized = 65.535f;
        g_emg_rms[ch] = (uint16_t)(normalized * 1000.0f); /* mV × 1000 */
    }
}

static void emg_compute_asymmetry(void)
{
    /* Bilateral pairs: (0,1)=Upper Trap, (2,3)=Erector, (4,5)=SCM, (6,7)=Rectus */
    float max_asym = 0;
    for (int pair = 0; pair < 4; pair++) {
        float left = g_emg_rms[pair * 2];
        float right = g_emg_rms[pair * 2 + 1];
        if (left + right > 0) {
            float asym = fabsf(left - right) / (left + right) * 100.0f;
            if (asym > max_asym) max_asym = asym;
        }
    }
    g_asymmetry_pct = (uint8_t)max_asym;
}

static void emg_compute_fatigue(void)
{
    /* Fatigue index: simplified — based on signal amplitude decline
     * In production: median frequency decline via FFT */
    static float initial_rms[8] = {0};
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < 8; i++) initial_rms[i] = g_emg_rms[i];
        initialized = true;
    }

    float decline_sum = 0;
    for (int i = 0; i < 8; i++) {
        if (initial_rms[i] > 0) {
            float ratio = g_emg_rms[i] / initial_rms[i];
            decline_sum += (1.0f - ratio);
        }
    }
    g_fatigue_idx = (uint8_t)(decline_sum / 8.0f * 100.0f);
    if (g_fatigue_idx > 100) g_fatigue_idx = 100;
}

/* ---- Spinal curvature calculation ---- */
static void compute_spinal_curvature(float *cervical, float *thoracic, float *lumbar)
{
    /* Get Euler angles from each IMU segment */
    float pitch[3], roll[3], yaw[3];
    for (int i = 0; i < 3; i++) {
        madgwick_get_euler(&g_ahrs[i], &pitch[i], &roll[i], &yaw[i]);
    }

    /* Curvature = angle difference between adjacent segments */
    *cervical = pitch[0] - pitch[1];  /* cervical relative to thoracic */
    *thoracic = pitch[1] - pitch[2];  /* thoracic relative to lumbar */
    *lumbar   = pitch[2];              /* lumbar relative to vertical */
}

/* ---- Fuel gauge ---- */
static uint8_t fuel_gauge_read(void)
{
    uint8_t reg = 0x02;
    uint8_t data[2];
    nrf_drv_twi_tx(&twi, 0x36, &reg, 1, true);
    nrf_drv_twi_rx(&twi, 0x36, data, 2);
    uint16_t vcell = ((data[0] << 8) | data[1]) >> 4;
    float voltage = vcell * 1.25f / 1000.0f;
    int soc = (int)((voltage - 3.0f) / 1.2f * 100.0f);
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;
    return (uint8_t)soc;
}

/* ---- BLE service ---- */
static ble_gatts_char_handles_t g_emg_handles;
static ble_gatts_char_handles_t g_battery_handles;
static ble_gatts_char_handles_t g_cal_handles;

static void ble_service_init(void)
{
    /* Simplified BLE service setup — same UUID base as Spine Band */
    /* PostureSync service with EMG data characteristic */
}

static void ble_send_garment_data(void)
{
    if (g_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    float cervical, thoracic, lumbar;
    compute_spinal_curvature(&cervical, &thoracic, &lumbar);

    garment_data_t data = {0};
    for (int i = 0; i < 8; i++) data.emg_rms[i] = g_emg_rms[i];
    data.cervical_angle = cervical;
    data.thoracic_angle = thoracic;
    data.lumbar_angle = lumbar;
    data.asymmetry_pct = g_asymmetry_pct;
    data.fatigue_idx = g_fatigue_idx;
    data.battery = fuel_gauge_read();
    data.flags = g_calibrated ? 0x01 : 0x00;

    /* Send via BLE notification */
    ble_gatts_hvx_params_t hvx = {0};
    uint16_t len = sizeof(data);
    hvx.handle = g_emg_handles.value_handle;
    hvx.type = BLE_GATT_HVX_NOTIFICATION;
    hvx.p_data = (uint8_t *)&data;
    hvx.p_len = &len;
    sd_ble_gatts_hvx(g_conn_handle, &hvx);
}

/* ---- Timers ---- */
APP_TIMER_DEF(g_emg_timer);   /* 2000 Hz EMG sampling */
APP_TIMER_DEF(g_imu_timer);   /* 200 Hz IMU sampling */
APP_TIMER_DEF(g_ble_timer);   /* 10 Hz BLE TX */

static void emg_timer_handler(void *p_context)
{
    /* Read 8-channel EMG from ADS1298 */
    int32_t ch[8];
    ads_read_channels(ch);

    /* Store in ring buffer */
    for (int i = 0; i < 8; i++) {
        g_emg_buffer[i][g_emg_buf_idx] = ch[i];
    }
    g_emg_buf_idx = (g_emg_buf_idx + 1) % EMG_RMS_WINDOW;

    /* Compute RMS every full window */
    if (g_emg_buf_idx == 0) {
        emg_compute_rms();
        emg_compute_asymmetry();
        emg_compute_fatigue();
    }
}

static void imu_timer_handler(void *p_context)
{
    /* Read all 3 IMUs and update AHRS */
    uint8_t cs_pins[3] = {PIN_SPI_CS_IMU1, PIN_SPI_CS_IMU2, PIN_SPI_CS_IMU3};
    for (int i = 0; i < 3; i++) {
        float ax, ay, az, gx, gy, gz;
        imu_read_xyz(cs_pins[i], &ax, &ay, &az, &gx, &gy, &gz);
        madgwick_update(&g_ahrs[i], gx, gy, gz, ax, ay, az);
    }
}

static void ble_timer_handler(void *p_context)
{
    ble_send_garment_data();
}

/* ---- Calibration ---- */
static void run_calibration(void)
{
    NRF_LOG_INFO("Garment calibration: stand neutral");
    nrf_gpio_pin_write(PIN_LED_B, 1);

    /* Record baseline EMG (2 seconds) */
    memset(g_emg_buffer, 0, sizeof(g_emg_buffer));
    nrf_delay_ms(2000);
    emg_compute_rms();
    float baseline[8];
    for (int i = 0; i < 8; i++) baseline[i] = g_emg_rms[i];

    NRF_LOG_INFO("Contract each muscle group (MVC)");

    /* MVC for each channel pair */
    const char *muscles[4] = {"Upper Trapezius", "Erector Spinae", "SCM", "Rectus Abdominis"};
    for (int pair = 0; pair < 4; pair++) {
        NRF_LOG_INFO("Contract %s (L+R) for 3 seconds", muscles[pair]);
        nrf_delay_ms(3000);
        emg_compute_rms();
        g_mvc[pair * 2] = g_emg_rms[pair * 2] / 1000.0f;
        g_mvc[pair * 2 + 1] = g_emg_rms[pair * 2 + 1] / 1000.0f;
        if (g_mvc[pair * 2] < 0.001f) g_mvc[pair * 2] = 0.001f;
        if (g_mvc[pair * 2 + 1] < 0.001f) g_mvc[pair * 2 + 1] = 0.001f;
    }

    g_calibrated = true;
    nrf_gpio_pin_write(PIN_LED_B, 0);
    nrf_gpio_pin_write(PIN_LED_G, 1);
    NRF_LOG_INFO("Garment calibration complete");
}

/* ---- Main ---- */
int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    NRF_LOG_INFO("PostureSync Posture Garment starting...");

    app_timer_init();

    /* I2C */
    nrf_drv_twi_config_t twi_cfg = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_cfg.sda = PIN_I2C_SDA;
    twi_cfg.scl = PIN_I2C_SCL;
    twi_cfg.frequency = NRF_TWIM_FREQ_400K;
    nrf_drv_twi_init(&twi, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&twi);

    /* SPI */
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_cfg.ss_pin = NRF_DRV_SPI_PIN_NOT_USED; /* Manual CS control */
    spi_cfg.mosi_pin = PIN_SPI_MOSI;
    spi_cfg.miso_pin = PIN_SPI_MISO;
    spi_cfg.sck_pin = PIN_SPI_SCK;
    spi_cfg.frequency = NRF_DRV_SPI_FREQ_4M;
    nrf_drv_spi_init(&spi, &spi_cfg, NULL, NULL);

    /* GPIO */
    nrf_gpio_cfg_output(PIN_LED_R);
    nrf_gpio_cfg_output(PIN_LED_G);
    nrf_gpio_cfg_output(PIN_LED_B);
    nrf_gpio_cfg_output(PIN_ADS_START);
    nrf_gpio_cfg_output(PIN_ADS_RESET);
    nrf_gpio_cfg_input(PIN_ADS_DRDY, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(PIN_BTN_PAIR, NRF_GPIO_PIN_PULLUP);

    /* Init AHRS for 3 segments */
    for (int i = 0; i < 3; i++) {
        madgwick_init(&g_ahrs[i], 0.1f, 1.0f / 200.0f);
    }

    /* Init sensors */
    ads_init();
    imu_init(PIN_SPI_CS_IMU1);
    imu_init(PIN_SPI_CS_IMU2);
    imu_init(PIN_SPI_CS_IMU3);

    /* BLE */
    nrf_sdh_enable_request();
    nrf_sdh_ble_default_cfg_set(BLE_CONN_CFG_TAG, 1);
    nrf_sdh_ble_enable(NULL);
    ble_service_init();

    /* Timers */
    app_timer_create(&g_emg_timer, APP_TIMER_MODE_REPEATED, emg_timer_handler);
    app_timer_start(&g_emg_timer, APP_TIMER_TICKS(0.5), NULL); /* 2000 Hz */

    app_timer_create(&g_imu_timer, APP_TIMER_MODE_REPEATED, imu_timer_handler);
    app_timer_start(&g_imu_timer, APP_TIMER_TICKS(5), NULL); /* 200 Hz */

    app_timer_create(&g_ble_timer, APP_TIMER_MODE_REPEATED, ble_timer_handler);
    app_timer_start(&g_ble_timer, APP_TIMER_TICKS(100), NULL); /* 10 Hz */

    /* Calibration check */
    if (nrf_gpio_pin_read(PIN_BTN_PAIR) == 0) {
        run_calibration();
    }

    NRF_LOG_INFO("Posture Garment running");

    while (1) {
        __WFE();
        NRF_LOG_FLUSH();
    }
}
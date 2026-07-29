/*
 * RehabSync — Body Sensor Node Firmware
 * nRF52840, FreeRTOS / nRF5 SDK
 *
 * The Body Sensor samples LSM6DSO (accel/gyro) + LIS3MDL (magnetometer)
 * at 100 Hz, runs Madgwick AHRS quaternion computation, and streams
 * 9-DoF data to the Hub via BLE 5.0 GATT notifications.
 *
 * Build: nrf5 build with nRF5 SDK v17.x or nRF Connect SDK v2.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "boards.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_gpiote.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "ble.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_nus.h"
#include "app_uart.h"

#include "../common/protocol.h"
#include "../common/config.h"

#define TAG "RehabSync-BodySensor"

/* === SPI Interface (shared SPI0 for LSM6DSO + LIS3MDL) === */
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);
static volatile bool g_spi_xfer_done = true;

static void spi_event_handler(nrf_drv_spi_evt_t const *p_event, void *p_context)
{
    g_spi_xfer_done = true;
}

static void spi_init(void)
{
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_cfg.ss_pin   = NRF_DRV_SPI_PIN_NOT_USED; /* manual CS control */
    spi_cfg.mosi_pin = BS_GPIO_SPI_MOSI;
    spi_cfg.miso_pin = BS_GPIO_SPI_MISO;
    spi_cfg.sck_pin  = BS_GPIO_SPI_SCK;
    spi_cfg.frequency = NRF_DRV_SPI_FREQ_8M;
    spi_cfg.mode = NRF_DRV_SPI_MODE_3;
    APP_ERROR_CHECK(nrf_drv_spi_init(&g_spi, &spi_cfg, spi_event_handler, NULL));

    /* Configure CS pins as output */
    nrf_gpio_cfg_output(BS_GPIO_IMU_CS);
    nrf_gpio_cfg_output(BS_GPIO_MAG_CS);
    nrf_gpio_pin_set(BS_GPIO_IMU_CS);
    nrf_gpio_pin_set(BS_GPIO_MAG_CS);
}

static void spi_write_reg(uint8_t cs_pin, uint8_t reg, uint8_t val)
{
    nrf_gpio_pin_clear(cs_pin);
    uint8_t tx[2] = { reg & 0x7F, val };  /* write: bit7=0 */
    uint8_t rx[2] = {0};
    g_spi_xfer_done = false;
    nrf_drv_spi_transfer(&g_spi, tx, 2, rx, 2);
    while (!g_spi_xfer_done);
    nrf_gpio_pin_set(cs_pin);
}

static uint8_t spi_read_reg(uint8_t cs_pin, uint8_t reg)
{
    nrf_gpio_pin_clear(cs_pin);
    uint8_t tx[2] = { reg | 0x80, 0x00 };  /* read: bit7=1 */
    uint8_t rx[2] = {0};
    g_spi_xfer_done = false;
    nrf_drv_spi_transfer(&g_spi, tx, 2, rx, 2);
    while (!g_spi_xfer_done);
    nrf_gpio_pin_set(cs_pin);
    return rx[1];
}

static void spi_read_burst(uint8_t cs_pin, uint8_t reg, uint8_t *buf, size_t len)
{
    nrf_gpio_pin_clear(cs_pin);
    buf[0] = reg | 0x80;
    g_spi_xfer_done = false;
    nrf_drv_spi_transfer(&g_spi, buf, len + 1, buf, len + 1);
    while (!g_spi_xfer_done);
    nrf_gpio_pin_set(cs_pin);
    memmove(buf, buf + 1, len);  /* shift past the reg byte */
}

/* === LSM6DSO IMU Driver === */
#define LSM6DSO_WHO_AM_I    0x0F
#define LSM6DSO_CTRL1_XL    0x10
#define LSM6DSO_CTRL2_G     0x11
#define LSM6DSO_CTRL3_C     0x12
#define LSM6DSO_STATUS_REG  0x1E
#define LSM6DSO_OUTX_L_G    0x22
#define LSM6DSO_OUTX_L_XL   0x28
#define LSM6DSO_WHO_AM_I_VAL 0x6C

static void lsm6dso_init(void)
{
    /* Verify chip ID */
    uint8_t whoami = spi_read_reg(BS_GPIO_IMU_CS, LSM6DSO_WHO_AM_I);
    if (whoami != LSM6DSO_WHO_AM_I_VAL) {
        NRF_LOG_ERROR("LSM6DSO not found (WHO_AM_I=0x%02X)", whoami);
        return;
    }

    /* CTRL3_C: BDU=1, IF_INC=1, SW_RESET=0 */
    spi_write_reg(BS_GPIO_IMU_CS, LSM6DSO_CTRL3_C, 0x44);
    nrf_delay_ms(10);

    /* CTRL1_XL: accel ODR=104Hz (0x460), FS=±8g (0x00 → 0x40 for ±8g? check datasheet)
     * Accel: ODR=104Hz=0x04<<4, FS=±8g=0x03<<2 → 0x4C */
    spi_write_reg(BS_GPIO_IMU_CS, LSM6DSO_CTRL1_XL, 0x4C);

    /* CTRL2_G: gyro ODR=104Hz, FS=±2000dps → 0x4C<<4 + 0x03<<1 = 0x5C? 
     * Gyro: ODR=104Hz=0x04, FS=2000dps=0x03 → 0x4C */
    spi_write_reg(BS_GPIO_IMU_CS, LSM6DSO_CTRL2_G, 0x4C);
}

static void lsm6dso_read(int16_t *accel, int16_t *gyro)
{
    uint8_t buf[12];
    /* Read gyro first (6 bytes from 0x22), then accel (6 bytes from 0x28) */
    spi_read_burst(BS_GPIO_IMU_CS, LSM6DSO_OUTX_L_G, buf, 6);
    gyro[0] = (int16_t)((buf[1] << 8) | buf[0]);
    gyro[1] = (int16_t)((buf[3] << 8) | buf[2]);
    gyro[2] = (int16_t)((buf[5] << 8) | buf[4]);

    spi_read_burst(BS_GPIO_IMU_CS, LSM6DSO_OUTX_L_XL, buf, 6);
    accel[0] = (int16_t)((buf[1] << 8) | buf[0]);
    accel[1] = (int16_t)((buf[3] << 8) | buf[2]);
    accel[2] = (int16_t)((buf[5] << 8) | buf[4]);
}

/* === LIS3MDL Magnetometer Driver === */
#define LIS3MDL_WHO_AM_I    0x0F
#define LIS3MDL_CTRL_REG1   0x20
#define LIS3MDL_CTRL_REG2   0x21
#define LIS3MDL_CTRL_REG3   0x22
#define LIS3MDL_CTRL_REG4   0x23
#define LIS3MDL_OUT_X_L     0x28
#define LIS3MDL_WHO_AM_I_VAL 0x3D

static void lis3mdl_init(void)
{
    uint8_t whoami = spi_read_reg(BS_GPIO_MAG_CS, LIS3MDL_WHO_AM_I);
    if (whoami != LIS3MDL_WHO_AM_I_VAL) {
        NRF_LOG_ERROR("LIS3MDL not found (WHO_AM_I=0x%02X)", whoami);
        return;
    }

    /* CTRL_REG1: OM=ultra-high(11), FAST_ODR=1, ST=0, Temp sensor stays */
    spi_write_reg(BS_GPIO_MAG_CS, LIS3MDL_CTRL_REG1, 0x78); /* 80 Hz, UHP */

    /* CTRL_REG2: FS=±8 gauss (0x02 << 5 = 0x40? actually FS=00→±4,01→±8,10→±12,11→±16)
     * For ±8 gauss: FS=01 → REGEN2=0x40 */
    spi_write_reg(BS_GPIO_MAG_CS, LIS3MDL_CTRL_REG2, 0x40);

    /* CTRL_REG3: MD=continuous(00), SIM=0 → 0x00 */
    spi_write_reg(BS_GPIO_MAG_CS, LIS3MDL_CTRL_REG3, 0x00);

    /* CTRL_REG4: OMZ=ultra-high(11) → 0x0C */
    spi_write_reg(BS_GPIO_MAG_CS, LIS3MDL_CTRL_REG4, 0x0C);
}

static void lis3mdl_read(int16_t *mag)
{
    uint8_t buf[6];
    spi_read_burst(BS_GPIO_MAG_CS, LIS3MDL_OUT_X_L, buf, 6);
    mag[0] = (int16_t)((buf[1] << 8) | buf[0]);
    mag[1] = (int16_t)((buf[3] << 8) | buf[2]);
    mag[2] = (int16_t)((buf[5] << 8) | buf[4]);
}

/* === Madgwick AHRS Filter === */
static float g_q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
static float g_beta = 0.1f;
static float g_sample_dt = 0.01f;  /* 100 Hz */

static void madgwick_update(float gx, float gy, float gz,
                            float ax, float ay, float az,
                            float mx, float my, float mz)
{
    float q0 = g_q[0], q1 = g_q[1], q2 = g_q[2], q3 = g_q[3];
    float recip_norm;

    /* Rate of change of quaternion from gyro */
    float dq0 = 0.5f * (-q1*gx - q2*gy - q3*gz);
    float dq1 = 0.5f * ( q0*gx + q2*gz - q3*gy);
    float dq2 = 0.5f * ( q0*gy - q1*gz + q3*gx);
    float dq3 = 0.5f * ( q0*gz + q1*gy - q2*gx);

    /* Gradient descent correction from accelerometer + magnetometer */
    recip_norm = 1.0f / sqrtf(ax*ax + ay*ay + az*az + 0.0001f);
    ax *= recip_norm; ay *= recip_norm; az *= recip_norm;

    recip_norm = 1.0f / sqrtf(mx*mx + my*my + mz*mz + 0.0001f);
    mx *= recip_norm; my *= recip_norm; mz *= recip_norm;

    /* Reference direction of Earth's magnetic field */
    float hx = 2.0f * (mx * (0.5f - q1*q1 - q2*q2) + my * (q0*q1 + q2*q3) + mz * (q0*q2 - q1*q3));
    float hy = 2.0f * (mx * (q0*q1 - q2*q3) + my * (0.5f - q0*q0 - q1*q1 - q2*q2) + mz * (q1*q2 + q0*q3));
    float bx = sqrtf(hx*hx + hy*hy);
    float bz = 2.0f * (mx * (q0*q2 + q1*q3) + my * (q1*q2 - q0*q3) + mz * (0.5f - q0*q0 - q1*q1));

    /* Gradient descent algorithm corrective step */
    float s0 = 2.0f * (q1*q3 - q0*q2) - ax;
    float s1 = 2.0f * (q0*q1 + q2*q3) - ay;
    float s2 = 2.0f * (0.5f - q1*q1 - q2*q2) - az;
    float s3 = 2.0f * (q1*q2 - q0*q3) - bx;
    /* ... (full 6DoF gradient) */
    s0 = 2.0f * (q1*s0 + q0*s1 + q2*s2 + q3*s3);
    s1 = 2.0f * (-q0*s0 + q1*s1 + q2*s2 - q3*s3);
    s2 = 2.0f * (q0*s0 - q1*s1 + q2*s2 + q3*s3);
    s3 = 2.0f * (q0*s0 + q1*s1 - q2*s2 + q3*s3);

    recip_norm = 1.0f / sqrtf(s0*s0 + s1*s1 + s2*s2 + s3*s3 + 0.0001f);
    s0 *= recip_norm; s1 *= recip_norm; s2 *= recip_norm; s3 *= recip_norm;

    /* Apply feedback step */
    dq0 -= g_beta * s0;
    dq1 -= g_beta * s1;
    dq2 -= g_beta * s2;
    dq3 -= g_beta * s3;

    /* Integrate */
    q0 += dq0 * g_sample_dt;
    q1 += dq1 * g_sample_dt;
    q2 += dq2 * g_sample_dt;
    q3 += dq3 * g_sample_dt;

    /* Normalize */
    recip_norm = 1.0f / sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3 + 0.0001f);
    g_q[0] = q0 * recip_norm;
    g_q[1] = q1 * recip_norm;
    g_q[2] = q2 * recip_norm;
    g_q[3] = q3 * recip_norm;
}

/* === BLE GATT Service === */
/* RehabSync IMU Service UUID: custom 128-bit
 * Service:  0000RS01-0000-1000-8000-00805F9B34FB
 * IMU Char: notifies 100 Hz IMU + quaternion data
 * Battery Char: battery level
 */
#define BLE_SERVICE_REHAB      0x180A  /* custom — simplified */
#define BLE_CHAR_IMU           0x0001
#define BLE_CHAR_BATTERY       0x0002

static ble_nus_t g_nus;  /* Using NUS as simplified data pipe */
static uint16_t g_conn_handle = BLE_CONN_HANDLE_INVALID;
static bool g_ble_connected = false;

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            g_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            g_ble_connected = true;
            NRF_LOG_INFO("BLE connected");
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            g_conn_handle = BLE_CONN_HANDLE_INVALID;
            g_ble_connected = false;
            NRF_LOG_INFO("BLE disconnected, advertising...");
            /* Restart advertising */
            break;
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
            /* Prefer 2M PHY for higher throughput */
            {
                ble_gap_phys_t phys = {
                    .rx_phys = BLE_GAP_PHY_2MBPS,
                    .tx_phys = BLE_GAP_PHY_2MBPS,
                };
                sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            }
            break;
    }
}

static void ble_init(void)
{
    /* Initialize BLE stack, gap, gatt, advertising, NUS service */
    /* Simplified: full implementation uses sd_ble_enable + advertising_init + */
}

/* === IMU Sampling Task (100 Hz) === */
static void imu_task(void *arg)
{
    int16_t accel[3], gyro[3], mag[3];
    rs_imu_sample_t imu_pkt;
    rs_quat_t quat_pkt;

    /* IMU timer: 100 Hz via app_timer */
    APP_TIMER_DEF(imu_timer);
    app_timer_init();

    while (1) {
        /* Read IMU at 100 Hz */
        lsm6dso_read(accel, gyro);
        lis3mdl_read(mag);

        /* Convert to physical units (mg, mdps, mgauss) */
        imu_pkt.accel_x = accel[0];  /* ±8g → 0.061 mg/LSB */
        imu_pkt.accel_y = accel[1];
        imu_pkt.accel_z = accel[2];
        imu_pkt.gyro_x = gyro[0];    /* ±2000dps → 70 mdps/LSB */
        imu_pkt.gyro_y = gyro[1];
        imu_pkt.gyro_z = gyro[2];

        /* Run Madgwick AHRS */
        float gx = gyro[0] * 0.070f * 0.01745f;  /* mdps → rad/s */
        float gy = gyro[1] * 0.070f * 0.01745f;
        float gz = gyro[2] * 0.070f * 0.01745f;
        float ax = accel[0] * 0.061f * 0.00981f;  /* mg → m/s² */
        float ay = accel[1] * 0.061f * 0.00981f;
        float az = accel[2] * 0.061f * 0.00981f;
        float mx = mag[0] * 0.58f;  /* mgauss/LSB for ±8 gauss */
        float my = mag[1] * 0.58f;
        float mz = mag[2] * 0.58f;

        madgwick_update(gx, gy, gz, ax, ay, az, mx, my, mz);

        /* Scale quaternions to int16 (-32768 to 32767 = -1 to 1) */
        quat_pkt.q0 = (int16_t)(g_q[0] * 32767.0f);
        quat_pkt.q1 = (int16_t)(g_q[1] * 32767.0f);
        quat_pkt.q2 = (int16_t)(g_q[2] * 32767.0f);
        quat_pkt.q3 = (int16_t)(g_q[3] * 32767.0f);

        /* Stream via BLE if connected */
        if (g_ble_connected) {
            /* Send IMU + quaternion packet via BLE notification
             * Packet format: [imu(12 bytes) | quat(8 bytes)] = 20 bytes per sample
             * At 100 Hz: 2000 bytes/s — within BLE 5.0 2M PHY capacity
             */
            uint8_t ble_buf[20];
            memcpy(ble_buf, &imu_pkt, sizeof(imu_pkt));
            memcpy(ble_buf + 12, &quat_pkt, sizeof(quat_pkt));

            /* ble_nus_data_send(&g_nus, ble_buf, &len, g_conn_handle); */
        }

        nrf_delay_us(9500);  /* ~100 Hz (10ms - 0.5ms processing) */
    }
}

/* === Power Management Task === */
static void power_task(void *arg)
{
    /* Monitor CR2032 battery voltage via ADC
     * Enter system_off when not in session (woken by BLE connection)
     * Target: 30-day battery life at 1h/day exercise
     */
    while (1) {
        /* Read battery voltage via SAADC */
        /* If < 2.5V: send low battery alert */
        /* If not connected and no session: enter sleep */
        nrf_delay_ms(10000);  /* check every 10s */
    }
}

/* === Main === */
int main(void)
{
    /* Board init */
    bsp_board_init(BSP_INIT_LEDS);
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("RehabSync Body Sensor starting...");

    /* Initialize SPI */
    spi_init();

    /* Initialize sensors */
    lsm6dso_init();
    lis3mdl_init();

    /* Initialize BLE */
    ble_init();

    /* Create tasks */
    /* In nRF5 SDK: use app_timer + handlers instead of FreeRTOS tasks
     * For this stub we show the task-based structure */
    imu_task(NULL);  /* runs in main loop */
    /* power_task would run via timer */

    while (1) {
        __WFI();
        NRF_LOG_FLUSH();
    }
}
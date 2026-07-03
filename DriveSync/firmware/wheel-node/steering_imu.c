/**
 * DriveSync Steering IMU Driver — LSM6DSO
 *
 * I2C interface (TWI1: SDA=P0.24, SCL=P0.25)
 * 1 kHz sample rate, FIFO watermark at 25 samples (25ms).
 *
 * License: MIT
 */

#include "steering_imu.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"
#include "app_util_platform.h"
#include <string.h>

#define LSM6DSO_ADDR        0x6A
#define LSM6DSO_WHOAMI      0x0F
#define LSM6DSO_CTRL1_XL    0x10  /* Accel config */
#define LSM6DSO_CTRL2_G     0x11  /* Gyro config */
#define LSM6DSO_FIFO_CTRL3  0x08
#define FIFO_DATA           0x3F

static nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(1);
static imu_data_cb_t s_callback = NULL;

/* ── TWI Init ────────────────────────────────────────────────────── */

static void twi_init(void)
{
    nrf_drv_twi_config_t twi_config = {
        .scl = 25,
        .sda = 24,
        .frequency = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false,
    };
    nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&m_twi);
}

/* ── I2C Write/Read ──────────────────────────────────────────────── */

static void lsm6dso_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, buf, 2, false);
}

static uint8_t lsm6dso_read_reg(uint8_t reg)
{
    uint8_t val = 0;
    nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, LSM6DSO_ADDR, &val, 1);
    return val;
}

static void lsm6dso_read_fifo(int16_t *gyro_z, int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[12];
    nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, &FIFO_DATA, 1, true);
    nrf_drv_twi_rx(&m_twi, LSM6DSO_ADDR, buf, 12);

    *ax = (int16_t)((buf[1] << 8) | buf[0]);
    *ay = (int16_t)((buf[3] << 8) | buf[2]);
    *az = (int16_t)((buf[5] << 8) | buf[4]);
    *gyro_z = (int16_t)((buf[11] << 8) | buf[10]);
}

/* ── Init ────────────────────────────────────────────────────────── */

void steering_imu_init(imu_data_cb_t callback)
{
    s_callback = callback;
    twi_init();

    /* Verify device ID */
    uint8_t whoami = lsm6dso_read_reg(LSM6DSO_WHOAMI);
    if (whoami != 0x6C) {
        NRF_LOG_ERROR("LSM6DSO not found (whoami=0x%02X)", whoami);
        return;
    }

    /* Configure accelerometer: 4g, 1.66 kHz */
    lsm6dso_write_reg(LSM6DSO_CTRL1_XL, 0x68);

    /* Configure gyroscope: 2000 dps, 1.66 kHz */
    lsm6dso_write_reg(LSM6DSO_CTRL2_G, 0x6C);

    /* Enable FIFO: continuous mode, batched at 1.66 kHz */
    lsm6dso_write_reg(LSM6DSO_FIFO_CTRL3, 0x01);

    NRF_LOG_INFO("LSM6DSO initialized (1.66 kHz, FIFO enabled)");
}

void steering_imu_process_fifo(void)
{
    int16_t gyro_z, ax, ay, az;
    lsm6dso_read_fifo(&gyro_z, &ax, &ay, &az);

    /* Convert to milli-g and milli-degrees/sec */
    /* LSM6DSO at 4g: 0.122 mg/LSB */
    /* LSM6DSO at 2000 dps: 70 mdps/LSB */
    int16_t ax_mg = (int16_t)(ax * 0.122f);
    int16_t ay_mg = (int16_t)(ay * 0.122f);
    int16_t az_mg = (int16_t)(az * 0.122f);
    int16_t gz_mds = (int16_t)(gyro_z * 70);

    if (s_callback) {
        s_callback(gz_mds, ax_mg, ay_mg, az_mg);
    }
}

int16_t steering_imu_get_angular_velocity(void)
{
    int16_t gz, ax, ay, az;
    lsm6dso_read_fifo(&gz, &ax, &ay, &az);
    return (int16_t)(gz * 70);
}
/**
 * DriveSync Body IMU Driver — LSM6DSO
 *
 * Measures torso sway for drowsiness detection.
 * I2C interface (TWI2: SDA=P0.27, SCL=P0.28).
 * 50 Hz sample rate.
 *
 * License: MIT
 */

#include "body_imu.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"

#define LSM6DSO_ADDR    0x6A
#define LSM6DSO_WHOAMI  0x0F
#define LSM6DSO_CTRL1_XL 0x10
#define LSM6DSO_CTRL2_G  0x11
#define LSM6DSO_OUTX_L_XL 0x28
#define LSM6DSO_OUTX_L_G  0x22

static nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(2);

static void twi_init(void)
{
    nrf_drv_twi_config_t twi_config = {
        .scl = 28,
        .sda = 27,
        .frequency = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false,
    };
    nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    nrf_drv_twi_enable(&m_twi);
}

static uint8_t lsm_read_reg(uint8_t reg)
{
    uint8_t val = 0;
    nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, LSM6DSO_ADDR, &val, 1);
    return val;
}

static void lsm_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, buf, 2, false);
}

void body_imu_init(void)
{
    twi_init();

    uint8_t whoami = lsm_read_reg(LSM6DSO_WHOAMI);
    if (whoami != 0x6C) {
        NRF_LOG_ERROR("LSM6DSO not found (whoami=0x%02X)", whoami);
        return;
    }

    /* Accel: 4g, 104 Hz */
    lsm_write_reg(LSM6DSO_CTRL1_XL, 0x48);

    /* Gyro: 2000 dps, 104 Hz */
    lsm_write_reg(LSM6DSO_CTRL2_G, 0x4C);

    NRF_LOG_INFO("LSM6DSO body IMU initialized (50 Hz)");
}

void body_imu_read(int16_t *ax, int16_t *ay, int16_t *az,
                   int16_t *gx, int16_t *gy, int16_t *gz)
{
    /* Read 6 bytes accel + 6 bytes gyro */
    uint8_t buf[12];
    uint8_t reg = LSM6DSO_OUTX_L_G;  /* Read gyro first (contiguous) */
    nrf_drv_twi_tx(&m_twi, LSM6DSO_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, LSM6DSO_ADDR, buf, 12);

    /* Gyro (first 6 bytes) */
    *gx = (int16_t)((buf[1] << 8) | buf[0]);
    *gy = (int16_t)((buf[3] << 8) | buf[2]);
    *gz = (int16_t)((buf[5] << 8) | buf[4]);

    /* Accel (next 6 bytes) */
    *ax = (int16_t)((buf[7] << 8) | buf[6]);
    *ay = (int16_t)((buf[9] << 8) | buf[8]);
    *az = (int16_t)((buf[11] << 8) | buf[10]);

    /* Convert: 4g range → 0.122 mg/LSB, 2000 dps → 70 mdps/LSB */
    *ax = (int16_t)(*ax * 0.122f);
    *ay = (int16_t)(*ay * 0.122f);
    *az = (int16_t)(*az * 0.122f);
    *gx = (int16_t)(*gx * 70);
    *gy = (int16_t)(*gy * 70);
    *gz = (int16_t)(*gz * 70);
}

uint16_t body_imu_get_sway(void)
{
    /* Sway computed in main.c from rolling window */
    return 0;
}
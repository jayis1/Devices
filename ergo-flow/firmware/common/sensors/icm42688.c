/*
 * ErgoFlow — ICM-42688-P 6-Axis IMU Driver
 * Used in wearable tag node for high-precision motion tracking
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "icm42688.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(icm42688, CONFIG_ERGO_LOG_LEVEL);

/* ICM-42688-P Register Map */
#define ICM42688_REG_DEVICE_CONFIG    0x11
#define ICM42688_REG_DRIVE_CONFIG      0x13
#define ICM42688_REG_INT_CONFIG        0x14
#define ICM42688_REG_FIFO_CONFIG       0x16
#define ICM42688_REG_TEMP_DATA1        0x1D
#define ICM42688_REG_TEMP_DATA0        0x1E
#define ICM42688_REG_ACCEL_DATA_X1     0x1F
#define ICM42688_REG_ACCEL_DATA_X0     0x20
#define ICM42688_REG_ACCEL_DATA_Y1     0x21
#define ICM42688_REG_ACCEL_DATA_Y0     0x22
#define ICM42688_REG_ACCEL_DATA_Z1     0x23
#define ICM42688_REG_ACCEL_DATA_Z0     0x24
#define ICM42688_REG_GYRO_DATA_X1      0x25
#define ICM42688_REG_GYRO_DATA_X0      0x26
#define ICM42688_REG_GYRO_DATA_Y1      0x27
#define ICM42688_REG_GYRO_DATA_Y0      0x28
#define ICM42688_REG_GYRO_DATA_Z1      0x29
#define ICM42688_REG_GYRO_DATA_Z0      0x2A
#define ICM42688_REG_TMST_FSYNCH       0x2B
#define ICM42688_REG_TMST_FSYNCL       0x2C
#define ICM42688_REG_INT_STATUS         0x2D
#define ICM42688_REG_FIFO_COUNTH        0x2E
#define ICM42688_REG_FIFO_COUNTL        0x2F
#define ICM42688_REG_FIFO_DATA          0x30
#define ICM42688_REG_APEX_DATA0         0x31
#define ICM42688_REG_APEX_DATA1         0x32
#define ICM42688_REG_APEX_DATA2         0x33
#define ICM42688_REG_APEX_DATA3         0x34
#define ICM42688_REG_APEX_DATA4         0x35
#define ICM42688_REG_APEX_DATA5         0x36
#define ICM42688_REG_INT_STATUS2        0x37
#define ICM42688_REG_INT_STATUS3        0x38
#define ICM42688_REG_SIGNAL_PATH_RESET   0x4B
#define ICM42688_REG_INTF_CONFIG0        0x4C
#define ICM42688_REG_INTF_CONFIG1        0x4D
#define ICM42688_REG_FIFO_LOST_PKT0     0x4E
#define ICM42688_REG_WHO_AM_I           0x75

/* Bank 0 selection (via BLK_SEL) */
#define ICM42688_BANK_SEL_0    0x00
#define ICM42688_BANK_SEL_1    0x01
#define ICM42688_BANK_SEL_2    0x02
#define ICM42688_BANK_SEL_3    0x03
#define ICM42688_BANK_SEL_4    0x04

/* Bank 0 registers */
#define ICM42688_REG_BANK_SEL         0x76
#define ICM42688_REG_PWR_MGMT0        0x4E   /* Bank 0 */
#define ICM42688_REG_GYRO_CONFIG0      0x4F
#define ICM42688_REG_ACCEL_CONFIG0     0x50
#define ICM42688_REG_GYRO_CONFIG1      0x51
#define ICM42688_REG_GYRO_ACCEL_CONFIG0 0x52
#define ICM42688_REG_ACCEL_CONFIG1     0x53

/* WHO_AM_I value */
#define ICM42688_WHO_AM_I_VAL   0x47

static const struct spi_config spi_cfg = {
    .frequency = 8000000,
    .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
                 SPI_MODE_CPOL | SPI_MODE_CPHA,
    .cs = NULL,
};

static const struct device *spi_dev;

static int spi_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg & 0x7F, val };  /* Write: bit7=0 */
    struct spi_buf buf = { .buf = tx, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &buf, .count = 1 };
    return spi_write(spi_dev, &spi_cfg, &tx_set);
}

static int spi_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { reg | 0x80, 0x00 };  /* Read: bit7=1 */
    uint8_t rx[2];
    struct spi_buf tx_buf = { .buf = tx, .len = 2 };
    struct spi_buf rx_buf = { .buf = rx, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };
    int ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
    if (ret == 0) *val = rx[1];
    return ret;
}

static int spi_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t cmd = reg | 0x80;
    struct spi_buf tx_buf = { .buf = &cmd, .len = 1 };
    struct spi_buf rx_bufs[2] = {
        { .buf = &cmd, .len = 1 },
        { .buf = buf, .len = len }
    };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = rx_bufs, .count = 2 };
    return spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
}

int icm42688_init(void)
{
    spi_dev = DEVICE_DT_GET(DT_ALIAS(spi0));
    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }

    /* Software reset */
    spi_write_reg(ICM42688_REG_SIGNAL_PATH_RESET, 0x01);
    k_msleep(10);

    /* Verify device ID */
    uint8_t who_am_i;
    int ret = spi_read_reg(ICM42688_REG_WHO_AM_I, &who_am_i);
    if (ret != 0 || who_am_i != ICM42688_WHO_AM_I_VAL) {
        LOG_ERR("ICM-42688 not found (got 0x%02X, expected 0x%02X)", who_am_i, ICM42688_WHO_AM_I_VAL);
        return -ENODEV;
    }

    /* Select Bank 0 */
    spi_write_reg(ICM42688_REG_BANK_SEL, ICM42688_BANK_SEL_0);

    /* Configure accelerometer: 100Hz, ±4g (ODR=6, FS=0) */
    spi_write_reg(ICM42688_REG_ACCEL_CONFIG0, 0x06);

    /* Configure gyroscope: 100Hz, ±500dps (ODR=6, FS=1) */
    spi_write_reg(ICM42688_REG_GYRO_CONFIG0, 0x16);

    /* Enable both accel and gyro in low-noise mode */
    spi_write_reg(ICM42688_REG_PWR_MGMT0, 0x03);

    k_msleep(50);  /* Wait for sensors to stabilize */

    LOG_INF("ICM-42688 initialized: Accel 100Hz/4g, Gyro 100Hz/500dps");
    return 0;
}

int icm42688_read_accel(icm42688_accel_t *accel)
{
    uint8_t buf[6];
    int ret = spi_read_regs(ICM42688_REG_ACCEL_DATA_X1, buf, 6);
    if (ret != 0) return ret;

    int16_t raw_x = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t raw_y = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t raw_z = (int16_t)((buf[4] << 8) | buf[5]);

    /* ±4g range: sensitivity = 8.0 / 65536 * 32768 = 0.061 mg/LSB → 0.000061 g/LSB */
    accel->x = raw_x * 0.061e-3f;
    accel->y = raw_y * 0.061e-3f;
    accel->z = raw_z * 0.061e-3f;

    return 0;
}

int icm42688_read_gyro(icm42688_gyro_t *gyro)
{
    uint8_t buf[6];
    int ret = spi_read_regs(ICM42688_REG_GYRO_DATA_X1, buf, 6);
    if (ret != 0) return ret;

    int16_t raw_x = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t raw_y = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t raw_z = (int16_t)((buf[4] << 8) | buf[5]);

    /* ±500dps: sensitivity = 500.0 / 32768 = 15.26 mdps/LSB */
    gyro->x = raw_x * 15.26e-3f;
    gyro->y = raw_y * 15.26e-3f;
    gyro->z = raw_z * 15.26e-3f;

    return 0;
}

int icm42688_read_temp(float *temp_c)
{
    uint8_t buf[2];
    int ret = spi_read_regs(ICM42688_REG_TEMP_DATA1, buf, 2);
    if (ret != 0) return ret;

    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    *temp_c = (raw / 132.48f) + 25.0f;
    return 0;
}

int icm42688_set_accel_odr(icm42688_odr_t odr)
{
    uint8_t config0;
    int ret = spi_read_reg(ICM42688_REG_ACCEL_CONFIG0, &config0);
    if (ret != 0) return ret;
    config0 = (config0 & 0xF0) | ((uint8_t)odr & 0x0F);
    return spi_write_reg(ICM42688_REG_ACCEL_CONFIG0, config0);
}

int icm42688_set_gyro_odr(icm42688_odr_t odr)
{
    uint8_t config0;
    int ret = spi_read_reg(ICM42688_REG_GYRO_CONFIG0, &config0);
    if (ret != 0) return ret;
    config0 = (config0 & 0xF0) | ((uint8_t)odr & 0x0F);
    return spi_write_reg(ICM42688_REG_GYRO_CONFIG0, config0);
}
/**
 * JointSync Joint Tag — BMI270 IMU Driver
 *
 * SPI interface to BMI270 6-DoF IMU.
 *
 * License: MIT
 */

#include "bmi270_driver.h"
#include "nrf_drv_spi.h"
#include "nrf_log.h"

#define BMI270_SPI_INSTANCE  0
#define BMI270_CS_PIN        11

static nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(BMI270_SPI_INSTANCE);

/* BMI270 Register Addresses */
#define BMI270_REG_CHIP_ID    0x00
#define BMI270_REG_PWR_CONF   0x7C
#define BMI270_REG_PWR_CTRL   0x7D
#define BMI270_REG_INIT_CTRL  0x59
#define BMI270_REG_INIT_DATA  0x5B
#define BMI270_REG_INT1_IO_CTRL 0x53
#define BMI270_REG_INT_MAP    0x56
#define BMI270_REG_ACC_CONF   0x40
#define BMI270_REG_ACC_RANGE  0x41
#define BMI270_REG_GYR_CONF   0x42
#define BMI270_REG_GYR_RANGE  0x43
#define BMI270_REG_DATA_8     0x04  /* Accel + Gyro data (8 registers) */

#define BMI270_CHIP_ID        0x24

/* ── SPI Transfer ─────────────────────────────────────────────────── */

static nrf_err_t bmi270_spi_write(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = {reg & 0x7F, value};  /* Bit 7 = 0 for write */
    return nrf_drv_spi_transfer(&spi, tx, 2, NULL, 0);
}

static uint8_t bmi270_spi_read(uint8_t reg)
{
    uint8_t tx[2] = {reg | 0x80, 0};  /* Bit 7 = 1 for read */
    uint8_t rx[2] = {0};
    nrf_drv_spi_transfer(&spi, tx, 2, rx, 2);
    return rx[1];
}

static nrf_err_t bmi270_spi_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t tx[1] = {reg | 0x80};
    return nrf_drv_spi_transfer(&spi, tx, 1, buf, len + 1);
}

/* ── Public API ───────────────────────────────────────────────────── */

nrf_err_t bmi270_init(void)
{
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin = BMI270_CS_PIN;
    spi_config.frequency = NRF_DRV_SPI_FREQ_8M;
    nrf_drv_spi_init(&spi, &spi_config, NULL, NULL);

    /* Verify chip ID */
    uint8_t chip_id = bmi270_spi_read(BMI270_REG_CHIP_ID);
    if (chip_id != BMI270_CHIP_ID) {
        NRF_LOG_ERROR("BMI270 chip ID mismatch: 0x%02X (expected 0x%02X)", chip_id, BMI270_CHIP_ID);
        return NRF_ERROR_INVALID_DATA;
    }

    /* Soft reset */
    bmi270_spi_write(BMI270_REG_PWR_CONF, 0x00);
    nrf_delay_ms(1);
    bmi270_spi_write(BMI270_REG_PWR_CTRL, 0x00);
    nrf_delay_ms(1);

    /* Configure accelerometer: 100 Hz, normal mode */
    bmi270_spi_write(BMI270_REG_ACC_CONF, 0x0A);  /* 100 Hz */
    bmi270_spi_write(BMI270_REG_ACC_RANGE, 0x01);  /* ±4g */

    /* Configure gyroscope: 100 Hz, normal mode */
    bmi270_spi_write(BMI270_REG_GYR_CONF, 0x0A);  /* 100 Hz */
    bmi270_spi_write(BMI270_REG_GYR_RANGE, 0x01);  /* ±500 dps */

    /* Enable sensors */
    bmi270_spi_write(BMI270_REG_PWR_CTRL, 0x0E);  /* Accel + Gyro on */
    nrf_delay_ms(2);

    /* Configure INT1 pin */
    bmi270_spi_write(BMI270_REG_INT1_IO_CTRL, 0x0A);

    NRF_LOG_INFO("BMI270 initialized (100 Hz, ±4g, ±500 dps)");
    return NRF_SUCCESS;
}

nrf_err_t bmi270_read(bmi270_data_t *data)
{
    uint8_t buf[12];
    nrf_err_t err = bmi270_spi_read_burst(BMI270_REG_DATA_8, buf, 12);
    if (err != NRF_SUCCESS) return err;

    /* Parse accelerometer (little-endian, 16-bit) */
    /* Convert to milli-g (±4g range: 1 LSB = 0.122 mg) */
    int16_t ax_raw = (int16_t)((buf[2] << 8) | buf[1]);
    int16_t ay_raw = (int16_t)((buf[4] << 8) | buf[3]);
    int16_t az_raw = (int16_t)((buf[6] << 8) | buf[5]);
    data->accel_x = (int16_t)(ax_raw * 122 / 1000);  /* milli-g */
    data->accel_y = (int16_t)(ay_raw * 122 / 1000);
    data->accel_z = (int16_t)(az_raw * 122 / 1000);

    /* Parse gyroscope (little-endian, 16-bit) */
    /* Convert to milli-degrees/sec (±500 dps: 1 LSB = 16.376 mdps) */
    int16_t gx_raw = (int16_t)((buf[8] << 8) | buf[7]);
    int16_t gy_raw = (int16_t)((buf[10] << 8) | buf[9]);
    int16_t gz_raw = (int16_t)((buf[12] << 8) | buf[11]);
    data->gyro_x = (int16_t)(gx_raw * 16376 / 1000000);  /* milli-dps */
    data->gyro_y = (int16_t)(gy_raw * 16376 / 1000000);
    data->gyro_z = (int16_t)(gz_raw * 16376 / 1000000);

    return NRF_SUCCESS;
}

void bmi270_config_int1(void (*handler)(void))
{
    /* Configure INT1 on data-ready */
    bmi270_spi_write(BMI270_REG_INT_MAP, 0x80);  /* Data-ready on INT1 */
    /* GPIO setup would go here with nrf_drv_gpiote_in_init */
}
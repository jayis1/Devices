/*
 * ErgoFlow — BGT60TR13C mmWave Radar Driver
 * Used in hub node for privacy-first pose/proximity detection
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "bgt60tr13c.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bgt60tr13c, CONFIG_ERGO_LOG_LEVEL);

/* BGT60TR13C SPI register addresses */
#define BGT60_REG_STATUS        0x4000
#define BGT60_REG_CONFIG        0x4100
#define BGT60_REG_CHIRP_CONFIG  0x4200
#define BGT60_REG_FRAME_CONFIG  0x4300
#define BGT60_REG_DCA_CONFIG    0x4400
#define BGT60_REG_PMU_CONFIG    0x4500
#define BGT60_REG_FIFO_CONFIG   0x4600

/* Radar configuration defaults */
#define BGT60_DEFAULT_CHIRPS_PER_FRAME  16
#define BGT60_DEFAULT_SAMPLES_PER_CHIRP  64
#define BGT60_DEFAULT_FRAME_RATE_HZ     20
#define BGT60_DEFAULT_START_FREQ_GHZ    60.0f
#define BGT60_DEFAULT_BANDWIDTH_GHZ      4.0f

static const struct spi_config spi_cfg = {
    .frequency = 8000000,
    .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
                 SPI_MODE_CPOL | SPI_MODE_CPHA,
};

static const struct device *spi_dev;
static bgt60_config_t active_config;

static int spi_write_reg16(uint16_t reg, const uint8_t *data, uint16_t len)
{
    uint8_t header[3] = { 0x00, (reg >> 8) & 0xFF, reg & 0xFF };
    struct spi_buf tx_bufs[2] = {
        { .buf = header, .len = 3 },
        { .buf = (uint8_t *)data, .len = len },
    };
    struct spi_buf_set tx_set = { .buffers = tx_bufs, .count = 2 };
    return spi_write(spi_dev, &spi_cfg, &tx_set);
}

static int spi_read_reg16(uint16_t reg, uint8_t *data, uint16_t len)
{
    uint8_t header[3] = { 0x80, (reg >> 8) & 0xFF, reg & 0xFF };
    struct spi_buf tx_buf = { .buf = header, .len = 3 };
    struct spi_buf rx_bufs[2] = {
        { .buf = header, .len = 3 },
        { .buf = data, .len = len },
    };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = rx_bufs, .count = 2 };
    return spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
}

int bgt60tr13c_init(void)
{
    spi_dev = DEVICE_DT_GET(DT_ALIAS(spi1));
    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI1 device not ready for BGT60TR13C");
        return -ENODEV;
    }

    /* Reset the radar */
    uint8_t reset_cmd = 0x01;
    spi_write_reg16(BGT60_REG_PMU_CONFIG, &reset_cmd, 1);
    k_msleep(50);

    /* Set default configuration */
    active_config.chirps_per_frame = BGT60_DEFAULT_CHIRPS_PER_FRAME;
    active_config.samples_per_chirp = BGT60_DEFAULT_SAMPLES_PER_CHIRP;
    active_config.frame_rate_hz = BGT60_DEFAULT_FRAME_RATE_HZ;
    active_config.start_freq_ghz = BGT60_DEFAULT_START_FREQ_GHZ;
    active_config.bandwidth_ghz = BGT60_DEFAULT_BANDWIDTH_GHZ;

    /* Configure radar parameters */
    uint8_t chirp_config[8] = {
        (active_config.chirps_per_frame >> 0) & 0xFF,
        (active_config.chirps_per_frame >> 8) & 0xFF,
        (active_config.samples_per_chirp >> 0) & 0xFF,
        (active_config.samples_per_chirp >> 8) & 0xFF,
        0x00, 0x00, 0x00, 0x00
    };
    spi_write_reg16(BGT60_REG_CHIRP_CONFIG, chirp_config, sizeof(chirp_config));

    /* Frame rate configuration */
    uint16_t frame_interval_ms = 1000 / active_config.frame_rate_hz;
    uint8_t frame_config[4] = {
        (frame_interval_ms >> 0) & 0xFF,
        (frame_interval_ms >> 8) & 0xFF,
        0x00, 0x00
    };
    spi_write_reg16(BGT60_REG_FRAME_CONFIG, frame_config, sizeof(frame_config));

    LOG_INF("BGT60TR13C initialized: %d chirps, %d samples, %dHz",
            active_config.chirps_per_frame, active_config.samples_per_chirp,
            active_config.frame_rate_hz);
    return 0;
}

int bgt60tr13c_start(void)
{
    uint8_t start_cmd = 0x01;
    return spi_write_reg16(BGT60_REG_CONFIG, &start_cmd, 1);
}

int bgt60tr13c_stop(void)
{
    uint8_t stop_cmd = 0x00;
    return spi_write_reg16(BGT60_REG_CONFIG, &stop_cmd, 1);
}

int bgt60tr13c_read_frame(bgt60_radar_frame_t *frame)
{
    if (!frame) return -EINVAL;

    /* Read raw radar data from FIFO
     * In production: read I/Q data from FIFO, compute range-Doppler map
     * For now: read status and raw data */
    uint8_t status[4];
    int ret = spi_read_reg16(BGT60_REG_STATUS, status, 4);
    if (ret != 0) return ret;

    /* Check if new frame is available */
    if (!(status[0] & 0x01)) {
        return -EAGAIN;  /* No new frame */
    }

    /* Read raw radar samples */
    int total_samples = active_config.chirps_per_frame * active_config.samples_per_chirp;
    if (total_samples > BGT60_MAX_SAMPLES) {
        total_samples = BGT60_MAX_SAMPLES;
    }

    /* Read from FIFO in chunks */
    int offset = 0;
    while (offset < total_samples * 4) {
        int chunk_size = total_samples * 4 - offset;
        if (chunk_size > 252) chunk_size = 252;
        ret = spi_read_reg16(BGT60_REG_FIFO_CONFIG,
                              (uint8_t *)&frame->raw_data[offset / 4],
                              chunk_size);
        if (ret != 0) return ret;
        offset += chunk_size;
    }

    frame->num_chirps = active_config.chirps_per_frame;
    frame->num_samples = active_config.samples_per_chirp;
    frame->timestamp = k_uptime_get_32();

    return 0;
}

int bgt60tr13c_get_config(bgt60_config_t *config)
{
    if (!config) return -EINVAL;
    *config = active_config;
    return 0;
}

int bgt60tr13c_set_config(const bgt60_config_t *config)
{
    if (!config) return -EINVAL;
    active_config = *config;

    /* Apply configuration to hardware */
    bgt60tr13c_stop();
    k_msleep(10);

    uint8_t chirp_config[8] = {
        (active_config.chirps_per_frame >> 0) & 0xFF,
        (active_config.chirps_per_frame >> 8) & 0xFF,
        (active_config.samples_per_chirp >> 0) & 0xFF,
        (active_config.samples_per_chirp >> 8) & 0xFF,
        0x00, 0x00, 0x00, 0x00
    };
    spi_write_reg16(BGT60_REG_CHIRP_CONFIG, chirp_config, sizeof(chirp_config));

    return bgt60tr13c_start();
}
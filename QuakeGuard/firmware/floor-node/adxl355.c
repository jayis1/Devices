/*
 * adxl355.c — ADXL355 3-axis MEMS accelerometer driver
 *
 * SPI implementation for ESP32-S3. Research-grade sensor:
 *   ±2g, 20-bit, 1 μg/√Hz noise, 4000 Hz max ODR
 *
 * License: MIT
 */
#include "adxl355.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ADXL355";

static spi_device_handle_t adxl_spi;
static gpio_num_t adxl_cs_pin;

/* ── SPI helpers ────────────────────────────────────────────── */

static esp_err_t adxl_write_reg(uint8_t addr, uint8_t val)
{
    spi_transaction_t t = {0};
    uint8_t tx[2] = {(addr << 1) | ADXL355_WRITE_CMD, val};
    t.length = 16;
    t.tx_buffer = tx;
    return spi_device_polling_transmit(adxl_spi, &t);
}

static uint8_t adxl_read_reg(uint8_t addr)
{
    spi_transaction_t t = {0};
    uint8_t tx[2] = {(addr << 1) | ADXL355_READ_CMD, 0x00};
    uint8_t rx[2] = {0};
    t.length = 16;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    spi_device_polling_transmit(adxl_spi, &t);
    return rx[1];
}

static void adxl_read_burst(uint8_t addr, uint8_t *buf, int len)
{
    spi_transaction_t t = {0};
    uint8_t tx[100];
    tx[0] = (addr << 1) | ADXL355_READ_CMD;
    memset(&tx[1], 0x00, len - 1);
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = buf;
    spi_device_polling_transmit(adxl_spi, &t);
}

/* ── Initialization ────────────────────────────────────────── */

int adxl355_init(const adxl355_config_t *cfg)
{
    esp_err_t ret;
    adxl_cs_pin = cfg->cs_pin;

    /* Configure SPI device on existing bus */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,  /* 8 MHz (ADXL355 max) */
        .mode = 0,                           /* CPOL=0, CPHA=0 */
        .spics_io_num = -1,                  /* manual CS */
        .queue_size = 7,
    };
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &adxl_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return -1;
    }

    gpio_set_direction(adxl_cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(adxl_cs_pin, 1);

    vTaskDelay(pdMS_TO_TICKS(10));

    /* Verify device ID */
    uint8_t devid = adxl_read_reg(ADXL355_DEVID_AD);
    if (devid != 0xAD) {
        ESP_LOGE(TAG, "ADXL355 not found (DEVID=0x%02X, expected 0xAD)", devid);
        return -2;
    }

    /* Software reset */
    adxl_write_reg(ADXL355_RESET, 0x52);  /* 'R' */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Configure range (±2g = 0x01) */
    adxl_write_reg(ADXL355_RANGE, cfg->range);

    /* Configure ODR and low-pass filter
     * Bits [7:4] = ODR, [3:0] = HPF corner
     * 1000 Hz ODR, HPF off (0x00)
     */
    adxl_write_reg(ADXL355_ODR_LPF, (cfg->odr << 4) | 0x00);

    /* Configure activity detection
     * ACT_EN: enable X, Y, Z activity
     * ACT_THRESH: threshold (in raw counts, 20-bit)
     * Scale: ±2g range = 256000 LSB/g → threshold_mg = threshold_raw / 256
     */
    uint32_t thresh_raw = (cfg->threshold_mg * 256) / 1000;  /* mg → LSB */
    adxl_write_reg(ADXL355_ACT_THRESH, (thresh_raw >> 12) & 0xFF);
    adxl_write_reg(ADXL355_ACT_THRESH + 1, (thresh_raw >> 4) & 0xFF);
    adxl_write_reg(ADXL355_ACT_THRESH + 2, (thresh_raw << 4) & 0xFF);
    adxl_write_reg(ADXL355_ACT_EN, 0x07);  /* enable X,Y,Z activity */

    /* Activity count: number of consecutive samples above threshold */
    adxl_write_reg(ADXL355_ACT_CNT, 3);  /* 3 samples = 3 ms at 1000 Hz */

    /* Interrupt map: ACT on INT1, DATA_RDY on INT2 (or vice versa) */
    adxl_write_reg(ADXL355_INT_MAP, 0x01);  /* ACT → INT1 */

    /* FIFO: set watermark for 96 samples (96 × 3 axes × 3 bytes = 864 bytes) */
    adxl_write_reg(ADXL355_FIFO_SAMPLES, 0x60);  /* 96 samples */

    /* Enter measurement mode */
    adxl_write_reg(ADXL355_POWER_CTL, ADXL355_MODE_MEASURE);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "ADXL355 initialized: range=%d, odr=%d, threshold=%ld mg",
             cfg->range, cfg->odr, (long)cfg->threshold_mg);
    return 0;
}

/* ── Read Sample ────────────────────────────────────────────── */

int adxl355_read_sample(adxl355_sample_t *sample)
{
    uint8_t buf[9];
    adxl_read_burst(ADXL355_XDATA3, buf, 10);  /* 1 cmd + 9 data */

    /* 20-bit signed, left-justified in 3 bytes per axis */
    /* XDATA3[7:0] = bits [19:12], XDATA2[7:0] = bits [11:4],
     * XDATA1[7:4] = bits [3:0], XDATA1[3:0] = don't care */
    int32_t x_raw = ((int32_t)(buf[1] << 12 | buf[2] << 4 | (buf[3] >> 4)))
                     << 12;  /* sign-extend 20-bit */
    int32_t y_raw = ((int32_t)(buf[4] << 12 | buf[5] << 4 | (buf[6] >> 4)))
                     << 12;
    int32_t z_raw = ((int32_t)(buf[7] << 12 | buf[8] << 4 | (buf[9] >> 4)))
                     << 12;

    /* Convert to milli-g:
     * ±2g range: 256000 LSB/g → 256 LSB/mg
     * Raw is 20-bit signed (−524288 to +524287)
     * mg = raw / 256
     */
    sample->x = x_raw / 256;
    sample->y = y_raw / 256;
    sample->z = z_raw / 256;

    return 0;
}

/* ── Read FIFO ──────────────────────────────────────────────── */

int adxl355_read_fifo(adxl355_sample_t *samples, int max_samples)
{
    uint8_t entries = adxl_read_reg(ADXL355_FIFO_ENTRIES);
    int n = entries > max_samples ? max_samples : entries;

    for (int i = 0; i < n; i++) {
        adxl355_read_sample(&samples[i]);
    }
    return n;
}

/* ── Set Activity Threshold ────────────────────────────────── */

int adxl355_set_activity_threshold(int32_t threshold_mg)
{
    uint32_t thresh_raw = (threshold_mg * 256) / 1000;
    adxl_write_reg(ADXL355_ACT_THRESH, (thresh_raw >> 12) & 0xFF);
    adxl_write_reg(ADXL355_ACT_THRESH + 1, (thresh_raw >> 4) & 0xFF);
    adxl_write_reg(ADXL355_ACT_THRESH + 2, (thresh_raw << 4) & 0xFF);
    ESP_LOGI(TAG, "Activity threshold set to %ld mg", (long)threshold_mg);
    return 0;
}

/* ── Set Mode ──────────────────────────────────────────────── */

int adxl355_set_mode(uint8_t mode)
{
    adxl_write_reg(ADXL355_POWER_CTL, mode);
    return 0;
}

/* ── Software Reset ────────────────────────────────────────── */

int adxl355_reset(void)
{
    adxl_write_reg(ADXL355_RESET, 0x52);
    vTaskDelay(pdMS_TO_TICKS(100));
    return 0;
}

/* ── Self-Test ─────────────────────────────────────────────── */

int adxl355_self_test(void)
{
    /* Read DEVID to verify communication */
    uint8_t devid = adxl_read_reg(ADXL355_DEVID_AD);
    if (devid != 0xAD) {
        ESP_LOGE(TAG, "Self-test failed: DEVID=0x%02X", devid);
        return -1;
    }
    /* Check PARTID */
    uint8_t partid = adxl_read_reg(ADXL355_PARTID);
    if (partid != 0x1D) {  /* ADXL355 PARTID */
        ESP_LOGE(TAG, "Self-test failed: PARTID=0x%02X", partid);
        return -1;
    }
    ESP_LOGI(TAG, "Self-test passed (DEVID=0xAD, PARTID=0x1D)");
    return 0;
}
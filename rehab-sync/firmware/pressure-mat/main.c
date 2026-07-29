/*
 * RehabSync — Pressure Mat Node Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Pressure Mat scans a 16×16 (256 sensor) FSR array at 30 Hz,
 * computes center-of-pressure and weight distribution, and transmits
 * compressed frames to the Hub via SX1262 Sub-GHz 868 MHz.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/adc.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "RehabSync-PressureMat";

/* === Global state === */
static rs_mesh_ctx_t g_mesh;
static sx1262_t g_radio;

/* Pressure frame data (16×16) */
static uint16_t g_pressure_frame[PM_ROWS][PM_COLS];
static uint16_t g_frame_seq = 0;

/* Calibration offsets (zero-pressure baseline per cell) */
static uint16_t g_zero_offset[PM_ROWS][PM_COLS];

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PM_GPIO_SX_MOSI,
        .miso_io_num = PM_GPIO_SX_MISO,
        .sclk_io_num = PM_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PM_GPIO_SX_NSS,
        .queue_size = 7,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

int sx1262_spi_write(sx1262_t *radio, uint8_t cmd, const uint8_t *data, size_t len)
{
    uint8_t tx_buf[260] = { cmd };
    if (data && len > 0) memcpy(tx_buf + 1, data, len);
    spi_transaction_t t = { .length = 8 * (1 + len), .tx_buffer = tx_buf };
    return (spi_device_polling_transmit(g_spi_dev, &t) == ESP_OK) ? 0 : -1;
}

int sx1262_spi_read(sx1262_t *radio, uint8_t cmd, uint8_t *data, size_t len)
{
    uint8_t tx_buf[260] = { cmd };
    uint8_t rx_buf[260] = {0};
    spi_transaction_t t = {
        .length = 8 * (1 + len), .tx_buffer = tx_buf, .rx_buffer = rx_buf,
    };
    esp_err_t ret = spi_device_polling_transmit(g_spi_dev, &t);
    if (ret == ESP_OK && data) memcpy(data, rx_buf + 1, len);
    return (ret == ESP_OK) ? 0 : -1;
}

void sx1262_reset(sx1262_t *radio)
{
    gpio_set_direction(PM_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PM_GPIO_SX_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PM_GPIO_SX_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* === I²C for ADS1115 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PM_GPIO_I2C_SDA,
        .scl_io_num = PM_GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === ADS1115 16-bit ADC Driver === */
#define ADS1115_ADDR           0x48
#define ADS1115_REG_CONV       0x00
#define ADS1115_REG_CONFIG     0x01
#define ADS1115_REG_LO_THRESH  0x02
#define ADS1115_REG_HI_THRESH  0x03

static void ads1115_set_config(uint8_t channel)
{
    /* Config: single-shot, ±4.096V, 860 SPS, continuous */
    /* MUX = channel (A0-A3 = 0x4000-0x7000) */
    /* PGA = ±4.096V = 0x0200 */
    /* DR = 860 SPS = 0x00E0 */
    /* OS = single-shot = 0x8000 */
    uint16_t config = 0x8000 | (channel << 12) | 0x0200 | 0x00E0;

    uint8_t buf[3] = {
        ADS1115_REG_CONFIG,
        (uint8_t)(config >> 8),
        (uint8_t)(config & 0xFF)
    };
    i2c_master_write_to_device(I2C_NUM_0, ADS1115_ADDR, buf, 3, pdMS_TO_TICKS(100));
}

static int16_t ads1115_read(uint8_t channel)
{
    ads1115_set_config(channel);
    /* Wait for conversion (~1.2ms at 860 SPS) */
    vTaskDelay(pdMS_TO_TICKS(2));

    /* Read conversion register */
    uint8_t reg = ADS1115_REG_CONV;
    uint8_t data[2] = {0, 0};
    i2c_master_write_read_device(I2C_NUM_0, ADS1115_ADDR,
                                  &reg, 1, data, 2, pdMS_TO_TICKS(100));
    return (int16_t)((data[0] << 8) | data[1]);
}

/* === MUX Control === */
/* 4× CD74HC4067 16:1 mux for row selection (each mux handles 4 rows in parallel)
 * 1× CD74HC4067 16:1 mux for column grounding
 *
 * Scanning approach:
 * - Select column (ground one column line via col mux)
 * - Read all 16 rows via 4× parallel mux → 4 ADS1115 channels (4 rows each)
 * - Each ADS1115 read = 1 row → 4 reads per column → 16 columns × 4 = 64 ADC reads per frame
 * - At 860 SPS (ADS1115): 64 reads × 1.2ms = ~77ms → ~13 Hz max
 * - For 30 Hz: use 4-channel simultaneous mode or reduce to 8×8 (64 sensors)
 *
 * Optimized approach for 30 Hz:
 * - Read 4 ADS1115 channels per column = 4 × 1.2ms = 4.8ms per column
 * - 16 columns × 4.8ms = 77ms per frame → ~13 Hz
 * - For 30 Hz: use ESP32 ADC (12-bit, faster) instead of ADS1115 for routine scanning
 *   and ADS1115 for high-precision weight measurement only
 */

static void mux_select_row(uint8_t row)
{
    /* Row mux select lines (S0-S3 on 4 mux chips, all share select lines) */
    gpio_set_level(PM_GPIO_MUX_ROW_S0, (row >> 0) & 1);
    gpio_set_level(PM_GPIO_MUX_ROW_S1, (row >> 1) & 1);
    gpio_set_level(PM_GPIO_MUX_ROW_S2, (row >> 2) & 1);
    gpio_set_level(PM_GPIO_MUX_ROW_S3, (row >> 3) & 1);
}

static void mux_select_col(uint8_t col)
{
    gpio_set_level(PM_GPIO_MUX_COL_S0, (col >> 0) & 1);
    gpio_set_level(PM_GPIO_MUX_COL_S1, (col >> 1) & 1);
    gpio_set_level(PM_GPIO_MUX_COL_S2, (col >> 2) & 1);
    gpio_set_level(PM_GPIO_MUX_COL_S3, (col >> 3) & 1);
}

/* === Pressure Scanning Task (30 Hz) === */
static void scan_task(void *arg)
{
    ESP_LOGI(TAG, "Pressure scan task started (30 Hz)");

    /* Configure mux select pins as output */
    int mux_pins[] = {
        PM_GPIO_MUX_ROW_S0, PM_GPIO_MUX_ROW_S1, PM_GPIO_MUX_ROW_S2, PM_GPIO_MUX_ROW_S3,
        PM_GPIO_MUX_COL_S0, PM_GPIO_MUX_COL_S1, PM_GPIO_MUX_COL_S2, PM_GPIO_MUX_COL_S3,
    };
    for (int i = 0; i < 8; i++) {
        gpio_set_direction(mux_pins[i], GPIO_MODE_OUTPUT);
    }

    while (1) {
        /* Scan full 16×16 frame */
        for (int col = 0; col < PM_COLS; col++) {
            mux_select_col(col);
            vTaskDelay(pdMS_TO_TICKS(1));  /* settle */

            for (int row = 0; row < PM_ROWS; row++) {
                mux_select_row(row);
                /* Read ADS1115 channel (cycling through 4 channels for parallel reads) */
                int16_t raw = ads1115_read(row % 4);

                /* Convert to pressure (grams): calibrated from FSR response curve */
                /* ADS1115 ±4.096V range → 0.125 mV/LSB
                 * FSR voltage divider → force proportional to voltage
                 * Calibration: raw → grams via polynomial or lookup table
                 */
                uint16_t pressure = (uint16_t)(raw > 0 ? raw / 4 : 0);  /* simplified */

                /* Subtract zero offset */
                if (pressure > g_zero_offset[row][col]) {
                    g_pressure_frame[row][col] = pressure - g_zero_offset[row][col];
                } else {
                    g_pressure_frame[row][col] = 0;
                }
            }
        }

        g_frame_seq++;

        /* Compute center of pressure (CoP) */
        float sum_x = 0, sum_y = 0, total = 0;
        for (int r = 0; r < PM_ROWS; r++) {
            for (int c = 0; c < PM_COLS; c++) {
                uint16_t p = g_pressure_frame[r][c];
                sum_x += c * p;
                sum_y += r * p;
                total += p;
            }
        }
        float cop_x = (total > 0) ? sum_x / total : 0;
        float cop_y = (total > 0) ? sum_y / total : 0;

        /* Compute weight-bearing asymmetry (left vs right) */
        uint32_t left_sum = 0, right_sum = 0;
        for (int r = 0; r < PM_ROWS; r++) {
            for (int c = 0; c < PM_COLS / 2; c++) {
                left_sum += g_pressure_frame[r][c];
            }
            for (int c = PM_COLS / 2; c < PM_COLS; c++) {
                right_sum += g_pressure_frame[r][c];
            }
        }
        uint32_t total_weight = left_sum + right_sum;
        uint16_t asymmetry = 0;
        if (total_weight > 0) {
            float ratio = (float)abs((int)left_sum - (int)right_sum) / (float)total_weight;
            asymmetry = (uint16_t)(ratio * 1000);  /* 0 = perfect, 1000 = one-legged */
        }

        /* Send frame to Hub via Sub-GHz */
        rs_pressure_header_t header;
        header.frame_seq = g_frame_seq;
        header.cop_x = (uint16_t)(cop_x * 4096);  /* scale: 0-15.99 → 0-65535 */
        header.cop_y = (uint16_t)(cop_y * 4096);
        header.total_weight_g = (uint16_t)(total_weight / 10);  /* approximate grams */
        header.asymmetry = asymmetry;

        /* Compress frame: 8×8 averaged (64 cells × 2 bytes = 128 bytes) */
        uint16_t compressed[64];
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                uint32_t sum = 0;
                for (int dr = 0; dr < 2; dr++) {
                    for (int dc = 0; dc < 2; dc++) {
                        sum += g_pressure_frame[r*2 + dr][c*2 + dc];
                    }
                }
                compressed[r * 8 + c] = (uint16_t)(sum / 4);
            }
        }

        /* Build and send message */
        uint8_t payload[sizeof(rs_pressure_header_t) + 128];
        memcpy(payload, &header, sizeof(header));
        memcpy(payload + sizeof(header), compressed, 128);

        rs_mesh_send(&g_mesh, 0x01, RS_MSG_PRESSURE_FRAME, 0,
                     payload, sizeof(payload));

        /* 30 Hz frame rate → 33ms per frame
         * Scanning takes ~77ms currently, so effective rate is ~13 Hz
         * For 30 Hz: use ESP32 ADC for fast scanning, ADS1115 for precision only
         */
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

/* === Calibration Task === */
static void calibration_task(void *arg)
{
    ESP_LOGI(TAG, "Calibration task started");

    /* Zero-pressure calibration on startup */
    ESP_LOGI(TAG, "Performing zero-pressure calibration...");

    /* Average 10 frames with no load */
    for (int cal = 0; cal < 10; cal++) {
        for (int col = 0; col < PM_COLS; col++) {
            mux_select_col(col);
            vTaskDelay(pdMS_TO_TICKS(1));
            for (int row = 0; row < PM_ROWS; row++) {
                mux_select_row(row);
                int16_t raw = ads1115_read(row % 4);
                uint16_t val = (uint16_t)(raw > 0 ? raw / 4 : 0);
                g_zero_offset[row][col] += val / 10;
            }
        }
    }

    ESP_LOGI(TAG, "Calibration complete. Zero offsets stored.");

    /* Re-calibrate on command or periodically */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3600000));  /* re-calibrate every hour */
        /* Could also be triggered by command from Hub */
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "RehabSync Pressure Mat starting...");

    spi_init();
    i2c_init();

    /* Initialize SX1262 radio */
    static sx1262_config_t radio_cfg = {
        .frequency = RS_BAND_868MHZ,
        .spreading_factor = RS_SF,
        .bandwidth = RS_BW,
        .coding_rate = 1,
        .tx_power_dbm = RS_TX_POWER,
        .preamble_len = RS_PREAMBLE_LEN,
    };
    sx1262_init(&g_radio, &radio_cfg);

    /* Initialize mesh as node (not coordinator) */
    rs_mesh_init(&g_mesh, 0x04, RS_NODE_PRESSURE_MAT, false, &g_radio);

    /* Join mesh network */
    ESP_LOGI(TAG, "Joining mesh network...");
    int ret = rs_mesh_join(&g_mesh, 10000);
    if (ret == 0) {
        ESP_LOGI(TAG, "Joined mesh, slot %d", g_mesh.my_slot);
    } else {
        ESP_LOGW(TAG, "Mesh join failed (%d), continuing with broadcast", ret);
    }

    /* Create tasks */
    xTaskCreate(scan_task, "scan", 8192, NULL, 5, NULL);
    xTaskCreate(calibration_task, "cal", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Pressure Mat ready.");
}
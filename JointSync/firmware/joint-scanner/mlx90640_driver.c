/**
 * JointSync Joint Scanner — MLX90640 Thermal Array Driver
 *
 * I²C interface to MLX90640 32×24 IR thermal array.
 *
 * License: MIT
 */

#include "mlx90640_driver.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "mlx90640";

#define MLX90640_ADDR     0x33
#define MLX90640_I2C_PORT I2C_NUM_0

/* MLX90640 register addresses */
#define MLX90640_REG_STATUS1  0x8000
#define MLX90640_REG_STATUS2  0x8001
#define MLX90640_REG_CTRL1    0x800D
#define MLX90640_REG_CTRL2    0x800E
#define MLX90640_REG_DATA     0x0400  /* Start of pixel data (768 × 2 bytes) */

static uint16_t g_ee_data[832];  /* EEPROM calibration data */

/* ── I²C Helpers ─────────────────────────────────────────────────── */

static esp_err_t mlx_read16(uint16_t reg, uint16_t *val)
{
    uint8_t reg_buf[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    uint8_t data[2];
    esp_err_t err = i2c_master_write_read_device(MLX90640_I2C_PORT,
                                                   MLX90640_ADDR,
                                                   reg_buf, 2, data, 2,
                                                   pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        *val = ((uint16_t)data[1] << 8) | data[0];  /* Little-endian */
    }
    return err;
}

static esp_err_t mlx_read_burst(uint16_t start_reg, uint16_t *buf, uint16_t count)
{
    uint8_t reg_buf[2] = {(uint8_t)(start_reg >> 8), (uint8_t)(start_reg & 0xFF)};
    uint8_t *data = (uint8_t *)malloc(count * 2);
    if (data == NULL) return ESP_ERR_NO_MEM;

    esp_err_t err = i2c_master_write_read_device(MLX90640_I2C_PORT,
                                                   MLX90640_ADDR,
                                                   reg_buf, 2, data, count * 2,
                                                   pdMS_TO_TICKS(500));
    if (err == ESP_OK) {
        for (int i = 0; i < count; i++) {
            buf[i] = ((uint16_t)data[2 * i + 1] << 8) | data[2 * i];
        }
    }
    free(data);
    return err;
}

static esp_err_t mlx_write16(uint16_t reg, uint16_t val)
{
    uint8_t buf[4] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
                      (uint8_t)(val & 0xFF), (uint8_t)(val >> 8)};
    return i2c_master_write_to_device(MLX90640_I2C_PORT, MLX90640_ADDR,
                                       buf, 4, pdMS_TO_TICKS(100));
}

/* ── Public API ───────────────────────────────────────────────────── */

esp_err_t mlx90640_init(void)
{
    /* I²C is initialized elsewhere (shared with other sensors) */
    /* But initialize if not already done */
    static bool i2c_initialized = false;
    if (!i2c_initialized) {
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = 1,  /* GPIO1 */
            .scl_io_num = 2,  /* GPIO2 */
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 1000000,  /* 1 MHz — MLX90640 supports up to 1 MHz */
        };
        i2c_param_config(MLX90640_I2C_PORT, &conf);
        i2c_driver_install(MLX90640_I2C_PORT, conf.mode, 0, 0, 0);
        i2c_initialized = true;
    }

    /* Read EEPROM calibration data */
    esp_err_t err = mlx_read_burst(0x2400, g_ee_data, 832);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MLX90640 EEPROM");
        return err;
    }

    /* Configure: 4 Hz refresh, 16-bit resolution */
    mlx_write16(MLX90640_REG_CTRL1, 0x003F);  /* Subpage repeat */
    mlx_write16(MLX90640_REG_CTRL2, 0x0040);  /* 4 Hz refresh */

    ESP_LOGI(TAG, "MLX90640 initialized (32×24, 4 Hz, 1 MHz I²C)");
    return ESP_OK;
}

esp_err_t mlx90640_read_frame(int16_t *pixels)
{
    /* Read both subpages (MLX90640 has 2 subpages of 384 pixels each) */
    uint16_t frame_data[832];

    /* Wait for data ready */
    uint16_t status;
    int timeout = 1000;
    do {
        mlx_read16(MLX90640_REG_STATUS1, &status);
        vTaskDelay(pdMS_TO_TICKS(1));
    } while ((!(status & 0x0008)) && timeout-- > 0);

    if (timeout <= 0) {
        ESP_LOGW(TAG, "MLX90640 data ready timeout");
        return ESP_ERR_TIMEOUT;
    }

    /* Read full frame (768 pixels × 2 bytes + overhead) */
    esp_err_t err = mlx_read_burst(MLX90640_REG_DATA, frame_data, 832);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MLX90640 frame read failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Convert raw data to temperature (simplified — full MLX90640 compensation
     * requires calibration data processing) */
    for (int i = 0; i < 768; i++) {
        /* Simplified: raw × 0.02 - 273.15 = temperature in Celsius */
        /* Full implementation uses EEPROM calibration coefficients */
        float raw = (float)frame_data[i];
        /* Apply rough conversion: centi-degrees C */
        pixels[i] = (int16_t)((raw * 0.02f - 273.15f) * 100.0f);
    }

    /* Clear data ready flag */
    mlx_write16(MLX90640_REG_STATUS1, status & ~0x0008);

    return ESP_OK;
}

float mlx90640_get_pixel(int16_t *pixels, int x, int y)
{
    if (x < 0 || x >= 32 || y < 0 || y >= 24) return 0.0f;
    return pixels[y * 32 + x] / 100.0f;
}
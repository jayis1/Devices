/*
 * Pest Sentinel — MLX90640 Thermal Array
 * firmware/pest-sentinel/thermal.c
 *
 * Reads 32×24 IR thermal grid from MLX90640 via I2C.
 * Detects warm bodies (rodents at 32-38°C) even in total darkness.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

#include "psp_protocol.h"

static const char *TAG = "THERMAL";

#define I2C_NUM      I2C_NUM_1
#define I2C_SDA      18
#define I2C_SCL      8
#define MLX_ADDR     0x33

#define MLX_WIDTH    32
#define MLX_HEIGHT   24
#define MLX_PIXELS   (MLX_WIDTH * MLX_HEIGHT)  /* 768 */

extern float g_thermal_max_c;

static float thermal_frame[MLX_PIXELS];

static void mlx_write_reg(uint16_t reg, uint16_t val)
{
    uint8_t buf[4] = {
        (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
        (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)
    };
    i2c_master_write_to_device(I2C_NUM, MLX_ADDR, buf, 4, pdMS_TO_TICKS(100));
}

static uint16_t mlx_read_reg(uint16_t reg)
{
    uint8_t cmd[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    uint8_t data[2];
    i2c_master_write_read_device(I2C_NUM, MLX_ADDR, cmd, 2, data, 2, pdMS_TO_TICKS(100));
    return (data[0] << 8) | data[1];
}

static void mlx_read_frame(float *temp_pixels)
{
    /* Read subpage 0 and subpage 1 (MLX90640 interleaved readout)
     * In production: use MLX90640 API library for proper refresh + processing.
     * Here: simplified single-subpage read for reference. */

    /* Set subpage 0 */
    mlx_write_reg(0x7FFF, 0x0100); /* status reg, trigger measurement */

    /* Wait for data ready */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read RAM addresses 0x0400-0x06FF (768 pixels × 2 bytes) */
    /* In production: burst read all 768 pixels in one I2C transaction */
    for (int i = 0; i < MLX_PIXELS; i++) {
        uint16_t addr = 0x0400 + i;
        uint16_t raw = mlx_read_reg(addr);
        /* Convert to temperature (simplified MLX90640 formula) */
        /* T = raw * 0.02 - 273.15 (simplified; real impl applies calibration) */
        temp_pixels[i] = raw * 0.02f - 273.15f;
    }

    /* Read subpage 1 similarly (omitted for brevity) */
}

static bool detect_warm_body(const float *temps)
{
    /* Look for pixels significantly above ambient (rodent ~32-38°C) */
    float max_temp = -100.0f;
    float avg_temp = 0.0f;

    for (int i = 0; i < MLX_PIXELS; i++) {
        avg_temp += temps[i];
        if (temps[i] > max_temp) max_temp = temps[i];
    }
    avg_temp /= MLX_PIXELS;

    g_thermal_max_c = max_temp;

    /* Warm body if max temp is >5°C above ambient AND in biological range */
    if (max_temp > avg_temp + 5.0f && max_temp > 25.0f && max_temp < 45.0f) {
        ESP_LOGD(TAG, "Warm body detected: max=%.1f°C avg=%.1f°C",
                 max_temp, avg_temp);
        return true;
    }
    return false;
}

void thermal_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Thermal task started (MLX90640 32×24)");

    /* I2C init for MLX90640 (400 kHz fast mode) */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM, &conf);
    i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);

    /* MLX90640 initialization */
    /* Set refresh rate: 2 Hz (reg 0x0019, bits 7-4) */
    uint16_t ctrl = mlx_read_reg(0x0019);
    ctrl = (ctrl & 0x0FFF) | (0x2000); /* 2 Hz refresh */
    mlx_write_reg(0x0019, ctrl);

    ESP_LOGI(TAG, "MLX90640 initialized, scanning for warm bodies...");

    while (1) {
        mlx_read_frame(thermal_frame);

        bool warm_body = detect_warm_body(thermal_frame);

        if (warm_body) {
            ESP_LOGI(TAG, "🔥 Warm body detected! Max temp: %.1f°C", g_thermal_max_c);
            /* The CNN task will combine thermal + camera for final classification */
        }

        /* Scan every 3 seconds (thermal-only, low power) */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
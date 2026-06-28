/*
 * Hub — OLED Display
 * firmware/hub/display.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#include "psp_protocol.h"
#include "sensor_types.h"

static const char *TAG = "DISPLAY";

#define I2C_NUM     I2C_NUM_0
#define I2C_SDA     21
#define I2C_SCL     22
#define SSD_ADDR    0x3C

static void ssd1306_command(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    i2c_master_write_to_device(I2C_NUM, SSD_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static void ssd1306_init(void)
{
    ssd1306_command(0xAE); /* display off */
    ssd1306_command(0xD5); ssd1306_command(0x80); /* clock divide */
    ssd1306_command(0xA8); ssd1306_command(0x3F); /* multiplex 1/64 */
    ssd1306_command(0xD3); ssd1306_command(0x00); /* offset */
    ssd1306_command(0x40); /* start line 0 */
    ssd1306_command(0x8D); ssd1306_command(0x14); /* charge pump on */
    ssd1306_command(0x20); ssd1306_command(0x00); /* addressing horizontal */
    ssd1306_command(0xA1); /* segment remap */
    ssd1306_command(0xC8); /* COM scan direction */
    ssd1306_command(0xDA); ssd1306_command(0x12); /* COM pins */
    ssd1306_command(0x81); ssd1306_command(0xCF); /* contrast */
    ssd1306_command(0xD9); ssd1306_command(0xF1); /* precharge */
    ssd1306_command(0xDB); ssd1306_command(0x40); /* VCOM deselect */
    ssd1306_command(0xA4); /* display RAM */
    ssd1306_command(0xA6); /* normal display */
    ssd1306_command(0xAF); /* display on */
}

static void display_clear(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_command(0xB0 + page);
        ssd1306_command(0x00);
        ssd1306_command(0x10);
        uint8_t buf[129] = { 0x40 };
        memset(buf + 1, 0, 128);
        i2c_master_write_to_device(I2C_NUM, SSD_ADDR, buf, 129, pdMS_TO_TICKS(100));
    }
}

static void display_text(uint8_t page, uint8_t col, const char *text)
{
    /* Simplified: write raw text pattern (real impl uses font table) */
    ssd1306_command(0xB0 + page);
    ssd1306_command(0x00 + (col & 0x0F));
    ssd1306_command(0x10 + ((col >> 4) & 0x0F));
    /* In production, map each char through 5x7 font bitmap */
}

void display_task(void *pvParameters)
{
    /* I2C init */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM, &conf);
    i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);

    ssd1306_init();
    display_clear();

    char line[32];

    while (1) {
        display_clear();

        snprintf(line, sizeof(line), "PestSync Hub");
        display_text(0, 0, line);

        snprintf(line, sizeof(line), "Pest: %s", pest_class_name(g_sentinel_data.pest_class));
        display_text(2, 0, line);

        snprintf(line, sizeof(line), "Trap: %s", trap_status_name(g_trap_data.trap_status));
        display_text(3, 0, line);

        snprintf(line, sizeof(line), "Deterrent: %s",
                 g_deterrent_data.ultrasonic_active ? "ACTIVE" : "idle");
        display_text(4, 0, line);

        snprintf(line, sizeof(line), "Bat: S%d T%d D%d",
                 g_sentinel_data.battery_pct,
                 g_trap_data.battery_pct,
                 g_deterrent_data.battery_pct);
        display_text(6, 0, line);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
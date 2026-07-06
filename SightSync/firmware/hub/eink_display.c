/**
 * SightSync Vision Hub — E-ink Display (SSD1680) Implementation
 *
 * 2.9" 296×128 monochrome e-ink, SPI.
 * Shows: Visual Fatigue Index, blink rate, viewing distance,
 * minutes since break, lamp CCT, time.
 *
 * License: MIT
 */

#include "eink_display.h"
#include "alert_engine.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "eink";

/* SSD1680 commands */
#define SSD1680_DRIVER_OUTPUT_CTRL     0x01
#define SSD1680_BOOST_SOFT_START       0x0C
#define SSD1680_GATE_SCAN_POS          0x0F
#define SSD1680_DEEP_SLEEP_MODE         0x10
#define SSD1680_DATA_ENTRY_MODE        0x11
#define SSD1680_SW_RESET               0x12
#define SSD1680_TEMP_SENSOR_CTRL       0x1A
#define SSD1680_MASTER_ACTIVATION       0x20
#define SSD1680_DISPLAY_UPDATE_CTRL2   0x22
#define SSD1680_WRITE_RAM              0x24
#define SSD1680_WRITE_LUT              0x32
#define SSD1680_SET_RAM_X_ADDR          0x4E
#define SSD1680_SET_RAM_Y_ADDR          0x4F
#define SSD1680_SET_RAM_X_ADDR_END     0x44
#define SSD1680_SET_RAM_Y_ADDR_END     0x45

#define EINK_WIDTH  296
#define EINK_HEIGHT 128

/* Pin assignments (from README) */
#define EINK_CS    7
#define EINK_DC    8
#define EINK_RST   9
#define EINK_BUSY  10
#define EINK_CLK   4
#define EINK_MOSI  5

static spi_device_handle_t s_spi;

/* ── SSD1680 helpers ─────────────────────────────────────────────── */

static void ssd1680_cmd(uint8_t cmd)
{
    gpio_set_level(EINK_DC, 0);  /* DC=0 for command */
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_data[0] = cmd;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(s_spi, &t);
}

static void ssd1680_data(uint8_t data)
{
    gpio_set_level(EINK_DC, 1);  /* DC=1 for data */
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_data[0] = data;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(s_spi, &t);
}

static void ssd1680_wait_busy(void)
{
    while (gpio_get_level(EINK_BUSY) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── Full screen buffer (296×128 = 37,888 bits = 4,736 bytes) ────── */

static uint8_t s_framebuf[4736];

static void ssd1680_update_full(void)
{
    ssd1680_cmd(SSD1680_SET_RAM_X_ADDR);
    ssd1680_data(0x00);
    ssd1680_cmd(SSD1680_SET_RAM_Y_ADDR);
    ssd1680_data(0x00);
    ssd1680_data(0x00);

    ssd1680_cmd(SSD1680_WRITE_RAM);
    gpio_set_level(EINK_DC, 1);
    spi_transaction_t t = {0};
    t.length = sizeof(s_framebuf) * 8;
    t.tx_buffer = s_framebuf;
    spi_device_polling_transmit(s_spi, &t);

    ssd1680_cmd(SSD1680_DISPLAY_UPDATE_CTRL2);
    ssd1680_data(0xC7);  /* full update */
    ssd1680_cmd(SSD1680_MASTER_ACTIVATION);
    ssd1680_wait_busy();
}

/* ── Simple text rendering (5×7 font, partial) ───────────────────── */

static void draw_pixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= EINK_WIDTH || y < 0 || y >= EINK_HEIGHT) return;
    uint16_t byte_idx = (y * EINK_WIDTH + x) / 8;
    uint8_t bit_idx = (y * EINK_WIDTH + x) % 8;
    if (color) {
        s_framebuf[byte_idx] &= ~(1 << (7 - bit_idx));  /* black */
    } else {
        s_framebuf[byte_idx] |= (1 << (7 - bit_idx));   /* white */
    }
}

/* Minimal 5x7 font for digits and letters used in the display */
static const uint8_t font5x7[][5] = {
    [0]= {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    [1]= {0x00,0x42,0x7F,0x40,0x00},
    [2]= {0x42,0x61,0x51,0x49,0x46},
    [3]= {0x21,0x41,0x45,0x4B,0x31},
    [4]= {0x18,0x14,0x12,0x7F,0x10},
    [5]= {0x27,0x45,0x45,0x45,0x39},
    [6]= {0x3C,0x4A,0x49,0x49,0x30},
    [7]= {0x01,0x71,0x09,0x05,0x03},
    [8]= {0x36,0x49,0x49,0x49,0x36},
    [9]= {0x06,0x49,0x49,0x29,0x1E},
};

static void draw_digit(int16_t x, int16_t y, char d, uint8_t color)
{
    if (d < '0' || d > '9') return;
    uint8_t idx = d - '0';
    for (int8_t col = 0; col < 5; col++) {
        uint8_t bits = font5x7[idx][col];
        for (int8_t row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

static void draw_number(int16_t x, int16_t y, int32_t value, uint8_t color)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    int16_t cx = x;
    for (uint8_t i = 0; buf[i]; i++) {
        draw_digit(cx, y, buf[i], color);
        cx += 6;
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void eink_display_init(void)
{
    /* Configure GPIO */
    gpio_set_direction(EINK_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(EINK_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(EINK_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(EINK_CS, GPIO_MODE_OUTPUT);

    /* Init SPI */
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = EINK_MOSI,
        .sclk_io_num = EINK_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192,
    };
    spi_bus_initialize(SPI3_HOST, &buscfg, 0);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = EINK_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI3_HOST, &devcfg, &s_spi);

    /* Hardware reset */
    gpio_set_level(EINK_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(EINK_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Software reset */
    ssd1680_cmd(SSD1680_SW_RESET);
    ssd1680_wait_busy();

    /* Driver output control: 128 lines */
    ssd1680_cmd(SSD1680_DRIVER_OUTPUT_CTRL);
    ssd1680_data(0x7F);   /* 128-1 = 0x7F */
    ssd1680_data(0x00);
    ssd1680_data(0x00);

    /* Data entry mode: X increment, Y increment */
    ssd1680_cmd(SSD1680_DATA_ENTRY_MODE);
    ssd1680_data(0x03);

    /* Set RAM area */
    ssd1680_cmd(SSD1680_SET_RAM_X_ADDR_END);
    ssd1680_data((EINK_WIDTH / 8) - 1);
    ssd1680_data(0x00);
    ssd1680_cmd(SSD1680_SET_RAM_Y_ADDR_END);
    ssd1680_data(0x7F);
    ssd1680_data(0x00);

    /* Clear frame buffer */
    memset(s_framebuf, 0xFF, sizeof(s_framebuf));
    ssd1680_update_full();

    ESP_LOGI(TAG, "E-ink display initialized (2.9\" 296×128)");
}

void eink_display_update(const void *state_ptr)
{
    const hub_state_t *s = (const hub_state_t *)state_ptr;

    /* Clear buffer */
    memset(s_framebuf, 0xFF, sizeof(s_framebuf));

    /* Draw fatigue score (large, top-left) */
    draw_number(5, 5, s->fatigue_score, 1);

    /* Draw blink rate (bottom-left) */
    draw_number(5, 30, s->blink_rate_bpm, 1);

    /* Draw viewing distance (bottom-left) */
    draw_number(5, 55, s->viewing_distance_mm, 1);

    /* Draw minutes since break (bottom-left) */
    draw_number(5, 80, s->minutes_since_break, 1);

    /* Refresh display */
    ssd1680_update_full();
}

void eink_display_clear(void)
{
    memset(s_framebuf, 0xFF, sizeof(s_framebuf));
    ssd1680_update_full();
}
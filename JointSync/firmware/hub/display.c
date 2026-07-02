/**
 * JointSync Hub — E-Paper Display Driver
 *
 * 7.5" Waveshare e-paper display (SPI).
 *
 * License: MIT
 */

#include "display.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "display";

#define DISPLAY_CS    7
#define DISPLAY_DC    8
#define DISPLAY_RST   9
#define DISPLAY_BUSY  10
#define DISPLAY_CLK   11
#define DISPLAY_MOSI  12
#define DISPLAY_MISO  13

static spi_device_handle_t g_display_spi;

/* Display dimensions: 800×480 */
#define DISP_WIDTH   800
#define DISP_HEIGHT  480

/* Current display page */
typedef enum {
    PAGE_OVERVIEW = 0,
    PAGE_JOINT_1  = 1,
    PAGE_JOINT_2  = 2,
    PAGE_THERAPY  = 3,
    PAGE_ALERTS   = 4,
    PAGE_COUNT    = 5
} display_page_t;

static display_page_t g_current_page = PAGE_OVERVIEW;
static bool g_btn1 = false, g_btn2 = false, g_btn3 = false;

/* Display buffer (1 bit per pixel) */
static uint8_t g_framebuffer[DISP_WIDTH * DISP_HEIGHT / 8];

/* ── SPI Transfer ─────────────────────────────────────────────────── */

static void display_send_cmd(uint8_t cmd)
{
    gpio_set_level(DISPLAY_DC, 0);
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(g_display_spi, &t);
}

static void display_send_data(uint8_t *data, int len)
{
    gpio_set_level(DISPLAY_DC, 1);
    spi_transaction_t t = {0};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(g_display_spi, &t);
}

/* ── Framebuffer Operations ─────────────────────────────────────── */

static void fb_clear(void)
{
    memset(g_framebuffer, 0xFF, sizeof(g_framebuffer));
}

static void fb_set_pixel(int x, int y, int val)
{
    if (x < 0 || x >= DISP_WIDTH || y < 0 || y >= DISP_HEIGHT) return;
    int byte_idx = (y * DISP_WIDTH + x) / 8;
    int bit_idx = 7 - ((y * DISP_WIDTH + x) % 8);
    if (val) {
        g_framebuffer[byte_idx] &= ~(1 << bit_idx);
    } else {
        g_framebuffer[byte_idx] |= (1 << bit_idx);
    }
}

static void fb_draw_text(int x, int y, const char *text, int size)
{
    /* Simplified text rendering — real impl uses bitmap font */
    int px = x;
    for (const char *p = text; *p; p++) {
        for (int dy = 0; dy < 8 * size; dy++) {
            for (int dx = 0; dx < 6 * size; dx++) {
                if (dx % 2 == 0) fb_set_pixel(px + dx, y + dy, 0);
            }
        }
        px += 6 * size;
    }
}

static void fb_draw_rect(int x, int y, int w, int h, int filled)
{
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            if (filled || dy == 0 || dy == h-1 || dx == 0 || dx == w-1) {
                fb_set_pixel(x + dx, y + dy, 1);
            }
        }
    }
}

/* ── Render Pages ─────────────────────────────────────────────────── */

static void render_overview(void)
{
    fb_clear();
    fb_draw_text(20, 20, "JointSync Hub", 3);
    fb_draw_text(20, 70, "Joint Health Overview", 2);

    /* Status line */
    fb_draw_text(20, 120, "Connected Tags: 4", 2);
    fb_draw_text(20, 160, "Scanner: Ready", 2);
    fb_draw_text(20, 200, "Sleeve: Idle", 2);

    /* Joint summary */
    fb_draw_text(20, 250, "Left Knee:  ROM 95°  Temp 32.1°C", 2);
    fb_draw_text(20, 290, "Right Knee: ROM 110° Temp 31.8°C", 2);
    fb_draw_text(20, 330, "Left Wrist: ROM 70°  Temp 31.5°C", 2);
    fb_draw_text(20, 370, "Right Wrist:ROM 75°  Temp 31.6°C", 2);

    /* Flare risk gauge */
    fb_draw_rect(550, 250, 200, 100, 0);
    fb_draw_text(580, 280, "Flare Risk", 2);
    fb_draw_text(620, 320, "12%", 4);

    /* Navigation hint */
    fb_draw_text(20, 430, "Btn1: Therapy  Btn2: Scan  Btn3: Next", 1);
}

static void render_joint_detail(int joint_idx)
{
    fb_clear();
    fb_draw_text(20, 20, "Joint Detail", 3);
    fb_draw_text(20, 70, joint_idx == 1 ? "Left Knee" : "Right Knee", 2);

    fb_draw_text(20, 120, "ROM History (7 days)", 2);
    fb_draw_rect(20, 160, 400, 200, 0);  /* Chart area */

    fb_draw_text(20, 380, "Current ROM: 95 degrees", 2);
    fb_draw_text(20, 420, "Temp Delta: +0.3°C (normal)", 2);

    fb_draw_text(20, 450, "Btn3: Next page", 1);
}

static void render_therapy(void)
{
    fb_clear();
    fb_draw_text(20, 20, "Compression Therapy", 3);
    fb_draw_text(20, 70, "Sleeve Status: Idle", 2);
    fb_draw_text(20, 120, "Pressure: 0 mmHg", 2);
    fb_draw_text(20, 160, "Last Session: 2h ago (30 min)", 2);
    fb_draw_text(20, 200, "Adherence: 85% this week", 2);
    fb_draw_text(20, 430, "Btn1: Start therapy", 1);
}

static void render_alerts(void)
{
    fb_clear();
    fb_draw_text(20, 20, "Alerts", 3);
    fb_draw_text(20, 70, "No active alerts", 2);
    fb_draw_text(20, 120, "Last flare warning: 3 days ago", 2);
    fb_draw_text(20, 430, "Btn3: Next page", 1);
}

/* ── Display Flush ────────────────────────────────────────────────── */

static void display_flush(void)
{
    /* Wait for not busy */
    while (gpio_get_level(DISPLAY_BUSY) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Set RAM address */
    display_send_cmd(0x4E); /* RAM X address */
    uint8_t x_data[1] = {0x00};
    display_send_data(x_data, 1);

    display_send_cmd(0x4F); /* RAM Y address */
    uint8_t y_data[2] = {0x00, 0x00};
    display_send_data(y_data, 2);

    /* Write framebuffer to display RAM */
    display_send_cmd(0x24); /* Write RAM */
    display_send_data(g_framebuffer, sizeof(g_framebuffer));

    /* Update display */
    display_send_cmd(0x22); /* Display update sequence */
    uint8_t seq[1] = {0xF7};
    display_send_data(seq, 1);

    ESP_LOGD(TAG, "Display flushed");
}

/* ── Button Polling ──────────────────────────────────────────────── */

static void button_poll_task(void *arg)
{
    while (1) {
        g_btn1 = (gpio_get_level(19) == 0);  /* Active low */
        g_btn2 = (gpio_get_level(20) == 0);
        g_btn3 = (gpio_get_level(21) == 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ── Public API ───────────────────────────────────────────────────── */

void display_init(void)
{
    /* Initialize SPI */
    spi_bus_config_t buscfg = {
        .mosi_io_num = DISPLAY_MOSI,
        .miso_io_num = DISPLAY_MISO,
        .sclk_io_num = DISPLAY_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,  /* 2 MHz */
        .mode = 0,
        .spics_io_num = DISPLAY_CS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_display_spi);

    /* Initialize GPIO */
    gpio_set_direction(DISPLAY_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(DISPLAY_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(DISPLAY_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(19, GPIO_MODE_INPUT);  /* Buttons */
    gpio_set_direction(20, GPIO_MODE_INPUT);
    gpio_set_direction(21, GPIO_MODE_INPUT);
    gpio_pullup_en(19);
    gpio_pullup_en(20);
    gpio_pullup_en(21);

    /* Hardware reset */
    gpio_set_level(DISPLAY_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(DISPLAY_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Initialize display with boot sequence */
    display_send_cmd(0x12); /* Software reset */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Render initial page */
    render_overview();
    display_flush();

    /* Start button polling task */
    xTaskCreate(button_poll_task, "btn_poll", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Display initialized (800×480 e-paper)");
}

void display_update_temp(uint16_t sender_id, float temp, float inflam_prob)
{
    /* Update overview page with latest temp */
    /* In production, this would update specific line based on sender_id */
}

void display_update_pressure(float pressure_mmhg, uint8_t pump_state)
{
    /* Update therapy page if active */
}

void display_update_heartbeat(uint8_t counter)
{
    /* Update status indicator */
}

bool display_get_button1(void) { return g_btn1; }
bool display_get_button2(void) { return g_btn2; }
bool display_get_button3(void) { return g_btn3; }

void display_next_page(void)
{
    g_current_page = (display_page_t)((g_current_page + 1) % PAGE_COUNT);

    switch (g_current_page) {
    case PAGE_OVERVIEW: render_overview(); break;
    case PAGE_JOINT_1:  render_joint_detail(1); break;
    case PAGE_JOINT_2:  render_joint_detail(2); break;
    case PAGE_THERAPY:  render_therapy(); break;
    case PAGE_ALERTS:   render_alerts(); break;
    default: break;
    }

    display_flush();
}
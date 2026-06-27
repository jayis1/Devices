/*
 * Hub — OLED Display (SSD1306 via I2C)
 * firmware/hub/display.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "csp_protocol.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "DISPLAY";

#define I2C_NUM     I2C_NUM_0
#define I2C_SDA     21
#define I2C_SCL     22
#define OLED_ADDR   0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

/* Latest display data (updated by other tasks via shared state) */
static struct {
    uint8_t phase;
    float   temp;
    float   co2;
    float   methane;
    float   moisture;
    uint8_t battery;
    char    recommendation[80];
} display_state = {0};

/* Called from other tasks to update display data */
void display_update(uint8_t phase, float temp, float co2, float methane,
                     float moisture, uint8_t battery, const char *rec)
{
    display_state.phase = phase;
    display_state.temp = temp;
    display_state.co2 = co2;
    display_state.methane = methane;
    display_state.moisture = moisture;
    display_state.battery = battery;
    strncpy(display_state.recommendation, rec ? rec : "",
            sizeof(display_state.recommendation) - 1);
}

/* SSD1306 commands */
#define SSD1306_CMD   0x00
#define SSD1306_DATA  0x40

static void ssd1306_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { SSD1306_CMD, cmd };
    i2c_master_write_to_device(I2C_NUM, OLED_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static void ssd1306_data(const uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = SSD1306_DATA;
    memcpy(buf + 1, data, len);
    i2c_master_write_to_device(I2C_NUM, OLED_ADDR, buf, len + 1, pdMS_TO_TICKS(100));
}

static void ssd1306_init(void)
{
    /* Init sequence for SSD1306 128x64 */
    ssd1306_cmd(0xAE); /* Display off */
    ssd1306_cmd(0xD5); ssd1306_cmd(0x80); /* Display clock div */
    ssd1306_cmd(0xA8); ssd1306_cmd(0x3F); /* Multiplex 1/64 */
    ssd1306_cmd(0xD3); ssd1306_cmd(0x00); /* Display offset */
    ssd1306_cmd(0x40); /* Start line 0 */
    ssd1306_cmd(0x8D); ssd1306_cmd(0x14); /* Charge pump on */
    ssd1306_cmd(0x20); ssd1306_cmd(0x00); /* Memory addressing mode horizontal */
    ssd1306_cmd(0xA1); /* Segment remap */
    ssd1306_cmd(0xC8); /* COM scan direction */
    ssd1306_cmd(0xDA); ssd1306_cmd(0x12); /* COM pins */
    ssd1306_cmd(0x81); ssd1306_cmd(0xCF); /* Contrast */
    ssd1306_cmd(0xD9); ssd1306_cmd(0xF1); /* Pre-charge period */
    ssd1306_cmd(0xDB); ssd1306_cmd(0x40); /* VCOMH deselect */
    ssd1306_cmd(0xA4); /* Display on RAM content */
    ssd1306_cmd(0xA6); /* Normal display */
    ssd1306_cmd(0xAF); /* Display on */
}

static uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

static void fb_clear(void)  { memset(framebuffer, 0, sizeof(framebuffer)); }

/* Simple 5x7 font (subset) */
static const uint8_t font5x7[][5] = {
    [' ']= {0x00,0x00,0x00,0x00,0x00},
    [':']= {0x00,0x00,0x0C,0x00,0x00},
    ['.']= {0x00,0x00,0x00,0x60,0x00},
    ['0']= {0x3E,0x51,0x49,0x45,0x3E},
    ['1']= {0x00,0x42,0x7F,0x40,0x00},
    ['2']= {0x42,0x61,0x51,0x49,0x46},
    ['3']= {0x21,0x41,0x45,0x4B,0x31},
    ['4']= {0x18,0x14,0x12,0x7F,0x10},
    ['5']= {0x27,0x45,0x45,0x45,0x39},
    ['6']= {0x3C,0x4A,0x49,0x49,0x30},
    ['7']= {0x01,0x71,0x09,0x05,0x03},
    ['8']= {0x36,0x49,0x49,0x49,0x36},
    ['9']= {0x06,0x49,0x49,0x29,0x1E},
    ['A']= {0x7E,0x11,0x11,0x11,0x7E},
    ['B']= {0x7F,0x49,0x49,0x49,0x36},
    ['C']= {0x3E,0x41,0x41,0x41,0x22},
    ['D']= {0x7F,0x41,0x41,0x22,0x1C},
    ['E']= {0x7F,0x49,0x49,0x49,0x41},
    ['F']= {0x7F,0x09,0x09,0x09,0x01},
    ['G']= {0x3E,0x41,0x49,0x49,0x7A},
    ['H']= {0x7F,0x08,0x08,0x08,0x7F},
    ['I']= {0x00,0x41,0x7F,0x41,0x00},
    ['J']= {0x20,0x40,0x41,0x3F,0x01},
    ['K']= {0x7F,0x08,0x14,0x22,0x41},
    ['L']= {0x7F,0x40,0x40,0x40,0x40},
    ['M']= {0x7F,0x02,0x0C,0x02,0x7F},
    ['N']= {0x7F,0x04,0x08,0x10,0x7F},
    ['O']= {0x3E,0x41,0x41,0x41,0x3E},
    ['P']= {0x7F,0x09,0x09,0x09,0x06},
    ['Q']= {0x3E,0x41,0x51,0x21,0x5E},
    ['R']= {0x7F,0x09,0x19,0x29,0x46},
    ['S']= {0x46,0x49,0x49,0x49,0x31},
    ['T']= {0x01,0x01,0x7F,0x01,0x01},
    ['U']= {0x3F,0x40,0x40,0x40,0x3F},
    ['V']= {0x1F,0x20,0x40,0x20,0x1F},
    ['W']= {0x3F,0x40,0x38,0x40,0x3F},
    ['X']= {0x63,0x14,0x08,0x14,0x63},
    ['Y']= {0x07,0x08,0x70,0x08,0x07},
    ['Z']= {0x61,0x51,0x49,0x45,0x43},
    ['a']= {0x20,0x54,0x54,0x54,0x78},
    ['b']= {0x7F,0x48,0x44,0x44,0x38},
    ['c']= {0x38,0x44,0x44,0x44,0x20},
    ['d']= {0x38,0x44,0x44,0x48,0x7F},
    ['e']= {0x38,0x54,0x54,0x54,0x18},
    ['f']= {0x08,0x7E,0x09,0x01,0x02},
    ['g']= {0x0C,0x52,0x52,0x52,0x3E},
    ['h']= {0x7F,0x08,0x04,0x04,0x78},
    ['i']= {0x00,0x44,0x7D,0x40,0x00},
    ['j']= {0x20,0x40,0x44,0x3D,0x00},
    ['k']= {0x7F,0x10,0x28,0x44,0x00},
    ['l']= {0x00,0x41,0x7F,0x40,0x00},
    ['m']= {0x7C,0x04,0x18,0x04,0x78},
    ['n']= {0x7C,0x08,0x04,0x04,0x78},
    ['o']= {0x38,0x44,0x44,0x44,0x38},
    ['p']= {0x7C,0x14,0x14,0x14,0x08},
    ['q']= {0x08,0x14,0x14,0x18,0x7C},
    ['r']= {0x7C,0x08,0x04,0x04,0x08},
    ['s']= {0x48,0x54,0x54,0x54,0x20},
    ['t']= {0x04,0x3F,0x44,0x40,0x20},
    ['u']= {0x3C,0x40,0x40,0x20,0x7C},
    ['v']= {0x1C,0x20,0x40,0x20,0x1C},
    ['w']= {0x3C,0x40,0x30,0x40,0x3C},
    ['x']= {0x44,0x28,0x10,0x28,0x44},
    ['y']= {0x0C,0x50,0x50,0x50,0x3C},
    ['z']= {0x44,0x64,0x54,0x4C,0x44},
    ['%']= {0x43,0x4D,0x52,0x54,0x42},
    ['!']= {0x00,0x00,0x5F,0x00,0x00},
    ['=']= {0x00,0x18,0x18,0x18,0x00},
    ['+']= {0x00,0x08,0x3E,0x08,0x00},
    ['/']= {0x20,0x10,0x08,0x04,0x02},
};

static void fb_draw_char(int x, int y, char c)
{
    if (c < ' ' || c > 'z') c = ' ';
    const uint8_t *glyph = font5x7[(uint8_t)c];
    for (int i = 0; i < 5; i++) {
        uint8_t col = glyph[i];
        for (int j = 0; j < 7; j++) {
            if (col & (1 << j)) {
                int px = x + i;
                int py = y + j;
                if (px < OLED_WIDTH && py < OLED_HEIGHT)
                    framebuffer[px + (py / 8) * OLED_WIDTH] |= (1 << (py & 7));
            }
        }
    }
}

static void fb_draw_text(int x, int y, const char *str)
{
    while (*str) {
        fb_draw_char(x, y, *str);
        x += 6;
        if (x > OLED_WIDTH - 6) { x = 0; y += 8; }
        str++;
    }
}

static void fb_render(void)
{
    /* Set column and page addressing */
    ssd1306_cmd(0x21); ssd1306_cmd(0); ssd1306_cmd(OLED_WIDTH - 1);
    ssd1306_cmd(0x22); ssd1306_cmd(0); ssd1306_cmd((OLED_HEIGHT/8) - 1);
    ssd1306_data(framebuffer, sizeof(framebuffer));
}

void display_task(void *pvParameters)
{
    /* Init I2C */
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

    vTaskDelay(pdMS_TO_TICKS(100));
    ssd1306_init();

    char buf[32];
    char phase_str[12];

    while (1) {
        fb_clear();

        /* Title */
        fb_draw_text(0, 0, "CompostSync");

        /* Phase */
        const char *phases[] = {"Mesoph", "Therm", "Cool", "Matur", "Cured", "Dorm"};
        if (display_state.phase < 6)
            strcpy(phase_str, phases[display_state.phase]);
        else
            strcpy(phase_str, "Unknown");
        snprintf(buf, sizeof(buf), "Phase: %s", phase_str);
        fb_draw_text(0, 10, buf);

        /* Temperature */
        snprintf(buf, sizeof(buf), "Temp: %.1fC", display_state.temp);
        fb_draw_text(0, 20, buf);

        /* CO2 + methane */
        snprintf(buf, sizeof(buf), "CO2: %.0fppm", display_state.co2);
        fb_draw_text(0, 30, buf);
        snprintf(buf, sizeof(buf), "CH4: %.0fppm", display_state.methane);
        fb_draw_text(0, 40, buf);

        /* Battery */
        snprintf(buf, sizeof(buf), "Batt: %d%%", display_state.battery);
        fb_draw_text(0, 54, buf);

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
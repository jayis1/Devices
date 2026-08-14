/*
 * PostureSync Desk Sentinel Firmware
 * Target: RP2040 (main) + ESP32-C3 (wireless bridge)
 *
 * Clips to monitor bezel. Monitors:
 *   - Screen-to-eye distance (VL53L1X, 400cm range)
 *   - Desk height (VL53L0X, 200cm range, downward-facing)
 *   - Ambient light (VEML7700 lux sensor)
 *   - Sit/stand reminders
 *
 * Power: USB-C 5V primary, 2× AAA backup
 * Wireless: ESP32-C3 (Wi-Fi/MQTT to Hub)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/i2c.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "pico/multicore.h"

#include "../common/protocol.h"

/* ---- Pin definitions (RP2040) ---- */
#define PIN_I2C_SDA     0
#define PIN_I2C_SCL     1
#define PIN_UART_TX     2   /* To ESP32-C3 RX */
#define PIN_UART_RX     3   /* From ESP32-C3 TX */
#define PIN_BUZZER      4
#define PIN_LED_DATA    5   /* WS2812B */
#define PIN_BTN_PAIR    6
#define PIN_USB_DET     7
#define PIN_BAT_SENSE   8   /* ADC0 */
#define PIN_ESP_RST     9
#define PIN_ESP_BOOT    10
#define PIN_VL53L1X_SHDN 11
#define PIN_VL53L0X_SHDN 12

#define I2C_PORT i2c0
#define I2C_FREQ 400000

/* VL53L1X I2C address */
#define VL53L1X_ADDR    0x29
/* VL53L0X I2C address (same, needs separate shutdown control) */
#define VL53L0X_ADDR    0x29
/* VEML7700 I2C address */
#define VEML7700_ADDR   0x10

/* UART to ESP32-C3 */
#define UART_ID uart1
#define BAUD_RATE 115200

/* Sensor data */
static uint16_t g_screen_distance_mm = 0;
static uint16_t g_desk_height_mm = 0;
static uint16_t g_ambient_lux = 0;
static uint8_t  g_sit_stand = 0;  /* 0=sitting, 1=standing, 2=transition */
static uint8_t  g_time_in_position = 0;  /* minutes */
static uint8_t  g_battery = 100;

/* Calibration */
static uint16_t g_desk_sit_height = 720;  /* mm, default 28.3" */
static uint16_t g_desk_stand_height = 1100; /* mm, default 43.3" */
static bool g_calibrated = false;

/* ---- I2C helpers ---- */
static void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
}

static void i2c_write_reg16(uint8_t addr, uint16_t reg, uint16_t val)
{
    uint8_t buf[4] = {(reg >> 8) & 0xFF, reg & 0xFF, (val >> 8) & 0xFF, val & 0xFF};
    i2c_write_blocking(I2C_PORT, addr, buf, 4, false);
}

static uint16_t i2c_read_reg16(uint8_t addr, uint16_t reg)
{
    uint8_t reg_buf[2] = {(reg >> 8) & 0xFF, reg & 0xFF};
    i2c_write_blocking(I2C_PORT, addr, reg_buf, 2, true);
    uint8_t data[2];
    i2c_read_blocking(I2C_PORT, addr, data, 2, false);
    return (data[0] << 8) | data[1];
}

static uint8_t i2c_read_reg(uint8_t addr, uint8_t reg)
{
    i2c_write_blocking(I2C_PORT, addr, &reg, 1, true);
    uint8_t val;
    i2c_read_blocking(I2C_PORT, addr, &val, 1, false);
    return val;
}

/* ---- VL53L1X (screen distance) ---- */
static void vl53l1x_init(void)
{
    /* Take out of shutdown */
    gpio_put(PIN_VL53L1X_SHDN, 1);
    sleep_ms(10);

    /* Software reset */
    i2c_write_reg16(VL53L1X_ADDR, 0x0000, 0x00);
    sleep_ms(100);

    /* Configure: short distance mode, 20ms timing */
    i2c_write_reg16(VL53L1X_ADDR, 0x0020, 0x0001); /* LLC0: range 1 */
    i2c_write_reg16(VL53L1X_ADDR, 0x0046, 0x0020); /* Phase cal */

    /* Start ranging */
    i2c_write_reg16(VL53L1X_ADDR, 0x0000, 0x0001);
}

static uint16_t vl53l1x_read_mm(void)
{
    /* Check if data ready */
    uint8_t status = i2c_read_reg(VL53L1X_ADDR, 0x0031 & 0xFF);
    if (!(status & 0x01)) return g_screen_distance_mm; /* Not ready, return last */

    /* Read distance */
    uint16_t dist = i2c_read_reg16(VL53L1X_ADDR, 0x0096);

    /* Clear interrupt */
    i2c_write_reg(VL53L1X_ADDR, 0x0015 & 0xFF, 0x01);

    /* Restart ranging */
    i2c_write_reg16(VL53L1X_ADDR, 0x0000, 0x0001);

    return dist;
}

/* ---- VL53L0X (desk height) ---- */
static void vl53l0x_init(void)
{
    /* VL53L0X needs to be re-addressed since same I2C address as VL53L1X
     * In production: power-cycle VL53L1X, set VL53L0X to new address,
     * then re-init VL53L1X. For simplicity, we time-multiplex. */

    /* Take out of shutdown */
    gpio_put(PIN_VL53L0X_SHDN, 1);
    sleep_ms(10);

    /* Read model ID to verify */
    uint8_t id = i2c_read_reg(VL53L0X_ADDR, 0xC0);
    if (id != 0xEE) return; /* Not a VL53L0X */

    /* Standard init sequence (simplified) */
    i2c_write_reg(VL53L0X_ADDR, 0x02, 0x01); /* Power on */
    sleep_ms(2);
    i2c_write_reg(VL53L0X_ADDR, 0x28, 0x00); /* I2C config */
    i2c_write_reg(VL53L0X_ADDR, 0x29, 0x00);
    i2c_write_reg(VL53L0X_ADDR, 0x0A, 0x02); /* GPIO HV mux */
    i2c_write_reg(VL53L0X_ADDR, 0x84, 0x00); /* GPIO active low */
    i2c_write_reg(VL53L0X_ADDR, 0x00, 0x01); /* Start measurement */
}

static uint16_t vl53l0x_read_mm(void)
{
    /* Read result */
    uint8_t status = i2c_read_reg(VL53L0X_ADDR, 0x13);
    if (!(status & 0x01)) return g_desk_height_mm;

    uint16_t dist = i2c_read_reg16(VL53L0X_ADDR, 0x14 + 0x10);
    i2c_write_reg(VL53L0X_ADDR, 0x0B, 0x01); /* Clear interrupt */
    return dist;
}

/* ---- VEML7700 (ambient light) ---- */
static void veml7700_init(void)
{
    /* Config register: ALS integration 100ms, gain 1/8 */
    i2c_write_reg16(VEML7700_ADDR, 0x00, 0x0000); /* Power on, no interrupt */
    /* Set integration time to 100ms */
    uint16_t config = (0 << 6) | (0 << 11) | (1 << 7); /* IT=100ms, gain=1/8 */
    i2c_write_reg16(VEML7700_ADDR, 0x00, config);
}

static uint16_t veml7700_read_lux(void)
{
    uint16_t raw = i2c_read_reg16(VEML7700_ADDR, 0x04); /* ALS result */
    /* Convert to lux (simplified, depends on gain + integration time) */
    /* For 100ms integration, gain 1/8: lux = raw * 0.0036 */
    return (uint16_t)(raw * 0.0036f * 1000.0f); /* millilux → lux */
}

/* ---- WS2812B LED ---- */
static void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    /* Simplified WS2812B bit-bang
     * In production: use PIO for precise timing */
    uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    /* PIO-based output would go here */
    (void)color;
}

/* ---- Buzzer ---- */
static void buzzer_beep(uint16_t ms)
{
    gpio_put(PIN_BUZZER, 1);
    sleep_ms(ms);
    gpio_put(PIN_BUZZER, 0);
}

/* ---- UART to ESP32-C3 ---- */
static void esp32_send_data(const desk_sentinel_data_t *data)
{
    uint8_t buf[sizeof(desk_sentinel_data_t) + 4];
    buf[0] = 0xAA;  /* Start byte */
    buf[1] = 0x55;
    memcpy(&buf[2], data, sizeof(*data));
    buf[sizeof(*data) + 2] = 0x0D;  /* End byte */
    buf[sizeof(*data) + 3] = 0x0A;

    uart_write_blocking(UART_ID, buf, sizeof(buf));
}

/* ---- Sit/stand detection ---- */
static void update_sit_stand(void)
{
    static uint8_t prev_state = 0;
    static uint32_t state_timer = 0;

    uint8_t new_state;
    if (g_desk_height_mm < (g_desk_sit_height + g_desk_stand_height) / 2) {
        new_state = 0; /* Sitting */
    } else {
        new_state = 1; /* Standing */
    }

    if (new_state != prev_state) {
        g_time_in_position = 0;
        g_sit_stand = 2; /* Transition */
        state_timer = to_ms_since_boot(get_absolute_time()) / 1000;
        prev_state = new_state;
    } else {
        g_sit_stand = new_state;
        g_time_in_position = (to_ms_since_boot(get_absolute_time()) / 1000 - state_timer) / 60;
    }
}

/* ---- Posture reminders ---- */
static void check_reminders(void)
{
    static uint32_t last_reminder = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time()) / 1000;

    /* Screen distance warning */
    if (g_screen_distance_mm > 0 && g_screen_distance_mm < 400) {
        if (now - last_reminder > 300) { /* 5 min cooldown */
            buzzer_beep(100);
            ws2812_set_color(255, 0, 0); /* Red */
            last_reminder = now;
        }
    }

    /* Sit/stand switch reminder (every 30 minutes) */
    if (g_time_in_position >= 30) {
        if (g_sit_stand == 0) {
            /* Time to stand */
            buzzer_beep(200);
            ws2812_set_color(0, 255, 0); /* Green */
        } else {
            /* Time to sit */
            buzzer_beep(200);
            ws2812_set_color(0, 0, 255); /* Blue */
        }
    }

    /* Low light warning */
    if (g_ambient_lux > 0 && g_ambient_lux < 300) {
        ws2812_set_color(255, 255, 0); /* Yellow */
    }
}

/* ---- Battery ADC ---- */
static uint8_t read_battery(void)
{
    adc_select_input(0); /* ADC0 = GPIO26 = PIN_BAT_SENSE */
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / 4095.0f * 2.0f; /* Voltage divider ×2 */
    /* 2× AAA: 2.0V=0%, 3.0V=100% */
    int soc = (int)((voltage - 2.0f) / 1.0f * 100.0f);
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;
    return (uint8_t)soc;
}

/* ---- Main loop ---- */
int main(void)
{
    stdio_init_all();

    /* I2C */
    i2c_init(I2C_PORT, PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    /* UART to ESP32-C3 */
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);

    /* GPIO */
    gpio_init(PIN_BUZZER);
    gpio_set_dir(PIN_BUZZER, GPIO_OUT);
    gpio_init(PIN_LED_DATA);
    gpio_set_dir(PIN_LED_DATA, GPIO_OUT);
    gpio_init(PIN_BTN_PAIR);
    gpio_set_dir(PIN_BTN_PAIR, GPIO_IN);
    gpio_pull_up(PIN_BTN_PAIR);
    gpio_init(PIN_VL53L1X_SHDN);
    gpio_set_dir(PIN_VL53L1X_SHDN, GPIO_OUT);
    gpio_init(PIN_VL53L0X_SHDN);
    gpio_set_dir(PIN_VL53L0X_SHDN, GPIO_OUT);

    /* ADC */
    adc_init();
    adc_gpio_init(PIN_BAT_SENSE);

    /* Init sensors */
    vl53l1x_init();
    sleep_ms(10);
    vl53l0x_init();
    sleep_ms(10);
    veml7700_init();

    /* Main loop */
    uint32_t last_tx = 0;
    uint32_t loop_count = 0;

    while (1) {
        /* Read sensors */
        g_screen_distance_mm = vl53l1x_read_mm();
        g_desk_height_mm = vl53l0x_read_mm();
        g_ambient_lux = veml7700_read_lux();

        /* Update sit/stand state */
        update_sit_stand();

        /* Check reminders */
        check_reminders();

        /* Battery (every 60 seconds) */
        if (loop_count % 60 == 0) {
            g_battery = read_battery();
        }

        /* Send to ESP32-C3 (every 10 seconds) */
        uint32_t now = to_ms_since_boot(get_absolute_time()) / 1000;
        if (now - last_tx >= 10) {
            desk_sentinel_data_t data = {0};
            data.screen_distance_mm = g_screen_distance_mm;
            data.desk_height_mm = g_desk_height_mm;
            data.ambient_lux = g_ambient_lux;
            data.sit_stand = g_sit_stand;
            data.time_in_position = g_time_in_position;
            data.battery = g_battery;
            data.flags = g_calibrated ? 0x01 : 0x00;

            esp32_send_data(&data);
            last_tx = now;
        }

        loop_count++;
        sleep_ms(1000); /* 1 Hz */
    }

    return 0;
}
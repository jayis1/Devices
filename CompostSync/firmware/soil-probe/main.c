/*
 * CompostSync Soil Probe — Main
 * RP2040 bare-metal C (Pico SDK)
 * firmware/soil-probe/main.c
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/sleep.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"

#include "csp_protocol.h"
#include "sensor_types.h"

/* Pin definitions */
#define PIN_I2C_SDA     0
#define PIN_I2C_SCL     1
#define PIN_DS18B20_1   2
#define PIN_DS18B20_2   3
#define PIN_DS18B20_3   4
#define PIN_DS18B20_4   5
#define PIN_MOISTURE_1  26  /* ADC0 */
#define PIN_MOISTURE_2  27  /* ADC1 */
#define PIN_MOISTURE_3  28  /* ADC2 */
#define PIN_PH_CLK      7
#define PIN_PH_DOUT     8
#define PIN_PH_CS       9
#define PIN_BLE_TX      10
#define PIN_BLE_RX      11
#define PIN_BUTTON      16
#define PIN_LED         17
#define PIN_SCD41_SDA   14  /* I2C #2 via TCA9548A mux */
#define PIN_SCD41_SCL   15

#define OLED_ADDR 0x3C
#define TCA9548A_ADDR 0x70

/* OLED 128x64 */
static uint8_t oled_fb[128 * 8]; /* 8 pages of 128 bytes */

/* Forward declarations */
static void init_hardware(void);
static void read_all_sensors(soil_sensor_set_t *s);
static void oled_init(void);
static void oled_show_readings(const soil_sensor_set_t *s, int page);
static void ble_send_data(const soil_sensor_set_t *s);
static void deep_sleep(uint32_t seconds);

/* Global state */
static int display_page = 0;
static absolute_time_t last_button_press;
static uint32_t uptime_s = 0;

int main(void)
{
    stdio_init_all();
    init_hardware();
    oled_init();

    /* Watchdog: 10 second timeout */
    watchdog_enable(10000, 1);

    soil_sensor_set_t sensors;
    memset(&sensors, 0, sizeof(sensors));
    sensors.co2.co2_ppm = 400; /* default */

    bool awake = true;
    absolute_time_t last_reading = get_absolute_time();
    absolute_time_t last_display_update = get_absolute_time();

    while (1) {
        watchdog_update();

        /* Check button press */
        if (gpio_get(PIN_BUTTON) == 0) {
            absolute_time_t now = get_absolute_time();
            if (absolute_time_diff_us(last_button_press, now) > 200000) {
                last_button_press = now;
                display_page = (display_page + 1) % 4;
                awake = true;
                last_reading = get_absolute_time();
                printf("Button pressed — page %d\n", display_page);
            }
        }

        /* Read sensors every 60 seconds (or immediately after button) */
        if (absolute_time_diff_us(last_reading, get_absolute_time()) > 60000000) {
            read_all_sensors(&sensors);
            sensors.battery_pct = 80; /* simplified */
            uptime_s += 60;

            /* Send via BLE to Bin Node */
            ble_send_data(&sensors);

            /* Update display */
            oled_show_readings(&sensors, display_page);

            last_reading = get_absolute_time();
        }

        /* If awake for >30 seconds with no button, go to sleep */
        if (awake && absolute_time_diff_us(last_button_press, get_absolute_time()) > 30000000) {
            awake = false;
            printf("Going to deep sleep...\n");
            deep_sleep(60);
            last_reading = get_absolute_time(); /* reset after wake */
        }

        sleep_ms(100);
    }
}

static void init_hardware(void)
{
    /* GPIO */
    gpio_init(PIN_BUTTON);
    gpio_set_dir(PIN_BUTTON, GPIO_IN);
    gpio_pull_up(PIN_BUTTON);

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    /* ADC */
    adc_init();
    adc_gpio_init(PIN_MOISTURE_1);
    adc_gpio_init(PIN_MOISTURE_2);
    adc_gpio_init(PIN_MOISTURE_3);

    /* I2C for OLED */
    i2c_init(i2c0, 400000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    /* I2C #2 for SCD41 via mux */
    i2c_init(i2c1, 100000);
    gpio_set_function(PIN_SCD41_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCD41_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SCD41_SDA);
    gpio_pull_up(PIN_SCD41_SCL);

    /* SPI for MCP3201 ADC (pH) */
    spi_init(spi0, 1000000);
    gpio_set_function(PIN_PH_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_PH_DOUT, GPIO_FUNC_SPI);
    gpio_init(PIN_PH_CS);
    gpio_set_dir(PIN_PH_CS, GPIO_OUT);
    gpio_put(PIN_PH_CS, 1);

    /* UART for BLE module (HM-19) */
    uart_init(uart0, 9600);
    gpio_set_function(PIN_BLE_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_BLE_RX, GPIO_FUNC_UART);

    printf("Soil Probe initialized\n");
}

/* ============ DS18B20 OneWire ============ */

static int ow_reset(int pin)
{
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    sleep_us(500);
    gpio_set_dir(pin, GPIO_IN);
    sleep_us(70);
    int presence = (gpio_get(pin) == 0);
    sleep_us(410);
    return presence;
}

static void ow_write_bit(int pin, int bit)
{
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    sleep_us(bit ? 6 : 60);
    gpio_set_dir(pin, GPIO_IN);
    sleep_us(bit ? 64 : 10);
}

static int ow_read_bit(int pin)
{
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    sleep_us(3);
    gpio_set_dir(pin, GPIO_IN);
    sleep_us(10);
    int bit = gpio_get(pin);
    sleep_us(53);
    return bit;
}

static void ow_write_byte(int pin, uint8_t byte)
{
    for (int i = 0; i < 8; i++) ow_write_bit(pin, (byte >> i) & 1);
}

static uint8_t ow_read_byte(int pin)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) byte |= (ow_read_bit(pin) << i);
    return byte;
}

static float read_ds18b20(int pin)
{
    if (!ow_reset(pin)) return -999.0f;
    ow_write_byte(pin, 0xCC);
    ow_write_byte(pin, 0x44);
    sleep_ms(750);
    if (!ow_reset(pin)) return -999.0f;
    ow_write_byte(pin, 0xCC);
    ow_write_byte(pin, 0xBE);
    uint8_t lsb = ow_read_byte(pin);
    uint8_t msb = ow_read_byte(pin);
    int16_t raw = (msb << 8) | lsb;
    return raw / 16.0f;
}

/* ============ Capacitive Moisture ============ */

static uint8_t read_moisture(uint8_t channel)
{
    adc_select_input(channel);
    uint32_t raw = 0;
    for (int i = 0; i < 10; i++) {
        raw += adc_read();
        sleep_ms(10);
    }
    raw /= 10;
    /* Capacitive v1.2: dry ≈ 4000 (RP2040 12-bit), wet ≈ 1800 */
    int moisture = (int)((4000 - raw) * 100 / (4000 - 1800));
    if (moisture < 0) moisture = 0;
    if (moisture > 100) moisture = 100;
    return (uint8_t)moisture;
}

/* ============ pH Probe (via MCP3201 SPI ADC) ============ */

static uint16_t mcp3201_read(void)
{
    gpio_put(PIN_PH_CS, 0);
    sleep_us(1);
    uint8_t tx[2] = {0, 0};
    uint8_t rx[2];
    spi_read_blocking(spi0, 0x00, rx, 2);
    gpio_put(PIN_CS, 1);
    /* MCP3201: 12-bit, data in bits 11:0 of 16-bit read */
    uint16_t val = ((rx[0] & 0x1F) << 8) | rx[1];
    val = val >> 3; /* align to 12 bits */
    return val;
}

static float read_ph(void)
{
    uint16_t adc = mcp3201_read();
    /* pH probe: 0V = pH 0, 3.3V = pH 14
     * ADC 0-4095 → 0-3.3V → pH 0-14 */
    float voltage = adc * 3.3f / 4095.0f;
    float ph = voltage * 14.0f / 3.3f;
    /* Calibration offset (calibrate with buffer solutions) */
    ph += 0.0f; /* offset from calibration */
    if (ph < 0) ph = 0;
    if (ph > 14) ph = 14;
    return ph;
}

/* ============ SCD41 CO2 ============ */

static int scd41_read_i2c1(uint16_t *co2, float *temp, float *hum)
{
    /* Select channel 0 on TCA9548A mux */
    uint8_t mux_cmd = 0x01;
    i2c_write_blocking(i2c1, TCA9548A_ADDR, &mux_cmd, 1, false);

    /* Read measurement */
    uint8_t cmd[2] = { 0xEC, 0x05 };
    uint8_t data[9];
    i2c_write_blocking(i2c1, SCD41_ADDR, cmd, 2, true);
    sleep_ms(100);
    i2c_read_blocking(i2c1, SCD41_ADDR, data, 9, false);

    *co2 = (data[0] << 8) | data[1];
    int16_t t_raw = (data[3] << 8) | data[4];
    int16_t h_raw = (data[6] << 8) | data[7];
    *temp = -45.0f + 175.0f * t_raw / 65535.0f;
    *hum = 100.0f * h_raw / 65535.0f;
    return 0;
}

/* ============ OLED Display ============ */

static void oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    i2c_write_blocking(i2c0, OLED_ADDR, buf, 2, false);
}

static void oled_data(const uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    i2c_write_blocking(i2c0, OLED_ADDR, buf, len + 1, false);
}

static void oled_init(void)
{
    oled_cmd(0xAE); oled_cmd(0xD5); oled_cmd(0x80);
    oled_cmd(0xA8); oled_cmd(0x3F);
    oled_cmd(0xD3); oled_cmd(0x00);
    oled_cmd(0x40); oled_cmd(0x8D); oled_cmd(0x14);
    oled_cmd(0x20); oled_cmd(0x00);
    oled_cmd(0xA1); oled_cmd(0xC8);
    oled_cmd(0xDA); oled_cmd(0x12);
    oled_cmd(0x81); oled_cmd(0xCF);
    oled_cmd(0xD9); oled_cmd(0xF1);
    oled_cmd(0xDB); oled_cmd(0x40);
    oled_cmd(0xA4); oled_cmd(0xA6); oled_cmd(0xAF);
    memset(oled_fb, 0, sizeof(oled_fb));
}

/* Simple 5x8 font */
static const uint8_t font5x8[][5] = {
    [' ']= {0,0,0,0,0}, ['0']= {0x3E,0x51,0x49,0x45,0x3E},
    ['1']= {0,0x42,0x7F,0x40,0}, ['2']= {0x42,0x61,0x51,0x49,0x46},
    ['3']= {0x21,0x41,0x45,0x4B,0x31}, ['4']= {0x18,0x14,0x12,0x7F,0x10},
    ['5']= {0x27,0x45,0x45,0x45,0x39}, ['6']= {0x3C,0x4A,0x49,0x49,0x30},
    ['7']= {0x01,0x71,0x09,0x05,0x03}, ['8']= {0x36,0x49,0x49,0x49,0x36},
    ['9']= {0x06,0x49,0x49,0x29,0x1E}, ['.']= {0,0,0,0x40,0},
    [':']= {0,0x0C,0,0x0C,0}, ['C']= {0x3E,0x41,0x41,0x41,0x22},
    ['D']= {0x7F,0x41,0x41,0x22,0x1C}, ['H']= {0x7F,0x08,0x08,0x08,0x7F},
    ['M']= {0x7F,0x02,0x0C,0x02,0x7F}, ['O']= {0x3E,0x41,0x41,0x41,0x3E},
    ['P']= {0x7F,0x09,0x09,0x09,0x06}, ['S']= {0x46,0x49,0x49,0x49,0x31},
    ['T']= {0x01,0x01,0x7F,0x01,0x01}, ['a']= {0x20,0x54,0x54,0x54,0x78},
    ['b']= {0x7F,0x48,0x44,0x44,0x38}, ['c']= {0x38,0x44,0x44,0x44,0x20},
    ['d']= {0x38,0x44,0x44,0x48,0x7F}, ['e']= {0x38,0x54,0x54,0x54,0x18},
    ['h']= {0x7F,0x08,0x04,0x04,0x78}, ['i']= {0,0x44,0x7D,0x40,0},
    ['l']= {0,0x41,0x7F,0x40,0}, ['m']= {0x7C,0x04,0x18,0x04,0x78},
    ['n']= {0x7C,0x08,0x04,0x04,0x78}, ['o']= {0x38,0x44,0x44,0x44,0x38},
    ['p']= {0x7C,0x14,0x14,0x14,0x08}, ['r']= {0x7C,0x08,0x04,0x04,0x08},
    ['s']= {0x48,0x54,0x54,0x54,0x20}, ['t']= {0x04,0x3F,0x44,0x40,0x20},
    ['u']= {0x3C,0x40,0x40,0x20,0x7C}, ['w']= {0x3C,0x40,0x30,0x40,0x3C},
    ['%']= {0x43,0x4D,0x52,0x54,0x42}, ['p']= {0x7C,0x14,0x14,0x14,0x08},
    ['x']= {0x44,0x28,0x10,0x28,0x44},
};

static void fb_draw_char(int x, int y, char c)
{
    if (c < ' ' || c > 'z') c = ' ';
    const uint8_t *g = font5x8[(uint8_t)c];
    for (int i = 0; i < 5; i++) {
        uint8_t col = g[i];
        for (int j = 0; j < 8; j++) {
            if (col & (1 << j)) {
                int px = x + i, py = y + j;
                if (px < 128 && py < 64)
                    oled_fb[px + (py/8)*128] |= (1 << (py&7));
            }
        }
    }
}

static void fb_draw_text(int x, int y, const char *s)
{
    while (*s) { fb_draw_char(x, y, *s); x += 6; s++; }
}

static void fb_render(void)
{
    oled_cmd(0x21); oled_cmd(0); oled_cmd(127);
    oled_cmd(0x22); oled_cmd(0); oled_cmd(7);
    oled_data(oled_fb, sizeof(oled_fb));
}

static void fb_clear(void) { memset(oled_fb, 0, sizeof(oled_fb)); }

static void oled_show_readings(const soil_sensor_set_t *s, int page)
{
    fb_clear();
    char buf[24];

    switch (page) {
        case 0: /* Temperatures */
            fb_draw_text(0, 0, "Temps (C)");
            snprintf(buf, sizeof(buf), "5cm:  %.1f", s->temp[0].temp_c);
            fb_draw_text(0, 12, buf);
            snprintf(buf, sizeof(buf), "15cm: %.1f", s->temp[1].temp_c);
            fb_draw_text(0, 22, buf);
            snprintf(buf, sizeof(buf), "25cm: %.1f", s->temp[2].temp_c);
            fb_draw_text(0, 32, buf);
            snprintf(buf, sizeof(buf), "35cm: %.1f", s->temp[3].temp_c);
            fb_draw_text(0, 42, buf);
            break;
        case 1: /* Moisture */
            fb_draw_text(0, 0, "Moisture %");
            snprintf(buf, sizeof(buf), "5cm:  %d", s->moisture[0].moisture_pct);
            fb_draw_text(0, 12, buf);
            snprintf(buf, sizeof(buf), "15cm: %d", s->moisture[1].moisture_pct);
            fb_draw_text(0, 22, buf);
            snprintf(buf, sizeof(buf), "25cm: %d", s->moisture[2].moisture_pct);
            fb_draw_text(0, 32, buf);
            break;
        case 2: /* pH + CO2 */
            fb_draw_text(0, 0, "pH & CO2");
            snprintf(buf, sizeof(buf), "pH:   %.2f", s->ph.ph);
            fb_draw_text(0, 12, buf);
            snprintf(buf, sizeof(buf), "CO2:  %d ppm", s->co2.co2_ppm);
            fb_draw_text(0, 22, buf);
            snprintf(buf, sizeof(buf), "Batt: %d%%", s->battery_pct);
            fb_draw_text(0, 32, buf);
            break;
        case 3: /* Status */
            fb_draw_text(0, 0, "CompostSync");
            fb_draw_text(0, 12, "Soil Probe");
            snprintf(buf, sizeof(buf), "Up: %lus", uptime_s);
            fb_draw_text(0, 24, buf);
            fb_draw_text(0, 36, "Press btn:");
            fb_draw_text(0, 46, "cycle page");
            break;
    }
    fb_render();
}

/* ============ BLE UART ============ */

static void ble_send_data(const soil_sensor_set_t *s)
{
    /* Pack into CSP soil_probe_data_t and send via UART to HM-19 */
    soil_probe_data_t data;
    data.node_id = 0x0005; /* Soil Probe ID */
    data.uptime_s = uptime_s;
    data.battery_pct = s->battery_pct;
    for (int i = 0; i < 4; i++)
        data.temp_c[i] = (int16_t)(s->temp[i].temp_c * 10);
    for (int i = 0; i < 3; i++)
        data.moisture_pct[i] = s->moisture[i].moisture_pct;
    data.ph = (int16_t)(s->ph.ph * 100);
    data.co2_ppm = s->co2.co2_ppm;
    data.alerts = 0;

    /* Send as raw bytes over UART to BLE module */
    uart_write_blocking(uart0, (uint8_t *)&data, sizeof(data));
    printf("BLE: sent %d bytes\n", (int)sizeof(data));
}

/* ============ Sensor Reading ============ */

static void read_all_sensors(soil_sensor_set_t *s)
{
    /* Temperatures at 4 depths */
    s->temp[0].temp_c = read_ds18b20(PIN_DS18B20_1);
    s->temp[1].temp_c = read_ds18b20(PIN_DS18B20_2);
    s->temp[2].temp_c = read_ds18b20(PIN_DS18B20_3);
    s->temp[3].temp_c = read_ds18b20(PIN_DS18B20_4);
    for (int i = 0; i < 4; i++) s->temp[i].valid = (s->temp[i].temp_c > -100);

    /* Moisture at 3 depths */
    s->moisture[0].moisture_pct = read_moisture(0); /* ADC0 = GP26 */
    s->moisture[1].moisture_pct = read_moisture(1); /* ADC1 = GP27 */
    s->moisture[2].moisture_pct = read_moisture(2); /* ADC2 = GP28 */
    for (int i = 0; i < 3; i++) s->moisture[i].valid = true;

    /* pH */
    s->ph.ph = read_ph();
    s->ph.valid = (s->ph.ph > 0 && s->ph.ph <= 14);

    /* CO2 from SCD41 */
    float co2_t, co2_h;
    scd41_read_i2c1(&s->co2.co2_ppm, &co2_t, &co2_h);
    s->co2.valid = (s->co2.co2_ppm > 0);

    printf("Readings: T=%.1f-%1.f pH=%.2f CO2=%d M=%d/%d/%d\n",
           s->temp[0].temp_c, s->temp[3].temp_c, s->ph.ph,
           s->co2.co2_ppm,
           s->moisture[0].moisture_pct, s->moisture[1].moisture_pct,
           s->moisture[2].moisture_pct);
}

/* ============ Deep Sleep ============ */

static void deep_sleep(uint32_t seconds)
{
    /* RP2040 doesn't have true deep sleep like ESP32, but we can
     * use sleep with WFE and a timer. For low power, disable peripherals. */
    gpio_put(PIN_LED, 0);

    /* Sleep using WFE for the specified duration */
    uint32_t sleep_ms_total = seconds * 1000;
    uint32_t sleep_start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - sleep_start < sleep_ms_total) {
        if (gpio_get(PIN_BUTTON) == 0) break; /* wake on button */
        sleep_ms(1000);
    }

    gpio_put(PIN_LED, 1);
}
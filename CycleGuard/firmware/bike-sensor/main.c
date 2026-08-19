/*
 * CycleGuard Bike Sensor Firmware
 * Target: RP2040 (sensor processing) + nRF52840 (BLE 5.0 radio)
 *
 * Wheel speed (Hall effect A1304) + cadence (crank Hall) + tire pressure (TPMS).
 * Interrupt-driven for ultra-low power (12-month coin cell).
 * RP2040 handles sensors; nRF52840 handles BLE 5.0 to Hub via UART bridge.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/sleep.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

#include "../common/protocol.h"

#define TAG "BIKE_SENSOR"

/* ---- Pin definitions (RP2040) ---- */
#define PIN_WHEEL_HALL   0
#define PIN_CRANK_HALL   1
#define PIN_UART_TX      2   /* to nRF52840 UART RX */
#define PIN_UART_RX      3   /* to nRF52840 UART TX */
#define PIN_TPMS_IRQ     4
#define PIN_LED_STATUS   5
#define PIN_BTN_PAIR     6
#define PIN_BAT_SENSE    7   /* ADC */
#define PIN_SPI_CS       8   /* external flash */

/* ---- Constants ---- */
#define WHEEL_CIRCUMFERENCE_M   2.105f  /* 700c road wheel */
#define UART_BAUD               1000000
#define DEBOUNCE_MS             50

/* ---- Sensor state ---- */
static float    g_speed_kmh       = 0.0f;
static uint8_t  g_cadence_rpm     = 0;
static float    g_tire_pressure   = 90.0f;
static float    g_tire_temp_c     = 25.0f;
static uint8_t  g_battery_pct     = 100;

/* ---- Timestamps for speed/cadence ---- */
static absolute_time_t g_last_wheel_time;
static absolute_time_t g_last_crank_time;
static bool g_wheel_first = true;
static bool g_crank_first = true;

/* ---- Speed smoothing (5-sample moving average) ---- */
#define SPEED_AVG_N 5
static float g_speed_samples[SPEED_AVG_N];
static int   g_speed_idx = 0;

/* ---- Wheel speed interrupt ---- */
static void wheel_hall_isr(void)
{
    absolute_time_t now = get_absolute_time();

    if (g_wheel_first) {
        g_wheel_first = false;
        g_last_wheel_time = now;
        return;
    }

    uint64_t delta_us = absolute_time_diff_us(g_last_wheel_time, now);
    g_last_wheel_time = now;

    if (delta_us > DEBOUNCE_MS * 1000) {
        /* Speed = circumference / time */
        float speed_ms = WHEEL_CIRCUMFERENCE_M / (delta_us / 1000000.0f);
        float speed_kmh = speed_ms * 3.6f;

        /* Moving average */
        g_speed_samples[g_speed_idx] = speed_kmh;
        g_speed_idx = (g_speed_idx + 1) % SPEED_AVG_N;

        float sum = 0;
        for (int i = 0; i < SPEED_AVG_N; i++) sum += g_speed_samples[i];
        g_speed_kmh = sum / SPEED_AVG_N;
    }
}

/* ---- Cadence interrupt ---- */
static void crank_hall_isr(void)
{
    absolute_time_t now = get_absolute_time();

    if (g_crank_first) {
        g_crank_first = false;
        g_last_crank_time = now;
        return;
    }

    uint64_t delta_us = absolute_time_diff_us(g_last_crank_time, now);
    g_last_crank_time = now;

    if (delta_us > DEBOUNCE_MS * 1000) {
        /* Cadence = 60 / (delta seconds) */
        float delta_s = delta_us / 1000000.0f;
        g_cadence_rpm = (uint8_t)(60.0f / delta_s);
    }
}

/* ---- TPMS data received (from nRF52840 2.4 GHz receiver) ---- */
/* nRF52840 receives 2.4 GHz TPMS broadcast, forwards via UART */
static void parse_tpms(const uint8_t *data, int len)
{
    if (len < 8) return;
    /* TPMS packet: [id(2)] [pressure(2)] [temp(2)] [flags(1)] [crc(1)] */
    uint16_t pressure_raw = (data[2] << 8) | data[3];
    uint16_t temp_raw = (data[4] << 8) | data[5];

    /* Convert: pressure in 0.1 PSI units, temp in 0.1 °C */
    g_tire_pressure = pressure_raw / 10.0f;
    g_tire_temp_c = temp_raw / 10.0f;
}

/* ---- UART communication with nRF52840 ---- */
static void uart_send_sensor_data(void)
{
    bike_sensor_payload_t payload;
    payload.speed_kmh       = g_speed_kmh;
    payload.cadence_rpm     = g_cadence_rpm;
    payload.tire_pressure_psi = g_tire_pressure;
    payload.tire_temp_c     = g_tire_temp_c;
    payload.battery_pct     = g_battery_pct;

    /* Send via UART to nRF52840 for BLE transmission */
    uint8_t header[4] = {0xC5, 0xC5, 0x01, sizeof(payload)};
    uart_write_blocking(uart0, header, 4);
    uart_write_blocking(uart0, (const uint8_t *)&payload, sizeof(payload));
}

/* ---- Main ---- */
int main(void)
{
    stdio_init_all();
    printf("CycleGuard Bike Sensor starting...\n");

    /* GPIO init */
    gpio_init(PIN_WHEEL_HALL);
    gpio_set_dir(PIN_WHEEL_HALL, GPIO_IN);
    gpio_pull_up(PIN_WHEEL_HALL);

    gpio_init(PIN_CRANK_HALL);
    gpio_set_dir(PIN_CRANK_HALL, GPIO_IN);
    gpio_pull_up(PIN_CRANK_HALL);

    gpio_init(PIN_LED_STATUS);
    gpio_set_dir(PIN_LED_STATUS, GPIO_OUT);

    gpio_init(PIN_BTN_PAIR);
    gpio_set_dir(PIN_BTN_PAIR, GPIO_IN);
    gpio_pull_up(PIN_BTN_PAIR);

    /* Interrupts */
    gpio_set_irq_enabled_with_callback(PIN_WHEEL_HALL, GPIO_IRQ_EDGE_FALL,
                                        true, wheel_hall_isr);
    gpio_set_irq_enabled(PIN_CRANK_HALL, GPIO_IRQ_EDGE_FALL, true);
    /* Note: in production, use separate callbacks or check pin in shared callback */

    /* UART to nRF52840 */
    uart_init(uart0, UART_BAUD);
    gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);

    printf("Bike Sensor initialized — wheel speed + cadence + TPMS\n");

    /* Main loop: report sensor data every 2 seconds */
    while (true) {
        sleep_ms(2000);

        /* If no wheel pulses for 5 seconds, speed = 0 (stopped) */
        if (!g_wheel_first) {
            absolute_time_t now = get_absolute_time();
            uint64_t since_last = absolute_time_diff_us(g_last_wheel_time, now);
            if (since_last > 5000000) {  /* 5 seconds */
                g_speed_kmh = 0.0f;
            }
        }

        /* If no crank pulses for 5 seconds, cadence = 0 */
        if (!g_crank_first) {
            absolute_time_t now = get_absolute_time();
            uint64_t since_last = absolute_time_diff_us(g_last_crank_time, now);
            if (since_last > 5000000) {
                g_cadence_rpm = 0;
            }
        }

        /* Low tire pressure alert */
        if (g_tire_pressure > 0 && g_tire_pressure < 60.0f) {
            printf("LOW TIRE PRESSURE: %.1f PSI\n", g_tire_pressure);
            gpio_put(PIN_LED_STATUS, 1);
            sleep_ms(100);
            gpio_put(PIN_LED_STATUS, 0);
        }

        /* Send sensor data to nRF52840 for BLE transmission */
        uart_send_sensor_data();

        printf("Speed: %.1f km/h  Cadence: %d RPM  Tire: %.1f PSI  Batt: %d%%\n",
               g_speed_kmh, g_cadence_rpm, g_tire_pressure, g_battery_pct);
    }

    return 0;
}
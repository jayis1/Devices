/**
 * MigraineSync — Hydrate Tag Load Cell (HX711)
 * ============================================
 * 24-bit load cell ADC driver for water bottle weight measurement.
 * Bit-bang interface: SCK (output) + DOUT (input).
 *
 * License: MIT
 */

#include "loadcell.h"
#include "config.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(migrainesync_loadcell, LOG_LEVEL_INF);

static const struct gpio_dt_spec sck =
    GPIO_DT_SPEC_GET(DT_NODELABEL(hx711_sck), gpios);
static const struct gpio_dt_spec dout =
    GPIO_DT_SPEC_GET(DT_NODELABEL(hx711_dout), gpios);

static int32_t s_tare_offset = HX711_TARE_COUNT;

int loadcell_init(void)
{
    if (!device_is_ready(sck.port) || !device_is_ready(dout.port))
        return -1;

    gpio_pin_configure_dt(&sck, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&dout, GPIO_INPUT);

    /* Power up: SCK low */
    gpio_pin_set_dt(&sck, 0);
    k_msleep(100);  /* HX711 settle time after power-up */

    LOG_INF("HX711 load cell initialized (SCK=%d DOUT=%d)", sck.pin, dout.pin);
    return 0;
}

int32_t loadcell_read_raw(void)
{
    /* Wait for DOUT to go low (data ready) — max 1 second */
    int timeout = 1000;
    while (gpio_pin_get_dt(&dout) == 1) {
        k_msleep(1);
        if (--timeout <= 0) {
            LOG_WRN("HX711 timeout (DOUT not ready)");
            return 0;
        }
    }

    /* Read 24 bits: pulse SCK 24 times, read DOUT each time */
    int32_t count = 0;
    for (int i = 0; i < 24; i++) {
        gpio_pin_set_dt(&sck, 1);
        k_us(1);  /* min 0.2 µs high */
        count = (count << 1) | gpio_pin_get_dt(&dout);
        gpio_pin_set_dt(&sck, 0);
        k_us(1);
    }

    /* 25th pulse sets channel A gain 128 (back to default) */
    gpio_pin_set_dt(&sck, 1);
    k_us(1);
    gpio_pin_set_dt(&sck, 0);

    /* Convert 24-bit two's complement to int32_t */
    if (count & 0x800000)
        count |= 0xFF000000;  /* sign extend */

    return count;
}

float loadcell_read_grams(void)
{
    int32_t raw = loadcell_read_raw();
    if (raw == 0)
        return -1.0f;

    int32_t net = raw - s_tare_offset;
    return (float)net / HX711_SCALE_FACTOR;
}

void loadcell_tare(void)
{
    /* Take 5 readings and average for stable tare */
    int64_t sum = 0;
    for (int i = 0; i < 5; i++) {
        int32_t raw = loadcell_read_raw();
        if (raw != 0)
            sum += raw;
        k_msleep(100);
    }
    s_tare_offset = (int32_t)(sum / 5);
    LOG_INF("HX711 tared: offset=%ld", (long)s_tare_offset);
}

void loadcell_power_down(void)
{
    /* HX711 enters power-down when SCK is held high >60 µs */
    gpio_pin_set_dt(&sck, 1);
    k_us(100);
}

void loadcell_power_up(void)
{
    /* SCK low wakes HX711 */
    gpio_pin_set_dt(&sck, 0);
    k_msleep(100);  /* settle after wake */
}
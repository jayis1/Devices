/*
 * BrewSync Cellar Monitor - Main Firmware
 * STM32L476RG, ultra-low-power
 *
 * Monitors: temp, humidity, barometric pressure, vibration, light
 * Reports via Sub-GHz SX1262 to Hub every 10 minutes
 *
 * Copyright (c) 2025 BrewSync. MIT License.
 */

#include <string.h>
#include "stm32l4xx.h"
#include "bsmp.h"
#include "bsmp_sensors.h"

/* ---- Configuration ---- */
#define FW_VERSION          "1.2.0"
#define REPORT_INTERVAL_MS  (10UL * 60UL * 1000UL)  /* 10 minutes */
#define VIBRATION_THRESHOLD 50.0f  /* mg RMS for movement alert */

/* ---- Global state ---- */
static struct {
    uint16_t node_addr;
    uint8_t  seq;
    uint8_t  aes_key[16];
    bool     paired;
    uint32_t last_report_ms;
} g;

static bsmp_hal_t hal;

/* ---- Sensor reading ---- */
static int read_cellar_sensors(bsmp_cellar_telem_t *telem) {
    sht40_t  sht;
    bmp390_t bmp;
    float    vib_rms = 0.0f;
    uint16_t light_lux = 0;
    uint16_t battery_mv;
    int rc;

    memset(telem, 0, sizeof(*telem));
    telem->timestamp = hal_get_ticks() / 1000;

    /* Temperature & humidity */
    rc = sht40_read(&hal, 0, &sht);
    if (rc == 0 && sht.valid) {
        telem->temp_c = sht.temp_c;
        telem->humidity_rh = sht.humidity_rh;
    }

    /* Barometric pressure */
    rc = bmp390_read(&hal, 0, &bmp);
    if (rc == 0 && bmp.valid) {
        telem->pressure_hpa = bmp.pressure_hpa;
    }

    /* Vibration - LIS2DH12 */
    /* Read 10 samples over 100ms and compute RMS */
    float sum_sq = 0.0f;
    for (int i = 0; i < 10; i++) {
        /* Simplified: read single-axis acceleration */
        uint16_t raw;
        hal.adc_read(1, &raw); /* Accelerometer analog out or SPI */
        float accel_mg = (raw - 2048) * 2.0f / 4096.0f; /* ±2g range */
        sum_sq += accel_mg * accel_mg;
        hal.delay_ms(10);
    }
    vib_rms = sqrtf(sum_sq / 10.0f);
    telem->vibration_rms_mg = vib_rms;

    /* Light - TSL2591 */
    /* Simplified: read from I2C */
    telem->light_lux = light_lux; /* Would come from TSL2591 driver */

    /* Battery */
    hal.adc_read(0, &battery_mv);
    telem->battery_mv = battery_mv;

    /* Flags */
    telem->flags = 0;
    if (vib_rms > VIBRATION_THRESHOLD)
        telem->flags |= 0x01; /* Movement detected */
    if (telem->temp_c < 0.0f || telem->temp_c > 40.0f)
        telem->flags |= 0x02; /* Temp excursion */
    if (battery_mv < 3300)
        telem->flags |= 0x04; /* Low battery */
    if (telem->light_lux > 200)
        telem->flags |= 0x08; /* UV exposure warning */

    return 0;
}

/* ---- Send telemetry ---- */
static int send_telemetry(const bsmp_cellar_telem_t *telem) {
    uint8_t buf[64];
    int len = bsmp_encode(BSMP_ADDR_HUB, g.seq++, BSMP_TYPE_TELEMETRY,
                          (const uint8_t *)telem, sizeof(*telem),
                          buf, sizeof(buf));
    if (len < 0) return len;
    return sx1262_send(&hal, buf, (uint8_t)len);
}

/* ---- Main ---- */
int main(void) {
    bsmp_cellar_telem_t telem;
    sx1262_rx_info_t rx_info;

    SystemInit();
    /* init_hal() would set up I2C, SPI, ADC, GPIO */

    /* Init sensors */
    sht40_init(&hal, 0);
    bmp390_init(&hal, 0);
    /* LIS2DH12 and TSL2591 init would go here */

    /* Init radio */
    sx1262_init(&hal, 0, 0, 0, 0, 0);
    sx1262_config(&rx_info, 9, 125, 5, 868000000, 14); /* SF9 for cellar range */

    g.node_addr = 0x0101; /* Default cellar address */
    g.seq = 0;

    while (1) {
        uint32_t now = hal_get_ticks();

        /* Read sensors */
        read_cellar_sensors(&telem);

        /* Send telemetry */
        if ((now - g.last_report_ms) >= REPORT_INTERVAL_MS) {
            send_telemetry(&telem);
            g.last_report_ms = now;
        }

        /* Check for incoming commands */
        bsmp_frame_t frame;
        int rc = sx1262_receive(&hal, (uint8_t *)&frame, sizeof(frame),
                                &rx_info, 100);
        if (rc > 0 && rx_info.crc_ok) {
            if (frame.type == BSMP_TYPE_COMMAND && frame.len > 0) {
                bsmp_command_t cmd;
                memcpy(&cmd, frame.payload, sizeof(cmd));
                if (cmd.cmd == BSMP_CMD_SET_REPORT_INTERVAL && cmd.param_len >= 2) {
                    /* Update report interval */
                }
            }
        }

        /* Deep sleep until next reading */
        hal_enter_lowpower(REPORT_INTERVAL_MS);
    }
}
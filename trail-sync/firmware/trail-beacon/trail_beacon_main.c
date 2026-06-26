/*
 * trail_beacon_main.c — TrailSync Trail Beacon firmware (nRF52833, nRF5 SDK)
 *
 * The trail intelligence. Solar-powered, weatherproof modules at trail junctions.
 * Broadcasts GPS position, trail difficulty, water availability, hazard conditions.
 * Relays LoRa messages for backcountry SOS. Reports temperature, humidity, PIR motion.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_spi.h"
#include "app_timer.h"
#include "nrf_saadc.h"
#include "trail_protocol.h"

/* ---- Configuration (set at install time via mobile app) ---- */
typedef struct {
    int32_t  lat_deg1e5;        /* beacon latitude (set once at install) */
    int32_t  lon_deg1e5;        /* beacon longitude */
    int16_t  altitude_dm;       /* beacon altitude */
    uint8_t  trail_difficulty;  /* TS_TRAIL_* */
    uint8_t  water_available;   /* 0=no 1=seasonal 2=reliable */
    uint16_t dist_trailhead_m;  /* distance to trailhead */
    uint16_t dist_next_water_m; /* distance to next water */
    uint8_t  hazard_flags;      /* current hazard bitmask */
    char     trail_name[32];    /* human-readable trail name */
    uint8_t  beacon_id;         /* unique beacon ID */
} beacon_config_t;

static beacon_config_t config = {
    .lat_deg1e5 = 0,
    .lon_deg1e5 = 0,
    .altitude_dm = 0,
    .trail_difficulty = TS_TRAIL_MODERATE,
    .water_available = 1,
    .dist_trailhead_m = 5000,
    .dist_next_water_m = 2000,
    .hazard_flags = 0,
    .trail_name = "Trail Junction",
    .beacon_id = 0x40,
};

/* ---- BME280 (temperature, humidity, pressure via I2C) ---- */
#define BME280_ADDR 0x76

static int16_t temperature_centic = 2200;   /* 22.0°C */
static uint16_t humidity_pct = 45;           /* 45% */
static uint16_t pressure_hpa = 101325;       /* 1013.25 hPa * 10 */

static void bme280_read(void)
{
    /* In production: read BME280 via I2C
     * Returns temperature in centi-degC, humidity in %, pressure in hPa * 10 */
    printf("BME280: temp=%.1f°C humidity=%u%% pressure=%.1fhPa\n",
           temperature_centic / 100.0f, humidity_pct, pressure_hpa / 10.0f);
}

/* ---- PIR motion sensor (AM312) ---- */
static uint8_t pir_event_count = 0;

static void pir_handler(void)
{
    /* PIR interrupt: wildlife or hiker detected */
    pir_event_count++;
    printf("PIR: motion detected (total=%u)\n", pir_event_count);
}

/* ---- Solar power management (MCP73871) ---- */
static uint8_t battery_pct = 100;
static uint8_t solar_charging = 1;

static void power_manage(void)
{
    /* In production: read battery voltage via ADC, solar panel voltage
     * Manage power modes based on battery level:
     * > 30%: full beacon (Sub-GHz + LoRa + GPS + sensors)
     * 15-30%: reduced (Sub-GHz + LoRa only, 10-min intervals)
     * < 15%: emergency (LoRa receive only, 30-min intervals) */
    if (battery_pct < 15) {
        printf("POWER: battery low (%u%%), emergency mode\n", battery_pct);
    } else if (battery_pct < 30) {
        printf("POWER: battery moderate (%u%%), reduced mode\n", battery_pct);
    } else {
        printf("POWER: battery OK (%u%%), full mode\n", battery_pct);
    }
}

/* ---- Beacon broadcast (Sub-GHz, every 60s) ---- */
static void beacon_broadcast_subghz(void)
{
    ts_nav_payload_t nav;
    memset(&nav, 0, sizeof(nav));
    nav.type = TS_MSG_NAV;
    nav.node_id = config.beacon_id;
    nav.trail_difficulty = config.trail_difficulty;
    nav.lat_deg1e5 = config.lat_deg1e5;
    nav.lon_deg1e5 = config.lon_deg1e5;
    nav.altitude_dm = config.altitude_dm;
    nav.water_available = config.water_available;
    nav.hazard_flags = config.hazard_flags;
    nav.dist_trailhead_m = config.dist_trailhead_m;
    nav.dist_next_water_m = config.dist_next_water_m;
    nav.cell_signal = 0; /* updated by hub if in range */
    ts_pack_crc(&nav, sizeof(nav) - sizeof(uint16_t));
    ts_mesh_send(TS_MSG_NAV, config.beacon_id, &nav, sizeof(nav));
}

/* ---- Beacon conditions broadcast (LoRa, every 5 min) ---- */
static void beacon_broadcast_lora(void)
{
    ts_beacon_data_payload_t bd;
    memset(&bd, 0, sizeof(bd));
    bd.type = TS_MSG_BEACON_DATA;
    bd.node_id = config.beacon_id;
    bd.lat_deg1e5 = config.lat_deg1e5;
    bd.lon_deg1e5 = config.lon_deg1e5;
    bd.altitude_dm = config.altitude_dm;
    bd.temp_centic = temperature_centic;
    bd.humidity_pct = humidity_pct;
    bd.pressure_hpa = pressure_hpa;
    bd.battery_pct = battery_pct;
    bd.pir_events = pir_event_count;
    bd.hazard_flags = config.hazard_flags;
    bd.trail_difficulty = config.trail_difficulty;
    ts_pack_crc(&bd, sizeof(bd) - sizeof(uint16_t));
    ts_lora_send(TS_MSG_BEACON_DATA, config.beacon_id, &bd, sizeof(bd));

    /* Reset PIR counter after broadcast */
    pir_event_count = 0;
}

/* ---- LoRa relay handler ---- */
static void lora_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    /* Relay SOS and other messages from wrist units toward hub */
    if (type == TS_MSG_SOS) {
        printf("LoRa: relaying SOS from node 0x%02X\n", data[1]);
        /* Forward to next beacon toward cell coverage */
        ts_lora_send(type, 0xFF, data, len); /* broadcast relay */
    } else if (type == TS_MSG_SOS_ACK) {
        printf("LoRa: SOS ACK received, forwarding to mesh\n");
        ts_mesh_send(type, 0x00, data, len);
    }
}

/* ---- Mesh RX callback ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    /* Handle trail condition updates from hub */
    if (type == TS_MSG_TRAIL_COND) {
        if (len < sizeof(ts_trail_cond_payload_t) - 2) return;
        const ts_trail_cond_payload_t *p = (const ts_trail_cond_payload_t *)data;
        if (p->beacon_id == 0xFF || p->beacon_id == config.beacon_id) {
            config.trail_difficulty = p->trail_difficulty;
            config.hazard_flags = p->hazard_flags;
            config.water_available = p->water_available;
            printf("Trail condition update: difficulty=%s hazards=0x%02X water=%u\n",
                   ts_trail_diff_name(p->trail_difficulty),
                   p->hazard_flags, p->water_available);
        }
    }
}

/* ---- GPS position acquisition (at install time) ---- */
static void acquire_gps_position(void)
{
    /* At install time, acquire GPS fix and store as beacon position
     * This is done once — the beacon doesn't move after installation */
    printf("GPS: acquiring position for beacon install...\n");
    /* In production: u-blox SAM-M10Q via UART
     * Wait for 3D fix (at least 6 satellites), average 100 readings */
    config.lat_deg1e5 = 4000000;  /* placeholder: 40.00000°N */
    config.lon_deg1e5 = -105000000; /* placeholder: 105.00000°W */
    config.altitude_dm = 3000;     /* placeholder: 300m */
    printf("GPS: position locked lat=%d lon=%d alt=%dm\n",
           config.lat_deg1e5, config.lon_deg1e5, config.altitude_dm / 10);
}

/* ---- Main ---- */
int main(void)
{
    printf("TrailSync Trail Beacon starting (ID=0x%02X)...\n", config.beacon_id);

    /* Acquire GPS position at install */
    acquire_gps_position();

    /* Initialize mesh and LoRa */
    ts_mesh_set_rx_callback(mesh_rx_handler);
    ts_lora_set_tx(NULL); /* set by RFM95W driver init */

    /* Initialize sensors */
    bme280_read();

    uint32_t last_beacon_subghz_ms = 0;
    uint32_t last_beacon_lora_ms = 0;
    uint32_t last_sensor_ms = 0;

    while (1) {
        uint32_t now = 0; /* app_timer_ms(); */

        /* Read sensors every 60 seconds */
        if (now - last_sensor_ms > 60000) {
            last_sensor_ms = now;
            bme280_read();
            power_manage();
        }

        /* Sub-GHz beacon broadcast every 60 seconds */
        if (now - last_beacon_subghz_ms > 60000) {
            last_beacon_subghz_ms = now;
            beacon_broadcast_subghz();
        }

        /* LoRa conditions broadcast every 5 minutes */
        if (now - last_beacon_lora_ms > 300000) {
            last_beacon_lora_ms = now;
            beacon_broadcast_lora();
        }

        /* Poll mesh UART for incoming frames */
        /* In production: poll nRF52833 UART for Sub-GHz data */
        /* In production: poll RFM95W SPI for LoRa data */

        nrf_delay_ms(100); /* 10 Hz main loop */
    }

    return 0;
}
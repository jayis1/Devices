/*
 * wrist_main.c — TrailSync Wrist Unit firmware (nRF52832, nRF5 SDK)
 *
 * The command center. Measures heart rate, HRV, SpO2, altitude, GPS position.
 * Runs on-device fall detection, altitude sickness screening, storm prediction.
 * Displays navigation, pace, and alerts on OLED. Coordinates with Shoe Pods
 * via Sub-GHz mesh and Trail Beacons via LoRa. Triggers SOS on fall + stillness.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "app_timer.h"
#include "trail_protocol.h"

/* ---- Per-pod gait state (left + right) ---- */
#define MAX_PODS 2

typedef struct {
    uint8_t  active;
    uint8_t  side;              /* TS_POD_LEFT or TS_POD_RIGHT */
    uint8_t  gait_class;        /* 0=normal 1=asymmetric 2=overpronating 3=high-impact */
    uint8_t  gait_conf;
    int16_t  cadence;           /* steps/min * 10 */
    int16_t  ground_contact_ms;
    int16_t  vertical_osc_mm;
    int16_t  impact_load_pct;
    int16_t  pronation_deg;
    int16_t  asymmetry_pct;
    int16_t  stride_length_cm;
    int8_t   terrain;           /* TS_TERRAIN_* */
    uint8_t  battery_pct;
    int64_t  last_gait_ms;
} pod_state_t;

static pod_state_t pods[MAX_PODS];

/* ---- Wrist sensor state ---- */
typedef struct {
    int32_t  lat_deg1e5;
    int32_t  lon_deg1e5;
    int16_t  altitude_dm;
    int16_t  speed_cm_s;
    uint16_t distance_dm;
    uint8_t  hr;
    uint8_t  spo2;
    int16_t  hrv_rmssd;
    int16_t  skin_temp_centic;
    int16_t  pressure_hpa;
    uint8_t  num_sats;
    int16_t  pressure_history[36];  /* 3 hours at 5-min intervals */
    uint8_t  pressure_idx;
    float    hrv_baseline;
    float    hrv_current;
    uint8_t  fall_detected;
    uint8_t  stillness_count;
    uint8_t  sos_active;
} wrist_state_t;

static wrist_state_t ws;

/* ---- GPS driver (u-blox SAM-M10Q via UART) ---- */
#define GPS_UART  NRF_UART0
static void gps_init(void)
{
    /* Initialize UART at 9600 baud for u-blox */
    /* Configure NMEA parsing for GGA + RMC sentences */
    printf("GPS: initializing SAM-M10Q...\n");
    ws.num_sats = 0;
    ws.lat_deg1e5 = 0;
    ws.lon_deg1e5 = 0;
}

static void gps_poll(void)
{
    /* In production: parse NMEA sentences from UART
     * Extract lat, lon, altitude, speed, number of satellites
     * This is a simplified simulation */
    printf("GPS: sats=%u lat=%d lon=%d alt=%dm speed=%dcm/s\n",
           ws.num_sats, ws.lat_deg1e5, ws.lon_deg1e5,
           ws.altitude_dm / 10, ws.speed_cm_s);
}

/* ---- Barometric altimeter (BMP390 via I2C) ---- */
#define BMP390_ADDR 0x77

static int16_t bmp390_read_pressure(void)
{
    /* In production: read pressure via I2C from BMP390
     * Returns pressure in hPa * 10 */
    return 101325; /* default: 1013.25 hPa at sea level */
}

static int16_t bmp390_pressure_to_alt_dm(int16_t pressure_hpa)
{
    /* Convert pressure to altitude using barometric formula
     * altitude = 44330 * (1 - (P/P0)^0.1903)
     * Returns altitude in decimeters */
    float p = (float)pressure_hpa / 10.0f;
    float alt_m = 44330.0f * (1.0f - powf(p / 1013.25f, 0.1903f));
    return (int16_t)(alt_m * 10.0f);
}

static void pressure_storm_check(void)
{
    /* Record pressure every 5 minutes for 3 hours (36 samples)
     * If pressure drops > 4 hPa (40 in our units) in 3 hours → storm alert */
    ws.pressure_history[ws.pressure_idx] = ws.pressure_hpa;
    ws.pressure_idx = (ws.pressure_idx + 1) % 36;

    /* Find pressure 3 hours ago (or oldest available) */
    int16_t oldest = ws.pressure_history[(ws.pressure_idx) % 36];
    int16_t delta = ws.pressure_hpa - oldest;

    if (delta < -40) { /* > 4 hPa drop */
        printf("⚠ STORM ALERT: pressure dropped %.1f hPa in 3 hours\n",
               (float)delta / 10.0f);
        ts_send_storm_alert(1, ws.pressure_hpa, delta,
                           2); /* estimate 2 hours */
    }
}

/* ---- PPG / SpO2 (MAX30101 via I2C) ---- */
#define MAX30101_ADDR 0x57

static void ppg_init(void)
{
    printf("PPG: initializing MAX30101...\n");
    ws.hr = 0;
    ws.spo2 = 0;
    ws.hrv_baseline = 50.0f; /* default RMSSD baseline in ms */
}

static void ppg_read(void)
{
    /* In production: read PPG FIFO from MAX30101
     * Compute HR from peak detection, HRV from R-R intervals
     * Compute SpO2 from red/IR ratio */
    printf("PPG: HR=%u bpm SpO2=%u%% HRV=%.1f ms\n",
           ws.hr, ws.spo2, ws.hrv_current);
}

static void altitude_sickness_check(void)
{
    /* AMS screening:
     * SpO2 < 94% + HRV drop > 20% + ascent > 500m/hr → AMS risk */
    if (ws.spo2 < TS_SPO2_AMS_THRESHOLD) {
        float hrv_drop_pct = 0.0f;
        if (ws.hrv_baseline > 0) {
            hrv_drop_pct = ((ws.hrv_baseline - ws.hrv_current) / ws.hrv_baseline) * 100.0f;
        }

        if (ws.spo2 < TS_SPO2_HACE_THRESHOLD) {
            printf("🚨 CRITICAL: SpO2 %u%% — HACE/HAPE risk! Descend immediately!\n",
                   ws.spo2);
            /* Auto-trigger SOS for critical altitude sickness */
            ts_send_sos(TS_SOS_ALTITUDE, 3 /* critical */,
                        ws.lat_deg1e5, ws.lon_deg1e5, ws.altitude_dm,
                        ws.hr, ws.spo2, (int16_t)(ws.hrv_current * 10),
                        0, 1, 0);
        } else if (hrv_drop_pct > TS_HRV_DROP_AMS) {
            printf("⚠ AMS WARNING: SpO2 %u%% + HRV drop %.0f%% — slow ascent or descend\n",
                   ws.spo2, hrv_drop_pct);
        }
    }
}

/* ---- IMU (LSM6DSL) for fall detection ---- */
static int16_t imu_ax, imu_ay, imu_az;

static void imu_read(void)
{
    /* In production: read 3D accel from LSM6DSL via I2C
     * Compute acceleration magnitude */
    float mag = sqrtf((float)imu_ax * imu_ax + (float)imu_ay * imu_ay +
                      (float)imu_az * imu_az);

    /* Fall detection: acceleration spike > 8G */
    if (mag > 8.0f * 16384.0f) { /* 8G in ±16G range, 16384 LSB/G */
        printf("⚠ FALL DETECTED: acceleration %.1f G\n", mag / 16384.0f);
        ws.fall_detected = 1;
        ws.stillness_count = 0;
    }

    /* After fall: check for stillness (no movement for 10 seconds) */
    if (ws.fall_detected) {
        if (mag < 0.5f * 16384.0f) { /* near-zero movement */
            ws.stillness_count++;
            if (ws.stillness_count > 10) { /* 10 * 1s = 10s stillness */
                printf("🚨 SOS: Fall + stillness detected! Triggering emergency SOS\n");
                ts_send_sos(TS_SOS_FALL_AUTO, 2 /* serious */,
                            ws.lat_deg1e5, ws.lon_deg1e5, ws.altitude_dm,
                            ws.hr, ws.spo2, (int16_t)(ws.hrv_current * 10),
                            0, 1, 0);
                ws.fall_detected = 0;
                ws.sos_active = 1;
            }
        } else {
            /* Movement detected after fall — person is OK */
            ws.fall_detected = 0;
            ws.stillness_count = 0;
        }
    }
}

/* ---- OLED display (SH1106 via SPI) ---- */
static void display_pace_screen(void)
{
    printf("[OLED] Pace: %.1f min/km  HR: %u  Alt: %dm  Dist: %.1fkm\n",
           (ws.speed_cm_s > 0) ? 100000.0f / (ws.speed_cm_s * 60.0f) : 0,
           ws.hr, ws.altitude_dm / 10, ws.distance_dm / 10000.0f);
}

static void display_nav_screen(void)
{
    printf("[OLED] Lat:%d Lon:%d Sats:%u  SpO2:%u%%  HRV:%.0fms\n",
           ws.lat_deg1e5, ws.lon_deg1e5, ws.num_sats,
           ws.spo2, ws.hrv_current);
}

static void display_alert_screen(const char *msg)
{
    printf("[OLED] ⚠ ALERT: %s\n", msg);
}

/* ---- Gait asymmetry computation from left/right pods ---- */
static void compute_gait_asymmetry(void)
{
    pod_state_t *left = NULL, *right = NULL;
    for (int i = 0; i < MAX_PODS; i++) {
        if (!pods[i].active) continue;
        if (pods[i].side == TS_POD_LEFT)  left = &pods[i];
        if (pods[i].side == TS_POD_RIGHT) right = &pods[i];
    }

    if (left && right) {
        /* L/R asymmetry from ground contact time difference */
        int16_t contact_diff = left->ground_contact_ms - right->ground_contact_ms;
        int16_t avg_contact = (left->ground_contact_ms + right->ground_contact_ms) / 2;
        int16_t asym_pct = 0;
        if (avg_contact > 0) {
            asym_pct = (int16_t)((float)contact_diff / (float)avg_contact * 1000.0f);
        }
        printf("Gait: L/R contact asymmetry = %d.%d%%\n",
               asym_pct / 10, (asym_pct > 0 ? asym_pct : -asym_pct) % 10);

        /* Injury alert if asymmetry > 5% or impact > 300% BW */
        if (abs(asym_pct) > 50 || /* 5% asymmetry */
            left->impact_load_pct > 300 || right->impact_load_pct > 300) {
            uint8_t injury_class = TS_INJURY_NONE;
            if (abs(asym_pct) > 80) injury_class = TS_INJURY_IT_BAND;
            if (left->pronation_deg > 150 || right->pronation_deg > 150)
                injury_class = TS_INJURY_PLANTAR;
            if (left->impact_load_pct > 350 || right->impact_load_pct > 350)
                injury_class = TS_INJURY_STRESS_FX;

            if (injury_class != TS_INJURY_NONE) {
                ts_send_injury_alert(injury_class, 60 /* risk % */,
                                    asym_pct, left->impact_load_pct,
                                    left->terrain);
                display_alert_screen(ts_injury_name(injury_class));
            }
        }
    }
}

/* ---- Mesh RX callback ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case TS_MSG_GAIT: {
        if (len < sizeof(ts_gait_payload_t) - 2) break;
        const ts_gait_payload_t *p = (const ts_gait_payload_t *)data;

        /* Find pod slot */
        pod_state_t *pod = NULL;
        for (int i = 0; i < MAX_PODS; i++) {
            if (pods[i].active && pods[i].side == p->side) {
                pod = &pods[i];
                break;
            }
        }
        if (!pod) {
            /* Register new pod */
            for (int i = 0; i < MAX_PODS; i++) {
                if (!pods[i].active) {
                    pod = &pods[i];
                    pod->active = 1;
                    pod->side = p->side;
                    break;
                }
            }
        }
        if (!pod) break;

        pod->gait_class = p->gait_class;
        pod->gait_conf = p->gait_conf;
        pod->cadence = p->cadence;
        pod->ground_contact_ms = p->ground_contact_ms;
        pod->vertical_osc_mm = p->vertical_osc_mm;
        pod->impact_load_pct = p->impact_load_pct;
        pod->pronation_deg = p->pronation_deg;
        pod->asymmetry_pct = p->asymmetry_pct;
        pod->stride_length_cm = p->stride_length_cm;
        pod->terrain = p->terrain;
        pod->battery_pct = p->battery_pct;
        pod->last_gait_ms = (int64_t)0; /* app_timer */

        compute_gait_asymmetry();
        break;
    }
    case TS_MSG_NAV: {
        if (len < sizeof(ts_nav_payload_t) - 2) break;
        const ts_nav_payload_t *p = (const ts_nav_payload_t *)data;
        printf("Trail beacon: difficulty=%s water=%u dist_trailhead=%um\n",
               ts_trail_diff_name(p->trail_difficulty),
               p->water_available, p->dist_trailhead_m);
        /* Display nearest beacon info on OLED */
        break;
    }
    case TS_MSG_BEACON_DATA: {
        if (len < sizeof(ts_beacon_data_payload_t) - 2) break;
        const ts_beacon_data_payload_t *p = (const ts_beacon_data_payload_t *)data;
        printf("Beacon %u: temp=%.1f°C humidity=%u%% pressure=%.1fhPa hazards=0x%02X\n",
               p->node_id, p->temp_centic / 100.0f, p->humidity_pct,
               p->pressure_hpa / 10.0f, p->hazard_flags);
        break;
    }
    default:
        break;
    }
}

/* ---- LoRa RX callback ---- */
static void lora_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    /* Handle SOS_ACK from hub */
    if (type == TS_MSG_SOS_ACK) {
        if (len < sizeof(ts_sos_ack_payload_t) - 2) return;
        const ts_sos_ack_payload_t *p = (const ts_sos_ack_payload_t *)data;
        printf("SOS ACK: status=%u ETA=%u min\n", p->status, p->eta_minutes);
        if (p->status >= 1) {
            display_alert_screen("SOS received — help is coming");
            ws.sos_active = 0; /* acknowledged */
        }
    }
}

/* ---- Main ---- */
int main(void)
{
    printf("TrailSync Wrist Unit starting...\n");

    /* Initialize sensors */
    gps_init();
    ppg_init();

    /* Initialize mesh */
    ts_mesh_set_rx_callback(mesh_rx_handler);
    ts_lora_set_tx(NULL); /* set by LoRa driver init */

    uint32_t last_telem_ms = 0;
    uint32_t last_pressure_ms = 0;
    uint32_t last_display_ms = 0;

    while (1) {
        uint32_t now = 0; /* app_timer_ms(); */

        /* Read sensors */
        gps_poll();
        ppg_read();
        imu_read();

        /* Read barometric pressure */
        ws.pressure_hpa = bmp390_read_pressure();
        ws.altitude_dm = bmp390_pressure_to_alt_dm(ws.pressure_hpa);

        /* Pressure-based storm check every 5 min (300000 ms) */
        if (now - last_pressure_ms > 300000) {
            last_pressure_ms = now;
            pressure_storm_check();
        }

        /* Altitude sickness check */
        altitude_sickness_check();

        /* Send telemetry every 60s */
        if (now - last_telem_ms > 60000) {
            last_telem_ms = now;
            uint8_t flags = 0;
            if (ws.sos_active) flags |= TS_ALERT_FALL_DETECTED;
            if (ws.spo2 < TS_SPO2_AMS_THRESHOLD) flags |= TS_ALERT_ALTITUDE_SICK;

            ts_send_telemetry(ws.lat_deg1e5, ws.lon_deg1e5, ws.altitude_dm,
                              ws.speed_cm_s, ws.distance_dm, ws.hr, ws.spo2,
                              (int16_t)(ws.hrv_current * 10),
                              ws.skin_temp_centic, ws.pressure_hpa,
                              100 /* battery */, ws.num_sats, flags);
        }

        /* Display update every 1s */
        if (now - last_display_ms > 1000) {
            last_display_ms = now;
            display_pace_screen();
        }

        nrf_delay_ms(10);
    }

    return 0;
}
/*
 * shoe_pod_main.c — TrailSync Shoe Pod firmware (nRF52833, nRF5 SDK)
 *
 * The biomechanics lab. Embedded in the midsole of each trail shoe.
 * Measures 24-point plantar pressure, 3D acceleration/gyro, and ground
 * reaction force at 200 Hz. Runs on-device gait CNN for real-time
 * classification. Sends gait summary to Wrist Unit every 5s via Sub-GHz.
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
#include "trail_protocol.h"

/* ---- IMU (LSM6DSL via I2C) ---- */
#define LSM6DSL_ADDR 0x6A

/* Accelerometer full-scale: ±16G, 6.66 Hz ODR for 200 Hz sampling */
#define IMU_SCALE_G  16384.0f  /* LSB per G at ±16G range */

typedef struct {
    int16_t ax, ay, az;    /* 3D acceleration */
    int16_t gx, gy, gz;    /* 3D gyroscope */
} imu_sample_t;

/* ---- Pressure insole (24 FSR array via ADC) ---- */
#define NUM_PRESSURE_POINTS 24

typedef struct {
    uint16_t pressure[NUM_PRESSURE_POINTS]; /* raw ADC values 0-4095 */
    int16_t  cop_x;    /* center of pressure X (heel-to-toe) */
    int16_t  cop_y;    /* center of pressure Y (lateral-to-medial) */
    uint16_t total_force; /* sum of all pressure points */
} pressure_t;

/* ---- Strain gauge (HX711 for ground reaction force) ---- */
typedef struct {
    int32_t  vertical_gf;   /* vertical ground reaction force (N * 10) */
    int32_t  ap_gf;         /* anterior-posterior ground reaction force (N * 10) */
} grf_t;

/* ---- Gait metrics (computed per stride) ---- */
typedef struct {
    int16_t  cadence;           /* steps/min * 10 */
    int16_t  ground_contact_ms; /* ground contact time in ms */
    int16_t  flight_time_ms;   /* flight time in ms */
    int16_t  vertical_osc_mm;   /* vertical oscillation in mm * 10 */
    int16_t  impact_load_pct;   /* vertical impact as % of body weight * 10 */
    int16_t  pronation_deg;    /* pronation angle in degrees * 10 */
    int16_t  stride_length_cm; /* stride length in cm * 10 */
    uint8_t  gait_class;        /* 0=normal 1=asymmetric 2=overpronating 3=high-impact */
    uint8_t  gait_conf;        /* confidence 0-100 */
    uint8_t  terrain;           /* TS_TERRAIN_* */
} gait_metrics_t;

/* ---- On-device gait CNN (TFLite Micro, <150KB) ---- */
#define GAIT_WINDOW_SAMPLES 400  /* 2 seconds at 200 Hz */
#define GAIT_INPUT_DIM      12   /* ax,ay,az,gx,gy,gz + 6 pressure zones */

typedef struct { void *handle; } GaitModel;

/* Stub: in production, load TFLite Micro model from flash */
static GaitModel *gait_model = NULL;

static int8_t gait_classify(const imu_sample_t *imu_window,
                            const pressure_t *pres_window,
                            int window_len, gait_metrics_t *metrics)
{
    /* In production: run TFLite Micro inference
     * Input: 2-second window of IMU + pressure data (12 channels)
     * Output: gait class (0-3) + confidence + terrain class (0-7)
     *
     * Fallback heuristic when model not available:
     * - Asymmetry: compare L/R ground contact time (if paired)
     * - Overpronation: medial pressure shift > 30% of total
     * - High-impact: vertical GRF > 3.5× body weight
     * - Terrain: from vertical oscillation variability
     */

    /* Compute pronation from pressure distribution */
    /* Medial pressure = points 0,3,4,7,8,11,12,15,16,19,20,23 (inside edge)
     * Lateral pressure = points 1,2,5,6,9,10,13,14,17,18,21,22 (outside edge) */
    uint32_t medial = 0, lateral = 0;
    /* Simplified: use first 8 points for heel, next 8 for midfoot, last 8 for forefoot */
    for (int i = 0; i < NUM_PRESSURE_POINTS; i++) {
        if (i % 2 == 0) medial += pres_window[0].pressure[i];
        else lateral += pres_window[0].pressure[i];
    }
    float pronation_ratio = (medial > 0) ? (float)lateral / (float)(medial + lateral) : 0.5f;

    /* Pronation angle: ratio < 0.4 = overpronating (medial shift) */
    if (pronation_ratio < 0.35f) {
        metrics->pronation_deg = (int16_t)((0.5f - pronation_ratio) * 200.0f);
    } else {
        metrics->pronation_deg = (int16_t)((pronation_ratio - 0.5f) * 100.0f);
    }

    /* Impact load from vertical GRF */
    if (metrics->impact_load_pct == 0) {
        metrics->impact_load_pct = 250; /* default 250% BW for running */
    }

    /* Terrain from vertical oscillation variability */
    float vert_var = 5.0f; /* default: moderate variability = dirt */
    if (vert_var < 2.0f) metrics->terrain = TS_TERRAIN_ROAD;
    else if (vert_var < 4.0f) metrics->terrain = TS_TERRAIN_GRAVEL;
    else if (vert_var < 7.0f) metrics->terrain = TS_TERRAIN_DIRT;
    else if (vert_var < 10.0f) metrics->terrain = TS_TERRAIN_MUD;
    else metrics->terrain = TS_TERRAIN_ROCK;

    /* Gait classification */
    if (metrics->pronation_deg > 150) {
        metrics->gait_class = 2; /* overpronating */
        metrics->gait_conf = 75;
    } else if (metrics->impact_load_pct > 350) {
        metrics->gait_class = 3; /* high-impact */
        metrics->gait_conf = 70;
    } else if (abs(metrics->asymmetry_pct) > 50) {
        metrics->gait_class = 1; /* asymmetric */
        metrics->gait_conf = 65;
    } else {
        metrics->gait_class = 0; /* normal */
        metrics->gait_conf = 90;
    }

    return metrics->gait_class;
}

/* ---- Sensor reading (200 Hz) ---- */
static imu_sample_t current_imu;
static pressure_t current_pressure;
static grf_t current_grf;

static void read_imu(imu_sample_t *imu)
{
    /* In production: read LSM6DSL via I2C at 200 Hz
     * Output data rate: 6.66 kHz (downsampled to 200 Hz)
     * Full-scale: ±16G accel, ±2000 dps gyro */
    /* Stub: return simulated data */
    imu->ax = 0;
    imu->ay = 0;
    imu->az = (int16_t)(1.0f * IMU_SCALE_G); /* 1G standing */
    imu->gx = 0;
    imu->gy = 0;
    imu->gz = 0;
}

static void read_pressure(pressure_t *pres)
{
    /* In production: read 24 FSR channels via ADC multiplexer
     * Compute center of pressure and total force */
    memset(pres, 0, sizeof(*pres));
    /* Simplified: simulate moderate running pressure */
    for (int i = 0; i < NUM_PRESSURE_POINTS; i++) {
        pres->pressure[i] = 800 + (i % 3) * 200;
        pres->total_force += pres->pressure[i];
    }
    pres->cop_x = 0;
    pres->cop_y = 0;
}

static void read_grf(grf_t *grf)
{
    /* In production: read HX711 strain gauge
     * Vertical and anterior-posterior ground reaction force */
    grf->vertical_gf = 2500; /* ~250% BW for running */
    grf->ap_gf = 300;
}

/* ---- Gait stride detection ---- */
static int in_contact = 0;
static int32_t contact_start_ms = 0;
static int32_t last_contact_end_ms = 0;
static int32_t stride_count = 0;
static gait_metrics_t gait;
static uint8_t gait_seq = 0;

static void process_stride(void)
{
    /* Compute gait metrics from the completed stride */
    int32_t contact_time = 200; /* ms, from stride detection */
    int32_t flight_time = 80;   /* ms */
    int32_t total_stride_time = contact_time + flight_time;

    gait.cadence = (int16_t)(600000.0f / total_stride_time); /* steps/min * 10 */
    gait.ground_contact_ms = (int16_t)contact_time;
    gait.flight_time_ms = (int16_t)flight_time;
    gait.vertical_osc_mm = 100; /* mm * 10, from IMU vertical displacement */
    gait.stride_length_cm = 1300; /* cm * 10 */

    /* Get impact load from ground reaction force */
    gait.impact_load_pct = (int16_t)(current_grf.vertical_gf / 10); /* proxy: % BW * 10 */

    /* Run gait CNN on 2-second window */
    gait_classify(&current_imu, &current_pressure, 1, &gait);

    /* Send gait summary to wrist unit */
    uint8_t flags = 0;
    if (gait.gait_class != 0) flags |= TS_ALERT_INJURY_RISK;

    ts_send_gait(TS_POD_LEFT, gait.terrain, gait.gait_class, gait.gait_conf,
                 gait.cadence, gait.ground_contact_ms, gait.vertical_osc_mm,
                 gait.impact_load_pct, gait.pronation_deg,
                 gait.asymmetry_pct, gait.stride_length_cm,
                 100 /* battery */, flags);

    gait_seq++;
}

/* ---- Detect foot contact from IMU vertical acceleration ---- */
static int detect_foot_contact(const imu_sample_t *imu)
{
    /* Vertical acceleration below threshold = foot on ground
     * (deceleration phase of ground contact)
     * Threshold: ~0.5G below gravity = 0.5G vertical component */
    float vert_accel = (float)imu->az / IMU_SCALE_G;
    return (vert_accel < 0.6f); /* in contact */
}

/* ---- Main loop ---- */
int main(void)
{
    printf("TrailSync Shoe Pod starting...\n");

    /* Initialize Sub-GHz mesh */
    ts_mesh_set_tx(NULL); /* set by SX1262 driver init */

    /* IMU motion detection for auto-sleep */
    uint32_t last_motion_ms = 0;
    uint32_t last_gait_tx_ms = 0;
    uint8_t running = 0;

    while (1) {
        /* Read sensors at 200 Hz (5ms per cycle) */
        read_imu(&current_imu);
        read_pressure(&current_pressure);
        read_grf(&current_grf);

        /* Detect foot contact */
        int contact = detect_foot_contact(&current_imu);

        if (contact && !in_contact) {
            /* Foot strike detected — start of ground contact */
            in_contact = 1;
            contact_start_ms = 0; /* app_timer_ms(); */
            stride_count++;
        } else if (!contact && in_contact) {
            /* Toe off — end of ground contact, start of flight */
            in_contact = 0;
            last_contact_end_ms = 0; /* app_timer_ms(); */

            /* Process completed stride every ~5 seconds */
            uint32_t now = 0; /* app_timer_ms(); */
            if (now - last_gait_tx_ms > 5000) {
                process_stride();
                last_gait_tx_ms = now;
            }
        }

        /* Auto-sleep: if no motion for 5 minutes, enter deep sleep
         * Wake on IMU motion threshold (LSM6DSL built-in) */
        uint32_t now = 0; /* app_timer_ms(); */
        if (contact || abs(current_imu.ax) > 500) {
            last_motion_ms = now;
            running = 1;
        } else if (now - last_motion_ms > 300000) {
            /* Enter deep sleep (~3 µA), wake on IMU interrupt */
            running = 0;
            printf("Shoe Pod: no motion for 5 min, entering deep sleep\n");
            /* nrf_power_system_off(); in production */
        }

        nrf_delay_ms(5); /* 200 Hz */
    }

    return 0;
}
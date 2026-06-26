/*
 * hub_main.c — TrailSync Hub firmware (ESP32-S3, ESP-IDF)
 *
 * The coordinator. Stays at home or in the vehicle at the trailhead.
 * Bridges Sub-GHz mesh and LoRa to WiFi/cellular for cloud sync.
 * Runs route planner, group tracker, and SOS relay coordination.
 * Displays group status and trail conditions on TFT dashboard.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mqtt.h"
#include "driver/uart.h"
#include "trail_protocol.h"

#define TAG "TrailSync-Hub"

/* ---- UART for Sub-GHz mesh (nRF52833 + SX1262 on UART0) ---- */
#define MESH_UART   UART_NUM_0
#define MESH_BAUD   1000000

/* ---- UART for LoRa (RFM95W on UART1) ---- */
#define LORA_UART    UART_NUM_1
#define LORA_BAUD    9600

/* ---- Per-runner state (supports up to 8 group members) ---- */
#define MAX_RUNNERS 8

typedef struct {
    uint8_t  active;
    uint8_t  wrist_id;
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
    uint8_t  battery_pct;
    uint8_t  num_sats;
    uint8_t  flags;
    int64_t  last_telem_ms;

    /* Gait state (from shoe pods, relayed through wrist unit) */
    uint8_t  gait_class;
    uint8_t  gait_conf;
    int16_t  cadence;
    int16_t  ground_contact_ms;
    int16_t  impact_load_pct;
    int16_t  asymmetry_pct;
    int16_t  pronation_deg;
    int8_t   terrain;
    uint8_t  injury_class;
    uint8_t  injury_risk_pct;
} runner_state_t;

static runner_state_t runners[MAX_RUNNERS];
static uint8_t runner_count = 0;

/* ---- Trail database (downloaded from cloud) ---- */
#define MAX_BEACONS 64

typedef struct {
    uint8_t  beacon_id;
    int32_t  lat_deg1e5;
    int32_t  lon_deg1e5;
    int16_t  altitude_dm;
    uint8_t  trail_difficulty;
    uint8_t  water_available;
    uint16_t dist_trailhead_m;
    uint16_t dist_next_water_m;
    uint8_t  hazard_flags;
    int16_t  temp_centic;
    uint16_t humidity_pct;
    uint16_t pressure_hpa;
    uint8_t  battery_pct;
    uint8_t  cell_signal;
    int64_t  last_update_ms;
} beacon_state_t;

static beacon_state_t beacons[MAX_BEACONS];
static uint8_t beacon_count = 0;

/* ---- SOS relay coordination ---- */
typedef struct {
    uint8_t  sos_node_id;
    uint8_t  sos_type;
    uint8_t  severity;
    int32_t  lat_deg1e5;
    int32_t  lon_deg1e5;
    int16_t  altitude_dm;
    uint8_t  hr;
    uint8_t  spo2;
    int16_t  hrv_rmssd;
    uint8_t  injury_class;
    uint8_t  status;          /* 0=pending 1=relaying 2=rescue_dispatched */
    int64_t  sos_time_ms;
} sos_state_t;

static sos_state_t active_sos = {0};

/* ---- Find or add runner ---- */
static runner_state_t *find_runner(uint8_t wrist_id)
{
    for (int i = 0; i < runner_count; i++)
        if (runners[i].active && runners[i].wrist_id == wrist_id)
            return &runners[i];
    return NULL;
}

static runner_state_t *add_runner(uint8_t wrist_id)
{
    if (runner_count >= MAX_RUNNERS) return NULL;
    runner_state_t *r = &runners[runner_count++];
    memset(r, 0, sizeof(*r));
    r->active = 1;
    r->wrist_id = wrist_id;
    return r;
}

/* ---- Find or add beacon ---- */
static beacon_state_t *find_beacon(uint8_t beacon_id)
{
    for (int i = 0; i < beacon_count; i++)
        if (beacons[i].beacon_id == beacon_id)
            return &beacons[i];
    return NULL;
}

static beacon_state_t *add_beacon(uint8_t beacon_id)
{
    if (beacon_count >= MAX_BEACONS) return NULL;
    beacon_state_t *b = &beacons[beacon_count++];
    memset(b, 0, sizeof(*b));
    b->beacon_id = beacon_id;
    return b;
}

/* ---- Route planner (simplified A*) ---- */
static void compute_route_to_trailhead(runner_state_t *r)
{
    /* In production: A* pathfinding on trail network graph
     * For now: compute direct bearing and distance to nearest beacon
     * with cell_signal > 0 (i.e., near trailhead) */
    float min_dist = 1e9;
    beacon_state_t *nearest = NULL;

    for (int i = 0; i < beacon_count; i++) {
        if (beacons[i].cell_signal > 0) {
            float dlat = (beacons[i].lat_deg1e5 - r->lat_deg1e5) / 1e5f;
            float dlon = (beacons[i].lon_deg1e5 - r->lon_deg1e5) / 1e5f;
            float dist = sqrtf(dlat * dlat + dlon * dlon) * 111000.0f; /* approx meters */
            if (dist < min_dist) {
                min_dist = dist;
                nearest = &beacons[i];
            }
        }
    }

    if (nearest) {
        ESP_LOGI(TAG, "Runner %u: nearest trailhead beacon %u, %.0fm away",
                 r->wrist_id, nearest->beacon_id, min_dist);
    }
}

/* ---- Group tracker dashboard (TFT display) ---- */
static void display_group_dashboard(void)
{
    ESP_LOGI(TAG, "=== Group Dashboard ===");
    for (int i = 0; i < runner_count; i++) {
        if (!runners[i].active) continue;
        runner_state_t *r = &runners[i];
        ESP_LOGI(TAG, "Runner %u: HR=%u SpO2=%u%% HRV=%.1fms dist=%.1fkm "
                 "gait=%s risk=%u%% terrain=%s",
                 r->wrist_id, r->hr, r->spo2, r->hrv_rmssd / 10.0f,
                 r->distance_dm / 10000.0f,
                 ts_gait_class_name(r->gait_class),
                 r->injury_risk_pct,
                 ts_terrain_name(r->terrain));

        /* Alert if injury risk > 60% */
        if (r->injury_risk_pct > 60) {
            ESP_LOGW(TAG, "⚠ INJURY RISK: Runner %u — %s risk %u%%",
                     r->wrist_id, ts_injury_name(r->injury_class),
                     r->injury_risk_pct);
        }

        /* Alert if altitude sickness */
        if (r->spo2 < TS_SPO2_AMS_THRESHOLD) {
            ESP_LOGW(TAG, "⚠ ALTITUDE: Runner %u — SpO2 %u%% < %u%%",
                     r->wrist_id, r->spo2, TS_SPO2_AMS_THRESHOLD);
        }
    }
    ESP_LOGI(TAG, "======================");
}

/* ---- SOS relay handler ---- */
static void handle_sos(const ts_sos_payload_t *sos)
{
    /* Received SOS from a wrist unit (via mesh or LoRa relay) */
    active_sos.sos_node_id = sos->node_id;
    active_sos.sos_type = sos->sos_type;
    active_sos.severity = sos->severity;
    active_sos.lat_deg1e5 = sos->lat_deg1e5;
    active_sos.lon_deg1e5 = sos->lon_deg1e5;
    active_sos.altitude_dm = sos->altitude_dm;
    active_sos.hr = sos->hr;
    active_sos.spo2 = sos->spo2;
    active_sos.hrv_rmssd = sos->hrv_rmssd;
    active_sos.injury_class = sos->injury_class;
    active_sos.status = 1; /* relaying */
    active_sos.sos_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    ESP_LOGW(TAG, "🚨 SOS from node 0x%02X: type=0x%02X severity=%u "
             "lat=%d lon=%d alt=%dm HR=%u SpO2=%u%% injury=%s",
             sos->node_id, sos->sos_type, sos->severity,
             sos->lat_deg1e5, sos->lon_deg1e5, sos->altitude_dm / 10,
             sos->hr, sos->spo2, ts_injury_name(sos->injury_class));

    /* Relay to cloud via WiFi MQTT */
    char json[512];
    snprintf(json, sizeof(json),
        "{\"type\":\"sos\",\"node_id\":%u,\"sos_type\":%u,\"severity\":%u,"
        "\"lat\":%d,\"lon\":%d,\"alt\":%d,\"hr\":%u,\"spo2\":%u,"
        "\"hrv_rmssd\":%d,\"injury_class\":%u,\"num_people\":%u}",
        sos->node_id, sos->sos_type, sos->severity,
        sos->lat_deg1e5, sos->lon_deg1e5, sos->altitude_dm / 10,
        sos->hr, sos->spo2, sos->hrv_rmssd, sos->injury_class, sos->num_people);

    /* In production: esp_mqtt_client_publish(mqtt_client, "trailsync/sos", json, 0, 1, 0) */
    ESP_LOGI(TAG, "MQTT: publishing SOS to cloud");

    /* Send SOS ACK back through LoRa mesh */
    ts_sos_ack_payload_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = TS_MSG_SOS_ACK;
    ack.hub_id = TS_NODE_ID_HUB;
    ack.sos_node_id = sos->node_id;
    ack.status = 1; /* received, relaying */
    ack.eta_minutes = 0; /* unknown yet */
    ts_pack_crc(&ack, sizeof(ack) - sizeof(uint16_t));
    ts_lora_send(TS_MSG_SOS_ACK, TS_NODE_ID_HUB, &ack, sizeof(ack));

    ESP_LOGI(TAG, "SOS ACK sent to node 0x%02X", sos->node_id);
}

/* ---- Mesh RX callback ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case TS_MSG_TELEMETRY: {
        if (len < sizeof(ts_telemetry_payload_t) - 2) break;
        const ts_telemetry_payload_t *p = (const ts_telemetry_payload_t *)data;
        runner_state_t *r = find_runner(p->node_id);
        if (!r) r = add_runner(p->node_id);
        if (!r) break;

        r->lat_deg1e5 = p->lat_deg1e5;
        r->lon_deg1e5 = p->lon_deg1e5;
        r->altitude_dm = p->altitude_dm;
        r->speed_cm_s = p->speed_cm_s;
        r->distance_dm = p->distance_dm;
        r->hr = p->hr;
        r->spo2 = p->spo2;
        r->hrv_rmssd = p->hrv_rmssd;
        r->skin_temp_centic = p->skin_temp_centic;
        r->pressure_hpa = p->pressure_hpa;
        r->battery_pct = p->battery_pct;
        r->num_sats = p->num_satellites;
        r->flags = p->flags;
        r->last_telem_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Check altitude sickness */
        if (r->spo2 < TS_SPO2_AMS_THRESHOLD) {
            ESP_LOGW(TAG, "Altitude warning for runner %u: SpO2 %u%%", r->wrist_id, r->spo2);
        }
        break;
    }
    case TS_MSG_INJURY_ALERT: {
        if (len < sizeof(ts_injury_alert_payload_t) - 2) break;
        const ts_injury_alert_payload_t *p = (const ts_injury_alert_payload_t *)data;
        runner_state_t *r = find_runner(p->node_id);
        if (r) {
            r->injury_class = p->injury_class;
            r->injury_risk_pct = p->risk_pct;
            r->asymmetry_pct = p->asymmetry_pct;
            r->impact_load_pct = p->impact_load_pct;
            r->terrain = p->terrain;
        }
        ESP_LOGW(TAG, "Injury alert: %s risk %u%% (asymmetry=%.1f%% impact=%d%% BW)",
                 ts_injury_name(p->injury_class), p->risk_pct,
                 p->asymmetry_pct / 10.0f, p->impact_load_pct / 10);
        break;
    }
    case TS_MSG_SOS: {
        if (len < sizeof(ts_sos_payload_t) - 2) break;
        const ts_sos_payload_t *p = (const ts_sos_payload_t *)data;
        handle_sos(p);
        break;
    }
    case TS_MSG_BEACON_DATA: {
        if (len < sizeof(ts_beacon_data_payload_t) - 2) break;
        const ts_beacon_data_payload_t *p = (const ts_beacon_data_payload_t *)data;
        beacon_state_t *b = find_beacon(p->node_id);
        if (!b) b = add_beacon(p->node_id);
        if (b) {
            b->temp_centic = p->temp_centic;
            b->humidity_pct = p->humidity_pct;
            b->pressure_hpa = p->pressure_hpa;
            b->battery_pct = p->battery_pct;
            b->hazard_flags = p->hazard_flags;
            b->trail_difficulty = p->trail_difficulty;
        }
        break;
    }
    default:
        break;
    }
}

/* ---- Cloud sync (MQTT over WiFi) ---- */
static void cloud_sync(void)
{
    for (int i = 0; i < runner_count; i++) {
        if (!runners[i].active) continue;
        runner_state_t *r = &runners[i];

        char json[512];
        snprintf(json, sizeof(json),
            "{\"type\":\"telemetry\",\"runner\":%u,\"lat\":%d,\"lon\":%d,"
            "\"alt\":%d,\"speed\":%d,\"dist\":%u,\"hr\":%u,\"spo2\":%u,"
            "\"hrv\":%d,\"temp\":%d,\"pressure\":%d,\"batt\":%u,"
            "\"gait_class\":%u,\"injury_risk\":%u,\"terrain\":%d}",
            r->wrist_id, r->lat_deg1e5, r->lon_deg1e5,
            r->altitude_dm / 10, r->speed_cm_s, r->distance_dm,
            r->hr, r->spo2, r->hrv_rmssd, r->skin_temp_centic,
            r->pressure_hpa, r->battery_pct,
            r->gait_class, r->injury_risk_pct, r->terrain);

        /* In production: esp_mqtt_client_publish(mqtt_client, "trailsync/telemetry", json, 0, 1, 0) */
    }

    /* Sync beacon conditions */
    for (int i = 0; i < beacon_count; i++) {
        beacon_state_t *b = &beacons[i];
        char json[256];
        snprintf(json, sizeof(json),
            "{\"type\":\"beacon\",\"id\":%u,\"temp\":%.1f,\"humidity\":%u,"
            "\"pressure\":%.1f,\"hazards\":0x%02X,\"difficulty\":%u}",
            b->beacon_id, b->temp_centic / 100.0f, b->humidity_pct,
            b->pressure_hpa / 10.0f, b->hazard_flags, b->trail_difficulty);
        /* In production: publish to MQTT */
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "TrailSync Hub starting...");

    /* Initialize WiFi */
    /* In production: esp_wifi_start(), esp_mqtt_client_init() */

    /* Initialize mesh */
    ts_mesh_set_rx_callback(mesh_rx_handler);

    /* Initialize LoRa */
    ts_lora_set_tx(NULL); /* set by RFM95W driver init */

    uint32_t last_dashboard_ms = 0;
    uint32_t last_cloud_sync_ms = 0;

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Poll mesh UART for incoming frames */
        /* In production: read from MESH_UART and call ts_mesh_on_rx() */

        /* Poll LoRa UART for incoming frames */
        /* In production: read from LORA_UART and call lora handler */

        /* Group dashboard every 10 seconds */
        if (now - last_dashboard_ms > 10000) {
            last_dashboard_ms = now;
            display_group_dashboard();
        }

        /* Cloud sync every 60 seconds */
        if (now - last_cloud_sync_ms > 60000) {
            last_cloud_sync_ms = now;
            cloud_sync();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
/*
 * HiveSync — Gateway Firmware (runs on Raspberry Pi Zero 2W)
 * Coordinates Sub-GHz mesh, runs local ML inference, bridges to cloud
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mosquitto.h>
#include <curl/curl.h>
#include "cc1101_pi.h"
#include "tflite_runtime.h"

#define MAX_NODES           64
#define CLOUD_API_URL       "https://api.hivesync.io/v1"
#define LOCAL_MQTT_PORT     1883
#define SWARM_MODEL_PATH    "/opt/hivesync/models/swarm_predictor.tflite"
#define QUEEN_MODEL_PATH    "/opt/hivesync/models/queen_health.tflite"

typedef struct {
    uint16_t node_id;
    uint8_t  node_type;
    uint8_t  slot;
    float    last_temp[3];
    float    last_humidity;
    float    last_weight;
    float    last_accel_rms;
    time_t   last_seen;
    int      active;
} node_info_t;

static node_info_t g_nodes[MAX_NODES];
static int g_num_nodes = 0;
static struct mosquitto *g_mosq = NULL;
static cc1101_pi_t g_radio;

/* ---- TFLite Models ---- */
static tflite_model_t *g_swarm_model = NULL;
static tflite_model_t *g_queen_model = NULL;

/* ---- Cloud Upload ---- */
static void upload_to_cloud(const char *topic, const char *payload) {
    CURL *curl = curl_easy_init();
    if (!curl) return;

    char url[256];
    snprintf(url, sizeof(url), "%s/readings/batch", CLOUD_API_URL);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-HiveSync-Key: " HIVESYNC_API_KEY);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Cloud upload failed: %s\n", curl_easy_strerror(res));
        /* Buffer locally for later retry */
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

/* ---- Swarm Prediction (runs every 15 min per node) ---- */
static float predict_swarm_risk(node_info_t *node) {
    /* Build 14-day feature vector from local data */
    float features[56]; /* 14 days × 4 features: temp, humidity, weight_delta, accel */
    /* ... fill from local time-series buffer ... */

    float output[1];
    tflite_invoke(g_swarm_model, features, 56, output, 1);
    return output[0]; /* Swarm probability 0-1 */
}

/* ---- Queen Health Assessment ---- */
static const char* assess_queen_health(node_info_t *node) {
    float features[24]; /* 6 hours × 4: brood_temp, temp_gradient, spectral_centroid, spectral_bw */
    /* ... fill from local buffer ... */

    float output[4]; /* healthy, laying, missing, supersedure */
    tflite_invoke(g_queen_model, features, 24, output, 4);

    int max_idx = 0;
    for (int i = 1; i < 4; i++) {
        if (output[i] > output[max_idx]) max_idx = i;
    }
    const char *labels[] = {"healthy", "laying_poorly", "missing", "supersedure"};
    return labels[max_idx];
}

/* ---- Process Incoming Sensor Data ---- */
static void process_sensor_data(uint8_t *buf, uint16_t len) {
    hivesync_msg_t msg;
    if (hivesync_parse(buf, len, &msg) != PARSE_OK) return;

    /* Find or register node */
    node_info_t *node = NULL;
    for (int i = 0; i < g_num_nodes; i++) {
        if (g_nodes[i].node_id == msg.src_id) {
            node = &g_nodes[i];
            break;
        }
    }
    if (!node && g_num_nodes < MAX_NODES) {
        node = &g_nodes[g_num_nodes++];
        node->node_id = msg.src_id;
        node->node_type = msg.node_type;
    }
    if (!node) return;

    /* Update node data */
    node->last_seen = time(NULL);

    /* Build MQTT message for local dashboard */
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"node_id\":\"0x%04X\",\"type\":\"%s\",\"temp_brood\":%.1f,"
        "\"temp_top\":%.1f,\"temp_entrance\":%.1f,\"humidity\":%.1f,"
        "\"weight_kg\":%.3f,\"accel_rms\":%.1f,\"battery_mv\":%.0f,"
        "\"spectral_centroid\":%.1f,\"peak_freq\":%.1f}",
        msg.src_id,
        msg.node_type == NODE_SENSOR ? "sensor" : "entrance",
        msg.data.temp_brood, msg.data.temp_top, msg.data.temp_entrance,
        msg.data.humidity, msg.data.weight_kg, msg.data.accel_rms,
        msg.data.battery_mv, msg.data.audio_centroid, msg.data.audio_peak);

    /* Publish locally */
    mosquitto_publish(g_mosq, NULL, "hivesync/sensors", strlen(payload), payload, 0, 0);

    /* Upload to cloud */
    upload_to_cloud("readings", payload);

    /* Run ML inference periodically */
    static time_t last_inference = 0;
    if (time(NULL) - last_inference > 900) { /* Every 15 min */
        float swarm_risk = predict_swarm_risk(node);
        const char *queen_status = assess_queen_health(node);

        char alert_buf[256];
        snprintf(alert_buf, sizeof(alert_buf),
            "{\"node_id\":\"0x%04X\",\"swarm_risk\":%.2f,\"queen\":\"%s\"}",
            msg.src_id, swarm_risk, queen_status);

        mosquitto_publish(g_mosq, NULL, "hivesync/alerts", strlen(alert_buf), alert_buf, 0, 0);

        /* Push critical alerts */
        if (swarm_risk > 0.7f) {
            /* Send push notification via cloud */
            char notif[256];
            snprintf(notif, sizeof(notif),
                "{\"type\":\"swarm_alert\",\"node_id\":\"0x%04X\",\"risk\":%.2f}",
                msg.src_id, swarm_risk);
            upload_to_cloud("alerts", notif);
        }
        last_inference = time(NULL);
    }
}

/* ---- Sub-GHz Radio Receive Thread ---- */
static void *radio_rx_thread(void *arg) {
    uint8_t rx_buf[256];
    uint16_t rx_len;

    while (1) {
        if (cc1101_pi_rx(&g_radio, rx_buf, &rx_len, 5000) == CC1101_OK) {
            process_sensor_data(rx_buf, rx_len);
        }
    }
    return NULL;
}

/* ---- TDMA Beacon Thread ---- */
static void *beacon_thread(void *arg) {
    while (1) {
        uint8_t beacon[12];
        uint16_t len = hivesync_pack_beacon(beacon, 0x0000, MAX_NODES);
        cc1101_pi_tx(&g_radio, beacon, len);
        sleep(60); /* Beacon every 60s */
    }
    return NULL;
}

/* ---- Main ---- */
int main(void) {
    /* Init Sub-GHz radio */
    cc1101_pi_init(&g_radio, 0, 17); /* SPI0, CS on GPIO17 */
    cc1101_pi_set_frequency(&g_radio, 868000000);

    /* Load TFLite models */
    g_swarm_model = tflite_load(SWARM_MODEL_PATH);
    g_queen_model = tflite_load(QUEEN_MODEL_PATH);

    /* Init MQTT */
    mosquitto_lib_init();
    g_mosq = mosquitto_new("hivesync-gateway", true, NULL);
    mosquitto_connect(g_mosq, "localhost", LOCAL_MQTT_PORT, 60);

    /* Launch threads */
    pthread_t radio_tid, beacon_tid;
    pthread_create(&radio_tid, NULL, radio_rx_thread, NULL);
    pthread_create(&beacon_tid, NULL, beacon_thread, NULL);

    /* Main loop: handle MQTT + periodic tasks */
    while (1) {
        mosquitto_loop(g_mosq, 1000, 10);
    }

    pthread_join(radio_tid, NULL);
    pthread_join(beacon_tid, NULL);
    return 0;
}
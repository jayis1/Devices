/**
 * AsthmaSync — Hub MQTT Client
 * ============================
 * TLS-encrypted MQTT client with offline queue (PSRAM ring buffer).
 * Publishes telemetry + events; subscribes to commands.
 *
 * License: MIT
 */

#include "config.h"
#include "mqtt.h"
#include "../common/protocol.h"
#include <string.h>
#include <stdio.h>

/* ── ESP-IDF MQTT includes ─────────────────────────────── */
#include "mqtt_client.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "asthmasync.mqtt";

/* ── State ─────────────────────────────────────────────── */
static esp_mqtt_client_handle_t s_client = NULL;
static mqtt_state_t s_state = MQTT_STATE_DISCONNECTED;

/* ── Offline Queue (PSRAM ring buffer) ────────────────── */
typedef struct {
    char topic[64];
    uint8_t payload[PKT_MAX_SIZE + 32];  /* JSON envelope */
    size_t payload_len;
} queued_msg_t;

static queued_msg_t *s_queue = NULL;
static uint16_t s_queue_head = 0;
static uint16_t s_queue_tail = 0;
static uint16_t s_queue_count = 0;

/* ── MQTT Event Handler ────────────────────────────────── */
static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to broker");
        s_state = MQTT_STATE_CONNECTED;
        /* Flush offline queue */
        mqtt_flush_queue();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        s_state = MQTT_STATE_DISCONNECTED;
        break;

    case MQTT_EVENT_DATA:
        /* Incoming command from cloud */
        ESP_LOGI(TAG, "Received command: topic=%.*s, data=%.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);
        /* TODO: parse JSON command and forward to node */
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error: type=%d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

/* ── Initialize MQTT Client ────────────────────────────── */
int mqtt_init(void)
{
    /* Allocate queue in PSRAM */
    s_queue = (queued_msg_t *)heap_caps_malloc(
        sizeof(queued_msg_t) * MQTT_OFFLINE_QUEUE_SIZE,
        MALLOC_CAP_SPIRAM);
    if (!s_queue) {
        ESP_LOGE(TAG, "Failed to allocate MQTT queue in PSRAM");
        return -1;
    }
    memset(s_queue, 0, sizeof(queued_msg_t) * MQTT_OFFLINE_QUEUE_SIZE);

    /* Configure MQTT client with TLS */
    esp_mqtt_client_config_t config = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URL,
            },
        },
        .credentials = {
            .client_id = MQTT_CLIENT_ID,
        },
        .network = {
            .timeout_ms = 5000,
            .reconnect_timeout_ms = 5000,
        },
        .buffer = {
            .size = 1024,
            .out_size = 1024,
        },
    };

    s_client = esp_mqtt_client_init(&config);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return -1;
    }

    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY,
                                    mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    return 0;
}

/* ── Publish ───────────────────────────────────────────── */
int mqtt_publish(const char *topic, const char *payload, size_t len, int qos)
{
    if (!topic || !payload)
        return -1;

    if (s_state != MQTT_STATE_CONNECTED) {
        /* Queue message for later */
        return mqtt_enqueue(topic, payload, len);
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, len, qos, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Publish failed, enqueueing");
        return mqtt_enqueue(topic, payload, len);
    }

    return 0;
}

/* ── Enqueue (offline buffer) ──────────────────────────── */
int mqtt_enqueue(const char *topic, const char *payload, size_t len)
{
    if (s_queue_count >= MQTT_OFFLINE_QUEUE_SIZE) {
        ESP_LOGW(TAG, "MQTT queue full, dropping oldest");
        s_queue_head = (s_queue_head + 1) % MQTT_OFFLINE_QUEUE_SIZE;
        s_queue_count--;
    }

    queued_msg_t *slot = &s_queue[s_queue_tail];
    strncpy(slot->topic, topic, sizeof(slot->topic) - 1);
    slot->topic[sizeof(slot->topic) - 1] = '\0';

    size_t copy_len = len < sizeof(slot->payload) ? len : sizeof(slot->payload);
    memcpy(slot->payload, payload, copy_len);
    slot->payload_len = copy_len;

    s_queue_tail = (s_queue_tail + 1) % MQTT_OFFLINE_QUEUE_SIZE;
    s_queue_count++;

    ESP_LOGI(TAG, "Queued message (queue: %d/%d)", s_queue_count, MQTT_OFFLINE_QUEUE_SIZE);
    return 0;
}

/* ── Flush Queue ────────────────────────────────────────── */
int mqtt_flush_queue(void)
{
    if (s_state != MQTT_STATE_CONNECTED)
        return -1;

    int flushed = 0;
    while (s_queue_count > 0) {
        queued_msg_t *slot = &s_queue[s_queue_head];
        int msg_id = esp_mqtt_client_publish(
            s_client, slot->topic,
            (const char *)slot->payload, slot->payload_len, 1, 0);

        if (msg_id < 0)
            break;  /* still can't send, try later */

        s_queue_head = (s_queue_head + 1) % MQTT_OFFLINE_QUEUE_SIZE;
        s_queue_count--;
        flushed++;
    }

    if (flushed > 0)
        ESP_LOGI(TAG, "Flushed %d queued messages", flushed);

    return flushed;
}

/* ── Get State ──────────────────────────────────────────── */
mqtt_state_t mqtt_get_state(void)
{
    return s_state;
}

/* ── Build JSON envelope for telemetry ─────────────────── */
int mqtt_build_telemetry_json(const pkt_header_t *hdr,
                              const uint8_t *payload, uint16_t plen,
                              char *out_json, size_t out_size)
{
    if (!hdr || !payload || !out_json)
        return -1;

    /* Parse TLV type from payload[0] */
    uint8_t tlv_type = payload[0];
    const uint8_t *data = payload + 1;
    int remaining = plen - 1;

    switch (tlv_type) {
    case TLV_AIR_QUALITY: {
        if (remaining < (int)sizeof(air_quality_t))
            return -1;
        air_quality_t *air = (air_quality_t *)data;
        snprintf(out_json, out_size,
            "{\"type\":\"air_quality\",\"node_id\":%u,"
            "\"pm25\":%.1f,\"pm10\":%.1f,\"voc\":%u,\"co2\":%u,"
            "\"temp\":%.1f,\"rh\":%.1f,\"ts\":%u}",
            hdr->src_id,
            air->pm2_5 / 10.0f, air->pm10 / 10.0f,
            air->voc_index, air->co2_ppm,
            air->temp_c_x10 / 10.0f, air->rh_x10 / 10.0f,
            (unsigned)0  /* TODO: add timestamp */
        );
        break;
    }
    case TLV_VITALS: {
        if (remaining < (int)sizeof(vitals_t))
            return -1;
        vitals_t *v = (vitals_t *)data;
        snprintf(out_json, out_size,
            "{\"type\":\"vitals\",\"node_id\":%u,"
            "\"hr\":%u,\"spo2\":%u,\"hrv\":%.1f,"
            "\"skin_temp\":%.1f,\"activity\":%u}",
            hdr->src_id, v->hr, v->spo2,
            v->hrv_rmssd_x10 / 10.0f,
            v->skin_temp_x10 / 10.0f, v->activity
        );
        break;
    }
    case TLV_AUDIO_FEATURE: {
        if (remaining < (int)sizeof(audio_feature_t))
            return -1;
        audio_feature_t *af = (audio_feature_t *)data;
        snprintf(out_json, out_size,
            "{\"type\":\"audio\",\"node_id\":%u,"
            "\"wheeze_prob\":%u,\"snr\":%u}",
            hdr->src_id, af->wheeze_prob, af->snr_db
        );
        break;
    }
    case TLV_ACTUATION: {
        if (remaining < (int)sizeof(actuation_t))
            return -1;
        actuation_t *act = (actuation_t *)data;
        snprintf(out_json, out_size,
            "{\"type\":\"actuation\",\"node_id\":%u,"
            "\"confidence\":%u,\"peak_accel\":%.3f,"
            "\"duration_ms\":%u,\"battery\":%u}",
            hdr->src_id, act->confidence,
            act->peak_accel_x1000 / 1000.0f,
            act->duration_ms, act->battery_pct
        );
        break;
    }
    default:
        snprintf(out_json, out_size,
            "{\"type\":\"unknown\",\"node_id\":%u,\"tlv\":%u}",
            hdr->src_id, tlv_type);
        break;
    }

    return 0;
}
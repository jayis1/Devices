/*
 * Hub — Sub-GHz TDMA Mesh Coordinator
 * firmware/hub/lora_mesh.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

#include "psp_protocol.h"
#include "sx1262_driver.h"

static const char *TAG = "LORA_MESH";

extern sx1262_t radio;
static const uint8_t *aes_key_ptr;

/* Activity heatmap from main.c */
extern uint16_t activity_heatmap[24];

/* TDMA slot assignment */
static uint16_t node_slots[TDMA_NUM_SLOTS];
static uint8_t  seq_num = 0;

/* Detection event queue (for edge ML + WiFi) */
static QueueHandle_t detection_queue;

typedef struct {
    uint8_t  msg_type;
    uint16_t src_id;
    uint8_t  payload[PSP_MAX_PAYLOAD];
    uint8_t  payload_len;
} psp_event_t;

QueueHandle_t get_detection_queue(void) { return detection_queue; }

static void send_beacon(void)
{
    uint8_t beacon[4];
    beacon[0] = (uint8_t)(TDMA_FRAME_MS >> 8);
    beacon[1] = (uint8_t)(TDMA_FRAME_MS & 0xFF);
    beacon[2] = TDMA_NUM_SLOTS;
    beacon[3] = seq_num;

    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_HUB, NODE_ID_BROADCAST,
                                PSP_MSG_SYNC, beacon, 4, seq_num++, aes_key_ptr);
    if (len > 0)
        sx1262_tx(&radio, packet, (size_t)len);
}

static void send_ack(uint16_t dst)
{
    uint8_t ack[1] = { 0x00 };
    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_HUB, dst,
                                PSP_MSG_ACK, ack, 1, seq_num++, aes_key_ptr);
    if (len > 0)
        sx1262_tx(&radio, packet, (size_t)len);
}

static void handle_data(uint16_t src, const uint8_t *payload, uint8_t len)
{
    /* Determine node type from ID range */
    if (src >= NODE_ID_SENTINEL_BASE && src < NODE_ID_TRAP_BASE && len >= sizeof(sentinel_data_t)) {
        sentinel_data_t *data = (sentinel_data_t *)payload;
        g_sentinel_data = *data;

        /* Update activity heatmap */
        uint8_t hour = (data->uptime_s / 3600) % 24;
        if (data->pest_class != PEST_NONE)
            activity_heatmap[hour] += data->count_since_last;

        ESP_LOGI(TAG, "Sentinel 0x%04X: pest=%s conf=%d%% count=%d thermal=%.1fC bat=%d%%",
                 src, pest_class_name(data->pest_class), data->confidence,
                 data->count_since_last, data->thermal_max_c / 10.0f,
                 data->battery_pct);

        /* Queue for edge ML + cloud */
        psp_event_t evt = {
            .msg_type = PSP_MSG_DATA,
            .src_id = src,
            .payload_len = len,
        };
        memcpy(evt.payload, payload, len);
        xQueueSend(detection_queue, &evt, 0);

        /* High-priority alert: termite swarm */
        if (data->pest_class == PEST_TERMITE_SWARMER) {
            ESP_LOGW(TAG, "⚠️  TERMITE SWARMER DETECTED — critical alert!");
        }
    } else if (src >= NODE_ID_TRAP_BASE && src < NODE_ID_DETERRENT_BASE && len >= sizeof(trap_data_t)) {
        trap_data_t *data = (trap_data_t *)payload;
        g_trap_data = *data;
        ESP_LOGI(TAG, "Trap 0x%04X: status=%s weight=%dg bait=%d%% class=%d bat=%d%%",
                 src, trap_status_name(data->trap_status), data->catch_weight_g,
                 data->bait_level, data->catch_class, data->battery_pct);

        if (data->trap_status == TRAP_TRIGGERED)
            ESP_LOGI(TAG, "🎯 Trap triggered! Catch: %dg (class=%s)",
                     data->catch_weight_g,
                     data->catch_class == CATCH_MOUSE ? "mouse" :
                     data->catch_class == CATCH_RAT ? "rat" : "unknown");

        psp_event_t evt = { .msg_type = PSP_MSG_DATA, .src_id = src, .payload_len = len };
        memcpy(evt.payload, payload, len);
        xQueueSend(detection_queue, &evt, 0);
    } else if (src >= NODE_ID_DETERRENT_BASE && len >= sizeof(deterrent_data_t)) {
        deterrent_data_t *data = (deterrent_data_t *)payload;
        g_deterrent_data = *data;
        ESP_LOGI(TAG, "Deterrent 0x%04X: us=%d strobe=%d diff=%d oil=%d%% bat=%d%%",
                 src, data->ultrasonic_active, data->strobe_active,
                 data->diffuser_active, data->oil_level, data->battery_pct);

        psp_event_t evt = { .msg_type = PSP_MSG_DATA, .src_id = src, .payload_len = len };
        memcpy(evt.payload, payload, len);
        xQueueSend(detection_queue, &evt, 0);
    }
}

static void handle_join(uint16_t src)
{
    /* Assign a TDMA slot */
    for (int i = 1; i < TDMA_NUM_SLOTS; i++) {
        if (node_slots[i] == 0) {
            node_slots[i] = src;
            ESP_LOGI(TAG, "Node 0x%04X assigned slot %d", src, i);

            /* Send ACK with slot info */
            uint8_t ack_payload[2] = { (uint8_t)i, (uint8_t)(TDMA_SLOT_MS / 1000) };
            uint8_t packet[PSP_MAX_PACKET];
            int len = psp_build_packet(packet, NODE_ID_HUB, src,
                                        PSP_MSG_ACK, ack_payload, 2, seq_num++, aes_key_ptr);
            if (len > 0)
                sx1262_tx(&radio, packet, (size_t)len);
            return;
        }
    }
    ESP_LOGW(TAG, "No free TDMA slots for node 0x%04X", src);
}

void lora_mesh_task(void *pvParameters)
{
    sx1262_t *r = (sx1262_t *)pvParameters;
    aes_key_ptr = r->aes_key;

    /* Initialize slot table */
    memset(node_slots, 0, sizeof(node_slots));
    node_slots[0] = NODE_ID_HUB;

    detection_queue = xQueueCreate(32, sizeof(psp_event_t));

    ESP_LOGI(TAG, "TDMA mesh coordinator started (frame=%d ms, %d slots)",
             TDMA_FRAME_MS, TDMA_NUM_SLOTS);

    while (1) {
        /* Slot 0: Hub beacon */
        send_beacon();
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS));

        /* Slots 1-6: Listen for node TX */
        for (int slot = 1; slot < TDMA_NUM_SLOTS - 1; slot++) {
            if (node_slots[slot] != 0) {
                uint8_t rx_buf[PSP_MAX_PACKET];
                int rssi;
                int rxlen = sx1262_rx(r, TDMA_SLOT_MS - 50, rx_buf, sizeof(rx_buf), &rssi);

                if (rxlen > 0) {
                    psp_header_t hdr;
                    uint8_t payload[PSP_MAX_PAYLOAD];
                    uint8_t plen;
                    int ret = psp_parse_packet(rx_buf, (size_t)rxlen, &hdr, payload, &plen, r->aes_key);
                    if (ret == 0) {
                        if (hdr.msg_type == PSP_MSG_DATA)
                            handle_data(hdr.src_id, payload, plen);
                        else if (hdr.msg_type == PSP_MSG_JOIN)
                            handle_join(hdr.src_id);
                        send_ack(hdr.src_id);
                    }
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS));
            }
        }

        /* Slot 7: Hub command/ACK window */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS));
    }
}
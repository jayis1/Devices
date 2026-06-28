/*
 * Pest Sentinel — Sub-GHz Node (TDMA)
 * firmware/pest-sentinel/lora_node.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#include "psp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "LORA_NODE";

extern sx1262_t radio;
extern uint8_t  g_latest_pest_class;
extern uint8_t  g_latest_confidence;
extern uint16_t g_detection_count;
extern float    g_thermal_max_c;
extern bool     g_ir_illumination_on;

static const uint8_t *aes_key_ptr;
static uint8_t seq_num = 0;
static uint8_t my_slot = 1; /* Assigned by Hub during JOIN */

#define NODE_ID_SELF  NODE_ID_SENTINEL_BASE

static void send_join(void)
{
    uint8_t payload[1] = { 0x00 };
    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_SELF, NODE_ID_HUB,
                                PSP_MSG_JOIN, payload, 1, seq_num++, aes_key_ptr);
    if (len > 0) {
        sx1262_tx(&radio, packet, (size_t)len);
        ESP_LOGI(TAG, "JOIN sent");
    }
}

static void send_detection_report(void)
{
    sentinel_data_t data;
    memset(&data, 0, sizeof(data));

    data.node_id         = NODE_ID_SELF;
    data.uptime_s        = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    data.battery_pct     = 85; /* placeholder — read from ADC */
    data.pest_class      = g_latest_pest_class;
    data.confidence      = g_latest_confidence;
    data.count_since_last= g_detection_count;
    data.thermal_max_c   = (int16_t)(g_thermal_max_c * 10);
    data.ir_illumination = g_ir_illumination_on ? 1 : 0;
    data.alerts          = 0;

    if (g_latest_pest_class != PEST_NONE)
        data.alerts |= ALERT_PEST_DETECTED;
    if (g_latest_pest_class == PEST_TERMITE_SWARMER)
        data.alerts |= ALERT_TERMITE_SWARM;
    if (data.battery_pct < 20)
        data.alerts |= ALERT_LOW_BATTERY;

    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_SELF, NODE_ID_HUB,
                                PSP_MSG_DATA, (const uint8_t *)&data,
                                sizeof(data), seq_num++, aes_key_ptr);
    if (len > 0) {
        sx1262_tx(&radio, packet, (size_t)len);
        ESP_LOGI(TAG, "Report sent: pest=%s conf=%d%% thermal=%.1f°C",
                 pest_class_name(data.pest_class), data.confidence,
                 data.thermal_max_c / 10.0f);
    }

    /* Reset counter */
    g_detection_count = 0;
}

void lora_node_task(void *pvParameters)
{
    sx1262_t *r = (sx1262_t *)pvParameters;
    aes_key_ptr = r->aes_key;

    ESP_LOGI(TAG, "Sub-GHz node task started, joining mesh...");

    /* Join mesh */
    vTaskDelay(pdMS_TO_TICKS(2000)); /* wait for Hub beacon */
    send_join();

    /* Wait for slot assignment ACK */
    uint8_t rx_buf[PSP_MAX_PACKET];
    int rssi;
    int rxlen = sx1262_rx(r, 5000, rx_buf, sizeof(rx_buf), &rssi);
    if (rxlen > 0) {
        psp_header_t hdr;
        uint8_t payload[PSP_MAX_PAYLOAD];
        uint8_t plen;
        if (psp_parse_packet(rx_buf, (size_t)rxlen, &hdr, payload, &plen, r->aes_key) == 0) {
            if (hdr.msg_type == PSP_MSG_ACK && plen >= 1) {
                my_slot = payload[0];
                ESP_LOGI(TAG, "Assigned TDMA slot %d", my_slot);
            }
        }
    }

    while (1) {
        /* Wait for our TDMA slot (slot * 1000 ms after frame start) */
        /* In production: sync to Hub beacon, then delay slot * TDMA_SLOT_MS.
         * Here: simplified timing. */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS * my_slot));

        /* Send detection report */
        send_detection_report();

        /* Wait for ACK */
        sx1262_rx(r, 500, rx_buf, sizeof(rx_buf), &rssi);

        /* Sleep until next frame */
        vTaskDelay(pdMS_TO_TICKS(TDMA_FRAME_MS - TDMA_SLOT_MS * my_slot - 500));
    }
}
/*
 * Hub — LoRa TDMA Mesh Coordinator
 * firmware/hub/lora_mesh.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "csp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "LORA_MESH";

extern QueueHandle_t telemetry_queue;

/* TDMA coordinator: beacon → listen for nodes → send ACKs */
void lora_mesh_task(void *pvParameters)
{
    sx1262_t *radio = (sx1262_t *)pvParameters;
    csp_header_t hdr;
    uint8_t payload[CSP_MAX_PAYLOAD];
    uint8_t payload_len;

    uint32_t frame_start = 0;
    uint8_t frame_counter = 0;

    while (1) {
        frame_start = radio->hal.millis();

        /* === Slot 0: Beacon (SYNC) === */
        uint8_t sync_payload[8] = {
            frame_counter & 0xFF,
            (uint8_t)(TDMA_FRAME_MS & 0xFF),
            (uint8_t)(TDMA_FRAME_MS >> 8),
            TDMA_NUM_SLOTS,
            0, 0, 0, 0  /* padding */
        };
        sx1262_tx(radio, NODE_ID_HUB, NODE_ID_BROADCAST,
                  CSP_MSG_SYNC, sync_payload, sizeof(sync_payload));
        ESP_LOGI(TAG, "Beacon sent, frame %u", frame_counter);
        frame_counter++;

        /* Wait for slot 1 */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS - 50));

        /* === Slot 1: Bin Node === */
        int rx_result = sx1262_rx(radio, TDMA_SLOT_MS - 100,
                                  &hdr, payload, &payload_len);
        if (rx_result == 0 && hdr.msg_type == CSP_MSG_DATA) {
            if (hdr.src_id == NODE_ID_BIN && payload_len == sizeof(bin_node_data_t)) {
                bin_node_data_t data;
                memcpy(&data, payload, sizeof(data));
                xQueueSend(telemetry_queue, &data, 0);
                ESP_LOGI(TAG, "Bin Node: T=%d.%d°C CO2=%dppm CH4=%dppm W=%dg",
                         data.temp_c[0]/10, abs(data.temp_c[0]%10),
                         data.co2_ppm, data.methane_ppm, data.mass_grams);

                /* Send ACK */
                uint8_t ack[2] = { CSP_MSG_ACK, data.node_id & 0xFF };
                sx1262_tx(radio, NODE_ID_HUB, NODE_ID_BIN,
                          CSP_MSG_ACK, ack, sizeof(ack));
            }
        }

        /* Wait for slot 3 (skip slot 2 = second bin node) */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS));

        /* === Slot 3: Weather Station === */
        rx_result = sx1262_rx(radio, TDMA_SLOT_MS - 100,
                              &hdr, payload, &payload_len);
        if (rx_result == 0 && hdr.msg_type == CSP_MSG_DATA) {
            if (hdr.src_id == NODE_ID_WEATHER &&
                payload_len == sizeof(weather_data_t)) {
                weather_data_t wx;
                memcpy(&wx, payload, sizeof(wx));
                ESP_LOGI(TAG, "Weather: T=%d.%d°C H=%d%% P=%d Wind=%d.%dm/s",
                         wx.temp_c/10, abs(wx.temp_c%10),
                         wx.humidity_pct, wx.pressure_hpa,
                         wx.wind_speed_ms/10, wx.wind_speed_ms%10);

                /* ACK */
                uint8_t ack[2] = { CSP_MSG_ACK, wx.node_id & 0xFF };
                sx1262_tx(radio, NODE_ID_HUB, NODE_ID_WEATHER,
                          CSP_MSG_ACK, ack, sizeof(ack));
            }
        }

        /* === Slot 4: Check for alerts === */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS));

        /* Frame complete — wait for next cycle */
        uint32_t elapsed = radio->hal.millis() - frame_start;
        if (elapsed < TDMA_FRAME_MS) {
            vTaskDelay(pdMS_TO_TICKS(TDMA_FRAME_MS - elapsed));
        }
    }
}
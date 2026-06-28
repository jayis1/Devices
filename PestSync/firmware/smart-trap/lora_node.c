/*
 * Smart Trap — Sub-GHz Node (TDMA)
 * firmware/smart-trap/lora_node.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#include "psp_protocol.h"
#include "sx1262_driver.h"

static const char *TAG = "LORA_NODE";

extern sx1262_t radio;
extern trap_data_t g_trap_data;
extern volatile bool trap_triggered;

static const uint8_t *aes_key_ptr;
static uint8_t seq_num = 0;
static uint8_t my_slot = 3;

#define NODE_ID_SELF  NODE_ID_TRAP_BASE

static void send_join(void)
{
    uint8_t payload[1] = { 0x00 };
    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_SELF, NODE_ID_HUB,
                                PSP_MSG_JOIN, payload, 1, seq_num++, aes_key_ptr);
    if (len > 0) {
        sx1262_tx(&radio, packet, (size_t)len);
    }
}

static void send_trap_report(void)
{
    g_trap_data.node_id = NODE_ID_SELF;
    g_trap_data.uptime_s = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    g_trap_data.battery_pct = 85; /* placeholder */

    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_SELF, NODE_ID_HUB,
                                PSP_MSG_DATA, (const uint8_t *)&g_trap_data,
                                sizeof(g_trap_data), seq_num++, aes_key_ptr);
    if (len > 0) {
        sx1262_tx(&radio, packet, (size_t)len);
        ESP_LOGI(TAG, "Report sent: status=%s weight=%dg class=%d",
                 trap_status_name(g_trap_data.trap_status),
                 g_trap_data.catch_weight_g,
                 g_trap_data.catch_class);
    }
}

void lora_node_task(void *pvParameters)
{
    sx1262_t *r = (sx1262_t *)pvParameters;
    aes_key_ptr = r->aes_key;

    ESP_LOGI(TAG, "Sub-GHz node task started, joining mesh...");

    vTaskDelay(pdMS_TO_TICKS(3000)); /* wait for Hub beacon */
    send_join();

    /* Wait for slot assignment */
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
        /* Wait for our TDMA slot */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS * my_slot));

        /* Send trap status report */
        send_trap_report();

        /* Wait for ACK / commands from Hub */
        rxlen = sx1262_rx(r, 500, rx_buf, sizeof(rx_buf), &rssi);
        if (rxlen > 0) {
            psp_header_t hdr;
            uint8_t payload[PSP_MAX_PAYLOAD];
            uint8_t plen;
            if (psp_parse_packet(rx_buf, (size_t)rxlen, &hdr, payload, &plen, r->aes_key) == 0) {
                if (hdr.msg_type == PSP_MSG_CMD && plen >= 1) {
                    uint8_t cmd = payload[0];
                    if (cmd == PSP_CMD_RESET_TRAP) {
                        ESP_LOGI(TAG, "Reset command received — rearming trap");
                        g_trap_data.trap_status = TRAP_ARMED;
                        g_trap_data.catch_weight_g = 0;
                        g_trap_data.catch_class = CATCH_UNKNOWN;
                        g_trap_data.alerts = 0;
                    }
                }
            }
        }

        /* Sleep until next frame */
        vTaskDelay(pdMS_TO_TICKS(TDMA_FRAME_MS - TDMA_SLOT_MS * my_slot - 500));
    }
}
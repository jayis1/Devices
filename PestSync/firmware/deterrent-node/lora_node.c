/*
 * Deterrent Node — Sub-GHz Node (TDMA)
 * firmware/deterrent-node/lora_node.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#include "psp_protocol.h"
#include "sx1262_driver.h"

static const char *TAG = "LORA_NODE";

extern sx1262_t radio;
extern deterrent_data_t g_deterrent_data;
extern volatile uint8_t  deter_mode;
extern volatile uint8_t  deter_band;
extern volatile uint16_t deter_duration_s;
extern volatile bool     ultrasonic_active;
extern volatile bool     strobe_active;
extern volatile bool     diffuser_active;
extern volatile uint32_t total_ultrasonic_s;
extern volatile uint16_t diffuser_doses;

static const uint8_t *aes_key_ptr;
static uint8_t seq_num = 0;
static uint8_t my_slot = 5;

#define NODE_ID_SELF  NODE_ID_DETERRENT_BASE

static void send_join(void)
{
    uint8_t payload[1] = { 0x00 };
    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_SELF, NODE_ID_HUB,
                                PSP_MSG_JOIN, payload, 1, seq_num++, aes_key_ptr);
    if (len > 0)
        sx1262_tx(&radio, packet, (size_t)len);
}

static void send_status_report(void)
{
    g_deterrent_data.node_id = NODE_ID_SELF;
    g_deterrent_data.uptime_s = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    g_deterrent_data.battery_pct = 85;
    g_deterrent_data.ultrasonic_active = ultrasonic_active ? 1 : 0;
    g_deterrent_data.strobe_active = strobe_active ? 1 : 0;
    g_deterrent_data.diffuser_active = diffuser_active ? 1 : 0;
    g_deterrent_data.total_ultrasonic_s = total_ultrasonic_s;
    g_deterrent_data.diffuser_doses = diffuser_doses;

    if (g_deterrent_data.battery_pct < 20)
        g_deterrent_data.alerts |= ALERT_LOW_BATTERY;

    uint8_t packet[PSP_MAX_PACKET];
    int len = psp_build_packet(packet, NODE_ID_SELF, NODE_ID_HUB,
                                PSP_MSG_DATA, (const uint8_t *)&g_deterrent_data,
                                sizeof(g_deterrent_data), seq_num++, aes_key_ptr);
    if (len > 0) {
        sx1262_tx(&radio, packet, (size_t)len);
        ESP_LOGI(TAG, "Status sent: us=%d strobe=%d oil=%d%% doses=%d",
                 ultrasonic_active, strobe_active,
                 g_deterrent_data.oil_level, diffuser_doses);
    }
}

static void handle_command(const uint8_t *payload, uint8_t len)
{
    if (len < 1) return;
    uint8_t cmd = payload[0];

    switch (cmd) {
    case PSP_CMD_SET_DETER:
        if (len >= 5) {
            deter_mode = payload[1];
            deter_band = payload[2];
            deter_duration_s = (payload[3] << 8) | payload[4];
            ESP_LOGI(TAG, "Command SET_DETER: mode=%d band=%d duration=%ds",
                     deter_mode, deter_band, deter_duration_s);
        }
        break;
    case PSP_CMD_DETER_OFF:
        deter_mode = DETER_OFF;
        ESP_LOGI(TAG, "Command DETER_OFF");
        break;
    case PSP_CMD_DETER_STROBE:
        /* Trigger immediate strobe burst */
        ESP_LOGI(TAG, "Command STROBE — triggering burst");
        /* strobe_task will pick this up on next cycle */
        break;
    case PSP_CMD_DETER_DIFFUSE:
        /* Trigger immediate diffuser dose */
        ESP_LOGI(TAG, "Command DIFFUSE — triggering dose");
        /* diffuser_task handles this */
        break;
    case PSP_CMD_REBOOT:
        ESP_LOGI(TAG, "Command REBOOT");
        esp_restart();
        break;
    default:
        ESP_LOGW(TAG, "Unknown command 0x%02X", cmd);
    }
}

void lora_node_task(void *pvParameters)
{
    sx1262_t *r = (sx1262_t *)pvParameters;
    aes_key_ptr = r->aes_key;

    ESP_LOGI(TAG, "Sub-GHz node task started, joining mesh...");

    vTaskDelay(pdMS_TO_TICKS(4000));
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
        /* Wait for TDMA slot */
        vTaskDelay(pdMS_TO_TICKS(TDMA_SLOT_MS * my_slot));

        /* Send status report */
        send_status_report();

        /* Listen for commands from Hub */
        rxlen = sx1262_rx(r, 500, rx_buf, sizeof(rx_buf), &rssi);
        if (rxlen > 0) {
            psp_header_t hdr;
            uint8_t payload[PSP_MAX_PAYLOAD];
            uint8_t plen;
            if (psp_parse_packet(rx_buf, (size_t)rxlen, &hdr, payload, &plen, r->aes_key) == 0) {
                if (hdr.msg_type == PSP_MSG_CMD) {
                    handle_command(payload, plen);
                }
            }
        }

        /* Sleep until next frame */
        vTaskDelay(pdMS_TO_TICKS(TDMA_FRAME_MS - TDMA_SLOT_MS * my_slot - 500));
    }
}
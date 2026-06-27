/*
 * Bin Node — LoRa TDMA Node
 * firmware/bin-node/lora_node.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "csp_protocol.h"
#include "sx1262_driver.h"

static const char *TAG = "LORA";

extern bin_node_data_t latest_data;

void lora_node_task(void *pvParameters)
{
    sx1262_t *radio = (sx1262_t *)pvParameters;
    csp_header_t hdr;
    uint8_t rx_payload[CSP_MAX_PAYLOAD];
    uint8_t rx_len;

    /* Wait for initial sensor reading */
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        /* Wait for hub beacon (SYNC) */
        int rx_result = sx1262_rx(radio, 2000, &hdr, rx_payload, &rx_len);
        if (rx_result == 0 && hdr.msg_type == CSP_MSG_SYNC &&
            hdr.dst_id == NODE_ID_BROADCAST) {
            ESP_LOGD(TAG, "Received beacon, frame sync OK");

            /* Wait for our TDMA slot (slot 1, ~1000ms after beacon) */
            vTaskDelay(pdMS_TO_TICKS(50)); /* small offset for TX setup */

            /* Send our data */
            int tx_result = sx1262_tx(radio, NODE_ID_BIN, NODE_ID_HUB,
                                      CSP_MSG_DATA,
                                      (uint8_t *)&latest_data,
                                      sizeof(latest_data));
            if (tx_result > 0) {
                ESP_LOGD(TAG, "Data sent: %d bytes", tx_result);
            } else {
                ESP_LOGW(TAG, "TX failed: %d", tx_result);
            }

            /* Listen for ACK/command in slot 4 */
            vTaskDelay(pdMS_TO_TICKS(2000)); /* wait for slot 4 */
            rx_result = sx1262_rx(radio, 1000, &hdr, rx_payload, &rx_len);
            if (rx_result == 0) {
                if (hdr.msg_type == CSP_MSG_CMD) {
                    csp_command_t *cmd = (csp_command_t *)rx_payload;
                    ESP_LOGI(TAG, "Received command: 0x%02X", cmd->cmd_id);
                    /* Handle command */
                    switch (cmd->cmd_id) {
                        case CSP_CMD_OPEN_VENT:
                            /* Signal servo task */
                            break;
                        case CSP_CMD_CLOSE_VENT:
                            break;
                        case CSP_CMD_TARE_WEIGHT:
                            break;
                        case CSP_CMD_REBOOT:
                            esp_restart();
                            break;
                    }
                }
            }
        }

        /* Wait for next TDMA frame */
        vTaskDelay(pdMS_TO_TICKS(TDMA_FRAME_MS - 3000));
    }
}
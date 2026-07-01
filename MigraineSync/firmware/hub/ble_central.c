/**
 * MigraineSync — Hub BLE Central
 * ==============================
 * NimBLE-based BLE 5.0 central for Aura Band + Hydrate Tag.
 *
 * License: MIT
 */

#include "ble_central.h"
#include "config.h"
#include "../common/protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>

static const char *TAG = "migrainesync_ble";

/* In production, this uses NimBLE:
 *   - ble_hs_init()
 *   - ble_gap_disc() for scanning
 *   - ble_gap_connect() on matching service UUID
 *   - ble_gattc_disc_all_chrs() to find TX/RX characteristics
 *   - ble_gattc_subscribe() for notify on TX characteristic
 *
 * For this stub, we simulate the interface.
 */

static QueueHandle_t s_ble_rx_queue;
static bool s_aura_connected = false;
static bool s_hydrate_connected = false;

int ble_central_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE central (NimBLE)");

    s_ble_rx_queue = xQueueCreate(16, sizeof(frame_t) + sizeof(tlv_t) * MAX_TLVS + sizeof(int));

    /* In production:
     * esp_nimble_init();
     * ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params, ble_gap_event_cb, NULL);
     */

    ESP_LOGI(TAG, "BLE central scanning for service UUID %08X", BLE_SERVICE_UUID);
    return 0;
}

int ble_send(uint16_t node_id, const frame_t *frame)
{
    if (node_id == NODE_AURA_BAND && !s_aura_connected) {
        ESP_LOGW(TAG, "Aura Band not connected");
        return -1;
    }
    if (node_id == NODE_HYDRATE_TAG && !s_hydrate_connected) {
        ESP_LOGW(TAG, "Hydrate Tag not connected");
        return -1;
    }

    /* In production: write to RX characteristic (6e400003-...) */
    ESP_LOGI(TAG, "BLE TX → node 0x%04X: %u bytes", node_id, frame->len);
    return frame->len;
}

bool ble_is_connected(uint16_t node_id)
{
    if (node_id == NODE_AURA_BAND)
        return s_aura_connected;
    if (node_id == NODE_HYDRATE_TAG)
        return s_hydrate_connected;
    return false;
}

int ble_recv(frame_t *out_frame, tlv_t *out_tlvs, int *out_n_tlvs, uint32_t timeout_ms)
{
    /* In production: dequeue from notification callback queue */
    return -1;  /* no data in stub */
}

/* In production, the BLE GAP event handler would:
 * 1. On BLE_GAP_EVENT_DISC: check if advertising UUID matches → connect
 * 2. On BLE_GAP_EVENT_CONNECT: discover services + characteristics
 * 3. On BLE_GAP_EVENT_SUBSCRIBE: start receiving notifications
 * 4. On BLE_GAP_EVENT_NOTIFY_RX: parse frame, enqueue to s_ble_rx_queue
 * 5. On BLE_GAP_EVENT_DISCONNECT: mark disconnected, re-scan
 */
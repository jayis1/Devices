/**
 * BLE central stub for ESP32-S3 hub.
 * Production: uses ESP-IDF NimBLE BLE_GAP / BLE_GATT central APIs.
 * License: MIT
 */

#include "ble_central.h"
#include <string.h>

static ble_central_rx_cb g_cb = NULL;

void ble_central_init(ble_central_rx_cb callback)
{
    g_cb = callback;
    /* Production: esp_nimble_hci_init(), ble_hs_start(), register gap_event_cb */
}

void ble_central_start_scan(void)
{
    /* Production: ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER,
     * &disc_params, gap_event_cb) — scan for GlucoSync service UUID */
}

void ble_central_stop_scan(void)
{
    /* Production: ble_gap_disc_cancel() */
}

void ble_send_to_scanner(const uint8_t *data, uint8_t len)
{
    (void)data; (void)len;
    /* Production: ble_gattc_write_flat() to scanner's GlucoSync characteristic */
}

void ble_send_to_band(const uint8_t *data, uint8_t len)
{
    (void)data; (void)len;
    /* Production: ble_gattc_write_flat() to band's GlucoSync characteristic */
}

void ble_send_to_pen(const uint8_t *data, uint8_t len)
{
    (void)data; (void)len;
    /* Production: ble_gattc_write_flat() to pen tag's GlucoSync characteristic */
}

void ble_send_to_all(const uint8_t *data, uint8_t len)
{
    ble_send_to_scanner(data, len);
    ble_send_to_band(data, len);
    ble_send_to_pen(data, len);
}
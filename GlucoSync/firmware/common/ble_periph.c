/**
 * BLE peripheral stub for nRF52840 nodes.
 * Production code uses Nordic SoftDevice S140 BLE_GAP_* APIs.
 * License: MIT
 */

#include "ble_periph.h"
#include <string.h>

static ble_periph_rx_cb g_rx_cb = NULL;
static bool g_connected = false;

void ble_periph_init(ble_periph_rx_cb rx_callback)
{
    g_rx_cb = rx_callback;
    /* Production: sd_ble_enable(), set GAP params, configure GATT service */
}

void ble_periph_start_advertising(const char *device_name)
{
    (void)device_name;
    /* Production: sd_ble_gap_adv_set_configure() with device name,
     * GlucoSync service UUID in scan response */
}

void ble_periph_stop_advertising(void)
{
    /* Production: sd_ble_gap_adv_stop() */
}

bool ble_periph_is_connected(void)
{
    return g_connected;
}

void ble_periph_send(const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    /* Production: sd_ble_gatts_hvx() to write to GlucoSync characteristic */
}

void ble_periph_set_tx_power(int8_t dbm)
{
    (void)dbm;
    /* Production: sd_ble_gap_tx_power_set() */
}
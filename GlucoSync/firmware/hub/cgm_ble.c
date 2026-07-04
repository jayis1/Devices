/**
 * CGM BLE bridge — connects to continuous glucose monitors.
 * Production: uses ESP-IDF NimBLE stack with GATT client.
 * License: MIT
 */

#include "cgm_ble.h"
#include <string.h>

static cgm_data_cb g_cb = NULL;
static cgm_type_t g_type = CGM_TYPE_NONE;
static bool g_connected = false;

/* GATT service UUIDs (documented BLE profiles) */
static const char *DEXCOM_G7_SVC = "F8083532-849E-531C-C594-8F1A251F1A7C";
static const char *LIBRE_3_SVC   = "0000FDE3-0000-1000-8000-00805F9B34FB";
static const char *CUSTOM_SVC   = "0000FE01-0000-1000-8000-00805F9B34FB";

void cgm_ble_init(cgm_data_cb callback)
{
    g_cb = callback;
    /* Production: esp_nimble_hci_init(), ble_gap_connect() */
}

void cgm_ble_start_scan(void)
{
    /* Production: scan for CGM advertisement packets.
     * Dexcom G7: advertises as "DX07xx", service data with glucose UUID.
     * Libre 3: advertises service FDE3 with manufacturer data.
     * Custom: advertises GlucoSync service FE01. */
}

void cgm_ble_stop_scan(void)
{
    /* Production: ble_gap_disc_cancel() */
}

void cgm_ble_set_type(cgm_type_t type)
{
    g_type = type;
}

cgm_type_t cgm_ble_get_type(void)
{
    return g_type;
}

bool cgm_ble_is_connected(void)
{
    return g_connected;
}
#ifndef GLUCOSYNC_CGM_BLE_H
#define GLUCOSYNC_CGM_BLE_H

#include "protocol.h"

/**
 * CGM BLE bridge — connects to continuous glucose monitors via BLE GATT.
 * Supported CGMs: Dexcom G7, FreeStyle Libre 3, custom GlucoSync CGM.
 */

typedef enum {
    CGM_TYPE_NONE = 0,
    CGM_TYPE_DEXCOM_G7 = 1,
    CGM_TYPE_LIBRE_3 = 2,
    CGM_TYPE_CUSTOM = 3,
} cgm_type_t;

typedef void (*cgm_data_cb)(const payload_cgm_t *reading);

void cgm_ble_init(cgm_data_cb callback);
void cgm_ble_start_scan(void);
void cgm_ble_stop_scan(void);
void cgm_ble_set_type(cgm_type_t type);
cgm_type_t cgm_ble_get_type(void);
bool cgm_ble_is_connected(void);

#endif
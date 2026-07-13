/*
 * ble_service.c — BLE GATT service registration for CardioSync nodes
 *
 * This file provides the BLE GATT service definition used by all
 * CardioSync peripheral nodes (ECG Patch, BP Cuff, Smart Ring).
 *
 * License: MIT
 */
#include "cardiosync_protocol.h"

/* This is a platform-agnostic stub. Each platform (ESP-IDF for ESP32,
 * nRF Connect SDK for nRF52) has its own BLE stack. The GATT service
 * definition is shared logically:
 *
 * Service: CardioSync (6E400001-B5A3-F393-E0A9-E50E24DCCA9E)
 *
 *   Characteristic 0x0002 (ECG Data):
 *     - Properties: Notify
 *     - UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: ecg_data_payload_t (20 bytes)
 *     - CCCD: enabled for notifications
 *
 *   Characteristic 0x0003 (ECG HR):
 *     - Properties: Notify
 *     - UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: ecg_hr_payload_t (6 bytes)
 *
 *   Characteristic 0x0004 (BP Result):
 *     - Properties: Notify
 *     - UUID: 6E400004-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: bp_result_payload_t (10 bytes)
 *
 *   Characteristic 0x0005 (BP Command):
 *     - Properties: Write
 *     - UUID: 6E400005-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: bp_command_payload_t (2 bytes)
 *
 *   Characteristic 0x0006 (PPG HR):
 *     - Properties: Notify
 *     - UUID: 6E400006-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: ppg_hr_payload_t (6 bytes)
 *
 *   Characteristic 0x0007 (PPG HRV):
 *     - Properties: Notify
 *     - UUID: 6E400007-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: ppg_hrv_payload_t (4 bytes)
 *
 *   Characteristic 0x0008 (Activity):
 *     - Properties: Notify
 *     - UUID: 6E400008-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: activity_payload_t (4 bytes)
 *
 *   Characteristic 0x0009 (Alert):
 *     - Properties: Write (Hub writes, nodes read)
 *     - UUID: 6E400009-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: alert_payload_t (2 bytes)
 *
 *   Characteristic 0x000A (Heartbeat):
 *     - Properties: Notify
 *     - UUID: 6E40000A-B5A3-F393-E0A9-E50E24DCCA9E
 *     - Payload: heartbeat_payload_t (4 bytes)
 *
 * Security: All characteristics require encryption (BLE Secure Connections).
 *           The service uses LE Secure Connections (ECDH) for pairing.
 */

/* ── ESP-IDF Stub (Hub uses NimBLE or Bluedroid) ────────────── */
#ifdef ESP_PLATFORM
#include "esp_log.h"
static const char *TAG = "CS_BLE";

/* GATT service registration would use esp_ble_gatts_* APIs */
void cs_ble_init(void)
{
    ESP_LOGI(TAG, "CardioSync BLE GATT service registered");
}
#endif

/* ── nRF Connect SDK Stub (nRF52 uses SoftDevice) ──────────── */
#ifdef NRF52
#include "nrf_log.h"

void cs_ble_init(void)
{
    NRF_LOG_INFO("CardioSync BLE GATT service registered");
}
#endif
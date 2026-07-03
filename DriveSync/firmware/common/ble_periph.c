/**
 * DriveSync BLE Peripheral — nRF52840 SoftDevice S140
 *
 * Shared by Steering Wheel Node and Seat Belt Tag.
 * Custom DriveSync GATT service with a single data characteristic.
 *
 * License: MIT
 */

#include "ble_periph.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_log.h"
#include "app_error.h"
#include <string.h>

#define BLE_DRIVE_UUID_BASE   {{0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00}}

#define BLE_DRIVE_SERVICE_UUID     0x0001
#define BLE_DRIVE_CHAR_UUID        0x0002
#define APP_BLE_CONN_CFG_TAG       1
#define APP_BLE_OBS_PRIO           3

static ble_cmd_cb_t         s_cmd_handler = NULL;
static ble_connected_cb_t   s_connected_cb = NULL;
static ble_disconnected_cb_t s_disconnected_cb = NULL;

static uint16_t s_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static bool s_connected = false;

/* ── GAP/GATT Setup ──────────────────────────────────────────────── */

static void gap_params_init(void)
{
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    sd_ble_gap_device_name_set(&sec_mode,
                                (const uint8_t *)"DriveSync",
                                strlen("DriveSync"));

    ble_gap_conn_params_t conn_params = {
        .min_conn_interval = MSEC_TO_UNITS(15, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(30, UNIT_1_25_MS),
        .slave_latency = 0,
        .conn_sup_timeout = MSEC_TO_UNITS(4000, UNIT_10_MS),
    };
    sd_ble_gap_ppcp_set(&conn_params);
}

static void services_init(void)
{
    ble_uuid_t service_uuid = {
        .uuid = BLE_DRIVE_SERVICE_UUID,
        .type = BLE_UUID_TYPE_VENDOR_BEGIN,
    };

    /* Add custom service */
    sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                              &service_uuid, &s_service_handle);

    /* Add data characteristic (write + notify) */
    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.notify = 1;
    char_md.char_props.write = 1;
    char_md.p_char_user_desc = NULL;

    ble_uuid_t char_uuid = {
        .uuid = BLE_DRIVE_CHAR_UUID,
        .type = BLE_UUID_TYPE_VENDOR_BEGIN,
    };

    ble_gatts_attr_md_t attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_gatts_attr_t attr_char_value = {0};
    attr_char_value.p_uuid = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.max_len = DS_MAX_PACKET_LEN;
    attr_char_value.init_len = 0;

    sd_ble_gatts_characteristic_add(s_service_handle, &char_md,
                                     &attr_char_value, &s_char_handle);
}

/* ── BLE Event Handler ──────────────────────────────────────────── */

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        s_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        s_connected = true;
        if (s_connected_cb) s_connected_cb();
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        s_conn_handle = BLE_CONN_HANDLE_INVALID;
        s_connected = false;
        if (s_disconnected_cb) s_disconnected_cb();
        ble_periph_advertise();
        break;

    case BLE_GATTS_EVT_WRITE: {
        ble_gatts_evt_write_t const *p_write = &p_ble_evt->evt.gatts_evt.params.write;
        if (p_write->handle == s_char_handle && s_cmd_handler) {
            s_cmd_handler(p_write->data, (uint8_t)p_write->len);
        }
        break;
    }

    default:
        break;
    }
}

NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBS_PRIO, ble_evt_handler, NULL);

/* ── Public API ──────────────────────────────────────────────────── */

void ble_periph_init(ble_cmd_cb_t cmd_handler,
                     ble_connected_cb_t connected_cb,
                     ble_disconnected_cb_t disconnected_cb)
{
    s_cmd_handler = cmd_handler;
    s_connected_cb = connected_cb;
    s_disconnected_cb = disconnected_cb;

    /* Enable SoftDevice */
    nrf_sdh_enable_request();

    /* Configure BLE stack */
    uint32_t ram_start = 0;
    nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    nrf_sdh_ble_enable(&ram_start);

    gap_params_init();
    services_init();

    NRF_LOG_INFO("BLE peripheral initialized");
}

void ble_periph_advertise(void)
{
    ble_gap_adv_data_t adv_data = {0};
    ble_gap_adv_params_t adv_params = {0};

    /* Advertising data */
    uint8_t adv_flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    uint8_t name[] = "DriveSync";

    static uint8_t enc_advdata[31];
    enc_advdata[0] = 0x02;  /* length */
    enc_advdata[1] = BLE_GAP_AD_TYPE_FLAGS;
    enc_advdata[2] = adv_flags;
    enc_advdata[3] = strlen((const char *)name) + 1;
    enc_advdata[4] = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&enc_advdata[5], name, strlen((const char *)name));

    adv_data.adv_data.p_data = enc_advdata;
    adv_data.adv_data.len = 5 + strlen((const char *)name);

    adv_params.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.interval = MSEC_TO_UNITS(100, UNIT_0_625_MS);

    sd_ble_gap_adv_set(&adv_data, &adv_params);
    NRF_LOG_INFO("BLE advertising started");
}

void ble_periph_send(const uint8_t *data, uint8_t len)
{
    if (!s_connected || s_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    uint16_t data_len = len;
    ble_gatts_hvx_params_t hvx_params = {0};
    hvx_params.handle = s_char_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len = &data_len;
    hvx_params.p_data = (uint8_t *)data;

    sd_ble_gatts_hvx(s_conn_handle, &hvx_params);
}

bool ble_periph_is_connected(void)
{
    return s_connected;
}
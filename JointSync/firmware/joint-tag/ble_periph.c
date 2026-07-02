/**
 * JointSync Joint Tag — BLE Peripheral
 *
 * nRF52840 BLE 5.0 peripheral with JointSync GATT service.
 *
 * License: MIT
 */

#include "ble_periph.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "app_timer.h"
#include "nrf_log.h"

#define JOINTSYNC_SERVICE_UUID  0x4A53
#define JOINTSYNC_CHAR_DATA     0x4A01
#define JOINTSYNC_CHAR_CMD      0x4A02
#define JOINTSYNC_CHAR_STATUS   0x4A03
#define JOINTSYNC_CHAR_CONFIG   0x4A04

#define DEVICE_NAME "JointSync_Tag"
#define APP_ADV_INTERVAL 64  /* 40 ms (64 × 0.625 ms) */
#define APP_ADV_DURATION  180 /* 180 seconds */

static ble_cb_t g_cmd_cb = NULL;
static ble_connect_cb_t g_connect_cb = NULL;
static ble_disconnect_cb_t g_disconnect_cb = NULL;

static uint16_t g_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t g_service_handle;
static uint16_t g_char_data_handle;
static uint16_t g_char_cmd_handle;
static uint16_t g_char_status_handle;

static uint8_t g_adv_handle;

/* ── Service UUID ────────────────────────────────────────────────── */

static ble_uuid_t g_service_uuid = {
    .uuid = JOINTSYNC_SERVICE_UUID,
    .type = BLE_UUID_TYPE_VENDOR_BEGIN,
};

/* ── Advertising Data ────────────────────────────────────────────── */

static void advertising_init(void)
{
    ble_advdata_t advdata;
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    ble_advdata_t srdata;
    memset(&srdata, 0, sizeof(srdata));
    srdata.uuids_complete.uuid_cnt = 1;
    srdata.uuids_complete.p_uuids = &g_service_uuid;

    ble_gap_adv_params_t adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr = NULL;
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = APP_ADV_INTERVAL;
    adv_params.duration = APP_ADV_DURATION;
    adv_params.max_adv_evts = 0;

    sd_ble_gap_adv_set_configure(&g_adv_handle, &advdata, &adv_params);
}

/* ── GATT Service Init ───────────────────────────────────────────── */

static void service_init(void)
{
    ble_uuid128_t base_uuid = {{0x53, 0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    sd_ble_uuid_vs_add(&base_uuid, &g_service_uuid.type);

    /* Add service */
    sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &g_service_uuid, &g_service_handle);

    /* Add Data characteristic (notify) */
    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.notify = 1;
    ble_gatts_attr_t attr_char_value = {0};
    ble_gatts_attr_md_t attr_md = {0};
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    ble_uuid_t char_uuid = {JOINTSYNC_CHAR_DATA, g_service_uuid.type};
    attr_char_value.p_uuid = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = 245;
    attr_char_value.max_len = 245;
    sd_ble_gatts_characteristic_add(g_service_handle, &char_md, &attr_char_value, &g_char_data_handle);

    /* Add Command characteristic (write) */
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.write = 1;
    ble_uuid_t cmd_uuid = {JOINTSYNC_CHAR_CMD, g_service_uuid.type};
    attr_char_value.p_uuid = &cmd_uuid;
    sd_ble_gatts_characteristic_add(g_service_handle, &char_md, &attr_char_value, &g_char_cmd_handle);
}

/* ── BLE Event Handler ───────────────────────────────────────────── */

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        g_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        if (g_connect_cb) g_connect_cb();
        NRF_LOG_INFO("BLE connected");
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        g_conn_handle = BLE_CONN_HANDLE_INVALID;
        if (g_disconnect_cb) g_disconnect_cb();
        NRF_LOG_INFO("BLE disconnected");
        break;

    case BLE_GATTS_EVT_WRITE: {
        ble_gatts_evt_write_t const *p_write = &p_ble_evt->evt.gatts_evt.params.write;
        if (p_write->handle == g_char_cmd_handle && g_cmd_cb) {
            g_cmd_cb(p_write->data, p_write->len);
        }
        break;
    }

    default:
        break;
    }
}

/* ── Public API ───────────────────────────────────────────────────── */

void ble_periph_init(ble_cb_t cmd_cb,
                     ble_connect_cb_t connect_cb,
                     ble_disconnect_cb_t disconnect_cb)
{
    g_cmd_cb = cmd_cb;
    g_connect_cb = connect_cb;
    g_disconnect_cb = disconnect_cb;

    /* Initialize SoftDevice */
    ble_cfg_id_ble_conn_cfg_tag_t conn_cfg = {0};
    sd_ble_enable(&conn_cfg);

    /* Set device name */
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));

    service_init();
    advertising_init();

    NRF_LOG_INFO("BLE peripheral initialized");
}

void ble_periph_advertise(void)
{
    sd_ble_gap_adv_start(g_adv_handle, BLE_CONN_CFG_TAG_DEFAULT);
    NRF_LOG_INFO("BLE advertising started");
}

void ble_periph_send(uint8_t *data, uint8_t len)
{
    if (g_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    uint16_t hvx_len = len;
    ble_gatts_hvx_params_t hvx_params = {0};
    hvx_params.handle = g_char_data_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len = &hvx_len;
    hvx_params.p_data = data;

    sd_ble_gatts_hvx(g_conn_handle, &hvx_params);
}

bool ble_periph_is_connected(void)
{
    return g_conn_handle != BLE_CONN_HANDLE_INVALID;
}
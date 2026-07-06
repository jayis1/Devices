/**
 * SightSync BLE Peripheral — nRF52840 implementation using Bluefruit.
 *
 * License: MIT
 */

#include "ble_periph.h"
#include <bluefruit.h>

static BLEService        ss_svc(SS_BLE_SVC_UUID);
static BLECharacteristic ss_char_tx(SS_BLE_CHAR_TX, BLENotify, 256);
static BLECharacteristic ss_char_rx(SS_BLE_CHAR_RX, BLEWrite, 256);

static ss_ble_rx_callback_t s_rx_cb = NULL;
static uint16_t s_node_id = 0;
static bool s_connected = false;

/* ── Connection callback ─────────────────────────────────────────── */

static void on_connect(uint16_t conn_handle)
{
    (void)conn_handle;
    s_connected = true;
}

static void on_disconnect(uint16_t conn_handle, uint8_t reason)
{
    (void)conn_handle;
    (void)reason;
    s_connected = false;
    Bluefruit.Advertising.start(0);  /* restart advertising */
}

/* ── RX write callback ───────────────────────────────────────────── */

static void on_rx_write(uint16_t conn_hdl, BLECharacteristic *chr)
{
    (void)conn_hdl;
    uint8_t buf[256];
    uint16_t len = chr->read(buf, sizeof(buf));

    sightsync_header_t header;
    const uint8_t *payload = NULL;

    if (sightsync_decode(buf, (uint8_t)len, &header, &payload)) {
        if (s_rx_cb != NULL) {
            s_rx_cb(&header, payload);
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void ss_ble_periph_init(uint16_t node_id, ss_ble_rx_callback_t rx_cb)
{
    s_node_id = node_id;
    s_rx_cb = rx_cb;

    Bluefruit.begin();
    Bluefruit.setTxPower(0);     /* 0 dBm — short range, low power */
    Bluefruit.setName("SightSync Eye Tag");

    /* Set connection/disconnection callbacks */
    Bluefruit.Periph.setConnectCallback(on_connect);
    Bluefruit.Periph.setDisconnectCallback(on_disconnect);

    /* Setup service */
    ss_svc.begin();

    ss_char_tx.begin();
    ss_char_tx.setBufferLen(256);

    ss_char_rx.begin();
    ss_char_rx.setWriteCallback(on_rx_write);

    /* Add service to advertising */
    Bluefruit.Advertising.addService(ss_svc);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.setInterval(32, 244);  /* 20ms – 152ms */
    Bluefruit.Advertising.setTimeout(0);          /* no timeout */
}

void ss_ble_periph_start_advertising(void)
{
    Bluefruit.Advertising.start(0);
}

void ss_ble_periph_stop_advertising(void)
{
    Bluefruit.Advertising.stop();
}

void ss_ble_periph_send(const uint8_t *data, uint8_t len)
{
    if (s_connected) {
        ss_char_tx.notify(data, len);
    }
}

bool ss_ble_periph_is_connected(void)
{
    return s_connected;
}
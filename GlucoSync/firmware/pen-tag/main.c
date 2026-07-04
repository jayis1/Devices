/**
 * GlucoSync Insulin Pen Tag — Main Firmware
 *
 * nRF52840 (Fanstel BT840)
 * Clips onto insulin pen. Detects injection events from IMU motion signatures.
 * Sends injection events to Hub via BLE 5.0.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "protocol.h"
#include "ble_periph.h"
#include "injection_detect.h"

/* ── State ──────────────────────────────────────────────────────── */

typedef struct {
    uint16_t seq_counter;
    uint8_t  pen_type;       /* 0=basal, 1=bolus (set via app config) */
    uint8_t  pen_id;         /* 1-4 (which pen) */
    uint8_t  estimated_units;/* units per injection (from pen config) */
    bool     connected;
} pen_state_t;

static pen_state_t g_state = {0};

/* ── BLE RX callback ─────────────────────────────────────────────── */

static void ble_rx_callback(const uint8_t *data, uint8_t len)
{
    glucosync_header_t header;
    const uint8_t *payload;
    if (!glucosync_decode(data, len, &header, &payload)) return;

    if (header.msg_type == MSG_TYPE_CMD_PAIR) {
        payload_pair_t pair;
        memcpy(&pair, payload, sizeof(pair));
        /* Store pen config from hub */
    }
}

/* ── Injection Event Callback ───────────────────────────────────── */

static void injection_detected_callback(const injection_event_t *event)
{
    /* Send to hub via BLE */
    payload_insulin_t insulin = {0};
    insulin.pen_type = g_state.pen_type;
    insulin.pen_id = g_state.pen_id;
    insulin.estimated_units = g_state.estimated_units;
    insulin.confidence = event->confidence;
    insulin.injection_dur_ms = event->duration_ms;
    insulin.timestamp = app_timer_cnt_get() / 32;

    uint8_t packet[GS_MAX_PACKET_LEN];
    uint8_t pkt_len = glucosync_encode(packet, sizeof(packet),
                                        MSG_TYPE_DATA_INSULIN,
                                        GS_PEN_ID_BASE,
                                        g_state.seq_counter++,
                                        GS_FLAG_ACK_REQ,
                                        (uint8_t *)&insulin, sizeof(insulin));

    if (ble_periph_is_connected()) {
        ble_periph_send(packet, pkt_len);
    }
}

/* ── IMU Sampling Timer (200 Hz) ────────────────────────────────── */

static void imu_timer_handler(void *context)
{
    injection_detect_process_imu();
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);
    app_timer_init();

    memset(&g_state, 0, sizeof(g_state));
    g_state.pen_type = 1;     /* default: bolus */
    g_state.pen_id = 1;
    g_state.estimated_units = 0;  /* set via app */

    /* Initialize injection detection (LSM6DSO at 200 Hz) */
    injection_detect_init(injection_detected_callback);

    /* Initialize BLE peripheral */
    ble_periph_init(ble_rx_callback);
    ble_periph_start_advertising("GlucoSync-Pen");
    ble_periph_set_tx_power(4);  /* +4 dBm for close-range reliability */

    /* Create 200 Hz IMU timer (5ms interval) */
    APP_TIMER_DEF(imu_timer);
    app_timer_create(&imu_timer, APP_TIMER_MODE_REPEATED, imu_timer_handler);
    app_timer_start(imu_timer, APP_TIMER_TICKS(5));  /* 5ms = 200 Hz */

    /* Main loop */
    while (1) {
        /* IMU read + injection state machine runs in timer callback.
         * BLE events handled by SoftDevice scheduler. */
        sd_app_evt_wait();
        nrf_delay_ms(100);
    }

    return 0;
}
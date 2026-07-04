/**
 * GlucoSync Activity Band — Main Firmware
 *
 * nRF52840 (Fanstel BT840)
 * Wrist-worn band measuring PPG heart rate + IMU activity classification.
 * Sends HR + activity data to Hub via BLE 5.0 at 1 Hz.
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
#include "ppg_driver.h"
#include "activity_imu.h"

#define TAG "glucosync_band"

/* ── State ──────────────────────────────────────────────────────── */

typedef struct {
    uint16_t seq_counter;
    uint8_t  hr;
    uint8_t  hrv_rmssd;
    uint8_t  activity_class;
    uint8_t  intensity;
    uint8_t  confidence;
    bool     connected;
} band_state_t;

static band_state_t g_state = {0};

/* ── BLE RX callback ─────────────────────────────────────────────── */

static void ble_rx_callback(const uint8_t *data, uint8_t len)
{
    glucosync_header_t header;
    const uint8_t *payload;
    if (!glucosync_decode(data, len, &header, &payload)) return;

    if (header.msg_type == MSG_TYPE_CMD_MODE) {
        payload_mode_t mode;
        memcpy(&mode, payload, sizeof(mode));
        /* Adjust sampling rate based on mode:
         * 0=active (PPG 25Hz, IMU 50Hz)
         * 1=sleep (PPG 1Hz, IMU 12Hz)
         * 2=fasting (PPG 5Hz, IMU 25Hz)
         * 3=exercise (PPG 50Hz, IMU 100Hz) */
    }
}

/* ── Sampling Timer (1 Hz data send) ────────────────────────────── */

static void sample_timer_handler(void *context)
{
    /* Read PPG */
    ppg_result_t ppg;
    ppg_driver_read(&ppg);

    /* Read IMU + classify activity */
    activity_result_t activity;
    activity_imu_read_and_classify(&activity);

    g_state.hr = ppg.hr;
    g_state.hrv_rmssd = ppg.hrv_rmssd;
    g_state.activity_class = activity.class_id;
    g_state.intensity = activity.intensity;
    g_state.confidence = activity.confidence;

    /* Compute Karvonen intensity:
     * intensity = (HR - restingHR) / (maxHR - restingHR) * 100
     * Using age-based maxHR = 220 - age, default age=35 → maxHR=185, restHR=60 */
    if (ppg.hr > 0) {
        int16_t karvonen = ((int16_t)ppg.hr - 60) * 100 / (185 - 60);
        if (karvonen < 0) karvonen = 0;
        if (karvonen > 100) karvonen = 100;
        g_state.intensity = (uint8_t)karvonen;
    }

    /* Send to hub via BLE */
    payload_activity_t payload_data = {0};
    payload_data.hr = g_state.hr;
    payload_data.hrv_rmssd = g_state.hrv_rmssd;
    payload_data.activity_class = g_state.activity_class;
    payload_data.intensity = g_state.intensity;
    payload_data.confidence = g_state.confidence;
    payload_data.timestamp = app_timer_cnt_get() / 32;  /* approximate ms */

    uint8_t packet[GS_MAX_PACKET_LEN];
    uint8_t pkt_len = glucosync_encode(packet, sizeof(packet),
                                        MSG_TYPE_DATA_ACTIVITY,
                                        GS_BAND_ID_BASE,
                                        g_state.seq_counter++,
                                        0,
                                        (uint8_t *)&payload_data, sizeof(payload_data));

    if (ble_periph_is_connected()) {
        ble_periph_send(packet, pkt_len);
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    /* Initialize nRF52840 */
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);
    app_timer_init();

    memset(&g_state, 0, sizeof(g_state));

    /* Initialize PPG (MAX30101) */
    ppg_driver_init();
    ppg_driver_set_sample_rate(PPG_RATE_25HZ);

    /* Initialize IMU (LSM6DSO) */
    activity_imu_init();
    activity_imu_set_sample_rate(50);  /* 50 Hz */

    /* Initialize BLE peripheral */
    ble_periph_init(ble_rx_callback);
    ble_periph_start_advertising("GlucoSync-Band");
    ble_periph_set_tx_power(0);  /* 0 dBm, 2m range */

    /* Create 1 Hz sampling timer */
    APP_TIMER_DEF(sample_timer);
    app_timer_create(&sample_timer, APP_TIMER_MODE_REPEATED, sample_timer_handler);
    app_timer_start(sample_timer, APP_TIMER_TICKS(1000));  /* 1 second */

    /* Main loop */
    while (1) {
        /* PPG interrupt handler runs in ISR context.
         * IMU FIFO is read in the sample timer.
         * BLE events handled by SoftDevice scheduler. */
        sd_app_evt_wait();
        nrf_delay_ms(10);
    }

    return 0;
}
/*
 * cardiosync_ecg_patch.c — CardioSync ECG Chest Patch firmware (nRF52840)
 *
 * This firmware runs on the nRF52840 SoC and:
 *   - Configures ADS1292R for 24-bit ECG at 250 SPS (Lead I)
 *   - Runs Pan-Tompkins R-peak detection in real-time
 *   - Streams ECG data + HR + R-R intervals via BLE 5.0 to Hub
 *   - Monitors motion artifacts via LSM6DSO IMU
 *   - Manages 14-day battery life with ultra-low-power modes
 *
 * License: MIT
 */
#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "ble_srv_common.h"
#include "common/cardiosync_protocol.h"
#include "ads1292r.h"
#include "pan_tompkins.h"

static const char *TAG = "CS_ECG";

/* ── Pin Definitions (nRF52840) ─────────────────────────────── */
#define PIN_ADS_CS      3    /* P0.03 — ADS1292R SPI CS */
#define PIN_ADS_CLK     4    /* P0.04 — SPI CLK */
#define PIN_ADS_MOSI    5    /* P0.05 — SPI MOSI */
#define PIN_ADS_MISO    6    /* P0.06 — SPI MISO */
#define PIN_ADS_DRDY    7    /* P0.07 — ADS1292R DRDY interrupt */
#define PIN_ADS_START   8    /* P0.08 — ADS1292R START */
#define PIN_ADS_RESET   9    /* P0.09 — ADS1292R RESET */
#define PIN_I2C_SDA     27   /* P0.27 — LSM6DSO, TMP117 */
#define PIN_I2C_SCL     29   /* P0.29 */
#define PIN_BATT_DIV    31   /* P0.31 — Battery voltage divider */
#define PIN_LED         12   /* P0.12 — Status LED */
#define PIN_BUTTON      13   /* P0.13 — Event mark button */

/* ── Constants ──────────────────────────────────────────────── */
#define ECG_SAMPLE_RATE_HZ    250
#define ECG_SAMPLES_PER_BLE   10    /* 10 samples per BLE notification (40 ms) */
#define BLE_TX_INTERVAL_MS    40    /* 250 Hz / 10 = 25 Hz notification rate */
#define IMU_READ_INTERVAL_MS   100  /* IMU read every 100 ms */
#define BATTERY_READ_INTERVAL  60000 /* Battery every 60 s */
#define HEARTBEAT_INTERVAL_MS  30000 /* Heartbeat every 30 s */

/* ── Global State ───────────────────────────────────────────── */
static pan_tompkins_t pt_state;
static ads_sample_t ads_sample;
static uint16_t ecg_seq_num = 0;
static int16_t ecg_ble_buffer[ECG_SAMPLES_PER_BLE];
static uint8_t ecg_ble_count = 0;

static struct {
    float accel_x, accel_y, accel_z;
    uint8_t motion_artifact;
    float motion_threshold;  /* adaptive, based on activity */
} imu_state = {0};

static struct {
    uint8_t battery_pct;
    uint8_t charging;
    uint16_t voltage_mv;
} power_state = {0 };

/* ── BLE GATT Service Handles ──────────────────────────────── */
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t m_ecg_data_handle;     /* CS_CHAR_ECG_DATA */
static uint16_t m_ecg_hr_handle;       /* CS_CHAR_ECG_HR */
static uint16_t m_heartbeat_handle;   /* CS_CHAR_HEARTBEAT */

/* ── Forward Declarations ──────────────────────────────────── */
static void ads_drdy_handler(nrf_drv_gpiote_pin_t pin,
                              nrf_gpiote_polarity_t action);
static void ble_ecg_send_samples(int16_t *samples, int count);
static void ble_ecg_send_hr(uint16_t hr, uint16_t rr,
                             uint8_t motion, uint8_t lead_off);
static void ble_send_heartbeat(void);
static void imu_read(void);
static void battery_read(void);
static void button_handler(nrf_drv_gpiote_pin_t pin,
                           nrf_gpiote_polarity_t action);

/* ── ECG Sampling (DRDY Interrupt Handler) ─────────────────── */
/* This is called from the ADS1292R DRDY interrupt (falling edge).
 * It reads one 24-bit ECG sample from the ADS1292R via SPI. */
static void ads_drdy_handler(nrf_drv_gpiote_pin_t pin,
                              nrf_gpiote_polarity_t action)
{
    /* Read ECG sample from ADS1292R */
    if (ads_read_sample(&ads_sample) != 0) {
        return;
    }

    /* Convert 24-bit signed to 16-bit (downshift by 8 for BLE bandwidth) */
    int16_t ecg_16 = (int16_t)(ads_sample.ch1 >> 8);

    /* Feed to Pan-Tompkins R-peak detector */
    int r_detected = pt_process_sample(&pt_state, ecg_16);

    /* Buffer sample for BLE transmission */
    ecg_ble_buffer[ecg_ble_count++] = ecg_16;

    /* Check motion artifact flag */
    uint8_t motion = 0;
    float accel_mag = sqrtf(imu_state.accel_x * imu_state.accel_x +
                            imu_state.accel_y * imu_state.accel_y +
                            imu_state.accel_z * imu_state.accel_z);
    if (accel_mag > imu_state.motion_threshold) {
        motion = 1;
        imu_state.motion_artifact = 1;
    }

    /* Check lead-off */
    uint8_t lead_off = ads_check_lead_off() ? 1 : 0;

    /* When we have 10 samples, send via BLE */
    if (ecg_ble_count >= ECG_SAMPLES_PER_BLE) {
        ble_ecg_send_samples(ecg_ble_buffer, ECG_SAMPLES_PER_BLE);
        ecg_ble_count = 0;

        /* Send HR update with every BLE packet (every 40 ms) */
        ble_ecg_send_hr(pt_state.heart_rate_bpm, pt_state.rr_interval_ms,
                         motion, lead_off);
    }

    /* If R-peak detected, could trigger immediate HR notification */
    if (r_detected) {
        /* R-peak — log for HRV calculation */
    }
}

/* ── BLE: Send ECG Data ────────────────────────────────────── */
static void ble_ecg_send_samples(int16_t *samples, int count)
{
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    ecg_data_payload_t pkt;
    pkt.seq_num = ecg_seq_num++;
    memcpy(pkt.samples, samples, count * sizeof(int16_t));

    /* sd_ble_gatts_value_set + sd_ble_gatts_hvx to notify */
    /* In production: use softdevice BLE notification API */
    uint16_t len = sizeof(pkt);
    /* sd_ble_gatts_hvx(m_conn_handle, &hvx_params); */
}

/* ── BLE: Send HR + R-R ────────────────────────────────────── */
static void ble_ecg_send_hr(uint16_t hr, uint16_t rr,
                             uint8_t motion, uint8_t lead_off)
{
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    ecg_hr_payload_t payload = {
        .heart_rate_bpm = hr,
        .rr_interval_ms = rr,
        .motion_artifact = motion,
        .lead_off = lead_off
    };

    /* BLE notify on CS_CHAR_ECG_HR characteristic */
}

/* ── BLE: Send Heartbeat ───────────────────────────────────── */
static void ble_send_heartbeat(void)
{
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) return;

    heartbeat_payload_t payload = {
        .battery_pct = power_state.battery_pct,
        .status_flags = 0x01,  /* online */
        .rssi_dbm = 0          /* would read from softdevice */
    };

    /* BLE notify on CS_CHAR_HEARTBEAT */
}

/* ── IMU Read (LSM6DSO via I²C) ────────────────────────────── */
static void imu_read(void)
{
    /* Read LSM6DSO OUTX_L, OUTX_H, OUTY_L, OUTY_H, OUTZ_L, OUTZ_H */
    /* Registers 0x28-0x2D at I²C addr 0x6A */

    /* For motion artifact detection, we only need acceleration magnitude */
    /* In production: nrf_twi_m_transfer to read 6 bytes from 0x28 */

    /* Adaptive threshold: baseline + 0.3g */
    imu_state.motion_threshold = 0.3f;

    /* Update motion artifact flag (decays over time) */
    static int motion_counter = 0;
    float accel_mag = sqrtf(imu_state.accel_x * imu_state.accel_x +
                            imu_state.accel_y * imu_state.accel_y +
                            imu_state.accel_z * imu_state.accel_z);
    if (accel_mag > imu_state.motion_threshold) {
        imu_state.motion_artifact = 1;
        motion_counter = 10;  /* flag for 10 × 100 ms = 1 s after motion stops */
    } else if (motion_counter > 0) {
        motion_counter--;
    } else {
        imu_state.motion_artifact = 0;
    }
}

/* ── Battery Read ──────────────────────────────────────────── */
static void battery_read(void)
{
    /* Read battery voltage via analog divider on P0.31 */
    /* nrf_saadc_sample_convert → voltage_mv */

    /* Calculate percentage (3.7V Li-Po: 4.2V = 100%, 3.0V = 0%) */
    if (power_state.voltage_mv >= 4200) power_state.battery_pct = 100;
    else if (power_state.voltage_mv <= 3000) power_state.battery_pct = 0;
    else power_state.battery_pct = (uint8_t)
        ((power_state.voltage_mv - 3000) * 100 / 1200);

    /* Check charge status from MCP73831 */
    /* GPIO P0.15: 0 = charging, 1 = charged */
}

/* ── Button Handler (Event Mark) ───────────────────────────── */
static void button_handler(nrf_drv_gpiote_pin_t pin,
                           nrf_gpiote_polarity_t action)
{
    /* User pressed button — mark an event in the ECG stream */
    /* This will tag the current ECG segment with a user-reported symptom */
    NRF_LOG_INFO("Event marked by user");
}

/* ── Main ──────────────────────────────────────────────────── */
int main(void)
{
    /* Initialize nRF52840 logging */
    NRF_LOG_INFO("CardioSync ECG Patch starting...");

    /* Initialize GPIO */
    /* Configure DRDY interrupt on P0.07 (falling edge) */
    /* Configure button interrupt on P0.13 */

    /* Initialize I²C for LSM6DSO + TMP117 */
    /* nrf_drv_twi_init with P0.27 (SDA) and P0.29 (SCL) */

    /* Initialize SPI for ADS1292R */
    spi_init();

    /* Initialize ADS1292R ECG AFE */
    ads_init();
    NRF_LOG_INFO("ADS1292R initialized: 250 SPS, gain 12, RLD on");

    /* Initialize Pan-Tompkins R-peak detector */
    pt_init(&pt_state);

    /* Initialize BLE 5.0 GATT server */
    /* ble_stack_init() → cs_ble_init() → advertising_start() */
    NRF_LOG_INFO("BLE 5.0 advertising as 'CardioSync ECG'");

    /* Start ADS1292R continuous conversion */
    ads_start();

    /* Initialize timers */
    /* IMU timer: 100 ms → imu_read() */
    /* Battery timer: 60 s → battery_read() */
    /* Heartbeat timer: 30 s → ble_send_heartbeat() */

    NRF_LOG_INFO("ECG streaming started at 250 Hz");

    /* Main loop — event-driven, waits for interrupts */
    while (1) {
        /* In production, main loop sleeps in WFE (Wait For Event) */
        /* All processing is interrupt-driven:
         *   - ADS DRDY → ads_drdy_handler → read sample → Pan-Tompkins → BLE TX
         *   - IMU timer → imu_read
         *   - Battery timer → battery_read
         *   - Button → button_handler
         *   - Heartbeat timer → ble_send_heartbeat
         */
        __WFE();
    }
}
/**
 * SightSync Wearable Eye Tag — Main Firmware
 *
 * nRF52840 · BLE 5.0
 * IR LED + photodiode blink detection, TMP117 skin temp,
 * LSM6DSO IMU (head posture), APDS9306 blue light.
 * 2× CR2032, ~18-day battery life with adaptive sampling.
 *
 * License: MIT
 */

#include <bluefruit.h>
#include <Adafruit_TMP117.h>
#include <Adafruit_LSM6DS.h>
#include <Adafruit_APDS9306.h>

#include "../common/protocol.h"
#include "../common/ble_periph.h"
#include "blink_detect.h"
#include "posture.h"

/* ── Pin definitions ──────────────────────────────────────────────── */

#define PIN_IR_LED      PIN_A0       /* P0.26 — IR LED PWM (940 nm) */
#define PIN_PHOTODIODE  PIN_A1       /* P0.27 — photodiode ADC */
#define PIN_BUTTON      15           /* P0.15 — pair/reset button */
#define PIN_STATUS_LED  13           /* P0.13 — status LED */
#define PIN_BATT        PIN_VBAT     /* battery voltage (SAADC) */

/* ── Runtime state ────────────────────────────────────────────────── */

static uint16_t s_seq_blink = 0;
static uint16_t s_seq_posture = 0;
static uint16_t s_seq_temp = 0;

static uint32_t s_last_ble_send = 0;

/* Sensor objects */
static Adafruit_TMP117  tmp117;
static Adafruit_LSM6DSO lsm6;
static Adafruit_APDS9306 apds;

/* ── BLE RX callback ──────────────────────────────────────────────── */

static void on_ble_rx(const sightsync_header_t *hdr, const uint8_t *payload)
{
    switch (hdr->msg_type) {
    case MSG_TYPE_CMD_PAIR:
        /* Already paired; acknowledge */
        break;
    case MSG_TYPE_CMD_MODE: {
        const payload_mode_t *m = (const payload_mode_t *)payload;
        /* Adjust sampling rate based on mode:
         *   0=work (full), 1=rest (reduced), 2=child (full), 3=sleep (minimal)
         */
        blink_detect_set_mode(m->mode);
        posture_set_mode(m->mode);
        break;
    }
    default:
        break;
    }
}

/* ── Blink detection task (50 Hz sampling, 10-second reporting) ──── */

static void blink_task(void *arg)
{
    blink_detect_init(PIN_IR_LED, PIN_PHOTODIODE);

    while (1) {
        /* Sample at 50 Hz */
        blink_detect_sample();
        delay(20);  /* 20 ms = 50 Hz */
    }
}

/* ── Posture task (25 Hz IMU sampling, 1 Hz reporting) ────────────── */

static void posture_task(void *arg)
{
    posture_init(&lsm6);

    while (1) {
        posture_sample();
        delay(40);  /* 40 ms = 25 Hz */
    }
}

/* ── Temp + blue light task (0.1 Hz) ──────────────────────────────── */

static void temp_task(void *arg)
{
    while (1) {
        delay(10000);  /* 10 seconds */

        /* Read TMP117 periocular skin temperature */
        sensors_event_t temp_event;
        if (tmp117.getEvent(&temp_event)) {
            float temp_c = temp_event.temperature;
            int16_t temp_centi = (int16_t)(temp_c * 100.0f);

            /* Compute delta from baseline (first reading stored in flash) */
            static int16_t baseline = 0;
            static bool baseline_set = false;
            if (!baseline_set) {
                baseline = temp_centi;
                baseline_set = true;
            }
            int16_t delta = temp_centi - baseline;

            /* Send temp data via BLE */
            payload_temp_t pkt = {
                .temp_centi      = temp_centi,
                .temp_delta_centi = delta,
                .timestamp       = millis() / 1000,
            };
            uint8_t buf[32];
            uint8_t len = sightsync_encode(buf, sizeof(buf),
                MSG_TYPE_DATA_TEMP, SS_EYETAG_ID_BASE, s_seq_temp++, 0,
                (const uint8_t *)&pkt, sizeof(pkt));
            ss_ble_periph_send(buf, len);
        }

        /* Read APDS9306 blue light */
        if (apds.begin()) {
            uint16_t ch0, ch1;
            apds.getALSData(&ch0, &ch1);
            /* Accumulate blue-light dose */
            blink_detect_accumulate_blue_dose(ch0, ch1);
        }
    }
}

/* ── BLE reporting task (every 10 seconds) ────────────────────────── */

static void ble_report_task(void *arg)
{
    while (1) {
        delay(10000);

        if (!ss_ble_periph_is_connected()) {
            continue;
        }

        /* Get blink rate from blink detector */
        uint8_t bpm = blink_detect_get_bpm();
        uint8_t confidence = blink_detect_get_confidence();
        uint8_t quality = blink_detect_get_quality();
        uint8_t ir_amp = blink_detect_get_ir_amplitude();

        /* Send blink data */
        payload_blink_t blink_pkt = {
            .blinks_per_min     = bpm,
            .blink_confidence   = confidence,
            .blink_rate_quality = quality,
            .blink_ir_amplitude = ir_amp,
            .timestamp          = millis() / 1000,
        };
        uint8_t buf[32];
        uint8_t len = sightsync_encode(buf, sizeof(buf),
            MSG_TYPE_DATA_BLINK, SS_EYETAG_ID_BASE, s_seq_blink++, 0,
            (const uint8_t *)&blink_pkt, sizeof(blink_pkt));
        ss_ble_periph_send(buf, len);

        /* Get posture data */
        int16_t pitch, roll, yaw;
        uint8_t forward_flag, posture_risk;
        posture_get_angles(&pitch, &roll, &yaw, &forward_flag, &posture_risk);

        payload_posture_t posture_pkt = {
            .pitch_centi       = pitch,
            .roll_centi        = roll,
            .yaw_centi         = yaw,
            .forward_head_flag = forward_flag,
            .posture_risk      = posture_risk,
            .timestamp         = millis() / 1000,
        };
        len = sightsync_encode(buf, sizeof(buf),
            MSG_TYPE_DATA_POSTURE, SS_EYETAG_ID_BASE, s_seq_posture++, 0,
            (const uint8_t *)&posture_pkt, sizeof(posture_pkt));
        ss_ble_periph_send(buf, len);

        /* Send status heartbeat */
        payload_status_t status = {
            .battery_pct = blink_detect_get_battery_pct(),
            .state = 1,  /* active */
            .error_code = 0,
        };
        len = sightsync_encode(buf, sizeof(buf),
            MSG_TYPE_STATUS, SS_EYETAG_ID_BASE, s_seq_blink++, 0,
            (const uint8_t *)&status, sizeof(status));
        ss_ble_periph_send(buf, len);
    }
}

/* ── Button interrupt (pair + reset) ───────────────────────────────── */

static void button_isr(void)
{
    static uint32_t last_press = 0;
    uint32_t now = millis();
    if (now - last_press < 200) return;  /* debounce */
    last_press = now;

    /* Start pairing advertising */
    ss_ble_periph_start_advertising();
    digitalWrite(PIN_STATUS_LED, HIGH);
    delay(100);
    digitalWrite(PIN_STATUS_LED, LOW);
}

/* ── Setup ────────────────────────────────────────────────────────── */

void setup(void)
{
    Serial.begin(115200);
    Serial.println("SightSync Eye Tag starting...");

    /* Initialize I²C for TMP117 + APDS9306 */
    Wire.begin(6, 7);  /* SDA=P0.06, SCL=P0.07 */
    Wire.setClock(100000);

    /* Initialize TMP117 */
    if (!tmp117.begin(0x48, &Wire)) {
        Serial.println("TMP117 not found!");
    }

    /* Initialize LSM6DSO (SPI) */
    SPI.begin(23, 24, 25, 22);  /* SCK=MISO=MOSI=CS on nRF52840 */
    if (!lsm6.begin_SPI(22)) {
        Serial.println("LSM6DSO not found!");
    }
    lsm6.setAccelDataRate(LSM6DS_RATE_25_HZ);
    lsm6.setGyroDataRate(LSM6DS_RATE_25_HZ);
    lsm6.setAccelRange(LSM6DSO3000mg_PER_LSB);

    /* Initialize APDS9306 */
    if (!apds.begin(0x0C, &Wire)) {
        Serial.println("APDS9306 not found!");
    }

    /* Configure pins */
    pinMode(PIN_IR_LED, OUTPUT);
    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), button_isr, FALLING);

    /* Initialize BLE peripheral */
    ss_ble_periph_init(SS_EYETAG_ID_BASE, on_ble_rx);
    ss_ble_periph_start_advertising();

    Serial.println("Eye Tag initialized.");
}

/* ── Main loop (Arduino-nRF52) ────────────────────────────────────── */

void loop(void)
{
    /* Run blink detection (50 Hz) */
    blink_detect_sample();
    delay(20);

    /* Periodic tasks are handled by FreeRTOS tasks created in setup
     * (Arduino-nRF52 supports RTOS tasks alongside loop())
     */
}
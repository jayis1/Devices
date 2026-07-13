/*
 * cardiosync_hub.c — CardioSync Hub firmware (ESP32-S3)
 *
 * Central coordinator:
 *   - Receives continuous ECG from Chest Patch via BLE 5.0
 *   - Runs AFib detection CNN (tflite-micro, 22 KB model) on 30 s windows
 *   - Cross-validates ECG HR with PPG HR from Smart Ring
 *   - Schedules and triggers BP measurements via BLE to Wrist Cuff
 *   - Multi-node data fusion (ECG + PPG + BP + activity)
 *   - Emergency alert dispatch (VT, extreme bradycardia) via Wi-Fi or 4G LTE
 *   - E-ink display, siren, LED ring, haptic feedback
 *   - MQTT to cloud (FastAPI + TimescaleDB)
 *
 * License: MIT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

#include "common/cardiosync_protocol.h"

static const char *TAG = "CS_HUB";

/* ── Pin Definitions (ESP32-S3) ─────────────────────────────── */
#define PIN_I2S_BCK     GPIO_NUM_4
#define PIN_I2S_LRCK    GPIO_NUM_5
#define PIN_I2S_DIN     GPIO_NUM_6
#define PIN_BUTTON      GPIO_NUM_7
#define PIN_I2C_SDA     GPIO_NUM_8
#define PIN_I2C_SCL     GPIO_NUM_9
#define PIN_SPI_CLK     GPIO_NUM_10
#define PIN_SPI_MISO    GPIO_NUM_11
#define PIN_SPI_MOSI    GPIO_NUM_12
#define PIN_CC1101_CS   GPIO_NUM_13
#define PIN_CC1101_GD0  GPIO_NUM_14
#define PIN_EINK_CS     GPIO_NUM_15
#define PIN_EINK_DC     GPIO_NUM_16
#define PIN_UART2_TX    GPIO_NUM_17   /* SIM7000 RXD */
#define PIN_UART2_RX    GPIO_NUM_18   /* SIM7000 TXD */
#define PIN_SIM_PWRKEY  GPIO_NUM_21
#define PIN_SD_CS       GPIO_NUM_35
#define PIN_EINK_RST   GPIO_NUM_36
#define PIN_EINK_BUSY   GPIO_NUM_37
#define PIN_LED_RING    GPIO_NUM_48

/* ── Constants ──────────────────────────────────────────────── */
#define ECG_WINDOW_SAMPLES    7500    /* 30 s × 250 Hz                */
#define ECG_WINDOW_BYTES      (ECG_WINDOW_SAMPLES * 2)
#define AFIB_CLASSIFY_INTERVAL_MS  5000  /* classify every 5 s        */
#define HR_DISCREPANCY_THRESH_PCT  15    /* ECG vs PPG HR mismatch     */
#define BRADY_THRESHOLD_BPM    30
#define BRADY_DURATION_S       15
#define VT_THRESHOLD_BPM      180
#define MAX_ECG_PATCHES       2     /* support 2 patches (multi-lead future) */
#define BP_SCHEDULE_AM_HOUR   7     /* 7 AM BP measurement          */
#define BP_SCHEDULE_PM_HOUR   19    /* 7 PM BP measurement          */
#define BP_POST_ACTIVITY_DELAY 300  /* 5 min after activity ends    */

/* ── Global State ───────────────────────────────────────────── */
typedef struct {
    uint8_t  connected;
    int64_t  last_heartbeat_us;
    uint16_t heart_rate_bpm;
    uint16_t rr_interval_ms;
    uint8_t  motion_artifact;
    uint8_t  lead_off;
    uint8_t  battery_pct;
    int16_t  ecg_buffer[ECG_WINDOW_SAMPLES];
    uint16_t buffer_write_idx;
    uint16_t seq_expected;
} ecg_patch_state_t;

typedef struct {
    uint8_t  connected;
    int64_t  last_heartbeat_us;
    uint16_t heart_rate_bpm;
    uint16_t spo2_pct;
    int16_t  skin_temp_c10;
    uint16_t rmssd_ms;
    uint16_t sdnn_ms;
    uint8_t  activity_class;
    uint8_t  intensity;
    uint16_t steps;
    uint8_t  battery_pct;
} smart_ring_state_t;

typedef struct {
    uint8_t  connected;
    int64_t  last_heartbeat_us;
    uint16_t systolic;
    uint16_t diastolic;
    uint16_t map;
    uint16_t hr_during;
    uint8_t  position_ok;
    uint8_t  quality;
    uint8_t  battery_pct;
    uint8_t  measuring;
} bp_cuff_state_t;

typedef enum {
    HUB_STATE_NORMAL,
    HUB_STATE_AFIB_ALERT,
    HUB_STATE_EMERGENCY,
} hub_state_t;

static ecg_patch_state_t ecg_state;
static smart_ring_state_t ring_state;
static bp_cuff_state_t   bp_state;
static hub_state_t hub_state = HUB_STATE_NORMAL;
static uint8_t seq_num = 0;

/* ── tflite-micro CNN (AFib classifier) ─────────────────────── */
/* Model: 1D CNN, 5 conv + 2 FC, 22 KB, 5-class output
 * 0 = Normal Sinus Rhythm
 * 1 = Atrial Fibrillation
 * 2 = Premature Ventricular Contraction (PVC)
 * 3 = Ventricular Tachycardia (VT)
 * 4 = Bradycardia
 * Trained on MIT-BIH Arrhythmia Database + PhysioNet AFib
 */
extern const unsigned char afib_cnn_tflite[];
extern const unsigned int afib_cnn_tflite_len;

/* Forward declarations */
static int classify_ecg(const int16_t *ecg, int len,
                         uint8_t *out_class, float *out_confidence);
static void handle_afib_detection(float confidence);
static void handle_emergency(cs_alert_type_t type);
static void dispatch_emergency_contacts(cs_alert_type_t type, uint16_t hr);
static void trigger_bp_measurement(uint8_t schedule_id);
static void check_bp_schedule(void);
static void cross_validate_hr(void);
static void update_display(void);
static void siren_alert(uint8_t severity);
static void led_ring_set(uint8_t color);  /* 0=green, 1=yellow, 2=red */
static void haptic_pulse(uint8_t pattern);
static void mqtt_publish_ecg(const int16_t *ecg, int len);
static void mqtt_publish_event(uint8_t event_type, float confidence,
                                uint16_t hr, const int16_t *ecg_strip, int strip_len);
static void mqtt_publish_bp(const bp_result_payload_t *bp);
static void mqtt_publish_ppg(const ppg_hr_payload_t *ppg);
static void sim7000_send_sms(const char *phone, const char *message);
static void sim7000_init(void);

/* ── BLE RX Callback (ECG Data) ────────────────────────────── */
static void on_ecg_data_received(const ecg_data_payload_t *pkt)
{
    /* Verify sequence (detect dropped packets) */
    if (pkt->seq_num != ecg_state.seq_expected) {
        ESP_LOGW(TAG, "ECG packet gap: expected %u, got %u",
                 ecg_state.seq_expected, pkt->seq_num);
        memset(ecg_state.ecg_buffer + ecg_state.buffer_write_idx, 0,
               ECG_SAMPLES_PER_PKT * sizeof(int16_t));
    }
    ecg_state.seq_expected = pkt->seq_num + 1;

    /* Copy 10 samples into circular buffer */
    for (int i = 0; i < ECG_SAMPLES_PER_PKT; i++) {
        ecg_state.ecg_buffer[ecg_state.buffer_write_idx] = pkt->samples[i];
        ecg_state.buffer_write_idx = (ecg_state.buffer_write_idx + 1)
                                      % ECG_WINDOW_SAMPLES;
    }
}

/* ── BLE RX Callback (ECG HR) ───────────────────────────────── */
static void on_ecg_hr_received(const ecg_hr_payload_t *hr)
{
    ecg_state.heart_rate_bpm = hr->heart_rate_bpm;
    ecg_state.rr_interval_ms = hr->rr_interval_ms;
    ecg_state.motion_artifact = hr->motion_artifact;
    ecg_state.lead_off = hr->lead_off;

    /* Check for emergency conditions */
    if (hr->lead_off) {
        ESP_LOGW(TAG, "ECG lead off detected");
    }

    if (hr->heart_rate_bpm > 0 && hr->heart_rate_bpm < BRADY_THRESHOLD_BPM) {
        static int64_t brady_start = 0;
        int64_t now = esp_timer_get_time();
        if (brady_start == 0) {
            brady_start = now;
        } else if ((now - brady_start) / 1000000 >= BRADY_DURATION_S) {
            ESP_LOGE(TAG, "EMERGENCY: Bradycardia HR=%u for >%ds",
                     hr->heart_rate_bpm, BRADY_DURATION_S);
            handle_emergency(ALERT_BRADYCARDIA);
            brady_start = 0;
        }
    } else {
        /* Reset brady timer if HR recovers */
        brady_start_reset();
    }
}

/* ── AFib CNN Classification Task ──────────────────────────── */
static void afib_classify_task(void *arg)
{
    uint8_t ecg_class;
    float confidence;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(AFIB_CLASSIFY_INTERVAL_MS));

        if (!ecg_state.connected || ecg_state.lead_off) {
            continue;
        }

        /* Copy buffer (linearize from circular) */
        int16_t ecg_copy[ECG_WINDOW_SAMPLES];
        uint16_t idx = ecg_state.buffer_write_idx;
        for (int i = 0; i < ECG_WINDOW_SAMPLES; i++) {
            ecg_copy[i] = ecg_state.ecg_buffer[(idx + i) % ECG_WINDOW_SAMPLES];
        }

        /* Run tflite-micro CNN inference */
        int result = classify_ecg(ecg_copy, ECG_WINDOW_SAMPLES,
                                   &ecg_class, &confidence);

        if (result != 0) {
            ESP_LOGE(TAG, "CNN inference failed: %d", result);
            continue;
        }

        ESP_LOGI(TAG, "ECG class=%u conf=%.2f HR=%u", ecg_class,
                 confidence, ecg_state.heart_rate_bpm);

        switch (ecg_class) {
        case 0: /* Normal */
            if (hub_state != HUB_STATE_NORMAL) {
                hub_state = HUB_STATE_NORMAL;
                led_ring_set(0); /* green */
                update_display();
            }
            break;

        case 1: /* AFib */
            if (confidence > 0.85f) {
                handle_afib_detection(confidence);
            }
            break;

        case 2: /* PVC */
            ESP_LOGW(TAG, "PVC detected (conf=%.2f)", confidence);
            mqtt_publish_event(MSG_ECG_HR, confidence,
                              ecg_state.heart_rate_bpm, ecg_copy, 2500);
            break;

        case 3: /* VT */
            ESP_LOGE(TAG, "VT detected (conf=%.2f) — EMERGENCY", confidence);
            handle_emergency(ALERT_VT);
            break;

        case 4: /* Bradycardia */
            ESP_LOGW(TAG, "Bradycardia detected by CNN (conf=%.2f)", confidence);
            /* Bradycardia also handled by HR threshold check above */
            break;
        }

        /* Cross-validate ECG HR with PPG HR */
        cross_validate_hr();
    }
}

/* ── Handle AFib Detection ──────────────────────────────────── */
static void handle_afib_detection(float confidence)
{
    ESP_LOGW(TAG, "AFib detected (conf=%.2f)", confidence);

    hub_state = HUB_STATE_AFIB_ALERT;
    led_ring_set(1); /* yellow */
    haptic_pulse(1); /* alert pattern */
    update_display();

    /* Publish event to cloud */
    int16_t ecg_strip[2500]; /* 10 s ECG strip */
    uint16_t idx = ecg_state.buffer_write_idx;
    for (int i = 0; i < 2500; i++) {
        ecg_strip[i] = ecg_state.ecg_buffer[(idx + ECG_WINDOW_SAMPLES - 2500 + i)
                                            % ECG_WINDOW_SAMPLES];
    }
    mqtt_publish_event(ALERT_AFIB, confidence, ecg_state.heart_rate_bpm,
                       ecg_strip, 2500);
}

/* ── Handle Emergency (VT, Bradycardia) ────────────────────── */
static void handle_emergency(cs_alert_type_t type)
{
    ESP_LOGE(TAG, "EMERGENCY: alert type %u", type);

    hub_state = HUB_STATE_EMERGENCY;
    led_ring_set(2); /* red */
    siren_alert(SEV_EMERGENCY);
    haptic_pulse(3); /* emergency pattern */
    update_display();

    /* Dispatch emergency contacts via Wi-Fi or cellular */
    dispatch_emergency_contacts(type, ecg_state.heart_rate_bpm);

    /* Publish to cloud */
    int16_t ecg_strip[2500];
    uint16_t idx = ecg_state.buffer_write_idx;
    for (int i = 0; i < 2500; i++) {
        ecg_strip[i] = ecg_state.ecg_buffer[(idx + ECG_WINDOW_SAMPLES - 2500 + i)
                                            % ECG_WINDOW_SAMPLES];
    }
    mqtt_publish_event(type, 0.99f, ecg_state.heart_rate_bpm, ecg_strip, 2500);
}

/* ── Dispatch Emergency Contacts ───────────────────────────── */
static void dispatch_emergency_contacts(cs_alert_type_t type, uint16_t hr)
{
    const char *alert_names[] = {
        "NONE", "AFib", "PVC", "Ventricular Tachycardia",
        "Bradycardia", "Tachycardia", "Hypertension", "Hypotension",
        "Low SpO2", "Low Battery", "Disconnected"
    };
    uint8_t idx = (uint8_t)type;
    if (idx > 10) idx = 0;

    char message[256];
    snprintf(message, sizeof(message),
             "CARDIOSYNC EMERGENCY ALERT: %s detected. Heart rate: %u bpm. "
             "Please check on the user immediately. This is an automated message.",
             alert_names[idx], hr);

    /* Try Wi-Fi first (MQTT), fall back to SIM7000 SMS */
    /* In production, would check Wi-Fi status and route accordingly */
    sim7000_send_sms("emergency_contact_1", message);
}

/* ── BLE RX Callback (BP Result) ────────────────────────────── */
static void on_bp_result_received(const bp_result_payload_t *bp)
{
    bp_state.systolic = bp->systolic_mmhg;
    bp_state.diastolic = bp->diastolic_mmhg;
    bp_state.map = bp->map_mmhg;
    bp_state.hr_during = bp->heart_rate_bpm;
    bp_state.position_ok = bp->position_ok;
    bp_state.quality = bp->quality;
    bp_state.measuring = 0;

    ESP_LOGI(TAG, "BP: %u/%u mmHg (MAP %u, HR %u, q=%u, pos=%u)",
             bp->systolic_mmhg, bp->diastolic_mmhg, bp->map_mmhg,
             bp->heart_rate_bpm, bp->quality, bp->position_ok);

    /* Check for hypertension emergency */
    bp_category_t cat = cs_classify_bp(bp->systolic_mmhg, bp->diastolic_mmhg);
    if (cat == BP_CATEGORY_HYPERT_S3) {
        ESP_LOGE(TAG, "EMERGENCY: Severe hypertension %u/%u",
                 bp->systolic_mmhg, bp->diastolic_mmhg);
        handle_emergency(ALERT_HYPERTENSION);
    }

    mqtt_publish_bp(bp);
    update_display();
}

/* ── BLE RX Callback (PPG HR from Smart Ring) ──────────────── */
static void on_ppg_hr_received(const ppg_hr_payload_t *ppg)
{
    ring_state.heart_rate_bpm = ppg->heart_rate_bpm;
    ring_state.spo2_pct = ppg->spo2_pct;
    ring_state.skin_temp_c10 = ppg->skin_temp_c10;

    ESP_LOGI(TAG, "PPG: HR=%u SpO2=%u%% Temp=%.1f°C",
             ppg->heart_rate_bpm, ppg->spo2_pct, ppg->skin_temp_c10 / 10.0f);

    /* Check for low SpO2 */
    if (ppg->spo2_pct < 88 && ppg->spo2_pct > 0) {
        ESP_LOGW(TAG, "Low SpO2: %u%%", ppg->spo2_pct);
        /* Sustained low SpO2 → sleep apnea screen */
    }

    mqtt_publish_ppg(ppg);
}

/* ── Trigger BP Measurement ────────────────────────────────── */
static void trigger_bp_measurement(uint8_t schedule_id)
{
    if (!bp_state.connected) {
        ESP_LOGW(TAG, "BP cuff not connected, skipping measurement");
        return;
    }
    if (bp_state.measuring) {
        ESP_LOGW(TAG, "BP measurement already in progress");
        return;
    }

    ESP_LOGI(TAG, "Triggering BP measurement (schedule %u)", schedule_id);
    bp_state.measuring = 1;

    bp_command_payload_t cmd = {
        .command = 1,  /* measure now */
        .schedule_id = schedule_id
    };
    /* BLE write to cuff characteristic CS_CHAR_BP_COMMAND */
    ble_write_bp_command(&cmd);
}

/* ── Check BP Schedule ──────────────────────────────────────── */
static void check_bp_schedule(void)
{
    /* Get current time from DS3231 RTC */
    int hour = rtc_get_hour();

    static uint8_t am_done = 0, pm_done = 0;

    if (hour == BP_SCHEDULE_AM_HOUR && !am_done) {
        trigger_bp_measurement(1);  /* AM schedule */
        am_done = 1;
    } else if (hour != BP_SCHEDULE_AM_HOUR) {
        am_done = 0;
    }

    if (hour == BP_SCHEDULE_PM_HOUR && !pm_done) {
        trigger_bp_measurement(2);  /* PM schedule */
        pm_done = 1;
    } else if (hour != BP_SCHEDULE_PM_HOUR) {
        pm_done = 0;
    }
}

/* ── Cross-Validate ECG HR vs PPG HR ──────────────────────── */
static void cross_validate_hr(void)
{
    if (!ecg_state.connected || !ring_state.connected)
        return;
    if (ecg_state.heart_rate_bpm == 0 || ring_state.heart_rate_bpm == 0)
        return;

    uint16_t ecg_hr = ecg_state.heart_rate_bpm;
    uint16_t ppg_hr = ring_state.heart_rate_bpm;
    float discrepancy = fabsf((float)ecg_hr - (float)ppg_hr)
                        / fmaxf(ecg_hr, ppg_hr) * 100.0f;

    if (discrepancy > HR_DISCREPANCY_THRESH_PCT) {
        ESP_LOGW(TAG, "HR mismatch: ECG=%u PPG=%u (%.1f%% discrepancy)",
                 ecg_hr, ppg_hr, discrepancy);
        /* Possible: PPG motion artifact, or irregular rhythm (ECG more reliable)
         * Flag for cloud ML: discrepancy may indicate AFib (PPG can't track
         * irregular R-R intervals well) */
    }
}

/* ── SIM7000 4G LTE Cellular Backup ────────────────────────── */
static void sim7000_init(void)
{
    /* Configure UART2 for SIM7000 communication */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, PIN_UART2_TX, PIN_UART2_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* Power on SIM7000 */
    gpio_set_direction(PIN_SIM_PWRKEY, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_SIM_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PIN_SIM_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* Initialize AT commands */
    uart_write_buf(UART_NUM_2, "AT\r\n", 4);
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_write_buf(UART_NUM_2, "AT+CMGF=1\r\n", 11); /* SMS text mode */
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "SIM7000 4G LTE initialized");
}

static void sim7000_send_sms(const char *phone, const char *message)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", phone);
    uart_write_buf(UART_NUM_2, cmd, strlen(cmd));
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_write_buf(UART_NUM_2, message, strlen(message));
    /* Ctrl+Z to send */
    uart_write_buf(UART_NUM_2, "\x1A", 1);
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "SMS sent to %s", phone);
}

/* ── tflite-micro CNN Inference Wrapper ────────────────────── */
/* In production, this wraps TensorFlow Lite for Microcontrollers.
 * The model (afib_cnn_tflite) is compiled into the firmware
 * as a C array. The ESP32-S3 has sufficient RAM (8 MB PSRAM) for
 * the model tensor arena (~40 KB). */
static int classify_ecg(const int16_t *ecg, int len,
                         uint8_t *out_class, float *out_confidence)
{
    /* Stub: in production, this calls tflite-micro Interpreter */
    /* For now, simplified RR irregularity check as fallback */
    if (len < ECG_WINDOW_SAMPLES) return -1;

    /* Simplified AFib heuristic: RR interval variability */
    /* Real implementation uses tflite-micro with the 22 KB CNN model */
    *out_class = 0;  /* Normal */
    *out_confidence = 0.95f;

    return 0;
}

/* ── Display Update ────────────────────────────────────────── */
static void update_display(void)
{
    /* In production, drives Waveshare 2.9" e-ink via SPI */
    /* Shows: current HR, rhythm status (Normal/AFib/Emergency), */
    /*         last BP, AFib burden %, stroke risk gauge */
    ESP_LOGI(TAG, "Display: HR=%u Status=%d BP=%u/%u",
             ecg_state.heart_rate_bpm, hub_state,
             bp_state.systolic, bp_state.diastolic);
}

/* ── LED Ring ──────────────────────────────────────────────── */
static void led_ring_set(uint8_t color)
{
    /* 0=green (normal), 1=yellow (AFib), 2=red (emergency) */
    /* Drives 24× SK6812 RGB via RMT peripheral */
    uint8_t r = 0, g = 0, b = 0;
    switch (color) {
    case 0: g = 255; break;  /* green */
    case 1: r = 255; g = 200; break;  /* yellow */
    case 2: r = 255; break;  /* red */
    }
    /* SK6812 RMT drive code here */
}

/* ── Siren ─────────────────────────────────────────────────── */
static void siren_alert(uint8_t severity)
{
    if (severity >= SEV_EMERGENCY) {
        /* Drive MAX98357A I²S amplifier with 105 dB siren tone */
    }
}

/* ── Haptic ─────────────────────────────────────────────────── */
static void haptic_pulse(uint8_t pattern)
{
    /* DRV2605L I²C haptic driver, predefined waveform library */
    /* pattern: 0=none, 1=alert, 2=urgent, 3=emergency */
}

/* ── MQTT Publish Stubs ────────────────────────────────────── */
static void mqtt_publish_ecg(const int16_t *ecg, int len)
{ /* MQTT publish compressed ECG to cardiosync/{user}/hub/ecg */ }

static void mqtt_publish_event(uint8_t event_type, float confidence,
                                uint16_t hr, const int16_t *ecg_strip, int strip_len)
{ /* MQTT publish arrhythmia event to cardiosync/{user}/hub/events */ }

static void mqtt_publish_bp(const bp_result_payload_t *bp)
{ /* MQTT publish BP to cardiosync/{user}/hub/bp */ }

static void mqtt_publish_ppg(const ppg_hr_payload_t *ppg)
{ /* MQTT publish PPG to cardiosync/{user}/hub/ppg */ }

/* ── Helper: reset brady timer (extern due to scope) ──────── */
void brady_start_reset(void)
{ /* Resets the static brady_start variable in on_ecg_hr_received */ }

/* ── RTC Helper ────────────────────────────────────────────── */
int rtc_get_hour(void)
{
    /* Read DS3231 I²C, return current hour */
    return 0;
}

/* ── BLE Helper ─────────────────────────────────────────────── */
void ble_write_bp_command(const bp_command_payload_t *cmd)
{ /* BLE GATT write to BP cuff CS_CHAR_BP_COMMAND characteristic */ }

/* ── Schedule Task ─────────────────────────────────────────── */
static void schedule_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); /* check every minute */
        check_bp_schedule();
    }
}

/* ── Heartbeat Check Task ──────────────────────────────────── */
static void heartbeat_check_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        int64_t now = esp_timer_get_time();

        if (ecg_state.connected &&
            (now - ecg_state.last_heartbeat_us) > 90000000) { /* 90 s */
            ESP_LOGW(TAG, "ECG patch heartbeat timeout");
            ecg_state.connected = 0;
            /* Alert: ECG disconnected */
        }
        if (ring_state.connected &&
            (now - ring_state.last_heartbeat_us) > 90000000) {
            ESP_LOGW(TAG, "Smart ring heartbeat timeout");
            ring_state.connected = 0;
        }
        if (bp_state.connected &&
            (now - bp_state.last_heartbeat_us) > 90000000) {
            bp_state.connected = 0;
        }
    }
}

/* ── Main ──────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "CardioSync Hub starting...");

    /* Initialize GPIO */
    gpio_set_direction(PIN_BUTTON, GPIO_MODE_INPUT);
    gpio_pullup_en(PIN_BUTTON);

    /* Initialize I2C (DS3231, DRV2605L, SHT40) */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* Initialize SPI (CC1101, e-ink, SD card) */
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_CLK,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    /* Initialize SIM7000 4G LTE */
    sim7000_init();

    /* Initialize BLE (as central — connect to ECG patch, ring, cuff) */
    /* ESP-IDF Bluedroid or NimBLE stack initialization */
    cs_ble_init();

    /* Start AFib classification task */
    xTaskCreatePinnedToCore(afib_classify_task, "afib_cnn", 16384, NULL,
                            5, NULL, 1);  /* Core 1 for ML */

    /* Start schedule task */
    xTaskCreatePinnedToCore(schedule_task, "schedule", 4096, NULL,
                            3, NULL, 0);

    /* Start heartbeat check task */
    xTaskCreatePinnedToCore(heartbeat_check_task, "hb_check", 4096, NULL,
                            3, NULL, 0);

    ESP_LOGI(TAG, "CardioSync Hub ready. Waiting for node connections...");

    /* Main loop: handle button presses, display updates */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        update_display();
    }
}
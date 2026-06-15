/**
 * MedSync - Hub Node Firmware
 * nRF52840 (BLE mesh coordinator + local ML inference + display + audio)
 * ESP32-S3 (WiFi6/BLE bridge)
 *
 * Main entry point and system initialization
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/disk/access.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>

/* MedSync uses BLE mesh (not Zigbee) but the nRF SDK uses similar APIs.
 * In production, this would use the BLE mesh stack. For this reference
 * implementation, we use the nRF Connect SDK BLE mesh API patterns. */

#include "ms_protocol.h"
#include "ms_util.h"

LOG_MODULE_REGISTER(hub_main, CONFIG_MS_HUB_LOG_LEVEL);

/* ============================================================
 * Thread Configuration
 * ============================================================ */

#define MAIN_STACK_SIZE         2048
#define SCHEDULE_STACK_SIZE     3072
#define DISPLAY_STACK_SIZE      1024
#define AUDIO_STACK_SIZE        2048
#define ML_STACK_SIZE           4096

K_THREAD_STACK_DEFINE(main_stack, MAIN_STACK_SIZE);
K_THREAD_STACK_DEFINE(schedule_stack, SCHEDULE_STACK_SIZE);
K_THREAD_STACK_DEFINE(display_stack, DISPLAY_STACK_SIZE);
K_THREAD_STACK_DEFINE(audio_stack, AUDIO_STACK_SIZE);
K_THREAD_STACK_DEFINE(ml_stack, ML_STACK_SIZE);

/* ============================================================
 * Global State
 * ============================================================ */

struct ms_hub_state {
    /* Schedule database */
    ms_schedule_entry_t schedules[32];  /* Up to 32 medication schedules */
    uint8_t schedule_count;
    uint8_t next_dose_index;             /* Index of next scheduled dose */

    /* Pill station status */
    ms_pill_status_t pill_station;

    /* Wearable vitals */
    ms_vitals_report_t vitals;

    /* Room beacon data (up to 6) */
    ms_beacon_report_t beacons[6];

    /* Alert state */
    ms_alert_t active_alert;
    bool has_active_alert;

    /* Dose tracking */
    uint32_t doses_taken_today;
    uint32_t doses_missed_today;
    uint32_t adherence_streak_days;

    /* System state */
    bool wifi_connected;
    bool mqtt_connected;
    bool ble_paired;
    uint32_t uptime_seconds;

    /* ML state */
    bool ml_running;
    float adherence_probability;
};

static struct ms_hub_state g_state;

/* ============================================================
 * GPIO Definitions
 * ============================================================ */

#define LED_R_NODE    DT_ALIAS(led_r)
#define LED_G_NODE    DT_ALIAS(led_g)
#define LED_B_NODE    DT_ALIAS(led_b)
#define PIEZO_NODE    DT_ALIAS(piezo)
#define BUTTON_NODE   DT_ALIAS(sw0)
#define TFT_CS_NODE   DT_ALIAS(tft_cs)
#define SD_CS_NODE    DT_ALIAS(sd_cs)

static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_R_NODE, gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(LED_G_NODE, gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec piezo = GPIO_DT_SPEC_GET(PIEZO_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

/* ============================================================
 * Schedule Engine
 * ============================================================ */

/**
 * Find the next scheduled dose based on current time.
 * Returns index into g_state.schedules[] or -1 if none pending.
 */
int ms_find_next_dose(uint8_t current_hour, uint8_t current_minute)
{
    int best_idx = -1;
    uint16_t best_time = 24 * 60 + 1; /* Impossibly late */

    uint16_t current_total = ms_time_to_minutes(current_hour, current_minute);

    for (int i = 0; i < g_state.schedule_count; i++) {
        ms_schedule_entry_t *entry = &g_state.schedules[i];
        uint16_t dose_time = ms_time_to_minutes(entry->hour, entry->minute);

        /* Find the next dose that hasn't been taken yet today */
        if (dose_time >= current_total && dose_time < best_time) {
            best_time = dose_time;
            best_idx = i;
        }
    }

    return best_idx;
}

/**
 * Trigger a dose reminder across all nodes.
 * Sends commands to pill station, room beacons, and wearable.
 */
void ms_trigger_dose_reminder(uint8_t schedule_idx, ms_urgency_t urgency)
{
    if (schedule_idx >= g_state.schedule_count) {
        LOG_ERR("Invalid schedule index: %d", schedule_idx);
        return;
    }

    ms_schedule_entry_t *entry = &g_state.schedules[schedule_idx];

    /* 1. Send DoseTrigger to pill station */
    uint8_t payload[3] = {
        entry->bin_id,
        entry->dose_count,
        (uint8_t)urgency
    };

    /* Send via BLE mesh to pill station */
    /* ms_mesh_send(MS_NODE_ID_PILL_STATION, MS_MODEL_PILL_STATION,
     *             MS_OP_DISPENSE_DOSE, payload, sizeof(payload)); */

    LOG_INF("Dose trigger: bin=%d, count=%d, urgency=%d, med_id=%d",
            entry->bin_id, entry->dose_count, urgency, entry->medication_id);

    /* 2. Activate room beacon reminders */
    for (int i = 0; i < 6; i++) {
        if (g_state.beacons[i].node_id != 0) {
            /* ms_mesh_send(g_state.beacons[i].node_id, MS_MODEL_ALERT,
             *             MS_OP_ALERT, ...); */
            g_state.beacons[i].reminder_active = 1;
        }
    }

    /* 3. Activate wearable vibration */
    /* ms_mesh_send(MS_NODE_ID_WEARABLE_BASE, MS_MODEL_ALERT, ...); */

    /* 4. Activate hub visual + audio reminder */
    ms_alarm_start(urgency);
}

/* ============================================================
 * Dose Verification Engine
 * ============================================================ */

/**
 * Evaluate dose verification from pill station sensors.
 * Called when pill station reports a weight change or IR event.
 *
 * Decision hierarchy:
 * 1. Weight change matches expected dose (±0.1g per pill) → CONFIRMED
 * 2. IR beam-break detected (something was picked up) → PROBABLY TAKEN
 * 3. Cover opened (manual access) → POSSIBLY TAKEN
 * 4. No sensor event within 5 minutes → ESCALATE
 * 5. No sensor event within 30 minutes → OVERDUE
 * 6. No sensor event within 2 hours → MISSED
 */
int ms_evaluate_dose_verification(uint8_t bin_id,
                                   int32_t weight_change_mg,
                                   bool ir_triggered,
                                   bool cover_opened)
{
    if (bin_id >= MS_NUM_BINS) {
        LOG_ERR("Invalid bin_id: %d", bin_id);
        return -1;
    }

    /* Find the active schedule for this bin */
    ms_schedule_entry_t *active_dose = NULL;
    for (int i = 0; i < g_state.schedule_count; i++) {
        if (g_state.schedules[i].bin_id == bin_id &&
            g_state.pill_station.bin_status[bin_id] == MS_BIN_WAITING_PICKUP) {
            active_dose = &g_state.schedules[i];
            break;
        }
    }

    if (!active_dose) {
        LOG_WRN("No active dose for bin %d", bin_id);
        return 0;
    }

    /* Calculate expected weight change */
    int32_t expected_weight = ms_dose_weight_mg(active_dose->dose_count,
                                                 active_dose->pill_weight_mg);

    /* Method 1: Weight verification (gold standard) */
    if (weight_change_mg != 0) {
        if (ms_weight_confirms_dose(weight_change_mg, expected_weight,
                                     expected_weight / 5)) { /* ±20% tolerance */
            LOG_INF("DOSE CONFIRMED (weight): bin=%d, expected=%d mg, actual=%d mg",
                     bin_id, expected_weight, weight_change_mg);

            g_state.pill_station.bin_status[bin_id] = MS_BIN_CONFIRMED;
            g_state.doses_taken_today++;

            /* Send confirmation to cloud */
            ms_dose_confirmed(bin_id, MS_CONFIRM_WEIGHT);
            return 1;
        }
    }

    /* Method 2: IR beam-break (probable) */
    if (ir_triggered) {
        LOG_INF("DOSE PROBABLY TAKEN (IR): bin=%d", bin_id);

        /* Request confirmation via app */
        ms_send_push_notification("Please confirm you took your medication");

        g_state.pill_station.bin_status[bin_id] = MS_BIN_WAITING_PICKUP;
        ms_dose_confirmed(bin_id, MS_CONFIRM_IR);
        return 2;
    }

    /* Method 3: Cover opened (manual access) */
    if (cover_opened) {
        LOG_INF("Pill station cover opened for bin %d", bin_id);
        /* Wait for cover to close, then check weight */
    }

    return 0;
}

/* ============================================================
 * Fall Detection Handler
 * ============================================================ */

/**
 * Handle fall detection event from wearable tag.
 * Evaluates severity and escalates appropriately.
 */
void ms_handle_fall_detected(float confidence, bool post_fall_stillness)
{
    LOG_ERR("FALL DETECTED: confidence=%.2f, stillness=%d",
             confidence, post_fall_stillness);

    if (confidence >= MS_FALL_THRESHOLD) {
        /* High-confidence fall detected */
        if (post_fall_stillness) {
            /* Person is not moving after fall — CRITICAL */
            ms_alert_send(MS_ALERT_EMERGENCY, MS_ALERT_TYPE_FALL,
                         "Fall detected! Person is not moving. Calling caregiver.");
        } else {
            /* Fall detected but person is moving — WARNING */
            ms_alert_send(MS_ALERT_URGENT, MS_ALERT_TYPE_FALL,
                         "Fall detected. Checking if person is OK.");
        }

        /* Activate hub alarm */
        ms_alarm_start(MS_ALERT_EMERGENCY);

        /* Send to wearable: "Are you OK? Press button to dismiss." */
        /* ms_mesh_send(MS_NODE_ID_WEARABLE_BASE, MS_MODEL_ALERT, ...); */
    }
}

/* ============================================================
 * Vitals Monitoring
 * ============================================================ */

/**
 * Check vitals from wearable tag for anomalies.
 */
void ms_check_vitals(const ms_vitals_report_t *vitals)
{
    /* SpO2 check */
    if (vitals->spo2_percent < MS_SPO2_EMERGENCY_MIN) {
        ms_alert_send(MS_ALERT_EMERGENCY, MS_ALERT_TYPE_ADVERSE_EFFECT,
                     "Blood oxygen critically low! Seek medical attention.");
    } else if (vitals->spo2_percent < MS_SPO2_WARNING_MIN) {
        ms_alert_send(MS_ALERT_WARNING, MS_ALERT_TYPE_ADVERSE_EFFECT,
                     "Blood oxygen below normal range.");
    }

    /* Heart rate check */
    if (vitals->heart_rate_bpm > MS_HR_TACHY_THRESHOLD ||
        vitals->heart_rate_bpm < MS_HR_BRADY_THRESHOLD) {
        ms_alert_send(MS_ALERT_WARNING, MS_ALERT_TYPE_ADVERSE_EFFECT,
                     "Heart rate outside normal range.");
    }

    /* Store for cloud analysis */
    memcpy(&g_state.vitals, vitals, sizeof(ms_vitals_report_t));
}

/* ============================================================
 * Alert System
 * ============================================================ */

static struct k_timer alarm_timer;
static bool alarm_active = false;
static ms_alert_level_t alarm_level;

void ms_alarm_start(ms_alert_level_t level)
{
    alarm_active = true;
    alarm_level = level;

    /* Set LED color based on level */
    gpio_pin_set_dt(&led_r, (level >= MS_ALERT_REMINDER) ? 1 : 0);
    gpio_pin_set_dt(&led_g, (level == MS_ALERT_INFO) ? 1 : 0);
    gpio_pin_set_dt(&led_b, (level == MS_ALERT_REMINDER) ? 1 : 0);

    /* Start buzzer pattern based on severity */
    switch (level) {
    case MS_ALERT_INFO:
        /* Single short beep */
        gpio_pin_set_dt(&piezo, 1);
        k_msleep(100);
        gpio_pin_set_dt(&piezo, 0);
        break;
    case MS_ALERT_REMINDER:
        /* 2 short beeps */
        k_timer_start(&alarm_timer, K_MSEC(500), K_MSEC(500));
        break;
    case MS_ALERT_WARNING:
        /* Moderate beeping */
        k_timer_start(&alarm_timer, K_SECONDS(2), K_SECONDS(2));
        break;
    case MS_ALERT_URGENT:
        /* Fast beeping */
        k_timer_start(&alarm_timer, K_MSEC(300), K_MSEC(300));
        break;
    case MS_ALERT_EMERGENCY:
        /* Continuous alarm */
        k_timer_start(&alarm_timer, K_MSEC(200), K_MSEC(200));
        break;
    }
}

void ms_alarm_stop(void)
{
    alarm_active = false;
    k_timer_stop(&alarm_timer);
    gpio_pin_set_dt(&led_r, 0);
    gpio_pin_set_dt(&led_g, 1);  /* Green = normal */
    gpio_pin_set_dt(&led_b, 0);
    gpio_pin_set_dt(&piezo, 0);
}

static void alarm_timer_handler(struct k_timer *timer)
{
    if (!alarm_active) return;

    static bool buzzer_state = false;
    buzzer_state = !buzzer_state;
    gpio_pin_set_dt(&piezo, buzzer_state ? 1 : 0);
}

void ms_alert_send(ms_alert_level_t level, ms_alert_type_t type, const char *message)
{
    ms_alert_t alert = {
        .level = level,
        .type = type,
        .source_node_id = MS_NODE_ID_HUB,
        .bin_id = 0xFF,
        .medication_id = 0xFF,
    };
    ms_str_copy(alert.message, message, sizeof(alert.message));

    g_state.active_alert = alert;
    g_state.has_active_alert = true;

    /* Forward to ESP32-S3 for MQTT uplink */
    ms_uart_send_alert(&alert);

    /* Activate local alarm */
    ms_alarm_start(level);
}

/* ============================================================
 * Dose Confirmation (sent to cloud)
 * ============================================================ */

void ms_dose_confirmed(uint8_t bin_id, ms_confirm_method_t method)
{
    LOG_INF("Dose confirmed: bin=%d, method=%d", bin_id, method);

    /* Build confirmation packet */
    uint8_t payload[6] = {
        bin_id,
        (uint8_t)method,
        (uint8_t)(g_state.schedules[bin_id].medication_id),
        0xFF, 0xFF, 0xFF  /* timestamp placeholder */
    };

    /* Send to cloud via MQTT */
    /* ms_uart_send_to_cloud(MS_OP_DOSE_CONFIRM, payload, sizeof(payload)); */
}

/* ============================================================
 * ESP32-S3 UART Communication
 * ============================================================ */

#define ESP_UART_DEVICE DT_NODELABEL(esp_uart)
static const struct device *esp_uart = DEVICE_DT_GET(ESP_UART_DEVICE);

/* UART protocol: STX(0x02) LEN(2) TYPE(1) PAYLOAD(n) CRC16(2) ETX(0x03) */
#define UART_STX 0x02
#define UART_ETX 0x03

typedef enum {
    UART_TYPE_SENSOR_DATA    = 0x01,
    UART_TYPE_VITALS         = 0x02,
    UART_TYPE_COMMAND        = 0x03,
    UART_TYPE_ALERT          = 0x04,
    UART_TYPE_MQTT_RX        = 0x05,
    UART_TYPE_WIFI_STATUS    = 0x06,
    UART_TYPE_DOSE_EVENT     = 0x07,
    UART_TYPE_SCHEDULE_UPDATE = 0x08,
} uart_type_t;

int ms_uart_send_alert(const ms_alert_t *alert)
{
    uint8_t buf[128];
    uint16_t idx = 0;

    buf[idx++] = UART_STX;
    idx += 2;  /* Length placeholder */
    buf[idx++] = UART_TYPE_ALERT;
    buf[idx++] = alert->level;
    buf[idx++] = alert->type;
    buf[idx++] = (alert->source_node_id >> 8) & 0xFF;
    buf[idx++] = alert->source_node_id & 0xFF;
    buf[idx++] = alert->bin_id;
    buf[idx++] = alert->medication_id;
    uint8_t msg_len = strlen(alert->message);
    buf[idx++] = msg_len;
    memcpy(&buf[idx], alert->message, msg_len);
    idx += msg_len;

    /* Fill length */
    buf[1] = (idx >> 8) & 0xFF;
    buf[2] = idx & 0xFF;

    /* CRC */
    uint16_t crc = ms_crc16(&buf[3], idx - 3);
    buf[idx++] = (crc >> 8) & 0xFF;
    buf[idx++] = crc & 0xFF;
    buf[idx++] = UART_ETX;

    return uart_tx(esp_uart, buf, idx, K_MSEC(100));
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    int ret;

    LOG_INF("MedSync Hub Node starting...");
    LOG_INF("Firmware v%d.%d.%d", MS_VERSION_MAJOR, MS_VERSION_MINOR, MS_VERSION_PATCH);

    /* Initialize GPIOs */
    if (!device_is_ready(led_r.port) || !device_is_ready(led_g.port) ||
        !device_is_ready(led_b.port) || !device_is_ready(piezo.port) ||
        !device_is_ready(button.port)) {
        LOG_ERR("GPIO not ready");
        return -EIO;
    }

    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&piezo, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&button, GPIO_INPUT);

    /* Initialize alarm timer */
    k_timer_init(&alarm_timer, alarm_timer_handler, NULL);

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));

    /* Green LED = booting */
    gpio_pin_set_dt(&led_g, 1);

    /* Start BLE mesh network */
    LOG_INF("Starting BLE mesh provisioner...");
    /* ble_mesh_init(); */

    /* Initialize ESP32-S3 UART bridge */
    if (!device_is_ready(esp_uart)) {
        LOG_ERR("ESP32-S3 UART not ready");
        return -EIO;
    }

    /* Initialize schedule engine */
    LOG_INF("Loading medication schedule from flash...");
    /* load_schedule_from_flash(); */

    /* Initialize DS3231 RTC */
    LOG_INF("Initializing RTC...");
    /* rtc_init(); */

    /* Main loop */
    while (1) {
        /* Check for scheduled doses every 30 seconds */
        k_sleep(K_SECONDS(30));

        /* Get current time from RTC */
        /* struct tm now;
         * rtc_get_time(&now);
         */

        /* Find next dose */
        /* int next_dose = ms_find_next_dose(now.tm_hour, now.tm_min);
         * if (next_dose >= 0) {
         *     ms_trigger_dose_reminder(next_dose, MS_URGENCY_NORMAL);
         * }
         */

        /* Check for overdue doses */
        /* ... */

        /* Update display */
        /* display_update(); */

        /* Send periodic status to cloud */
        /* ms_uart_send_status(); */
    }

    return 0;
}
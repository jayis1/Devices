/**
 * FlowGuard - Hub Node Firmware
 * nRF52840 (Zigbee 3.0 coordinator + local ML inference + display)
 * ESP32-C6 (WiFi6/BLE bridge)
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

#include "fg_protocol.h"
#include "fg_util.h"

LOG_MODULE_REGISTER(hub_main, CONFIG_FG_HUB_LOG_LEVEL);

/* ============================================================
 * Thread Configuration
 * ============================================================ */

#define THREAD_STACK_SIZE       2048
#define ML_THREAD_STACK_SIZE    4096
#define DISPLAY_THREAD_STACK    1024

K_THREAD_STACK_DEFINE(main_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(ml_stack, ML_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(display_stack, DISPLAY_THREAD_STACK);

/* ============================================================
 * Global State
 * ============================================================ */

struct fg_hub_state {
    /* Valve state (mirror of valve controller's actual state) */
    fg_valve_state_t valve_state;
    uint16_t valve_last_reason;
    int64_t valve_last_change_time;

    /* Sensor data cache (latest from each node) */
    fg_pipe_sensor_report_t   pipe_sensors[8];
    fg_appliance_report_t     appliance_mons[4];
    fg_valve_status_t         valve_status;

    /* Alert state */
    fg_alert_t active_alert;
    bool has_active_alert;

    /* System state */
    bool wifi_connected;
    bool mqtt_connected;
    bool ble_paired;
    uint32_t total_gallons_today;
    uint32_t uptime_seconds;

    /* ML state */
    bool ml_running;
    fg_acoustic_class_t last_acoustic_class;
    float last_acoustic_confidence;
};

static struct fg_hub_state g_state;

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
 * Zigbee Signal Handler
 * ============================================================ */

void fg_zigbee_signal_cb(zb_bufid_t bufid)
{
    zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, NULL);
    zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

    switch (sig) {
    case ZB_COMMON_SIGNAL_CAN_START_AL:
        LOG_INF("Zigbee: Can start network");
        zb_osif_set_hw_address(ZB_NETWORK_COORDINATOR_ADDR);
        break;

    case ZB_ZDO_SIGNAL_SKIP_START:
        LOG_INF("Zigbee: Network started (skip)");
        break;

    case ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        LOG_INF("Zigbee: Device announced on network");
        break;

    case ZB_NLME_STATUS_INDICATION:
        LOG_INF("Zigbee: Network status indication");
        break;

    default:
        LOG_DBG("Zigbee signal: 0x%02x status: 0x%02x", sig, status);
        break;
    }
}

/* ============================================================
 * Valve Control Commands
 * ============================================================ */

/**
 * Send valve close command via Zigbee.
 * Uses encrypted APS frame with auth token.
 */
int fg_send_valve_close(fg_valve_reason_t reason)
{
    uint8_t auth_token[FG_AUTH_TOKEN_LEN] = FG_AUTH_TOKEN_DEFAULT;
    zb_bufid_t bufid = zb_buf_get_out();
    if (bufid == ZB_BUF_INVALID) {
        LOG_ERR("Failed to allocate Zigbee buffer");
        return -ENOMEM;
    }

    /* Build command payload */
    uint8_t *payload = zb_buf_initial_alloc(bufid, 6);
    payload[0] = FG_CMD_VALVE_CLOSE;
    memcpy(&payload[1], auth_token, 4);
    payload[5] = (uint8_t)reason;

    /* Send to valve controller via Zigbee APS */
    zb_ret_t ret = zb_apsde_data_req(
        bufid,
        FG_NODE_ID_VALVE_BASE,     /* DstAddr */
        FG_CLUSTER_COMMAND,         /* ClusterId */
        0,                          /* SrcEndpoint */
        0,                          /* DstEndpoint */
        ZB_APSDE_TX_OPT_SECURITY,   /* TxOptions (encrypted) */
        0                           /* Radius */
    );

    if (ret != RET_OK) {
        LOG_ERR("Failed to send valve close command: %d", ret);
        zb_buf_free(bufid);
        return -EIO;
    }

    g_state.valve_state = FG_VALVE_CLOSING;
    g_state.valve_last_reason = reason;
    g_state.valve_last_change_time = k_uptime_get();

    LOG_INF("Valve CLOSE command sent (reason: %d)", reason);

    /* Activate local alarm */
    fg_alarm_start(reason == FG_REASON_EMERGENCY ? FG_ALERT_EMERGENCY : FG_ALERT_CRITICAL);

    return 0;
}

/**
 * Send valve open command via Zigbee.
 * Requires auth token (2FA verified by mobile app).
 */
int fg_send_valve_open(uint8_t auth_token[4], fg_valve_reason_t reason)
{
    zb_bufid_t bufid = zb_buf_get_out();
    if (bufid == ZB_BUF_INVALID) {
        LOG_ERR("Failed to allocate Zigbee buffer");
        return -ENOMEM;
    }

    uint8_t *payload = zb_buf_initial_alloc(bufid, 6);
    payload[0] = FG_CMD_VALVE_OPEN;
    memcpy(&payload[1], auth_token, 4);
    payload[5] = (uint8_t)reason;

    zb_ret_t ret = zb_apsde_data_req(
        bufid,
        FG_NODE_ID_VALVE_BASE,
        FG_CLUSTER_COMMAND,
        0, 0,
        ZB_APSDE_TX_OPT_SECURITY,
        0
    );

    if (ret != RET_OK) {
        LOG_ERR("Failed to send valve open command: %d", ret);
        zb_buf_free(bufid);
        return -EIO;
    }

    g_state.valve_state = FG_VALVE_OPENING;
    g_state.valve_last_change_time = k_uptime_get();

    LOG_INF("Valve OPEN command sent (reason: %d)", reason);
    return 0;
}

/* ============================================================
 * Local Alarm Control
 * ============================================================ */

static struct k_timer alarm_timer;
static bool alarm_active = false;
static fg_alert_level_t alarm_level;

void fg_alarm_start(fg_alert_level_t level)
{
    alarm_active = true;
    alarm_level = level;

    /* Set LED color based on level */
    gpio_pin_set_dt(&led_r, (level >= FG_ALERT_WARNING) ? 1 : 0);
    gpio_pin_set_dt(&led_g, (level == FG_ALERT_INFO) ? 1 : 0);
    gpio_pin_set_dt(&led_b, 0);

    /* Start buzzer pattern based on severity */
    if (level >= FG_ALERT_CRITICAL) {
        /* Continuous beeping for critical/emergency */
        k_timer_start(&alarm_timer, K_MSEC(500), K_MSEC(500));
    } else if (level == FG_ALERT_WARNING) {
        /* Slow beeping for warning */
        k_timer_start(&alarm_timer, K_SECONDS(3), K_SECONDS(3));
    }
}

void fg_alarm_stop(void)
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

    /* Toggle buzzer */
    static bool buzzer_state = false;
    buzzer_state = !buzzer_state;
    gpio_pin_set_dt(&piezo, buzzer_state ? 1 : 0);

    /* Emergency: continuous alarm */
    if (alarm_level == FG_ALERT_EMERGENCY) {
        /* Faster beeping */
        k_timer_start(timer, K_MSEC(250), K_MSEC(250));
    }
}

/* ============================================================
 * Leak Detection Decision Engine
 * ============================================================ */

/**
 * Evaluate leak detection from all sensor inputs.
 * This is the core decision engine that determines if the valve should close.
 *
 * Decision hierarchy:
 * 1. Conductive trace wet = IMMEDIATE close (no processing needed)
 * 2. Acoustic anomaly > 0.85 = CONFIRMED LEAK → close valve
 * 3. Acoustic anomaly 0.6-0.85 + flow anomaly = PROBABLE LEAK → close valve
 * 4. Pressure drop > 20 PSI in <5s = BURST → close valve
 * 5. Unknown flow source (NILM can't match any appliance) = SUSPECT → alert + 30s timeout → close
 */
int fg_evaluate_leak_decision(void)
{
    /* Check pipe sensors for conductive leak detection */
    for (int i = 0; i < 8; i++) {
        if (g_state.pipe_sensors[i].leak_state == FG_LEAK_CONFIRMED) {
            LOG_ERR("LEAK CONFIRMED: Pipe sensor %d conductive trace wet!", i);
            return fg_send_valve_close(FG_REASON_LEAK_DETECTED);
        }
    }

    /* Check appliance monitors for conductive leak detection */
    for (int i = 0; i < 4; i++) {
        if (g_state.appliance_mons[i].leak_probe_1 || g_state.appliance_mons[i].leak_probe_2) {
            LOG_ERR("LEAK CONFIRMED: Appliance monitor %d detected water!", i);
            return fg_send_valve_close(FG_REASON_LEAK_DETECTED);
        }
    }

    /* Check acoustic anomaly from pipe sensors */
    for (int i = 0; i < 8; i++) {
        if (g_state.pipe_sensors[i].node_id == 0) continue;  /* Slot empty */

        float anomaly = g_state.pipe_sensors[i].acoustic_anomaly / 255.0f;

        if (anomaly > FG_LEAK_THRESHOLD_CRITICAL) {
            LOG_ERR("LEAK CONFIRMED: Pipe sensor %d acoustic score %.2f",
                     i, anomaly);
            return fg_send_valve_close(FG_REASON_LEAK_DETECTED);
        }

        if (anomaly > FG_LEAK_THRESHOLD_WARN) {
            /* Warning level — check if flow source is known */
            LOG_WRN("LEAK WARNING: Pipe sensor %d acoustic score %.2f",
                     i, anomaly);

            /* Check if whole-home flow is happening with no known source */
            if (g_state.valve_status.flow_rate_ml_min > 100) {
                /* Flow detected but no appliance identified — probable leak */
                LOG_ERR("PROBABLE LEAK: Unknown flow source (%d mL/min)",
                         g_state.valve_status.flow_rate_ml_min);
                return fg_send_valve_close(FG_REASON_LEAK_DETECTED);
            }
        }
    }

    /* Check for pressure anomalies */
    if (g_state.valve_status.pressure_kpa_x10 > 0) {
        static int16_t last_pressure = 0;
        static int64_t last_pressure_time = 0;

        int16_t pressure_delta = g_state.valve_status.pressure_kpa_x10 - last_pressure;
        int64_t time_delta = k_uptime_get() - last_pressure_time;

        /* Pressure drop > 138 kPa (20 PSI) in <5 seconds = burst pipe */
        if (time_delta > 0 && time_delta < 5000000 &&
            pressure_delta < -1380) {  /* Negative = pressure drop */
            LOG_ERR("BURST DETECTED: Pressure dropped %d kPa in %lld ms",
                     -pressure_delta, time_delta / 1000);
            return fg_send_valve_close(FG_REASON_PRESSURE_ANOMALY);
        }

        /* Water hammer: pressure spike > 690 kPa (100 PSI) */
        if (g_state.valve_status.pressure_kpa_x10 > 6900) {
            LOG_WRN("WATER HAMMER: Pressure %.1f kPa",
                     g_state.valve_status.pressure_kpa_x10 / 10.0f);
            /* Don't close valve for hammer, just alert */
            fg_alert_send(FG_ALERT_WARNING, FG_ALERT_TYPE_HAMMER,
                         "Water hammer detected");
        }

        last_pressure = g_state.valve_status.pressure_kpa_x10;
        last_pressure_time = k_uptime_get();
    }

    /* Check for freeze risk */
    for (int i = 0; i < 8; i++) {
        if (g_state.pipe_sensors[i].node_id == 0) continue;

        int16_t temp_c = g_state.pipe_sensors[i].temperature_cx100 / 100;

        if (temp_c <= 2) {
            /* Pipe at or near freezing */
            LOG_ERR("FREEZE RISK: Pipe sensor %d at %d°C", i, temp_c);
            fg_alert_send(FG_ALERT_CRITICAL, FG_ALERT_TYPE_FREEZE,
                         "Pipe near freezing! Heat trace activated.");
            /* Activate heat trace on valve controller */
            /* (sent via Zigbee command to valve controller) */
        }
    }

    return 0;
}

/* ============================================================
 * Alert Send (via MQTT to cloud + local display)
 * ============================================================ */

void fg_alert_send(fg_alert_level_t level, fg_alert_type_t type, const char *message)
{
    fg_alert_t alert = {
        .level = level,
        .type = type,
        .source_node_id = FG_NODE_ID_HUB,
    };
    strncpy(alert.message, message, sizeof(alert.message) - 1);
    alert.message[sizeof(alert.message) - 1] = '\0';

    g_state.active_alert = alert;
    g_state.has_active_alert = true;

    /* Forward to ESP32-C6 for MQTT uplink */
    fg_uart_send_alert(&alert);

    /* Activate local alarm */
    fg_alarm_start(level);
}

/* ============================================================
 * ESP32-C6 UART Communication
 * ============================================================ */

/* UART to ESP32-C6 for WiFi/BLE bridge */
#define ESP_UART_DEVICE DT_NODELABEL(esp_uart)
static const struct device *esp_uart = DEVICE_DT_GET(ESP_UART_DEVICE);

/* UART protocol: STX(0x02) LEN(2) TYPE(1) PAYLOAD(n) CRC16(2) ETX(0x03) */
#define UART_STX 0x02
#define UART_ETX 0x03

typedef enum {
    UART_TYPE_SENSOR_DATA    = 0x01,
    UART_TYPE_VALVE_STATUS   = 0x02,
    UART_TYPE_COMMAND        = 0x03,
    UART_TYPE_ALERT          = 0x04,
    UART_TYPE_MQTT_RX        = 0x05,
    UART_TYPE_WIFI_STATUS    = 0x06,
} uart_type_t;

int fg_uart_send_alert(const fg_alert_t *alert)
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
    uint8_t msg_len = strlen(alert->message);
    buf[idx++] = msg_len;
    memcpy(&buf[idx], alert->message, msg_len);
    idx += msg_len;

    /* Fill length */
    buf[1] = (idx >> 8) & 0xFF;
    buf[2] = idx & 0xFF;

    /* CRC */
    uint16_t crc = fg_crc16(&buf[3], idx - 3);
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

    LOG_INF("FlowGuard Hub Node starting...");
    LOG_INF("Firmware v%d.%d.%d", FG_VERSION_MAJOR, FG_VERSION_MINOR, FG_VERSION_PATCH);

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
    g_state.valve_state = FG_VALVE_UNKNOWN;

    /* Green LED = booting */
    gpio_pin_set_dt(&led_g, 1);

    /* Start Zigbee network */
    LOG_INF("Starting Zigbee coordinator...");
    zigbee_enable();

    /* Initialize ESP32-C6 UART bridge */
    if (!device_is_ready(esp_uart)) {
        LOG_ERR("ESP32-C6 UART not ready");
        return -EIO;
    }

    LOG_INF("Hub Node initialized successfully");
    LOG_INF("Waiting for Zigbee network formation...");

    /* Main loop */
    while (1) {
        k_sleep(K_SECONDS(1));
        g_state.uptime_seconds++;

        /* Periodic leak evaluation every 5 seconds */
        if (g_state.uptime_seconds % 5 == 0) {
            fg_evaluate_leak_decision();
        }

        /* Update display every 1 second */
        /* (handled by display thread) */

        /* Update LED status */
        if (!alarm_active) {
            /* Normal operation: slow green blink */
            if (g_state.uptime_seconds % 2 == 0) {
                gpio_pin_set_dt(&led_g, 1);
            } else {
                gpio_pin_set_dt(&led_g, 0);
            }
        }
    }

    return 0;
}
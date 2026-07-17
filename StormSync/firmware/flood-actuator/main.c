/*
 * StormSync — Flood Actuator Firmware
 * ESP32-WROOM-32E, FreeRTOS
 *
 * Controls motorized backflow preventer valve, backup pump relay,
 * and audible alarm. Has independent float switch safety interlock
 * (hardware path, independent of MCU). 12V SLA battery backup.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/adc.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "StormSync-Act";

static ss_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;

/* Actuator state */
static uint8_t g_valve_status = 0;  /* 0=open, 1=closed, 2=moving */
static uint8_t g_pump_relay = 0;
static uint8_t g_alarm_status = 0;

/* === SX1262 SPI === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ACT_GPIO_SX_MOSI,
        .miso_io_num = ACT_GPIO_SX_MISO,
        .sclk_io_num = ACT_GPIO_SX_SCK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000, .mode = 0,
        .spics_io_num = ACT_GPIO_SX_NSS, .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) { gpio_set_level(ACT_GPIO_SX_RST, assert ? 0 : 1); }
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(ACT_GPIO_SX_DIO1); }

static const ss_spi_interface_t g_spi_iface = {
    .init = spi_init, .cs_select = NULL, .cs_release = NULL,
    .transfer = spi_transfer, .reset = spi_reset,
    .delay_ms = spi_delay_ms, .dio1_read = spi_dio1_read,
    .dio1_irq_enable = NULL,
};

/* === Valve Control === */
static void valve_close(void)
{
    if (g_valve_status == 1) return; /* Already closed */
    ESP_LOGI(TAG, "Closing backflow valve...");
    g_valve_status = 2; /* Moving */
    gpio_set_level(ACT_GPIO_VALVE_CLOSE, 1);
    gpio_set_level(ACT_GPIO_VALVE_OPEN, 0);

    /* Wait for closed position reed switch (timeout 30s) */
    for (int i = 0; i < 300; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (gpio_get_level(ACT_GPIO_VALVE_CL)) {
            g_valve_status = 1; /* Closed */
            break;
        }
    }

    gpio_set_level(ACT_GPIO_VALVE_CLOSE, 0);
    if (g_valve_status != 1) {
        ESP_LOGE(TAG, "Valve close timeout!");
        /* Alert */
        ss_message_t alert;
        ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                       SS_ALERT_VALVE_FAULT, 3, NULL, 0);
        ss_mesh_send(&g_mesh, &alert);
    } else {
        ESP_LOGI(TAG, "Valve closed");
    }
}

static void valve_open(void)
{
    if (g_valve_status == 0) return; /* Already open */
    ESP_LOGI(TAG, "Opening backflow valve...");
    g_valve_status = 2;
    gpio_set_level(ACT_GPIO_VALVE_OPEN, 1);
    gpio_set_level(ACT_GPIO_VALVE_CLOSE, 0);

    for (int i = 0; i < 300; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (gpio_get_level(ACT_GPIO_VALVE_OP)) {
            g_valve_status = 0; /* Open */
            break;
        }
    }

    gpio_set_level(ACT_GPIO_VALVE_OPEN, 0);
    if (g_valve_status != 0) {
        ESP_LOGE(TAG, "Valve open timeout!");
    } else {
        ESP_LOGI(TAG, "Valve opened");
    }
}

/* === Backup Pump Control === */
static void pump_on(void)
{
    if (g_pump_relay) return;
    ESP_LOGI(TAG, "Activating backup pump");
    gpio_set_level(ACT_GPIO_PUMP_RELAY, 1);
    g_pump_relay = 1;
}

static void pump_off(void)
{
    if (!g_pump_relay) return;
    ESP_LOGI(TAG, "Deactivating backup pump");
    gpio_set_level(ACT_GPIO_PUMP_RELAY, 0);
    g_pump_relay = 0;
}

/* === Alarm Control === */
static void alarm_on(void)
{
    if (g_alarm_status) return;
    ESP_LOGW(TAG, "ALARM ACTIVATED");
    gpio_set_level(ACT_GPIO_SIREN, 1);
    g_alarm_status = 1;
}

static void alarm_off(void)
{
    if (!g_alarm_status) return;
    ESP_LOGI(TAG, "Alarm silenced");
    gpio_set_level(ACT_GPIO_SIREN, 0);
    g_alarm_status = 0;
}

/* === Battery Health === */
static uint8_t read_battery_v(void)
{
    int raw = adc1_get_raw(ADC1_CHANNEL_4); /* GPIO32 */
    float v = raw * 0.0129; /* Divider factor */
    return (uint8_t)(v);
}

static uint8_t compute_battery_health(void)
{
    uint8_t v = read_battery_v();
    /* 12V SLA: 12.6V=100%, 11.8V=50%, 10.5V=0% */
    if (v >= 126) return 100;
    if (v <= 105) return 0;
    return (uint8_t)((v - 105) * 100 / 21);
}

/* === Float Switch Safety Interlock === */
/* This is a HARDWARE interlock — the float switch directly drives the
 * valve close and pump relay through discrete logic (independent of MCU).
 * The MCU monitors the float switch state for reporting and alarm.
 */
static void IRAM_ATTR float_switch_isr(void *arg)
{
    /* Hardware path already triggers valve + pump.
     * MCU just activates alarm and sends alert.
     */
    /* Note: avoid heavy operations in ISR — set flag */
}

/* === Command Handler Task === */
static void command_task(void *arg)
{
    ss_message_t msg;
    while (1) {
        if (ss_mesh_recv(&g_mesh, &msg, 2000) == 0) {
            if (msg.header.type == SS_MSG_COMMAND) {
                uint8_t cmd = msg.payload[0];
                ESP_LOGI(TAG, "Command received: 0x%02X", cmd);

                switch (cmd) {
                    case SS_CMD_VALVE_CLOSE:
                        valve_close();
                        break;
                    case SS_CMD_VALVE_OPEN:
                        valve_open();
                        break;
                    case SS_CMD_PUMP_ON:
                        pump_on();
                        break;
                    case SS_CMD_PUMP_OFF:
                        pump_off();
                        break;
                    case SS_CMD_ALARM_ON:
                        alarm_on();
                        break;
                    case SS_CMD_ALARM_OFF:
                        alarm_off();
                        break;
                    case SS_CMD_STORM_MODE:
                        ESP_LOGI(TAG, "Storm mode — pre-closing valve");
                        valve_close();
                        break;
                    case SS_CMD_NORMAL_MODE:
                        ESP_LOGI(TAG, "Normal mode — opening valve, stopping pump");
                        valve_open();
                        pump_off();
                        alarm_off();
                        break;
                }

                /* Send CMD_ACK */
                ss_message_t ack;
                memset(&ack, 0, sizeof(ack));
                ack.header.sync[0] = SS_SYNC0;
                ack.header.sync[1] = SS_SYNC1;
                ack.header.src = g_mesh.node_id;
                ack.header.dst = 0x00;
                ack.header.type = SS_MSG_CMD_ACK;
                ack.header.msg_id = g_msg_seq++;
                ack.payload[0] = cmd;
                ack.payload[1] = 0; /* Success */
                ack.payload_len = 2;
                ss_mesh_send(&g_mesh, &ack);
            }
        }

        /* Check float switch (independent of mesh) */
        if (gpio_get_level(ACT_GPIO_FLOAT_SW)) {
            /* High water level detected by float switch!
             * Hardware path already closed valve + started pump.
             * MCU activates alarm + sends critical alert.
             */
            if (!g_alarm_status) {
                alarm_on();
                ss_message_t alert;
                uint8_t data[1] = { 1 };
                ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                               SS_ALERT_FLOAT_TRIGGER, 3, data, 1);
                ss_mesh_send(&g_mesh, &alert);
                ESP_LOGE(TAG, "FLOAT SWITCH TRIGGERED — FLOOD IMMINENT!");
            }
        } else {
            /* Float switch normal — silence alarm after delay */
            /* (Don't auto-silence; user must acknowledge) */
        }

        /* Check manual override */
        if (gpio_get_level(ACT_GPIO_OVERRIDE)) {
            ESP_LOGI(TAG, "Manual override pressed");
            valve_close();
            pump_on();
            alarm_on();
            vTaskDelay(pdMS_TO_TICKS(3000)); /* Debounce */
        }
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        uint8_t bat_v = read_battery_v();
        uint8_t bat_health = compute_battery_health();
        uint8_t mains = gpio_get_level(ACT_GPIO_MAINS) ? 1 : 0;
        uint8_t flt = gpio_get_level(ACT_GPIO_FLOAT_SW) ? 1 : 0;

        ss_message_t msg;
        ss_build_actuator_telem(&msg, g_mesh.node_id, g_msg_seq++,
                                bat_v, g_valve_status, g_pump_relay,
                                flt, mains, g_alarm_status, bat_health,
                                g_mesh.last_rssi);

        ss_mesh_wait_slot(&g_mesh);
        ss_mesh_send(&g_mesh, &msg);

        /* Low battery alert */
        if (bat_v < SUMP_BATTERY_LOW_V) {
            ss_message_t alert;
            uint8_t data[1] = { bat_v };
            ss_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           SS_ALERT_LOW_BATTERY, 2, data, 1);
            ss_mesh_send(&g_mesh, &alert);
        }

        vTaskDelay(pdMS_TO_TICKS(30000)); /* 30 second telemetry */
    }
}

/* === Alarm Timeout Task === */
static void alarm_timeout_task(void *arg)
{
    static uint32_t alarm_start = 0;
    while (1) {
        if (g_alarm_status && alarm_start == 0) {
            alarm_start = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
        }
        if (g_alarm_status && alarm_start > 0) {
            uint32_t elapsed = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ) - alarm_start;
            if (elapsed > 1800) { /* 30 minutes */
                ESP_LOGI(TAG, "Auto-silencing alarm after 30 min (noise ordinance)");
                alarm_off();
                alarm_start = 0;
            }
        }
        if (!g_alarm_status) alarm_start = 0;
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "StormSync Flood Actuator starting...");

    /* Initialize GPIOs */
    gpio_set_direction(ACT_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(ACT_GPIO_SX_RST, 1);
    gpio_set_direction(ACT_GPIO_SX_DIO1, GPIO_MODE_INPUT);

    gpio_set_direction(ACT_GPIO_VALVE_CLOSE, GPIO_MODE_OUTPUT);
    gpio_set_direction(ACT_GPIO_VALVE_OPEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(ACT_GPIO_PUMP_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_direction(ACT_GPIO_SIREN, GPIO_MODE_OUTPUT);

    gpio_set_direction(ACT_GPIO_FLOAT_SW, GPIO_MODE_INPUT);
    gpio_set_direction(ACT_GPIO_MAINS, GPIO_MODE_INPUT);
    gpio_set_direction(ACT_GPIO_VALVE_CL, GPIO_MODE_INPUT);
    gpio_set_direction(ACT_GPIO_VALVE_OP, GPIO_MODE_INPUT);
    gpio_set_direction(ACT_GPIO_OVERRIDE, GPIO_MODE_INPUT);

    /* All outputs off initially */
    gpio_set_level(ACT_GPIO_VALVE_CLOSE, 0);
    gpio_set_level(ACT_GPIO_VALVE_OPEN, 0);
    gpio_set_level(ACT_GPIO_PUMP_RELAY, 0);
    gpio_set_level(ACT_GPIO_SIREN, 0);

    /* Float switch interrupt */
    gpio_set_intr_type(ACT_GPIO_FLOAT_SW, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ACT_GPIO_FLOAT_SW, float_switch_isr, NULL);

    /* ADC for battery */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);

    /* Initialize SPI + Radio */
    spi_init();
    ss_radio_config_t radio_cfg = {
        .frequency = SS_NET_FREQ_HZ,
        .bandwidth = SS_NET_BW_HZ,
        .spreading_factor = SS_NET_SF,
        .coding_rate = SS_NET_CR,
        .preamble_len = SS_NET_PREAMBLE,
        .tx_power_dbm = SS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    ss_mesh_init(&g_mesh, SS_NODE_ACTUATOR, &g_spi_iface, &radio_cfg);

    if (ss_mesh_join(&g_mesh) != 0) {
        ESP_LOGW(TAG, "Mesh join failed, will retry");
    }

    /* Start tasks */
    xTaskCreate(command_task, "cmd", 6144, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telem", 4096, NULL, 4, NULL);
    xTaskCreate(alarm_timeout_task, "alarm_to", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Flood Actuator running. Free heap: %lu",
             (unsigned long)esp_get_free_heap_size());
}
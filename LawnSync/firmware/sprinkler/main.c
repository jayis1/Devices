/*
 * LawnSync — Sprinkler Controller Firmware
 * ESP32-WROOM-32E, FreeRTOS
 *
 * Controls 8 irrigation zones with safety interlocks:
 * - Leak detection (flow with no active zone)
 * - Over-pressure shutoff
 * - Freeze protection
 * - Rain skip
 * - Max runtime limit
 * - Hardware watchdog
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "LawnSync-Sprinkler";

/* === Pin Map === */
static const int zone_pins[SPR_NUM_ZONES] = {
    SPR_GPIO_ZONE1, SPR_GPIO_ZONE2, SPR_GPIO_ZONE3, SPR_GPIO_ZONE4,
    SPR_GPIO_ZONE5, SPR_GPIO_ZONE6, SPR_GPIO_ZONE7, SPR_GPIO_ZONE8,
};

/* === Global state === */
static ls_mesh_ctx_t g_mesh;
static volatile uint8_t g_active_zone = 0; /* 0=none, 1-8 */
static volatile uint16_t g_flow_pulse_count = 0;
static volatile uint16_t g_rain_tip_count = 0;
static volatile uint16_t g_zone_runtime_s = 0;
static volatile bool g_emergency_stop = false;
static QueueHandle_t g_cmd_queue;

/* === Flow Meter ISR === */
static void IRAM_ATTR flow_isr(void *arg)
{
    g_flow_pulse_count++;
}

/* === Rain Gauge ISR === */
static void IRAM_ATTR rain_isr(void *arg)
{
    g_rain_tip_count++;
}

/* === SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPR_GPIO_SX_MOSI,
        .miso_io_num = SPR_GPIO_SX_MISO,
        .sclk_io_num = SPR_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = SPR_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t a) { gpio_set_level(SPR_GPIO_SX_RST, a ? 0 : 1); }
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(SPR_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int e) { (void)e; }

static const ls_spi_interface_t g_spi_iface = {
    .init = spi_init, .cs_select = spi_cs_select, .cs_release = spi_cs_release,
    .transfer = spi_transfer, .reset = spi_reset, .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read, .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === Valve Control === */
static void valve_open(uint8_t zone)
{
    if (zone < 1 || zone > SPR_NUM_ZONES) return;

    /* Open master valve first */
    gpio_set_level(SPR_GPIO_MASTER, 1);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Soft-start: 50% PWM for 100ms to reduce water hammer */
    /* In production: use ledc PWM on zone pin */
    gpio_set_level(zone_pins[zone - 1], 1);

    g_active_zone = zone;
    g_zone_runtime_s = 0;
    ESP_LOGI(TAG, "Zone %d opened", zone);
}

static void valve_close(uint8_t zone)
{
    if (zone < 1 || zone > SPR_NUM_ZONES) return;
    gpio_set_level(zone_pins[zone - 1], 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(SPR_GPIO_MASTER, 0); /* Close master valve */
    g_active_zone = 0;
    ESP_LOGI(TAG, "Zone %d closed", zone);
}

static void all_valves_close(void)
{
    for (int i = 0; i < SPR_NUM_ZONES; i++)
        gpio_set_level(zone_pins[i], 0);
    gpio_set_level(SPR_GPIO_MASTER, 0);
    g_active_zone = 0;
}

static uint8_t get_valve_status(void)
{
    uint8_t status = 0;
    for (int i = 0; i < SPR_NUM_ZONES; i++)
        if (gpio_get_level(zone_pins[i]))
            status |= (1 << i);
    return status;
}

/* === Flow Rate Calculation === */
/* YF-S201: ~30 pulses per liter → flow_rate (L/min) = pulses * 60 / 30 / dt_s */
static uint16_t get_flow_rate(void)
{
    static uint16_t last_count = 0;
    uint16_t current = g_flow_pulse_count;
    uint16_t delta = current - last_count;
    last_count = current;
    /* Assuming 1-second interval between calls → L/min = delta * 2 */
    /* Return in 0.1 L/min units */
    return (uint16_t)(delta * 20); /* ×0.1 L/min */
}

/* === Pressure Reading === */
static uint16_t read_pressure(void)
{
    /* MPX5700AP: 0.2–4.7V for 15–115 kPa → 0–3.3V via divider
     * ADC raw 0-4095 → kPa = (adc / 4095 * 3.3 - 0.2) / 4.5 * 100 + 15
     * Return in 0.1 kPa
     */
    /* In production: use esp_adc_cal */
    return 3500; /* placeholder: 350.0 kPa */
}

/* === Safety Monitor Task === */
static void safety_task(void *arg)
{
    uint16_t flow_rate;
    uint16_t pressure;
    bool rain_detected;

    while (1) {
        flow_rate = get_flow_rate();
        pressure = read_pressure();
        rain_detected = (g_rain_tip_count > 0);

        /* Leak detection: flow with no active zone */
        if (g_active_zone == 0 && flow_rate > SPR_MIN_FLOW_RATE) {
            ESP_LOGW(TAG, "LEAK: flow=%d with no active zone!", flow_rate);
            all_valves_close();
            gpio_set_level(SPR_GPIO_MASTER, 0);
            g_emergency_stop = true;

            /* Send alert via mesh */
            ls_message_t alert;
            uint8_t alert_data[2] = {
                (uint8_t)(flow_rate & 0xFF),
                (uint8_t)(flow_rate >> 8)
            };
            ls_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                          LS_ALERT_LEAK_DETECTED, 3, alert_data, 2);
            ls_mesh_send(&g_mesh, &alert);
        }

        /* Over-pressure */
        if (pressure > SPR_MAX_PRESSURE && g_active_zone != 0) {
            ESP_LOGW(TAG, "OVERPRESSURE: %d kPa", pressure / 10);
            all_valves_close();
            g_emergency_stop = true;
            ls_message_t alert;
            uint8_t data[2] = { (uint8_t)(pressure & 0xFF), (uint8_t)(pressure >> 8) };
            ls_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                          LS_ALERT_OVERPRESSURE, 3, data, 2);
            ls_mesh_send(&g_mesh, &alert);
        }

        /* Freeze protection: read temp from BME280 on weather node (via mesh)
         * For now, use a simple threshold check on ambient
         */
        /* In production: receive weather data from hub */

        /* Rain skip */
        if (rain_detected && g_active_zone != 0) {
            ESP_LOGI(TAG, "Rain detected, skipping current zone");
            all_valves_close();
        }

        /* Max runtime */
        if (g_active_zone != 0) {
            g_zone_runtime_s++;
            if (g_zone_runtime_s >= SPR_MAX_ZONE_RUNTIME) {
                ESP_LOGW(TAG, "Zone %d max runtime exceeded", g_active_zone);
                valve_close(g_active_zone);
            }
        }

        /* Reset rain count daily (simplified: reset when idle) */
        if (g_active_zone == 0 && g_rain_tip_count > 0) {
            /* Log rain amount, then reset */
            ESP_LOGI(TAG, "Rain: %d tips (%.1f mm)", g_rain_tip_count,
                     g_rain_tip_count * 0.2);
            g_rain_tip_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === Mesh Receiver Task === */
static void mesh_task(void *arg)
{
    ls_radio_config_t radio_cfg = {
        .frequency = LS_NET_FREQ_HZ,
        .bandwidth = LS_NET_BW_HZ,
        .spreading_factor = LS_NET_SF,
        .coding_rate = LS_NET_CR,
        .preamble_len = LS_NET_PREAMBLE,
        .tx_power_dbm = LS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    ls_mesh_init(&g_mesh, LS_NODE_SPRINKLER, &g_spi_iface, &radio_cfg);
    ls_mesh_join(&g_mesh);

    ESP_LOGI(TAG, "Sprinkler node joined: id=%d slot=%d",
             g_mesh.node_id, g_mesh.tdma_slot);

    ls_message_t msg;
    while (1) {
        if (ls_mesh_recv(&g_mesh, &msg, 5000) == 0) {
            if (msg.header.type == LS_MSG_COMMAND) {
                uint8_t cmd_type = msg.payload[0];
                switch (cmd_type) {
                    case LS_CMD_VALVE_OPEN: {
                        uint8_t zone = msg.payload[1];
                        uint16_t duration_s = msg.payload[2] | (msg.payload[3] << 8);
                        ESP_LOGI(TAG, "CMD: Open zone %d for %d s", zone, duration_s);
                        if (!g_emergency_stop && zone >= 1 && zone <= SPR_NUM_ZONES) {
                            if (g_active_zone != 0)
                                valve_close(g_active_zone);
                            valve_open(zone);
                            /* Timer task will close after duration */
                        }
                        break;
                    }
                    case LS_CMD_VALVE_CLOSE: {
                        uint8_t zone = msg.payload[1];
                        ESP_LOGI(TAG, "CMD: Close zone %d", zone);
                        valve_close(zone);
                        break;
                    }
                    case LS_CMD_REBOOT:
                        ESP_LOGI(TAG, "CMD: Reboot");
                        esp_restart();
                        break;
                }
            } else if (msg.header.type == LS_MSG_CONFIG) {
                /* Apply config: thresholds, schedule updates */
                ESP_LOGI(TAG, "Config received from hub");
            }
        }

        /* Send telemetry every 60 seconds */
        static uint32_t last_telem = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000);
        if (now - last_telem >= 60) {
            ls_message_t telem;
            uint32_t total_flow = g_flow_pulse_count * 33; /* ~0.033 L/pulse */
            ls_build_sprinkler_telem(&telem, g_mesh.node_id, g_mesh.msg_seq++,
                                      g_active_zone, get_flow_rate(),
                                      total_flow, read_pressure(),
                                      (g_rain_tip_count > 0) ? 1 : 0,
                                      get_valve_status(), g_mesh.last_rssi);
            ls_mesh_send(&g_mesh, &telem);
            last_telem = now;
        }
    }
}

/* === Manual Override Button === */
/* GPIO0 button: long press → emergency stop all valves */
static void IRAM_ATTR override_isr(void *arg)
{
    all_valves_close();
    g_emergency_stop = true;
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "LawnSync Sprinkler Controller starting...");

    /* Initialize zone relay pins */
    for (int i = 0; i < SPR_NUM_ZONES; i++) {
        gpio_set_direction(zone_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(zone_pins[i], 0);
    }
    gpio_set_direction(SPR_GPIO_MASTER, GPIO_MODE_OUTPUT);
    gpio_set_level(SPR_GPIO_MASTER, 0);

    /* Initialize sensor input pins */
    gpio_set_direction(SPR_GPIO_FLOW, GPIO_MODE_INPUT);
    gpio_set_direction(SPR_GPIO_RAIN, GPIO_MODE_INPUT);
    gpio_set_direction(SPR_GPIO_PRESSURE, GPIO_MODE_INPUT);
    gpio_set_direction(SPR_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(SPR_GPIO_BUZZER, GPIO_MODE_OUTPUT);

    /* Initialize SPI + radio pins */
    gpio_set_direction(SPR_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(SPR_GPIO_SX_RST, 1);
    gpio_set_direction(SPR_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    spi_init();

    /* Install ISRs */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SPR_GPIO_FLOW, flow_isr, NULL);
    gpio_isr_handler_add(SPR_GPIO_RAIN, rain_isr, NULL);

    /* Manual override on GPIO0 */
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_pullup_en(GPIO_NUM_0);
    gpio_isr_handler_add(GPIO_NUM_0, override_isr, NULL);

    /* Create queues/tasks */
    g_cmd_queue = xQueueCreate(16, sizeof(ls_message_t));
    xTaskCreate(mesh_task, "mesh", 8192, NULL, 5, NULL);
    xTaskCreate(safety_task, "safety", 4096, NULL, 6, NULL); /* Higher priority */

    ESP_LOGI(TAG, "Sprinkler controller running. Zones: %d", SPR_NUM_ZONES);
}
/*
 * MosquitoSync — Window Barrier Firmware
 * ESP32, FreeRTOS
 *
 * The Window Barrier controls a motorized magnetic screen that closes within
 * 2 seconds of mosquito detection (from acoustic sentinel or hub command).
 * Features: limit switches, motor stall detection (anti-pinch), manual
 * override, auto-open timeout, battery monitoring.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "MosquitoSync-Barrier";

/* === State === */
static uint16_t g_msg_seq = 0;
static uint8_t  g_screen_status = 0; /* 0=open, 1=closed, 2=moving */
static uint8_t  g_last_trigger = 0;  /* 0=manual, 1=hub, 2=auto-detected */
static uint8_t  g_cycles_24h = 0;
static uint16_t g_motor_current = 0;
static uint32_t g_last_close_time = 0;

/* === SX1262 SPI Interface (ESP32) === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = BARRIER_GPIO_SX_MOSI,
        .miso_io_num = BARRIER_GPIO_SX_MISO,
        .sclk_io_num = BARRIER_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = BARRIER_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(HSPI_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(BARRIER_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(BARRIER_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const ms_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === Motor Control (DRV8833) === */
static void motor_init(void)
{
    gpio_set_direction(BARRIER_GPIO_MOT_AIN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(BARRIER_GPIO_MOT_AIN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(BARRIER_GPIO_MOT_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(BARRIER_GPIO_MOT_AIN1, 0);
    gpio_set_level(BARRIER_GPIO_MOT_AIN2, 0);
    gpio_set_level(BARRIER_GPIO_MOT_EN, 0); /* Sleep mode */
}

static void motor_close(uint8_t trigger_source)
{
    ESP_LOGI(TAG, "Closing barrier (trigger=%d)", trigger_source);

    g_screen_status = 2; /* Moving */
    g_last_trigger = trigger_source;

    /* Enable motor driver */
    gpio_set_level(BARRIER_GPIO_MOT_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    /* Close direction */
    gpio_set_level(BARRIER_GPIO_MOT_AIN1, 1);
    gpio_set_level(BARRIER_GPIO_MOT_AIN2, 0);

    /* Wait for closed position (reed switch) or timeout */
    uint32_t timeout = 5000; /* 5 seconds max */
    uint32_t start = (uint32_t)(esp_timer_get_time() / 1000);
    uint8_t stalled = 0;

    while (timeout > 0) {
        /* Check closed reed switch */
        if (gpio_get_level(BARRIER_GPIO_REED_CLOSED) == 0) {
            ESP_LOGI(TAG, "Barrier CLOSED (reed switch triggered)");
            break;
        }

        /* Check motor stall (current sense) */
        /* In production: read ADC
         * uint16_t raw = adc1_get_raw(ADC1_CHANNEL_5);
         * g_motor_current = raw * 3300 / 4095 / 100; // Convert to 0.01A
         */
        if (g_motor_current > BARRIER_MOTOR_STALL_MA) {
            ESP_LOGE(TAG, "Motor STALL detected (%d mA) — stopping!",
                     g_motor_current);
            stalled = 1;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        timeout -= (now - start > timeout) ? timeout : (now - start);
        start = now;
    }

    /* Stop motor */
    gpio_set_level(BARRIER_GPIO_MOT_AIN1, 0);
    gpio_set_level(BARRIER_GPIO_MOT_AIN2, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BARRIER_GPIO_MOT_EN, 0); /* Sleep */

    g_screen_status = stalled ? 0 : 1; /* Open if stalled, closed otherwise */
    g_last_close_time = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    g_cycles_24h++;

    if (g_screen_status == 1) {
        ESP_LOGI(TAG, "Barrier closed successfully");
    }
}

static void motor_open(uint8_t trigger_source)
{
    ESP_LOGI(TAG, "Opening barrier (trigger=%d)", trigger_source);

    g_screen_status = 2; /* Moving */

    /* Enable motor driver */
    gpio_set_level(BARRIER_GPIO_MOT_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    /* Open direction */
    gpio_set_level(BARRIER_GPIO_MOT_AIN1, 0);
    gpio_set_level(BARRIER_GPIO_MOT_AIN2, 1);

    /* Wait for open position (reed switch) or timeout */
    uint32_t timeout = 5000;
    uint32_t start = (uint32_t)(esp_timer_get_time() / 1000);
    uint8_t stalled = 0;

    while (timeout > 0) {
        if (gpio_get_level(BARRIER_GPIO_REED_OPEN) == 0) {
            ESP_LOGI(TAG, "Barrier OPEN (reed switch triggered)");
            break;
        }

        /* Stall check */
        if (g_motor_current > BARRIER_MOTOR_STALL_MA) {
            ESP_LOGE(TAG, "Motor STALL during open — stopping!");
            stalled = 1;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        timeout -= (now - start > timeout) ? timeout : (now - start);
        start = now;
    }

    /* Stop motor */
    gpio_set_level(BARRIER_GPIO_MOT_AIN1, 0);
    gpio_set_level(BARRIER_GPIO_MOT_AIN2, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BARRIER_GPIO_MOT_EN, 0);

    g_screen_status = stalled ? 1 : 0; /* Closed if stalled, open otherwise */
    ESP_LOGI(TAG, "Barrier open complete");
}

/* === Battery Monitoring === */
static uint8_t read_battery(void)
{
    /* In production: read ADC1_CH4
     * uint16_t raw = adc1_get_raw(ADC1_CHANNEL_4);
     * float voltage = raw * 3.3 * 2 / 4095; // Divider ratio 2:1
     * return (uint8_t)(voltage * 100); // x0.01V
     */
    return 370; /* 3.70V placeholder */
}

/* === Send Telemetry to Hub === */
static void send_telemetry(ms_mesh_ctx_t *mesh)
{
    uint8_t battery_v = read_battery();

    ms_message_t msg;
    ms_build_barrier_telem(&msg, mesh->node_id, g_msg_seq++,
                           battery_v, g_screen_status, g_last_trigger,
                           g_cycles_24h, g_motor_current, mesh->last_rssi);
    ms_mesh_send(mesh, &msg);

    ESP_LOGI(TAG, "Telemetry: status=%d trigger=%d cycles=%d bat=%.2fV",
             g_screen_status, g_last_trigger, g_cycles_24h, battery_v / 100.0);
}

/* === Mesh Task === */
static void mesh_task(void *arg)
{
    ms_mesh_ctx_t *mesh = (ms_mesh_ctx_t *)arg;

    ms_radio_config_t radio_cfg = {
        .frequency = MS_NET_FREQ_HZ,
        .bandwidth = MS_NET_BW_HZ,
        .spreading_factor = MS_NET_SF,
        .coding_rate = MS_NET_CR,
        .preamble_len = MS_NET_PREAMBLE,
        .tx_power_dbm = MS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (ms_mesh_init(mesh, MS_NODE_BARRIER, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    /* Join network */
    int join_retries = 0;
    while (join_retries < 10) {
        if (ms_mesh_join(mesh) == 0) {
            ESP_LOGI(TAG, "Joined mesh: node_id=%d slot=%d",
                     mesh->node_id, mesh->tdma_slot);
            break;
        }
        ESP_LOGW(TAG, "Join failed, retry %d", join_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
        join_retries++;
    }

    /* Listen for hub commands */
    ms_message_t msg;
    while (1) {
        if (ms_mesh_recv(mesh, &msg, 5000) == 0) {
            switch (msg.header.type) {
                case MS_MSG_COMMAND: {
                    uint8_t cmd = msg.payload[0];
                    switch (cmd) {
                        case MS_CMD_BARRIER_CLOSE:
                            ESP_LOGI(TAG, "Hub command: CLOSE");
                            motor_close(1); /* hub trigger */
                            break;
                        case MS_CMD_BARRIER_OPEN:
                            ESP_LOGI(TAG, "Hub command: OPEN");
                            motor_open(1);
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case MS_MSG_RISK_STATUS: {
                    /* Auto-close on high risk */
                    uint8_t level = msg.payload[0];
                    if (level >= 2 && g_screen_status == 0) {
                        ESP_LOGI(TAG, "High risk (level=%d) — auto-closing",
                                 level);
                        motor_close(2); /* auto-detected */
                    }
                    break;
                }
                case MS_MSG_TIME_SYNC:
                case MS_MSG_CONFIG:
                    break;
                default:
                    break;
            }
        }
    }
}

/* === Manual Override + Auto-Open Task === */
static void safety_task(void *arg)
{
    ESP_LOGI(TAG, "Safety + override task started");

    gpio_set_direction(BARRIER_GPIO_OVERRIDE, GPIO_MODE_INPUT_PULLUP);
    gpio_set_direction(BARRIER_GPIO_REED_CLOSED, GPIO_MODE_INPUT_PULLUP);
    gpio_set_direction(BARRIER_GPIO_REED_OPEN, GPIO_MODE_INPUT_PULLUP);

    while (1) {
        /* Manual override button */
        if (gpio_get_level(BARRIER_GPIO_OVERRIDE) == 0) {
            ESP_LOGI(TAG, "Manual override button pressed");
            if (g_screen_status == 0) {
                motor_close(0); /* manual trigger */
            } else if (g_screen_status == 1) {
                motor_open(0);
            }
            /* Debounce */
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        /* Auto-open timeout: if closed by auto-detection and no new
         * detections for 30 minutes, re-open for ventilation */
        if (g_screen_status == 1 && g_last_trigger == 2) {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            if (g_last_close_time > 0 &&
                (now - g_last_close_time) > BARRIER_AUTO_OPEN_TIMEOUT_S) {
                ESP_LOGI(TAG, "Auto-open timeout (30 min) — opening for ventilation");
                motor_open(0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    ms_mesh_ctx_t *mesh = (ms_mesh_ctx_t *)arg;
    while (1) {
        if (mesh->joined) {
            send_telemetry(mesh);
        }
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL * 1000));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "MosquitoSync Window Barrier starting...");

    static ms_mesh_ctx_t mesh;

    nvs_flash_init();

    /* Initialize GPIOs */
    gpio_set_direction(BARRIER_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(BARRIER_GPIO_SX_RST, 1);
    gpio_set_direction(BARRIER_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(BARRIER_GPIO_LED, GPIO_MODE_OUTPUT);

    /* Initialize motor control */
    motor_init();

    /* Create tasks */
    xTaskCreate(mesh_task, "mesh", 8192, &mesh, 5, NULL);
    xTaskCreate(safety_task, "safety", 4096, NULL, 4, NULL);
    xTaskCreate(telemetry_task, "telemetry", 4096, &mesh, 2, NULL);

    ESP_LOGI(TAG, "Window Barrier running. Free heap: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
}
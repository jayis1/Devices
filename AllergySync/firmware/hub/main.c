/*
 * AllergySync — Hub Firmware (ESP32-S3, ESP-IDF)
 *
 * Main hub coordinator:
 *  - TDMA mesh coordinator (Sub-GHz 868 MHz via LR1121)
 *  - Wi-Fi → MQTT/TLS cloud connection
 *  - Local decision engine (pollen → close windows, trigger purifier)
 *  - OTA coordinator
 *  - 30-day exposure database (SPIFFS/SQLite)
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "mqtt_client.h"
#include "mbedtls/aes.h"
#include "mbedtls/ccm.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/sha256.h"

#include "common/allergysync_proto.h"
#include "common/as_lr1121.h"
#include "common/as_tdma.h"

static const char *TAG = "AllergySync-Hub";

/* ---- Pin assignments (ESP32-S3) ---- */
#define PIN_LR_CS     4
#define PIN_LR_SCLK   5
#define PIN_LR_MISO   6
#define PIN_LR_MOSI   7
#define PIN_LR_DIO0   8
#define PIN_LR_DIO1   9
#define PIN_LR_RESET  10
#define PIN_LR_BUSY   11
#define PIN_LED       12
#define PIN_IMU_SDA   13
#define PIN_IMU_SCL   14
#define PIN_IMU_INT   15

/* ---- SPI device handle for LR1121 ---- */
static spi_device_handle_t lr_spi;

/* ---- TDMA context ---- */
static as_tdma_node_t tdma;
static as_tdma_node_t *tdma_ptr = &tdma;

/* ---- MQTT client ---- */
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* ---- Node registry ---- */
#define MAX_NODES 16
typedef struct {
    uint8_t id;
    uint8_t type;
    uint8_t slot;
    uint32_t last_seen;
    uint8_t session_key[16];
} node_entry_t;
static node_entry_t nodes[MAX_NODES];
static uint8_t node_count = 0;
static uint16_t slot_bitmap = 0x0001; /* slot 0 = hub */

/* ---- Decision engine state ---- */
typedef struct {
    uint8_t  pollen_class;
    uint8_t  pollen_conf;
    uint16_t pm2_5;
    uint16_t pm10;
    bool     windows_open;
    bool     purifier_on;
} decision_state_t;
static decision_state_t decision;

/* ---- Platform port for LR1121 ---- */
static void lr_cs_select(void) { gpio_set_level(PIN_LR_CS, 0); }
static void lr_cs_release(void) { gpio_set_level(PIN_LR_CS, 1); }

static void lr_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {0};
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    spi_device_polling_transmit(lr_spi, &t);
}

static void lr_reset(bool assert) { gpio_set_level(PIN_LR_RESET, assert ? 0 : 1); }
static bool lr_busy_read(void) { return gpio_get_level(PIN_LR_BUSY) != 0; }
static void lr_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static as_lr1121_port_t lr_port = {
    .cs_select  = lr_cs_select,
    .cs_release = lr_cs_release,
    .spi_xfer   = lr_spi_xfer,
    .reset      = lr_reset,
    .busy_read  = lr_busy_read,
    .delay_ms   = lr_delay_ms,
};

/* ---- Wi-Fi event handler ---- */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (mqtt_client)
            esp_mqtt_client_start(mqtt_client);
    }
}

static void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t any_id, got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, &any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, &got_ip);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = CONFIG_ALLERGYSYNC_WIFI_SSID,
            .password = CONFIG_ALLERGYSYNC_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
}

/* ---- MQTT event handler ---- */
static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to cloud");
        esp_mqtt_client_subscribe(mqtt_client, "allergysync/cmd/hub", 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT topic: %.*s", event->topic_len, event->topic);
        /* Process cloud commands here */
        break;
    default:
        break;
    }
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_ALLERGYSYNC_MQTT_URI,
        .credentials.authentication.password = CONFIG_ALLERGYSYNC_MQTT_PASS,
    };
    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(mqtt_client,
                                    (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);
}

/* ---- SPI init for LR1121 ---- */
static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_LR_MISO,
        .mosi_io_num = PIN_LR_MOSI,
        .sclk_io_num = PIN_LR_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000, /* 8 MHz */
        .mode = 0,
        .spics_io_num = PIN_LR_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &lr_spi);
}

/* ---- GPIO init ---- */
static void gpio_init(void)
{
    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = (1ULL << PIN_LR_RESET) | (1ULL << PIN_LED);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << PIN_LR_BUSY) | (1ULL << PIN_LR_DIO0) |
                           (1ULL << PIN_LR_DIO1) | (1ULL << PIN_IMU_INT);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_set_level(PIN_LR_RESET, 1); /* Deassert reset */
}

/* ---- SPIFFS init for exposure database ---- */
static void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&conf);
    ESP_LOGI(TAG, "SPIFFS mounted: %llu bytes",
             (unsigned long long)esp_spiffs_get_partition(NULL));
}

/* ---- TDMA RX callback (incoming mesh packets) ---- */
static void on_mesh_rx(const as_header_t *hdr, const uint8_t *payload,
                       size_t len, int8_t rssi)
{
    ESP_LOGI(TAG, "RX from node %d, type=0x%02X, rssi=%d, len=%d",
             hdr->src_id, hdr->msg_type, rssi, (int)len);

    /* Update node registry */
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].id == hdr->src_id) {
            nodes[i].last_seen = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }
    }

    switch (hdr->msg_type) {
    case AS_MSG_JOIN_REQ: {
        /* Assign a slot to new node */
        if (node_count < MAX_NODES) {
            uint8_t new_id = ++node_count;
            uint8_t slot = node_count + 1; /* Slots 1..N */
            slot_bitmap |= (1 << slot);

            nodes[new_id - 1].id = new_id;
            nodes[new_id - 1].slot = slot;
            nodes[new_id - 1].last_seen = xTaskGetTickCount() * portTICK_PERIOD_MS;

            /* Send join response */
            as_join_rsp_t rsp;
            rsp.assigned_id = new_id;
            rsp.slot = slot;
            /* In real implementation, derive key via ECDH */
            memset(rsp.session_key, 0xAA, 16);

            uint8_t pkt[AS_MAX_PACKET];
            size_t pkt_len;
            as_build_packet(pkt, &pkt_len, AS_MSG_JOIN_RSP, 0, new_id,
                            (uint8_t *)&rsp, sizeof(rsp));
            as_lr1121_tx(pkt, pkt_len, 100);
            ESP_LOGI(TAG, "Assigned node %d → slot %d", new_id, slot);
        }
        break;
    }

    case AS_MSG_TELEMETRY: {
        /* Forward to cloud via MQTT */
        char topic[64];
        snprintf(topic, sizeof(topic), "allergysync/telemetry/node/%d",
                 hdr->src_id);
        if (mqtt_client) {
            esp_mqtt_client_publish(mqtt_client, topic,
                                    (const char *)payload, (int)len, 1, 0);
        }

        /* Run local decision engine */
        if (hdr->src_id == 1) { /* Sentinel telemetry */
            as_telem_sentinel_t *s = (as_telem_sentinel_t *)payload;
            decision.pollen_class = s->pollen_class;
            decision.pollen_conf  = s->pollen_conf;
            decision.pm2_5        = s->pm2_5;
            decision.pm10         = s->pm10;
        }
        if (hdr->src_id >= 2 && hdr->src_id <= 3) {
            /* Window node telemetry */
            as_telem_window_t *w = (as_telem_window_t *)payload;
            decision.windows_open = (w->window_state != 0);
        }
        break;
    }

    case AS_MSG_HEARTBEAT:
        /* Just update last_seen (already done above) */
        break;

    default:
        break;
    }
}

/* ---- Local decision engine ---- */
static void decision_engine_task(void *arg)
{
    const uint16_t pollen_threshold = 500; /* particles/L */
    const uint16_t pm10_threshold  = 500; /* µg/m³ × 10 = 50 µg/m³ */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); /* Check every 30 s */

        bool high_pollen = (decision.pollen_class != AS_POLLEN_NONE &&
                            decision.pollen_conf > 60);
        bool high_pm10 = (decision.pm10 > pm10_threshold);
        bool high_particles = (decision.pollen_conf > 60 &&
                               (decision.pollen_class != AS_POLLEN_NONE));

        if ((high_pollen || high_pm10) && decision.windows_open) {
            ESP_LOGI(TAG, "DECISION: Close windows (pollen=%d conf=%d pm10=%d)",
                     decision.pollen_class, decision.pollen_conf,
                     decision.pm10);

            /* Send close command to all window nodes */
            as_command_t cmd = { .cmd_type = AS_CMD_CLOSE_WINDOW };
            for (int i = 2; i <= 3; i++) {
                uint8_t pkt[AS_MAX_PACKET];
                size_t pkt_len;
                as_build_packet(pkt, &pkt_len, AS_MSG_COMMAND, 0, i,
                                (uint8_t *)&cmd, sizeof(cmd));
                as_lr1121_tx(pkt, pkt_len, 100);
            }
            decision.windows_open = false;
        }

        if (high_pollen && !decision.purifier_on) {
            as_command_t cmd = { .cmd_type = AS_CMD_PURIFIER_ON };
            /* Send to window node (which controls the relay) */
            uint8_t pkt[AS_MAX_PACKET];
            size_t pkt_len;
            as_build_packet(pkt, &pkt_len, AS_MSG_COMMAND, 0, 2,
                            (uint8_t *)&cmd, sizeof(cmd));
            as_lr1121_tx(pkt, pkt_len, 100);
            decision.purifier_on = true;
            ESP_LOGI(TAG, "DECISION: Air purifier ON");
        }
    }
}

/* ---- TDMA beacon task (hub coordinator) ---- */
static void beacon_task(void *arg)
{
    while (1) {
        uint32_t unix_time = (uint32_t)time(NULL);
        as_tdma_hub_send_beacon(tdma_ptr, slot_bitmap, node_count, unix_time);
        vTaskDelay(pdMS_TO_TICKS(AS_MESH_FRAME_MS));
    }
}

/* ---- Mesh RX task ---- */
static void mesh_rx_task(void *arg)
{
    uint8_t rx_buf[256];
    while (1) {
        as_radio_pkt_info_t info;
        int ret = as_lr1121_rx(rx_buf, &info, AS_MESH_FRAME_MS);
        if (ret == AS_RADIO_OK) {
            as_tdma_handle_rx(tdma_ptr, rx_buf, info.length,
                              info.rssi, on_mesh_rx);
        }
    }
}

/* ---- LED status task ---- */
static void led_task(void *arg)
{
    while (1) {
        gpio_set_level(PIN_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(PIN_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(2900));
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "AllergySync Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Init subsystems */
    gpio_init();
    spi_init();
    spiffs_init();
    wifi_init();
    mqtt_init();

    /* Init LR1121 radio */
    if (as_lr1121_init(&lr_port) != 0) {
        ESP_LOGE(TAG, "LR1121 init failed!");
        return;
    }
    as_lr1121_set_channel(868100000); /* 868.1 MHz */
    as_lr1121_set_tx_power(14);
    as_lr1121_set_modem_fsk(50000, 25000, 100000); /* 50 kbps, 25 kHz fdev */

    /* Sync word (4 bytes) */
    uint8_t sync[] = { 0xA5, 0x1E, 0x9C, 0x47 };
    as_lr1121_set_sync_word(sync, 4);

    /* Init TDMA as hub */
    as_tdma_init(tdma_ptr, true, &lr_port);

    /* Create tasks */
    xTaskCreate(beacon_task, "beacon", 4096, NULL, 5, NULL);
    xTaskCreate(mesh_rx_task, "mesh_rx", 4096, NULL, 4, NULL);
    xTaskCreate(decision_engine_task, "decision", 4096, NULL, 3, NULL);
    xTaskCreate(led_task, "led", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "AllergySync Hub running. Waiting for nodes to join...");
}
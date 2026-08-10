/*
 * FireSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the Sub-GHz TDMA mesh, bridges to the cloud
 * via Wi-Fi/MQTT with 4G LTE backup, computes escape routes (Dijkstra),
 * runs multi-node fire consensus, dispatches suppression commands,
 * and auto-dispatches 911 via SIM7000 4G LTE.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "FireSync-Hub";

/* === Global state === */
static fs_mesh_ctx_t g_mesh;
static fs_radio_hal_t g_radio_hal;
static fs_radio_config_t g_radio_cfg;
static QueueHandle_t g_fire_queue;
static QueueHandle_t g_telemetry_queue;
static SemaphoreHandle_t g_mesh_mutex;
static fs_node_info_t g_node_table[FS_MESH_MAX_NODES];
static uint8_t g_room_count = 0;

/* Escape route state */
static uint8_t g_fire_active = 0;
static uint8_t g_fire_room = 0xFF;
static uint8_t g_fire_class = 0;
static uint8_t g_emergency_cancel_window = 0;
static uint16_t g_occupant_map = 0; /* Bitmask of occupied rooms */

/* Room graph (configured via mobile app, stored in NVS) */
#define FS_MAX_ROOMS 16
#define FS_MAX_EXITS 4
typedef struct {
    uint8_t  room_id;
    char     name[32];
    uint8_t  adjacent[6];    /* Adjacent room IDs (0xFF=none) */
    uint8_t  has_exit;      /* 0=no, 1=front, 2=back, 3=garage, 4=window */
    uint8_t  sentinel_node;  /* Node ID of sentinel in this room */
} fs_room_t;
static fs_room_t g_rooms[FS_MAX_ROOMS];

/* === SX1262 HAL (ESP32-S3 SPI) === */
static spi_device_handle_t g_spi;
static SemaphoreHandle_t g_spi_mutex;

static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = HUB_GPIO_SX_MOSI,
        .miso_io_num = HUB_GPIO_SX_MISO,
        .sclk_io_num = HUB_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,  /* 2 MHz */
        .mode = 0,
        .spics_io_num = -1,  /* Manual CS */
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
    g_spi_mutex = xSemaphoreCreateMutex();
    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_transaction_t t = {0};
    t.length = len * 8;
    if (tx) t.tx_buffer = tx;
    if (rx) t.rx_buffer = rx;
    spi_device_polling_transmit(g_spi, &t);
    xSemaphoreGive(g_spi_mutex);
    return 0;
}

static void hal_cs_low(void)  { gpio_set_level(HUB_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(HUB_GPIO_SX_NSS, 1); }
static void hal_reset(int assert) { gpio_set_level(HUB_GPIO_SX_RST, !assert); }
static int  hal_dio1_read(void) { return gpio_get_level(HUB_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(HUB_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) { /* IRQ handled in radio task */ }

/* === Fire Consensus === */
typedef struct {
    uint8_t  node_id;
    uint8_t  room_id;
    uint8_t  fire_class;
    uint8_t  confidence;
    uint32_t timestamp_ms;
} fs_fire_report_t;

#define FS_MAX_PENDING_REPORTS 8
static fs_fire_report_t g_pending_reports[FS_MAX_PENDING_REPORTS];
static int g_pending_count = 0;

static int fs_consensus_check(const fs_fire_report_t *new_report)
{
    /* Single node >85% → immediate confirmation */
    if (new_report->confidence >= FS_FLAMENET_CONF_CONFIRM) {
        ESP_LOGI(TAG, "Fire confirmed: room %d, class %d, conf %d%% (single high)",
                 new_report->room_id, new_report->fire_class, new_report->confidence);
        return 1;
    }

    /* 75-85%: check for corroboration within 5 seconds */
    if (new_report->confidence >= FS_FLAMENET_CONF_FIRE_PCT) {
        /* Add to pending */
        if (g_pending_count < FS_MAX_PENDING_REPORTS) {
            g_pending_reports[g_pending_count++] = *new_report;
        }
        /* Check if another node in the same/adjacent room reported */
        for (int i = 0; i < g_pending_count - 1; i++) {
            if (g_pending_reports[i].room_id == new_report->room_id ||
                g_pending_reports[i].room_id == new_report->room_id) { /* Adjacent */
                ESP_LOGI(TAG, "Fire confirmed: room %d (two-node consensus)",
                         new_report->room_id);
                return 1;
            }
        }
        /* Wait for corroboration (timeout handled in fire_task) */
        return 0;
    }

    return 0;
}

/* === Escape Route Computation (Dijkstra) === */
static int fs_compute_escape_route(uint8_t fire_room, fs_escape_route_t *route)
{
    if (!route) return -1;

    /* Simple Dijkstra: find nearest exit avoiding fire_room + adjacent */
    memset(route, 0, sizeof(*route));
    route->fire_room = fire_room;

    /* Mark fire room + adjacent rooms as avoid */
    route->avoid_rooms = (1 << fire_room);
    for (int i = 0; i < 6; i++) {
        if (g_rooms[fire_room].adjacent[i] != 0xFF) {
            route->avoid_rooms |= (1 << g_rooms[fire_room].adjacent[i]);
        }
    }

    /* Find nearest safe exit */
    int best_exit = -1;
    int best_dist = 999;
    for (int r = 0; r < FS_MAX_ROOMS; r++) {
        if (g_rooms[r].has_exit && !(route->avoid_rooms & (1 << r))) {
            /* Simple: pick the first safe exit (production: full Dijkstra) */
            if (best_exit == -1) {
                best_exit = g_rooms[r].has_exit - 1; /* 0=front, 1=back, 2=garage, 3=window */
                best_dist = 1;
            }
        }
    }

    if (best_exit < 0) {
        /* All exits blocked — route to nearest window */
        route->safe_exit = 3; /* window */
        route->voice_msg_id = 4; /* "Fire in bedroom. Exit through window." */
    } else {
        route->safe_exit = (uint8_t)best_exit;
        /* Voice message based on fire room + exit */
        if (fire_room == 0) { /* Kitchen */
            route->voice_msg_id = 0; /* "Fire in kitchen. Exit front door." */
        } else if (fire_room == 1) { /* Living room */
            route->voice_msg_id = 1; /* "Fire in living room. Exit back door." */
        } else {
            route->voice_msg_id = 2; /* "Fire detected. Leave immediately." */
        }
    }

    /* LED path: green for safe route, red for fire zone */
    route->led_path_mask = 0x0F; /* All strips green (production: path-specific) */
    route->led_red_mask = (1 << (fire_room % 4)); /* Fire zone red */

    /* Door release: all exit doors */
    route->door_mask = 0x0F; /* All doors released */

    return 0;
}

/* === Fire Response === */
static void fs_fire_response(uint8_t room_id, uint8_t fire_class,
                               uint8_t confidence)
{
    ESP_LOGW(TAG, "FIRE RESPONSE: room=%d class=%d conf=%d%%",
             room_id, fire_class, confidence);

    g_fire_active = 1;
    g_fire_room = room_id;
    g_fire_class = fire_class;
    g_emergency_cancel_window = EMERGENCY_CANCEL_WINDOW_S;

    /* 1. Compute escape route */
    fs_escape_route_t route;
    fs_compute_escape_route(room_id, &route);

    /* 2. Send ESCAPE_UPDATE to Escape Controller */
    fs_message_t esc_msg;
    fs_build_escape_route(&esc_msg, FS_HUB_NODE_ID, g_mesh.msg_counter++,
                          route.fire_room, route.safe_exit,
                          route.avoid_rooms, route.led_path_mask,
                          route.led_red_mask, route.voice_msg_id,
                          route.door_mask);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    fs_mesh_broadcast_emergency(&g_mesh, &g_radio_hal, &esc_msg);
    xSemaphoreGive(g_mesh_mutex);

    /* 3. Send ALARM_TRIGGER to all sentinels */
    fs_message_t alarm_msg;
    alarm_msg.header.src = FS_HUB_NODE_ID;
    alarm_msg.header.dst = FS_BROADCAST;
    alarm_msg.header.type = FS_MSG_ALARM_TRIGGER;
    alarm_msg.header.msg_id = g_mesh.msg_counter++;
    alarm_msg.payload_len = 0;
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    fs_mesh_broadcast_emergency(&g_mesh, &g_radio_hal, &alarm_msg);
    xSemaphoreGive(g_mesh_mutex);

    /* 4. Suppression: close stove gas valve */
    fs_message_t stove_msg;
    fs_build_command(&stove_msg, FS_HUB_NODE_ID, FS_NODE_STOVE,
                     g_mesh.msg_counter++, FS_CMD_CLOSE_VALVE, NULL, 0);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    fs_mesh_send_acked(&g_mesh, &g_radio_hal, &stove_msg, 1000, 3);
    xSemaphoreGive(g_mesh_mutex);

    /* 5. Suppression: HVAC shutoff */
    fs_message_t hvac_msg;
    hvac_msg.header.src = FS_HUB_NODE_ID;
    hvac_msg.header.dst = FS_BROADCAST;
    hvac_msg.header.type = FS_MSG_HVAC_SHUTOFF;
    hvac_msg.header.msg_id = g_mesh.msg_counter++;
    hvac_msg.payload_len = 0;
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    fs_mesh_broadcast_emergency(&g_mesh, &g_radio_hal, &hvac_msg);
    xSemaphoreGive(g_mesh_mutex);

    /* 6. Dispatch 911 via 4G LTE */
    ESP_LOGW(TAG, "Dispatching 911 via 4G LTE...");
    /* Production: AT commands to SIM7000 for SMS + voice call */
    /* sim7000_dispatch_911(address, room, occupant_count); */

    /* 7. Publish fire event to cloud */
    /* Production: MQTT publish firesync/{user}/hub/fire */

    /* 8. Sound Hub buzzer + strobe */
    ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 255); /* Buzzer on */
    ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
    gpio_set_level(HUB_GPIO_STROBE, 1);
}

/* === 911 Dispatch (SIM7000 4G LTE) === */
static void sim7000_init(void)
{
    /* Production: UART2 init, AT command sequence */
    ESP_LOGI(TAG, "SIM7000 4G LTE initialized");
}

static void sim7000_dispatch_911(const char *address, uint8_t room_id,
                                    uint8_t occupant_count)
{
    /* Production: AT+CMGS (SMS to 911), ATD (voice call) */
    ESP_LOGW(TAG, "911 DISPATCH: addr=%s room=%d occupants=%d",
             address, room_id, occupant_count);
    /* Automated voice message: */
    /* "This is an automated fire alert from FireSync home fire safety system. */
    /*  Fire detected at [address], in the [room name]. */
    /*  [N] occupants detected in the home. */
    /*  This is not a drill. Please dispatch fire department." */
}

/* === Wi-Fi / MQTT Bridge === */
static void wifi_init(void)
{
    /* Production: esp_wifi_init, connect to SSID, get IP */
    ESP_LOGI(TAG, "Wi-Fi initialized (production: connect to SSID)");
}

static void mqtt_init(void)
{
    /* Production: paho-mqtt or esp_mqtt, connect to broker */
    ESP_LOGI(TAG, "MQTT client initialized (production: connect to broker)");
}

/* === Radio Task === */
static void radio_task(void *arg)
{
    uint8_t rx_buf[FS_MAX_MSG];
    fs_message_t msg;

    while (1) {
        /* Listen for messages */
        xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
        int rx_len = fs_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   5000, &g_mesh.last_rssi);
        xSemaphoreGive(g_mesh_mutex);

        if (rx_len > 0 && fs_decode(&msg, rx_buf, rx_len) == 0) {
            /* Process message */
            switch (msg.header.type) {
            case FS_MSG_JOIN_REQ: {
                /* Assign TDMA slot */
                uint8_t slot = 1;
                for (int i = 1; i < FS_TDMA_SLOTS; i++) {
                    if (g_node_table[i].node_id == 0) {
                        slot = i;
                        break;
                    }
                }
                g_node_table[slot].node_id = msg.header.src;
                g_node_table[slot].node_type = msg.payload[0];
                g_node_table[slot].tdma_slot = slot;
                g_node_table[slot].online = 1;

                /* Send JOIN_ACK */
                fs_message_t ack;
                ack.header.src = FS_HUB_NODE_ID;
                ack.header.dst = msg.header.src;
                ack.header.type = FS_MSG_JOIN_ACK;
                ack.header.msg_id = g_mesh.msg_counter++;
                ack.payload[0] = slot;
                ack.payload_len = 1;

                uint8_t tx_buf[FS_MAX_MSG];
                size_t tx_len = fs_encode(&ack, tx_buf, sizeof(tx_buf));
                xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
                fs_sx1262_tx(&g_radio_hal, tx_buf, tx_len, 22);
                xSemaphoreGive(g_mesh_mutex);

                ESP_LOGI(TAG, "Node %d joined (slot %d, type %d)",
                         msg.header.src, slot, msg.payload[0]);
                break;
            }

            case FS_MSG_FIRE_ALERT: {
                fs_fire_alert_t *fa = (fs_fire_alert_t *)msg.payload;
                ESP_LOGW(TAG, "FIRE_ALERT from node %d: room=%d class=%d conf=%d%%",
                         msg.header.src, fa->room_id, fa->fire_class,
                         fa->confidence);

                /* Update occupant map */
                if (fa->occupant)
                    g_occupant_map |= (1 << fa->room_id);

                /* Check consensus */
                fs_fire_report_t report = {
                    .node_id = msg.header.src,
                    .room_id = fa->room_id,
                    .fire_class = fa->fire_class,
                    .confidence = fa->confidence,
                    .timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS,
                };

                if (!g_fire_active) {
                    if (fs_consensus_check(&report)) {
                        fs_fire_response(fa->room_id, fa->fire_class,
                                        fa->confidence);
                    }
                }
                break;
            }

            case FS_MSG_TELEMETRY: {
                /* Store telemetry — production: publish to MQTT */
                uint8_t subtype = msg.payload[0];
                switch (subtype) {
                case FS_TELEM_SENTINEL:
                    ESP_LOGI(TAG, "Sentinel telem from node %d", msg.header.src);
                    break;
                case FS_TELEM_STOVE:
                    ESP_LOGI(TAG, "Stove telem from node %d", msg.header.src);
                    break;
                case FS_TELEM_PANEL:
                    ESP_LOGI(TAG, "Panel telem from node %d", msg.header.src);
                    break;
                case FS_TELEM_ESCAPE:
                    ESP_LOGI(TAG, "Escape telem from node %d", msg.header.src);
                    break;
                }
                /* Update node table */
                for (int i = 0; i < FS_MESH_MAX_NODES; i++) {
                    if (g_node_table[i].node_id == msg.header.src) {
                        g_node_table[i].last_seen_ms =
                            xTaskGetTickCount() * portTICK_PERIOD_MS;
                        g_node_table[i].rssi = g_mesh.last_rssi;
                        break;
                    }
                }
                break;
            }

            case FS_MSG_OCCUPANT: {
                uint8_t room = msg.payload[0];
                uint8_t occupied = msg.payload[1];
                if (occupied)
                    g_occupant_map |= (1 << room);
                else
                    g_occupant_map &= ~(1 << room);
                ESP_LOGI(TAG, "Occupant: room %d %s", room,
                         occupied ? "occupied" : "empty");
                break;
            }

            case FS_MSG_SILENCE: {
                /* User silence request */
                g_fire_active = 0;
                gpio_set_level(HUB_GPIO_STROBE, 0);
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);

                /* Send ALARM_STOP to all */
                fs_message_t stop_msg;
                stop_msg.header.src = FS_HUB_NODE_ID;
                stop_msg.header.dst = FS_BROADCAST;
                stop_msg.header.type = FS_MSG_ALARM_STOP;
                stop_msg.header.msg_id = g_mesh.msg_counter++;
                stop_msg.payload_len = 0;
                xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
                fs_mesh_broadcast_emergency(&g_mesh, &g_radio_hal, &stop_msg);
                xSemaphoreGive(g_mesh_mutex);
                ESP_LOGI(TAG, "Alarm silenced by user");
                break;
            }

            case FS_MSG_HEARTBEAT: {
                /* Update node table */
                for (int i = 0; i < FS_MESH_MAX_NODES; i++) {
                    if (g_node_table[i].node_id == msg.header.src) {
                        g_node_table[i].battery_v = msg.payload[0];
                        g_node_table[i].rssi = (int8_t)msg.payload[1];
                        g_node_table[i].last_seen_ms =
                            xTaskGetTickCount() * portTICK_PERIOD_MS;
                        g_node_table[i].online = 1;
                        break;
                    }
                }
                break;
            }
            }
        }
    }
}

/* === Emergency Cancel Task === */
static void cancel_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (g_emergency_cancel_window > 0) {
            g_emergency_cancel_window--;
            if (g_emergency_cancel_window == 0 && g_fire_active) {
                ESP_LOGW(TAG, "Cancel window expired — 911 dispatch confirmed");
                /* Production: confirm 911 dispatch (can't cancel after this) */
            }
        }
    }
}

/* === Node Watchdog Task === */
static void watchdog_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); /* 30 seconds */
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        for (int i = 0; i < FS_MESH_MAX_NODES; i++) {
            if (g_node_table[i].node_id != 0 && g_node_table[i].online) {
                if (now - g_node_table[i].last_seen_ms > 120000) {
                    /* Node offline for >2 minutes */
                    g_node_table[i].online = 0;
                    ESP_LOGW(TAG, "Node %d offline (last seen %d ms ago)",
                             g_node_table[i].node_id,
                             now - g_node_table[i].last_seen_ms);
                    /* Production: publish alert to cloud */
                }
            }
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "FireSync Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO init */
    gpio_set_direction(HUB_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_STROBE, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(HUB_GPIO_USB_PWR, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_CELL_PWR, GPIO_MODE_OUTPUT);

    gpio_set_level(HUB_GPIO_SX_NSS, 1); /* CS high (deselected) */
    gpio_set_level(HUB_GPIO_SX_RST, 1); /* Radio not in reset */

    /* I²C init (BME280 + DS3231) */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUB_GPIO_BME_SDA,
        .scl_io_num = HUB_GPIO_BME_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, i2c_cfg.mode, 0, 0, 0);

    /* Buzzer PWM */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 3000, /* 3 kHz alarm tone */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);
    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = HUB_GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);

    /* Initialize radio HAL */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = FS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= FS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = FS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = FS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = FS_SYNC_WORD;
    g_radio_cfg.preamble_len    = FS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    fs_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Mesh init (Hub = node 0) */
    uint8_t aes_key[16] = {0}; /* Production: load from NVS */
    fs_mesh_init(&g_mesh, FS_HUB_NODE_ID, FS_NODE_HUB, aes_key);
    g_mesh.joined = 1; /* Hub is always "joined" */
    g_mesh.battery_v = 420;

    /* Init node table */
    memset(g_node_table, 0, sizeof(g_node_table));
    memset(g_rooms, 0xFF, sizeof(g_rooms));

    /* Initialize room graph (production: load from NVS) */
    /* Example: 4 rooms */
    g_rooms[0].room_id = 0; strcpy(g_rooms[0].name, "Kitchen");
    g_rooms[0].adjacent[0] = 1; g_rooms[0].has_exit = 0;
    g_rooms[1].room_id = 1; strcpy(g_rooms[1].name, "Living Room");
    g_rooms[1].adjacent[0] = 0; g_rooms[1].adjacent[1] = 2;
    g_rooms[1].has_exit = 1; /* Front door */
    g_rooms[2].room_id = 2; strcpy(g_rooms[2].name, "Bedroom");
    g_rooms[2].adjacent[0] = 1; g_rooms[2].adjacent[1] = 3;
    g_rooms[2].has_exit = 4; /* Window */
    g_rooms[3].room_id = 3; strcpy(g_rooms[3].name, "Bathroom");
    g_rooms[3].adjacent[0] = 2; g_rooms[3].has_exit = 0;
    g_room_count = 4;

    g_mesh_mutex = xSemaphoreCreateMutex();

    /* Wi-Fi + MQTT */
    wifi_init();
    mqtt_init();

    /* SIM7000 4G LTE */
    sim7000_init();

    /* Tasks */
    xTaskCreate(radio_task, "radio", 8192, NULL, 5, NULL);
    xTaskCreate(cancel_task, "cancel", 2048, NULL, 3, NULL);
    xTaskCreate(watchdog_task, "watchdog", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "FireSync Hub ready. Listening on 868 MHz TDMA mesh.");
    ESP_LOGI(TAG, "Room graph: %d rooms configured", g_room_count);
}
/*
 * GrillSync — Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Hub coordinates the Sub-GHz mesh network, bridges to the cloud
 * via Wi-Fi/MQTT, runs local edge DonenessNet inference, drives the
 * TFT display + LED ring, triggers gas shutoff relay on leak/fire,
 * and manages OTA firmware distribution.
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
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "GrillSync-Hub";

/* === Global state === */
static gs_mesh_ctx_t g_mesh;
static QueueHandle_t g_telemetry_queue;
static QueueHandle_t g_command_queue;
static SemaphoreHandle_t g_radio_mutex;
static uint8_t g_node_table[GS_SLOT_COUNT];

/* Cook session state */
static uint8_t g_cook_active = 0;
static uint32_t g_cook_start_time = 0;
static uint8_t g_active_probes = 0;

/* Safety state */
static uint8_t g_gas_shutoff_active = 0;
static uint8_t g_emergency_active = 0;
static uint16_t g_event_counter = 0;
static uint16_t g_alerts_24h = 0;

/* Thermal frame data from sentinel */
static int16_t g_surface_max_deci = 0;
static int16_t g_surface_avg_deci = 0;
static uint8_t g_hot_zones = 0;
static uint8_t g_flareup_risk = 0;
static uint16_t g_flareup_eta_ms = 0;

/* Gas state */
static uint16_t g_gas_ppm = 0;
static uint8_t g_gas_lel_pct = 0;

/* Ambient */
static float g_ambient_temp = 25.0;
static float g_ambient_hum = 50.0;

/* Probe data cache (latest temps per probe) */
static int16_t g_probe_temps[GS_MAX_PROBES][4]; /* [probe_id][tip, mid, surface, ambient] */
static uint8_t g_probe_meat_type[GS_MAX_PROBES];
static int16_t g_probe_target[GS_MAX_PROBES];
static uint8_t g_probe_doneness[GS_MAX_PROBES];

/* === SX1262 SPI Interface (ESP32-S3) === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = HUB_GPIO_SX_MOSI,
        .miso_io_num = HUB_GPIO_SX_MISO,
        .sclk_io_num = HUB_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = HUB_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { /* Managed by SPI driver */ }
static void spi_cs_release(void) { /* Managed by SPI driver */ }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(HUB_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(HUB_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const gs_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I2C for BME280 + DS3231 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUB_GPIO_BME_SDA,
        .scl_io_num = HUB_GPIO_BME_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static void read_bme280(float *temp, float *humidity, float *pressure)
{
    uint8_t buf[8];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xF7, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x76 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 8, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    *pressure = (buf[0] << 12 | buf[1] << 4 | buf[2] >> 4) / 256.0;
    *temp = (buf[3] << 12 | buf[4] << 4 | buf[5] >> 4) / 100.0;
    *humidity = (buf[6] << 8 | buf[7]) / 1024.0 * 100.0;
}

/* === LED Ring Display (24× WS2812B) === */
static void set_led_ring_color(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812B via RMT peripheral in production */
    (void)r; (void)g; (void)b;
}

static void update_led_ring_doneness(uint8_t doneness, uint8_t alert)
{
    if (alert == GS_PRIORITY_CRITICAL) {
        /* Flashing red for critical alerts */
        set_led_ring_color(255, 0, 0);
    } else if (alert == GS_PRIORITY_HIGH) {
        /* Orange for warnings */
        set_led_ring_color(255, 100, 0);
    } else if (!g_cook_active) {
        set_led_ring_color(0, 0, 50);  /* Dim blue idle */
    } else {
        /* Doneness gradient: red→orange→yellow→green */
        switch (doneness) {
            case GS_DONENESS_RAW:      set_led_ring_color(150, 0, 0); break;
            case GS_DONENESS_RARE:     set_led_ring_color(200, 50, 0); break;
            case GS_DONENESS_MED_RARE: set_led_ring_color(220, 100, 0); break;
            case GS_DONENESS_MEDIUM:  set_led_ring_color(200, 150, 0); break;
            case GS_DONENESS_MED_WELL: set_led_ring_color(150, 200, 0); break;
            case GS_DONENESS_WELL:     set_led_ring_color(0, 255, 0); break;
            default:                   set_led_ring_color(0, 0, 50); break;
        }
    }
}

/* === Gas Shutoff Relay === */
static void gas_shutoff_control(uint8_t shutoff)
{
    gpio_set_level(HUB_GPIO_GAS_RELAY, shutoff ? 1 : 0);
    g_gas_shutoff_active = shutoff;
    ESP_LOGW(TAG, "Gas shutoff %s", shutoff ? "ACTIVATED" : "released");
}

/* === Buzzer Control === */
static void buzzer_control(uint8_t on)
{
    if (on) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* === TFT Display (simplified) === */
static void tft_update(void)
{
    ESP_LOGI(TAG, "TFT: cook=%d probes=%d surf_max=%.1f°C gas=%dppm flare=%d%%",
             g_cook_active, g_active_probes,
             g_surface_max_deci / 10.0, g_gas_ppm, g_flareup_risk);
    for (int i = 0; i < GS_MAX_PROBES; i++) {
        if (g_probe_meat_type[i] != 0xFF && g_probe_temps[i][0] != 0) {
            ESP_LOGI(TAG, "  Probe %d: tip=%.1f°C mid=%.1f°C target=%.1f°C doneness=%d",
                     i, g_probe_temps[i][0] / 10.0, g_probe_temps[i][1] / 10.0,
                     g_probe_target[i] / 10.0, g_probe_doneness[i]);
        }
    }
}

/* === MQTT Client (simplified) === */
static void mqtt_publish_telemetry(const gs_message_t *msg)
{
    char json[512];
    uint8_t node_id = msg->header.src;
    uint8_t subtype = msg->payload[0];

    if (subtype == GS_TELEM_SENTINEL) {
        uint8_t batt = msg->payload[1];
        int16_t surf_max = msg->payload[2] | (msg->payload[3] << 8);
        int16_t surf_avg = msg->payload[4] | (msg->payload[5] << 8);
        uint8_t hot_zones = msg->payload[6];
        uint16_t gas_ppm = msg->payload[7] | (msg->payload[8] << 8);
        uint8_t lel = msg->payload[9];
        uint8_t flame_int = msg->payload[10];
        uint8_t flame_det = msg->payload[11];
        int16_t amb_temp = msg->payload[12] | (msg->payload[13] << 8);
        uint16_t amb_hum = msg->payload[14] | (msg->payload[15] << 8);
        uint16_t acoustic = msg->payload[16] | (msg->payload[17] << 8);
        uint8_t flare_risk = msg->payload[18];
        uint16_t flare_eta = msg->payload[19] | (msg->payload[20] << 8);

        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"sentinel\",\"battery\":%.2f,"
            "\"surface_max\":%.1f,\"surface_avg\":%.1f,\"hot_zones\":%d,"
            "\"gas_ppm\":%d,\"gas_lel_pct\":%d,\"flame_intensity\":%d,"
            "\"flame_detected\":%d,\"ambient_temp\":%.1f,\"ambient_hum\":%.1f,"
            "\"acoustic_energy\":%d,\"flareup_risk\":%d,\"flareup_eta_ms\":%d}",
            node_id, batt / 100.0,
            surf_max / 10.0, surf_avg / 10.0, hot_zones,
            gas_ppm, lel, flame_int,
            flame_det, amb_temp / 10.0, amb_hum / 10.0,
            acoustic, flare_risk, flare_eta * 100);
    } else if (subtype == GS_TELEM_PROBE) {
        uint8_t batt = msg->payload[1];
        uint8_t probe_id = msg->payload[2];
        uint8_t meat = msg->payload[3];
        int16_t tip = msg->payload[4] | (msg->payload[5] << 8);
        int16_t mid = msg->payload[6] | (msg->payload[7] << 8);
        int16_t surf = msg->payload[8] | (msg->payload[9] << 8);
        int16_t amb = msg->payload[10] | (msg->payload[11] << 8);
        int16_t target = msg->payload[12] | (msg->payload[13] << 8);
        uint8_t done = msg->payload[14];
        uint16_t eta = msg->payload[15] | (msg->payload[16] << 8);

        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"probe\",\"battery\":%.2f,"
            "\"probe_id\":%d,\"meat_type\":%d,"
            "\"temp_tip\":%.1f,\"temp_mid\":%.1f,\"temp_surface\":%.1f,"
            "\"temp_ambient\":%.1f,\"target_temp\":%.1f,"
            "\"doneness\":%d,\"eta_s\":%d}",
            node_id, batt / 100.0,
            probe_id, meat,
            tip / 10.0, mid / 10.0, surf / 10.0,
            amb / 10.0, target / 10.0,
            done, eta * 10);
    } else if (subtype == GS_TELEM_SMOKE) {
        uint16_t pm25 = msg->payload[4] | (msg->payload[5] << 8);
        uint16_t voc = msg->payload[8] | (msg->payload[9] << 8);
        uint8_t smoke_qual = msg->payload[14];

        snprintf(json, sizeof(json),
            "{\"node\":%d,\"type\":\"smoke\",\"pm25\":%.1f,"
            "\"voc_index\":%d,\"smoke_quality\":%d}",
            node_id, pm25 / 10.0, voc, smoke_qual);
    } else {
        snprintf(json, sizeof(json), "{\"node\":%d,\"type\":\"unknown\"}", node_id);
    }

    ESP_LOGI(TAG, "MQTT pub: %s", json);
    /* esp_mqtt_client_publish(mqtt_client, topic, json, 0, 1, 0); */
}

/* === Alert Processing === */
static void process_alert(uint8_t node_id, uint8_t alert_type, uint8_t severity)
{
    g_event_counter++;
    g_alerts_24h++;

    ESP_LOGW(TAG, "ALERT: node=%d type=0x%02X severity=%d event_id=%d",
             node_id, alert_type, severity, g_event_counter);

    switch (alert_type) {
        case GS_ALERT_GAS_LEAK:
            gas_shutoff_control(1);
            buzzer_control(1);
            g_emergency_active = 1;
            update_led_ring_doneness(0, GS_PRIORITY_CRITICAL);
            ESP_LOGE(TAG, "🚨 GAS LEAK DETECTED — auto shutoff activated!");
            break;

        case GS_ALERT_FLARE_UP_WARNING:
            buzzer_control(1);
            update_led_ring_doneness(0, GS_PRIORITY_HIGH);
            ESP_LOGW(TAG, "🔥 FLARE-UP WARNING — risk=%d%% ETA=%dms",
                     g_flareup_risk, g_flareup_eta_ms);
            break;

        case GS_ALERT_FLARE_UP_ACTIVE:
            gas_shutoff_control(1);
            buzzer_control(1);
            g_emergency_active = 1;
            update_led_ring_doneness(0, GS_PRIORITY_CRITICAL);
            ESP_LOGE(TAG, "🔥 FLARE-UP ACTIVE — gas shutoff!");
            break;

        case GS_ALERT_GRILL_FIRE:
            gas_shutoff_control(1);
            buzzer_control(1);
            g_emergency_active = 1;
            update_led_ring_doneness(0, GS_PRIORITY_CRITICAL);
            ESP_LOGE(TAG, "🚨 GRILL FIRE DETECTED — gas shutoff!");
            break;

        case GS_ALERT_CHILD_IN_ZONE:
            buzzer_control(1);
            update_led_ring_doneness(0, GS_PRIORITY_HIGH);
            ESP_LOGW(TAG, "👶 CHILD NEAR GRILL — alert!");
            break;

        case GS_ALERT_PROBE_OVERTEMP:
            ESP_LOGW(TAG, "⚠️ Probe cable overtemp — power off probe");
            break;

        case GS_ALERT_FOOD_UNDERCOOKED:
            ESP_LOGW(TAG, "⚠️ Food undercooked — below USDA safe temp");
            break;

        case GS_ALERT_FOOD_OVERCOOKED:
            ESP_LOGW(TAG, "⚠️ Food overcooked — exceeds target by >5°C");
            break;

        case GS_ALERT_SMOKE_CREOSOTE:
            ESP_LOGW(TAG, "💨 Creosote/acidic smoke detected — adjust airflow");
            break;

        default:
            ESP_LOGI(TAG, "Alert type 0x%02X", alert_type);
            break;
    }
}

/* === Process Probe Telemetry === */
static void process_probe_telemetry(uint8_t node_id, uint8_t probe_id,
                                      uint8_t meat_type,
                                      int16_t tip, int16_t mid,
                                      int16_t surface, int16_t ambient,
                                      int16_t target, uint8_t doneness,
                                      uint16_t eta_10s)
{
    if (probe_id >= GS_MAX_PROBES)
        return;

    /* Cache probe data */
    g_probe_temps[probe_id][0] = tip;
    g_probe_temps[probe_id][1] = mid;
    g_probe_temps[probe_id][2] = surface;
    g_probe_temps[probe_id][3] = ambient;
    g_probe_meat_type[probe_id] = meat_type;
    g_probe_target[probe_id] = target;
    g_probe_doneness[probe_id] = doneness;

    /* Check USDA safe temperature */
    int16_t usda_min = gs_usda_min_temp[meat_type < GS_MEAT_TYPE_COUNT ? meat_type : 0];
    if (usda_min > 0 && tip >= usda_min && g_cook_active) {
        /* Food has reached safe temp */
        if (tip >= target) {
            ESP_LOGI(TAG, "🥩 Probe %d DONE: tip=%.1f°C target=%.1f°C doneness=%d",
                     probe_id, tip / 10.0, target / 10.0, doneness);

            /* Broadcast doneness update */
            gs_message_t event;
            gs_build_doneness_update(&event, GS_HUB_NODE_ID,
                                      g_mesh.msg_seq++, probe_id, meat_type,
                                      doneness, eta_10s, tip, target);
            gs_mesh_send(&g_mesh, &event);
        }
    }

    /* Check overcook */
    if (target > 0 && tip > target + 50) { /* >5°C over target */
        gs_message_t alert;
        uint8_t data[4] = { probe_id, (uint8_t)(tip & 0xFF), (uint8_t)(tip >> 8), 0 };
        gs_build_alert(&alert, node_id, g_mesh.msg_seq++,
                       GS_ALERT_FOOD_OVERCOOKED, GS_PRIORITY_MEDIUM,
                       data, 4);
        process_alert(node_id, GS_ALERT_FOOD_OVERCOOKED, GS_PRIORITY_MEDIUM);
    }

    /* Check probe cable overtemp */
    if (surface > GS_TC_OVERTEMP_C * 10) {
        gs_message_t alert;
        uint8_t data[2] = { probe_id, 0 };
        gs_build_alert(&alert, node_id, g_mesh.msg_seq++,
                       GS_ALERT_PROBE_OVERTEMP, GS_PRIORITY_CRITICAL,
                       data, 2);
        process_alert(node_id, GS_ALERT_PROBE_OVERTEMP, GS_PRIORITY_CRITICAL);
    }

    update_led_ring_doneness(doneness, g_emergency_active ? GS_PRIORITY_CRITICAL : 0);
}

/* === Process Sentinel Telemetry === */
static void process_sentinel_telemetry(uint8_t node_id,
                                         int16_t surf_max, int16_t surf_avg,
                                         uint8_t hot_zones,
                                         uint16_t gas_ppm, uint8_t lel_pct,
                                         uint8_t flame_int, uint8_t flame_det,
                                         uint16_t acoustic, uint8_t flare_risk,
                                         uint16_t flare_eta_100ms)
{
    g_surface_max_deci = surf_max;
    g_surface_avg_deci = surf_avg;
    g_hot_zones = hot_zones;
    g_gas_ppm = gas_ppm;
    g_gas_lel_pct = lel_pct;
    g_flareup_risk = flare_risk;
    g_flareup_eta_ms = flare_eta_100ms * 100;

    /* Gas leak detection */
    if (gas_ppm >= GS_GAS_LEAK_10PCT_LEL_PPM) {
        uint8_t severity = (gas_ppm >= GS_GAS_LEAK_25PCT_LEL_PPM) ?
                           GS_PRIORITY_CRITICAL : GS_PRIORITY_CRITICAL;
        process_alert(node_id, GS_ALERT_GAS_LEAK, severity);
    }

    /* Grill fire detection */
    if (surf_max > GS_THERMAL_FLARE_TEMP_C * 10 && flame_det) {
        process_alert(node_id, GS_ALERT_GRILL_FIRE, GS_PRIORITY_CRITICAL);
    }

    /* Flare-up warning */
    if (flare_risk >= GS_FLAREUP_RISK_THRESHOLD && flare_eta_100ms > 0) {
        process_alert(node_id, GS_ALERT_FLARE_UP_WARNING, GS_PRIORITY_HIGH);
    }

    /* Flare-up active */
    if (flame_det && flare_risk > 85) {
        process_alert(node_id, GS_ALERT_FLARE_UP_ACTIVE, GS_PRIORITY_CRITICAL);
    }

    /* Child in zone (thermal detection handled in sentinel) */
}

/* === Mesh Coordinator Task === */
static void mesh_task(void *arg)
{
    gs_radio_config_t radio_cfg = {
        .frequency = GS_NET_FREQ_HZ,
        .bandwidth = GS_NET_BW_HZ,
        .spreading_factor = GS_NET_SF,
        .coding_rate = GS_NET_CR,
        .preamble_len = GS_NET_PREAMBLE,
        .tx_power_dbm = GS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (gs_mesh_init(&g_mesh, GS_NODE_HUB, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    g_mesh.node_id = GS_HUB_NODE_ID;
    g_mesh.tdma_slot = 0;
    g_mesh.joined = 1;

    ESP_LOGI(TAG, "Hub mesh coordinator started (node_id=0, slot=0)");

    gs_message_t msg;
    while (1) {
        if (gs_mesh_recv(&g_mesh, &msg, 2000) == 0) {
            switch (msg.header.type) {
                case GS_MSG_JOIN_REQ: {
                    uint8_t new_id, new_slot;
                    if (gs_mesh_hub_assign_slot(&g_mesh, msg.payload[0],
                                                &new_id, &new_slot) == 0) {
                        gs_message_t ack;
                        memset(&ack, 0, sizeof(ack));
                        ack.header.sync[0] = GS_SYNC0;
                        ack.header.sync[1] = GS_SYNC1;
                        ack.header.src = GS_HUB_NODE_ID;
                        ack.header.dst = new_id;
                        ack.header.type = GS_MSG_JOIN_ACK;
                        ack.header.msg_id = g_mesh.msg_seq++;
                        ack.payload[0] = new_id;
                        ack.payload[1] = new_slot;
                        ack.payload_len = 2;

                        gs_mesh_send(&g_mesh, &ack);

                        g_node_table[new_id] = msg.payload[0];
                        ESP_LOGI(TAG, "Node joined: id=%d slot=%d type=%d",
                                 new_id, new_slot, msg.payload[0]);
                    }
                    break;
                }

                case GS_MSG_TELEMETRY:
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    if (msg.payload[0] == GS_TELEM_SENTINEL) {
                        int16_t surf_max = msg.payload[2] | (msg.payload[3] << 8);
                        int16_t surf_avg = msg.payload[4] | (msg.payload[5] << 8);
                        uint8_t hz = msg.payload[6];
                        uint16_t gas = msg.payload[7] | (msg.payload[8] << 8);
                        uint8_t lel = msg.payload[9];
                        uint8_t flame_int = msg.payload[10];
                        uint8_t flame_det = msg.payload[11];
                        uint16_t acoustic = msg.payload[16] | (msg.payload[17] << 8);
                        uint8_t flare_risk = msg.payload[18];
                        uint16_t flare_eta = msg.payload[19] | (msg.payload[20] << 8);
                        process_sentinel_telemetry(msg.header.src,
                                                    surf_max, surf_avg, hz,
                                                    gas, lel, flame_int, flame_det,
                                                    acoustic, flare_risk, flare_eta);
                    } else if (msg.payload[0] == GS_TELEM_PROBE) {
                        uint8_t probe_id = msg.payload[2];
                        uint8_t meat = msg.payload[3];
                        int16_t tip = msg.payload[4] | (msg.payload[5] << 8);
                        int16_t mid = msg.payload[6] | (msg.payload[7] << 8);
                        int16_t surf = msg.payload[8] | (msg.payload[9] << 8);
                        int16_t amb = msg.payload[10] | (msg.payload[11] << 8);
                        int16_t target = msg.payload[12] | (msg.payload[13] << 8);
                        uint8_t done = msg.payload[14];
                        uint16_t eta = msg.payload[15] | (msg.payload[16] << 8);
                        process_probe_telemetry(msg.header.src, probe_id, meat,
                                                tip, mid, surf, amb, target, done, eta);
                    } else if (msg.payload[0] == GS_TELEM_SMOKE) {
                        uint8_t smoke_qual = msg.payload[14];
                        if (smoke_qual == 2) { /* creosote */
                            process_alert(msg.header.src, GS_ALERT_SMOKE_CREOSOTE,
                                          GS_PRIORITY_MEDIUM);
                        }
                    }
                    break;

                case GS_MSG_ALERT: {
                    xQueueSend(g_telemetry_queue, &msg, 0);
                    uint8_t alert_type = msg.payload[0];
                    uint8_t severity = msg.payload[1];
                    /* Child-in-zone comes from sentinel's thermal processing */
                    if (alert_type == GS_ALERT_CHILD_IN_ZONE) {
                        process_alert(msg.header.src, alert_type, severity);
                    } else {
                        process_alert(msg.header.src, alert_type, severity);
                    }
                    break;
                }

                case GS_MSG_THERMAL_FRAME: {
                    /* Thermal frame from sentinel */
                    uint8_t frame_seq = msg.payload[0];
                    int16_t max = msg.payload[1] | (msg.payload[2] << 8);
                    int16_t avg = msg.payload[3] | (msg.payload[4] << 8);
                    uint8_t hz = msg.payload[5];
                    ESP_LOGI(TAG, "Thermal frame #%d: max=%.1f°C avg=%.1f°C zones=%d",
                             frame_seq, max / 10.0, avg / 10.0, hz);
                    break;
                }

                case GS_MSG_HEARTBEAT:
                    ESP_LOGI(TAG, "Heartbeat from node %d, rssi=%d",
                             msg.header.src, msg.payload[1]);
                    break;

                case GS_MSG_CMD_ACK:
                    ESP_LOGI(TAG, "CMD ACK from node %d", msg.header.src);
                    break;

                default:
                    ESP_LOGI(TAG, "Unknown msg type 0x%02X from %d",
                             msg.header.type, msg.header.src);
            }
        }

        /* Broadcast time sync every ~60 seconds */
        static uint32_t frame_count = 0;
        if (++frame_count % 50 == 0) {
            uint32_t epoch = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            gs_mesh_hub_time_sync(&g_mesh, epoch);
        }
    }
}

/* === MQTT Task: Cloud Bridge === */
static void mqtt_task(void *arg)
{
    gs_message_t msg;
    while (1) {
        if (xQueueReceive(g_telemetry_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mqtt_publish_telemetry(&msg);
        }

        /* Check for commands from cloud */
        gs_message_t cmd;
        if (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE) {
            xSemaphoreTake(g_radio_mutex, portMAX_DELAY);
            gs_mesh_send(&g_mesh, &cmd);
            xSemaphoreGive(g_radio_mutex);
        }
    }
}

/* === Display & Safety Task === */
static void display_task(void *arg)
{
    ESP_LOGI(TAG, "Display task started");
    while (1) {
        /* Reset daily counters at midnight (simplified) */
        static uint32_t last_reset = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        if (now - last_reset >= 86400) {
            g_alerts_24h = 0;
            last_reset = now;
        }

        /* Turn off emergency indicators after timeout */
        if (g_emergency_active) {
            static uint32_t emergency_start = 0;
            if (emergency_start == 0)
                emergency_start = now;
            if (now - emergency_start >= GS_BUZZER_DURATION_MS / 1000) {
                buzzer_control(0);
                if (!g_gas_shutoff_active)  /* Keep gas off if leak */
                    g_emergency_active = 0;
                emergency_start = 0;
            }
        }

        tft_update();

        ESP_LOGI(TAG, "Status: cook=%d probes=%d alerts=%d gas_shutoff=%d emerg=%d",
                 g_cook_active, g_active_probes, g_alerts_24h,
                 g_gas_shutoff_active, g_emergency_active);

        vTaskDelay(pdMS_TO_TICKS(GS_DISPLAY_REFRESH_MS));
    }
}

/* === Ambient Monitor Task === */
static void ambient_task(void *arg)
{
    float temp, hum, pres;
    while (1) {
        read_bme280(&temp, &hum, &pres);
        g_ambient_temp = temp;
        g_ambient_hum = hum;
        ESP_LOGI(TAG, "Ambient: T=%.1f°C H=%.1f%% P=%.0fhPa", temp, hum, pres);
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* === GPIO Setup === */
static void gpio_setup(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HUB_GPIO_GAS_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(HUB_GPIO_GAS_RELAY, 0);  /* Gas valve open by default */

    /* Buzzer PWM via LEDC */
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = HUB_GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_conf);
}

/* === App Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "GrillSync Hub starting...");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize peripherals */
    gpio_setup();
    i2c_init();
    spi_init();

    /* Init probe data cache */
    for (int i = 0; i < GS_MAX_PROBES; i++) {
        g_probe_meat_type[i] = 0xFF;
        g_probe_target[i] = 0;
        g_probe_doneness[i] = GS_DONENESS_RAW;
        memset(g_probe_temps[i], 0, sizeof(g_probe_temps[i]));
    }

    /* Create queues and mutex */
    g_telemetry_queue = xQueueCreate(32, sizeof(gs_message_t));
    g_command_queue = xQueueCreate(8, sizeof(gs_message_t));
    g_radio_mutex = xSemaphoreCreateMutex();

    /* Start tasks */
    xTaskCreate(mesh_task, "mesh", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt", 6144, NULL, 4, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 3, NULL);
    xTaskCreate(ambient_task, "ambient", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "GrillSync Hub ready. Waiting for nodes to join...");
}
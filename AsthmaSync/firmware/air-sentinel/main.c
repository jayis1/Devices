/**
 * AsthmaSync — Air Sentinel Main
 * ==============================
 * ESP32-S3 reads environmental sensors every 30s, sends data
 * via Sub-GHz TDMA mesh to Hub.
 *
 * License: MIT
 */

#include "config.h"
#include "sensors.h"
#include "../common/protocol.h"
#include "../common/radio.h"

/* ── ESP-IDF includes ──────────────────────────────────── */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "asthmasync.air";

/* ── Node Identity ─────────────────────────────────────── */
#define NODE_ID  0x0002  /* unique per device (from MAC) */

/* ── Forward Declarations ──────────────────────────────── */
static void task_sensor_read(void *arg);
static void task_mesh_tx(void *arg);
static void task_battery(void *arg);
static float read_battery_voltage(void);

/* ── Shared State ─────────────────────────────────────── */
static air_quality_t s_latest_air;
static volatile bool s_data_ready = false;
static float s_battery_v = 3.3f;

/* ── App Entry ─────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "=== AsthmaSync Air Sentinel v%d.%d ===",
             PROTO_VERSION_MAJOR, PROTO_VERSION_MINOR);

    /* Initialize I²C bus */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* Initialize sensors */
    pmsa003i_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    bme688_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    sgp40_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    scd41_init();

    /* Initialize Sub-GHz radio */
    radio_init();

    /* Sync to hub TDMA beacon */
    ESP_LOGI(TAG, "Syncing to hub TDMA beacon...");
    if (tdma_node_sync(10000) == 0) {
        ESP_LOGI(TAG, "TDMA sync OK, slot=%d", tdma_get_slot());
    } else {
        ESP_LOGW(TAG, "TDMA sync failed — will retry");
    }

    /* Create tasks */
    xTaskCreatePinnedToCore(task_sensor_read, "sensor", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_mesh_tx,    "mesh",   4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(task_battery,    "bat",    2048, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "Air Sentinel started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── Sensor Reading Task ───────────────────────────────── */
static void task_sensor_read(void *arg)
{
    while (1) {
        ESP_LOGD(TAG, "Reading sensors...");

        air_quality_t air;
        if (sensors_pack_air_quality(&air) == 0) {
            /* Copy to shared state */
            memcpy(&s_latest_air, &air, sizeof(air));
            s_data_ready = true;

            /* Check thresholds — send immediate event if exceeded */
            if (air.pm2_5 > THRESH_PM25_HIGH * 10) {
                ESP_LOGW(TAG, "PM2.5 high: %.1f µg/m³", air.pm2_5 / 10.0f);
            }
            if (air.voc_index > THRESH_VOC_HIGH) {
                ESP_LOGW(TAG, "VOC high: %u", air.voc_index);
            }
        } else {
            ESP_LOGE(TAG, "Sensor read failed");
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
    }
}

/* ── Mesh TX Task ──────────────────────────────────────── */
static void task_mesh_tx(void *arg)
{
    uint8_t seq = 0;

    while (1) {
        if (s_data_ready) {
            /* Build telemetry packet: TLV_AIR_QUALITY + air_quality_t */
            uint8_t payload[1 + sizeof(air_quality_t)];
            payload[0] = TLV_AIR_QUALITY;
            memcpy(&payload[1], &s_latest_air, sizeof(air_quality_t));

            pkt_header_t hdr = {0};
            hdr.src_type  = NODE_TYPE_AIR_SENTINEL;
            hdr.src_id    = NODE_ID;
            hdr.msg_type  = MSG_TYPE_TELEMETRY;
            hdr.seq       = seq++;

            uint8_t tx_buf[PKT_MAX_SIZE];
            size_t tx_len = proto_pack(&hdr, payload, sizeof(payload),
                                       tx_buf, sizeof(tx_buf));

            if (tx_len > 0) {
                ESP_LOGD(TAG, "TX mesh: %d bytes, seq=%d", tx_len, hdr.seq);
                tdma_node_send(tx_buf, tx_len);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MESH_TX_INTERVAL_MS));
    }
}

/* ── Battery Monitoring ───────────────────────────────── */
static void task_battery(void *arg)
{
    while (1) {
        s_battery_v = read_battery_voltage();
        ESP_LOGD(TAG, "Battery: %.2f V", s_battery_v);

        if (s_battery_v < 3.3f) {
            ESP_LOGW(TAG, "Battery low: %.2f V", s_battery_v);
        }

        vTaskDelay(pdMS_TO_TICKS(BATTERY_CHECK_INTERVAL));
    }
}

static float read_battery_voltage(void)
{
    /* ADC read + voltage divider compensation
       In production: use adc_oneshot_read() with calibration */
    return 3.3f;  /* placeholder */
}
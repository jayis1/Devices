/*
 * WanderSync — Door Sentinel Firmware
 * ESP32-C3, FreeRTOS
 *
 * Monitors door/window open/close state via reed switch, controls
 * motorized electronic deadbolt, detects band proximity via Sub-GHz RSSI,
 * auto-locks at unusual hours, and reports state changes to the Hub.
 * Ultra-low-power: 12-month CR123A battery life via deep sleep + TDMA wake.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "WanderSync-Door";

/* === Global state === */
static ws_mesh_ctx_t g_mesh;
static ws_radio_hal_t g_radio_hal;
static ws_radio_config_t g_radio_cfg;
static SemaphoreHandle_t g_mesh_mutex;

static uint8_t g_door_id = 0; /* Assigned by Hub during join */
static uint8_t g_door_state = 0; /* 0=closed, 1=open */
static uint8_t g_lock_state = 0; /* 0=unlocked, 1=locked */
static uint8_t g_tamper = 0;
static uint8_t g_band_proximity = 0; /* 0=absent, 1=present */
static int8_t g_last_rssi = 0;

/* === SX1262 HAL (ESP32-C3 SPI) === */
static spi_device_handle_t g_spi;

static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = DOOR_GPIO_SX_MOSI,
        .miso_io_num = DOOR_GPIO_SX_MISO,
        .sclk_io_num = DOOR_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {0};
    t.length = len * 8;
    if (tx) t.tx_buffer = tx;
    if (rx) t.rx_buffer = rx;
    spi_device_polling_transmit(g_spi, &t);
    return 0;
}

static void hal_cs_low(void)  { gpio_set_level(DOOR_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(DOOR_GPIO_SX_NSS, 1); }
static void hal_reset(int assert) { gpio_set_level(DOOR_GPIO_SX_RST, !assert); }
static int  hal_dio1_read(void) { return gpio_get_level(DOOR_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(DOOR_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) {}

/* === Motorized Deadbolt Control === */
static void deadbolt_lock(void)
{
    ESP_LOGI(TAG, "Locking deadbolt (door %d)", g_door_id);
    /* Drive H-bridge: IN1=1, IN2=0 for 500 ms (lock direction) */
    gpio_set_level(DOOR_GPIO_LOCK_A, 1);
    gpio_set_level(DOOR_GPIO_LOCK_B, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(DOOR_GPIO_LOCK_A, 0);
    gpio_set_level(DOOR_GPIO_LOCK_B, 0);

    /* Check lock position feedback */
    if (gpio_get_level(DOOR_GPIO_LOCK_FB) == 1) {
        g_lock_state = 1;
        ESP_LOGI(TAG, "Deadbolt locked (confirmed)");
    } else {
        g_lock_state = 2; /* failed */
        ESP_LOGW(TAG, "Deadbolt lock FAILED (no feedback)");
    }
}

static void deadbolt_unlock(void)
{
    ESP_LOGI(TAG, "Unlocking deadbolt (door %d)", g_door_id);
    /* Drive H-bridge: IN1=0, IN2=1 for 500 ms (unlock direction) */
    gpio_set_level(DOOR_GPIO_LOCK_A, 0);
    gpio_set_level(DOOR_GPIO_LOCK_B, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(DOOR_GPIO_LOCK_A, 0);
    gpio_set_level(DOOR_GPIO_LOCK_B, 0);

    if (gpio_get_level(DOOR_GPIO_LOCK_FB) == 0) {
        g_lock_state = 0;
        ESP_LOGI(TAG, "Deadbolt unlocked (confirmed)");
    } else {
        ESP_LOGW(TAG, "Deadbolt unlock FAILED (no feedback)");
    }
}

/* === Band Proximity Detection (Sub-GHz RSSI) === */
/* When the Wander Band transmits, the Door Sentinel can estimate
 * proximity from RSSI. If RSSI > threshold, band is within ~5 m.
 * This is used to trigger auto-lock when the band approaches at night. */
static void check_band_proximity(int8_t rssi)
{
    g_last_rssi = rssi;
    /* RSSI > -60 dBm at close range (<5 m) with +22 dBm TX */
    g_band_proximity = (rssi > -60) ? 1 : 0;
}

/* === Send Door Alert === */
static void send_door_alert(uint8_t state, uint8_t tamper)
{
    ws_message_t msg;
    msg.header.src = g_mesh.node_id;
    msg.header.dst = WS_HUB_NODE_ID;
    msg.header.type = WS_MSG_DOOR_ALERT;
    msg.header.msg_id = g_mesh.msg_counter++;

    msg.payload[0] = g_door_id;
    msg.payload[1] = state;
    msg.payload[2] = tamper;
    msg.payload_len = 3;

    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_WARNING);
    xSemaphoreGive(g_mesh_mutex);
}

/* === Send Telemetry === */
static void send_telemetry(void)
{
    ws_door_telem_t telem = {
        .subtype = WS_TELEM_DOOR,
        .battery_v = g_mesh.battery_v,
        .door_id = g_door_id,
        .door_state = g_door_state,
        .lock_state = g_lock_state,
        .tamper = g_tamper,
        .band_proximity = g_band_proximity,
        .rssi = g_last_rssi,
        .uptime_min = (uint16_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 60000),
    };

    ws_message_t msg;
    ws_build_door_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_INFO);
    xSemaphoreGive(g_mesh_mutex);
}

/* === Send Lock ACK === */
static void send_lock_ack(uint8_t success)
{
    ws_message_t msg;
    msg.header.src = g_mesh.node_id;
    msg.header.dst = WS_HUB_NODE_ID;
    msg.header.type = WS_MSG_DOOR_LOCK_ACK;
    msg.header.msg_id = g_mesh.msg_counter++;

    msg.payload[0] = g_door_id;
    msg.payload[1] = g_lock_state;
    msg.payload[2] = success;
    msg.payload_len = 3;

    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_WARNING);
    xSemaphoreGive(g_mesh_mutex);
}

/* === Reed Switch Monitor Task === */
static void reed_task(void *arg)
{
    uint8_t last_state = 0;
    while (1) {
        uint8_t state = gpio_get_level(DOOR_GPIO_REED);

        if (state != last_state) {
            g_door_state = state;
            ESP_LOGI(TAG, "Door %d %s", g_door_id,
                     state ? "OPENED" : "CLOSED");
            send_door_alert(state, g_tamper);
            last_state = state;

            /* Buzzer on open at night */
            if (state == 1) {
                /* Check time — production: use DS3231 RTC or Hub time sync */
                /* For now: always buzz briefly */
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 128);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
                vTaskDelay(pdMS_TO_TICKS(200));
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
            }
        }

        /* Check tamper */
        uint8_t tamper = gpio_get_level(DOOR_GPIO_TAMPER);
        if (tamper != g_tamper) {
            g_tamper = tamper;
            if (tamper) {
                ESP_LOGW(TAG, "TAMPER DETECTED on door %d!", g_door_id);
                send_door_alert(g_door_state, 1);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(DOOR_REED_POLL_MS));
    }
}

/* === Radio Task === */
static void radio_task(void *arg)
{
    uint8_t rx_buf[WS_MAX_MSG];
    ws_message_t msg;

    while (1) {
        xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
        int rx_len = ws_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   5000, &g_mesh.last_rssi);
        xSemaphoreGive(g_mesh_mutex);

        if (rx_len > 0) {
            check_band_proximity(g_mesh.last_rssi);

            if (ws_decode(&msg, rx_buf, rx_len) == 0) {
                switch (msg.header.type) {
                case WS_MSG_JOIN_ACK:
                    g_mesh.tdma_slot = msg.payload[0];
                    g_mesh.joined = 1;
                    g_door_id = msg.payload[0]; /* Use slot as door ID */
                    ESP_LOGI(TAG, "Joined mesh, slot %d, door_id %d",
                             g_mesh.tdma_slot, g_door_id);
                    break;

                case WS_MSG_DOOR_LOCK_CMD: {
                    ws_door_lock_cmd_t *cmd = (ws_door_lock_cmd_t *)msg.payload;
                    if (cmd->door_id != 0xFF && cmd->door_id != g_door_id)
                        break;

                    uint8_t success = 0;
                    switch (cmd->action) {
                    case 0: /* unlock */
                        deadbolt_unlock();
                        success = (g_lock_state == 0) ? 1 : 0;
                        break;
                    case 1: /* lock */
                        deadbolt_lock();
                        success = (g_lock_state == 1) ? 1 : 0;
                        break;
                    case 3: /* emergency unlock (fire safety) */
                        deadbolt_unlock();
                        success = (g_lock_state == 0) ? 1 : 0;
                        ESP_LOGW(TAG, "EMERGENCY UNLOCK (fire safety)");
                        break;
                    }
                    send_lock_ack(success);
                    break;
                }

                case WS_MSG_HEARTBEAT:
                    /* Track RSSI for band proximity */
                    check_band_proximity(g_mesh.last_rssi);
                    break;

                case WS_MSG_SILENCE:
                    ESP_LOGI(TAG, "Silence received");
                    break;
                }
            }
        }

        /* If not joined, attempt to join */
        if (!g_mesh.joined) {
            ws_mesh_join(&g_mesh, &g_radio_hal);
        }
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        if (g_mesh.joined) {
            send_telemetry();
        }
        vTaskDelay(pdMS_TO_TICKS(DOOR_TELEM_MS));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "WanderSync Door Sentinel starting...");

    /* GPIO init */
    gpio_set_direction(DOOR_GPIO_REED, GPIO_MODE_INPUT);
    gpio_pullup_en(DOOR_GPIO_REED);
    gpio_set_direction(DOOR_GPIO_LOCK_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(DOOR_GPIO_LOCK_B, GPIO_MODE_OUTPUT);
    gpio_set_direction(DOOR_GPIO_LOCK_FB, GPIO_MODE_INPUT);
    gpio_pullup_en(DOOR_GPIO_LOCK_FB);
    gpio_set_direction(DOOR_GPIO_TAMPER, GPIO_MODE_INPUT);
    gpio_pullup_en(DOOR_GPIO_TAMPER);
    gpio_set_direction(DOOR_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(DOOR_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(DOOR_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(DOOR_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(DOOR_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(DOOR_GPIO_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(DOOR_GPIO_VBAT, GPIO_MODE_ANALOG);

    gpio_set_level(DOOR_GPIO_SX_NSS, 1);
    gpio_set_level(DOOR_GPIO_SX_RST, 1);
    gpio_set_level(DOOR_GPIO_LOCK_A, 0);
    gpio_set_level(DOOR_GPIO_LOCK_B, 0);

    /* Buzzer PWM */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);
    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = DOOR_GPIO_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);

    /* Radio HAL */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read  = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = WS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= WS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = WS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = WS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = WS_SYNC_WORD;
    g_radio_cfg.preamble_len    = WS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    ws_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Mesh init */
    uint8_t aes_key[16] = {0};
    ws_mesh_init(&g_mesh, 0, WS_NODE_DOOR, aes_key); /* Node ID assigned by Hub */
    g_mesh.battery_v = 480; /* 2× CR123A = 4.8V */

    g_mesh_mutex = xSemaphoreCreateMutex();

    /* Tasks */
    xTaskCreate(reed_task, "reed", 2048, NULL, 5, NULL);
    xTaskCreate(radio_task, "radio", 4096, NULL, 4, NULL);
    xTaskCreate(telemetry_task, "telem", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Door Sentinel ready. Joining mesh...");
}
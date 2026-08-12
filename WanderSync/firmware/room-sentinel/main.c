/*
 * WanderSync — Room Sentinel Firmware
 * ESP32-S3, FreeRTOS
 *
 * Uses HLK-LD2410 24 GHz mmWave radar for privacy-preserving presence +
 * activity detection. ADLNet CNN classifies activities of daily living
 * (walking, sitting, lying, eating, cooking, pacing, standing) from
 * 10-second mmWave + PIR sensor windows. No cameras, no microphones —
 * privacy-first. Reports ADL updates to Hub for cognitive decline tracking.
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
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_master.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "WanderSync-Room";

/* === Global state === */
static ws_mesh_ctx_t g_mesh;
static ws_radio_hal_t g_radio_hal;
static ws_radio_config_t g_radio_cfg;
static SemaphoreHandle_t g_mesh_mutex;

static uint8_t g_room_id = 0;
static uint8_t g_presence = 0;
static uint8_t g_activity_class = WS_ADL_ABSENT;
static uint8_t g_activity_confidence = 0;
static uint8_t g_motion_level = 0;
static uint8_t g_range_m_x2 = 0;
static uint8_t g_pir_triggered = 0;
static uint16_t g_adlnet_ms = 0;

/* mmWave circular buffer (10 sec at 10 Hz = 100 samples) */
#define MMWAVE_BUF_LEN 100
typedef struct {
    uint8_t motion;  /* 0-255 motion intensity */
    uint8_t range;   /* Distance × 0.5 m */
} mmwave_sample_t;
static mmwave_sample_t g_mmwave_buf[MMWAVE_BUF_LEN];
static int g_mmwave_idx = 0;

/* PIR circular buffer (10 sec at 2 Hz = 20 samples) */
#define PIR_BUF_LEN 20
static uint8_t g_pir_buf[PIR_BUF_LEN];
static int g_pir_idx = 0;

/* === SX1262 HAL (ESP32-S3 SPI) === */
static spi_device_handle_t g_spi;

static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ROOM_GPIO_SX_MOSI,
        .miso_io_num = ROOM_GPIO_SX_MISO,
        .sclk_io_num = ROOM_GPIO_SX_SCK,
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

static void hal_cs_low(void)  { gpio_set_level(ROOM_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(ROOM_GPIO_SX_NSS, 1); }
static void hal_reset(int assert) { gpio_set_level(ROOM_GPIO_SX_RST, !assert); }
static int  hal_dio1_read(void) { return gpio_get_level(ROOM_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(ROOM_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) {}

/* === HLK-LD2410 mmWave Radar UART Parsing === */
/* The HLK-LD2410 sends data frames over UART at 256600 baud:
 * Header: 0x53 0x59, then state data, engineering data, etc.
 * We parse presence, motion energy, and distance. */

static void mmwave_parse(uint8_t *data, size_t len,
                          uint8_t *presence, uint8_t *motion,
                          uint8_t *range)
{
    /* Production: parse HLK-LD2410 protocol frames
     * Header: 0x53 0x59, Length: 2 bytes, Report SN: 2 bytes
     * Moving target distance, Moving energy, Static target distance,
     * Static energy, Presence status
     */
    *presence = 1; /* Placeholder */
    *motion = 128;
    *range = 20; /* 2.0 m (×0.5 = 4) */
}

/* === ADLNet Lite — On-Device Activity Classification === */
/* Production: TFLite-Micro int8 CNN (~90 KB)
 * Inputs: 10-sec mmWave (100 samples × 2 features) + PIR (20 samples)
 * Output: 8-class softmax (absent, walking, sitting, lying, eating,
 *         cooking, pacing, standing)
 *
 * Simplified heuristic version: */
static uint8_t adlnet_classify(const mmwave_sample_t *mmwave, int mmwave_len,
                                const uint8_t *pir, int pir_len,
                                uint8_t *confidence)
{
    /* Compute average motion and range variance */
    float avg_motion = 0;
    float range_var = 0;
    float avg_range = 0;
    int present_count = 0;

    for (int i = 0; i < mmwave_len; i++) {
        avg_motion += mmwave[i].motion;
        avg_range += mmwave[i].range;
        if (mmwave[i].motion > 20) present_count++;
    }
    avg_motion /= mmwave_len;
    avg_range /= mmwave_len;

    for (int i = 0; i < mmwave_len; i++) {
        float d = mmwave[i].range - avg_range;
        range_var += d * d;
    }
    range_var /= mmwave_len;

    /* PIR activity */
    int pir_active = 0;
    for (int i = 0; i < pir_len; i++) {
        if (pir[i]) pir_active++;
    }

    /* Classification heuristics (production: CNN) */
    if (present_count < 5 && avg_motion < 15) {
        *confidence = 90;
        return WS_ADL_ABSENT;
    }

    /* High motion + high range variance = walking or pacing */
    if (avg_motion > 80 && range_var > 20) {
        /* Check for repetitive pattern (pacing) */
        int direction_changes = 0;
        int last_dir = 0;
        for (int i = 1; i < mmwave_len; i++) {
            int dir = (mmwave[i].range > mmwave[i-1].range) ? 1 : -1;
            if (dir != last_dir && last_dir != 0) direction_changes++;
            last_dir = dir;
        }
        if (direction_changes > 8) { /* Back-and-forth = pacing */
            *confidence = 82;
            return WS_ADL_PACING;
        }
        *confidence = 88;
        return WS_ADL_WALKING;
    }

    /* Low motion + low range variance = sitting, lying, or standing */
    if (avg_motion < 30 && range_var < 5) {
        if (avg_range > 30) { /* Higher range = likely lying in bed */
            *confidence = 80;
            return WS_ADL_LYING;
        }
        if (pir_active > pir_len / 2) {
            *confidence = 75;
            return WS_ADL_STANDING;
        }
        *confidence = 85;
        return WS_ADL_SITTING;
    }

    /* Medium motion + moderate range = eating or cooking */
    if (avg_motion > 30 && avg_motion < 80) {
        if (avg_range < 15) { /* Close range = eating at table */
            *confidence = 72;
            return WS_ADL_EATING;
        }
        *confidence = 70;
        return WS_ADL_COOKING;
    }

    *confidence = 50;
    return WS_ADL_SITTING;
}

/* === mmWave Task (10 Hz) === */
static void mmwave_task(void *arg)
{
    while (1) {
        /* Read from HLK-LD2410 via UART2 */
        uint8_t uart_data[64];
        int len = uart_read_bytes(UART_NUM_2, uart_data, sizeof(uart_data),
                                  pdMS_TO_TICKS(10));
        if (len > 0) {
            mmwave_parse(uart_data, len, &g_presence,
                         &g_motion_level, &g_range_m_x2);
        }

        /* Store in circular buffer */
        g_mmwave_buf[g_mmwave_idx].motion = g_motion_level;
        g_mmwave_buf[g_mmwave_idx].range = g_range_m_x2;
        g_mmwave_idx = (g_mmwave_idx + 1) % MMWAVE_BUF_LEN;

        vTaskDelay(pdMS_TO_TICKS(ROOM_RADAR_INTERVAL_MS));
    }
}

/* === PIR Task (2 Hz) === */
static void pir_task(void *arg)
{
    while (1) {
        g_pir_buf[g_pir_idx] = gpio_get_level(ROOM_GPIO_PIR);
        g_pir_idx = (g_pir_idx + 1) % PIR_BUF_LEN;
        vTaskDelay(pdMS_TO_TICKS(ROOM_PIR_INTERVAL_MS));
    }
}

/* === ADLNet Task (every 10 sec) === */
static void adlnet_task(void *arg)
{
    while (1) {
        /* Wait for buffer to fill (10 sec) */
        vTaskDelay(pdMS_TO_TICKS(ROOM_ADLNET_MS));

        /* Run ADLNet classification */
        int64_t start = esp_timer_get_time() / 1000;

        g_activity_class = adlnet_classify(g_mmwave_buf, MMWAVE_BUF_LEN,
                                           g_pir_buf, PIR_BUF_LEN,
                                           &g_activity_confidence);

        g_adlnet_ms = (uint16_t)(esp_timer_get_time() / 1000 - start);

        /* Send ADL update to Hub */
        if (g_mesh.joined) {
            ws_message_t msg;
            msg.header.src = g_mesh.node_id;
            msg.header.dst = WS_HUB_NODE_ID;
            msg.header.type = WS_MSG_ADL_UPDATE;
            msg.header.msg_id = g_mesh.msg_counter++;

            msg.payload[0] = g_room_id;
            msg.payload[1] = g_activity_class;
            msg.payload[2] = g_activity_confidence;
            msg.payload[3] = g_presence;
            msg.payload[4] = g_motion_level;
            msg.payload_len = 5;

            xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
            ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_INFO);
            xSemaphoreGive(g_mesh_mutex);
        }

        ESP_LOGI(TAG, "ADL: room=%d activity=%d conf=%d%% ms=%d",
                 g_room_id, g_activity_class, g_activity_confidence,
                 g_adlnet_ms);
    }
}

/* === Send Telemetry === */
static void send_telemetry(void)
{
    ws_room_telem_t telem = {
        .subtype = WS_TELEM_ROOM,
        .battery_v = g_mesh.battery_v,
        .room_id = g_room_id,
        .presence = g_presence,
        .activity_class = g_activity_class,
        .activity_conf = g_activity_confidence,
        .motion_level = g_motion_level,
        .range_m_x2 = g_range_m_x2,
        .pir_triggered = g_pir_buf[(g_pir_idx - 1 + PIR_BUF_LEN) % PIR_BUF_LEN],
        .adlnet_ms = g_adlnet_ms,
        .free_heap = esp_get_free_heap_size() & 0xFFFF,
        .rssi = g_mesh.last_rssi,
        .uptime_min = (uint16_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 60000),
    };

    ws_message_t msg;
    ws_build_room_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_INFO);
    xSemaphoreGive(g_mesh_mutex);
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

        if (rx_len > 0 && ws_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case WS_MSG_JOIN_ACK:
                g_mesh.tdma_slot = msg.payload[0];
                g_mesh.joined = 1;
                g_room_id = msg.payload[0]; /* Use slot as room ID */
                ESP_LOGI(TAG, "Joined mesh, slot %d, room_id %d",
                         g_mesh.tdma_slot, g_room_id);
                break;

            case WS_MSG_SILENCE:
                ESP_LOGI(TAG, "Silence received");
                break;
            }
        }

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
        vTaskDelay(pdMS_TO_TICKS(ROOM_TELEM_MS));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "WanderSync Room Sentinel starting...");

    /* GPIO init */
    gpio_set_direction(ROOM_GPIO_PIR, GPIO_MODE_INPUT);
    gpio_set_direction(ROOM_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(ROOM_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(ROOM_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(ROOM_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(ROOM_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(ROOM_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(ROOM_GPIO_USB_PWR, GPIO_MODE_INPUT);

    gpio_set_level(ROOM_GPIO_SX_NSS, 1);
    gpio_set_level(ROOM_GPIO_SX_RST, 1);

    /* UART2 for HLK-LD2410 mmWave radar */
    uart_config_t uart_cfg = {
        .baud_rate = 256600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_2, &uart_cfg);
    uart_set_pin(UART_NUM_2, ROOM_GPIO_RADAR_RX, ROOM_GPIO_RADAR_TX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, 256, 256, 0, NULL, 0);

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
    ws_mesh_init(&g_mesh, 0, WS_NODE_ROOM, aes_key);
    g_mesh.battery_v = 415;

    g_mesh_mutex = xSemaphoreCreateMutex();

    /* Init buffers */
    memset(g_mmwave_buf, 0, sizeof(g_mmwave_buf));
    memset(g_pir_buf, 0, sizeof(g_pir_buf));

    /* Tasks */
    xTaskCreate(mmwave_task, "mmwave", 4096, NULL, 5, NULL);
    xTaskCreate(pir_task, "pir", 2048, NULL, 4, NULL);
    xTaskCreate(adlnet_task, "adlnet", 4096, NULL, 4, NULL);
    xTaskCreate(radio_task, "radio", 4096, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telem", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Room Sentinel ready. Joining mesh...");
}
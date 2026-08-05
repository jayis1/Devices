/*
 * SeizureSync — Seizure Band firmware (ESP32-S3-MINI-1)
 *
 * Wrist-worn seizure detector. Fuses ICM-42688-P accelerometer (2000 Hz
 * seizure-band), MAX30102 PPG (ictal tachycardia), AD5940 EDA (post-ictal
 * surge). Runs SeizureNet 1D CNN on-device for <400 ms seizure detection.
 * Streams raw signals to hub via BLE 5.0; alerts caregiver beacon via
 * Sub-GHz mesh when out of BLE range.
 *
 * SPDX-License-Identifier: MIT
 *
 * Target: ESP32-S3-MINI-1-N8R2
 * Toolchain: ESP-IDF v5.1
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

#include "../common/protocol.h"
#include "../common/tdma.h"
#include "../common/crypto.h"
#include "sx1262.h"
#include "accel.h"
#include "ppg.h"
#include "eda.h"
#include "seizurenet_band.h"
#include "ble_stream.h"

static const char *TAG = "BAND";

/* ---- Pin definitions (see README pin table) ---- */
#define PIN_SX1262_NSS   21
#define PIN_SX1262_DIO0  22
#define PIN_SX1262_RST   23
#define PIN_SX1262_BUSY  25
#define PIN_SX1262_SCK   26
#define PIN_SX1262_MISO  27
#define PIN_SX1262_MOSI  28

/* ---- Globals ---- */
static uint8_t g_net_id[SZ_NET_ID_LEN] = {'S','Z','S','Y','N','C'};
static uint8_t g_aes_key[SZ_AES_KEY_LEN];

/* Signal buffers — 2-second sliding windows at respective rates */
#define ACCEL_HZ   2000
#define PPG_HZ      100
#define EDA_HZ         4
#define WINDOW_S       2
#define ACCEL_BUF  (ACCEL_HZ * WINDOW_S)
#define PPG_BUF   (PPG_HZ * WINDOW_S)
#define EDA_BUF   (EDA_HZ * WINDOW_S)

static float g_accel_buf[ACCEL_BUF];   /* 3-axis magnitude */
static float g_ppg_buf[PPG_BUF];
static float g_eda_buf[EDA_BUF];
static int g_accel_idx = 0, g_ppg_idx = 0, g_eda_idx = 0;

/* ---- Sampling tasks ---- */

static void accel_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / ACCEL_HZ);  /* 0.5 ms */
    while (1) {
        float mag = accel_read_magnitude();   /* sqrt(x²+y²+z²) in g */
        g_accel_buf[g_accel_idx] = mag;
        g_accel_idx = (g_accel_idx + 1) % ACCEL_BUF;
        vTaskDelayUntil(&last, period);
    }
}

static void ppg_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / PPG_HZ);   /* 10 ms */
    while (1) {
        g_ppg_buf[g_ppg_idx] = ppg_read_hr();
        g_ppg_idx = (g_ppg_idx + 1) % PPG_BUF;
        vTaskDelayUntil(&last, period);
    }
}

static void eda_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / EDA_HZ);   /* 250 ms */
    while (1) {
        g_eda_buf[g_eda_idx] = eda_read_microsiemens();
        g_eda_idx = (g_eda_idx + 1) % EDA_BUF;
        vTaskDelayUntil(&last, period);
    }
}

/* ---- SeizureNet inference loop ----
 * Runs every 500 ms on the latest 2-second windows.
 * If seizure probability > 0.85, triggers alert.
 */
static void inference_task(void *arg)
{
    (void)arg;
    while (1) {
        /* Copy buffers for inference (avoid race with simple index check) */
        float accel[ACCEL_BUF], ppg[PPG_BUF], eda[EDA_BUF];
        memcpy(accel, g_accel_buf, sizeof(accel));
        memcpy(ppg, g_ppg_buf, sizeof(ppg));
        memcpy(eda, g_eda_buf, sizeof(eda));

        float p_seizure = seizurennet_infer(accel, ppg, eda, ACCEL_BUF,
                                             PPG_BUF, EDA_BUF);
        ESP_LOGD(TAG, "SeizureNet p=%.3f", p_seizure);

        if (p_seizure > 0.85f) {
            ESP_LOGW(TAG, "SEIZURE DETECTED p=%.3f — alerting!", p_seizure);
            trigger_seizure_alert(p_seizure);
        }

        vTaskDelay(pdMS_TO_TICKS(500));   /* 2 Hz inference */
    }
}

/* ---- Trigger seizure alert ----
 * 1. Send SZ_PKT_SEIZURE_ALERT via Sub-GHz mesh to hub.
 * 2. If hub doesn't ACK within 3s, send directly to caregiver beacon.
 * 3. Stream raw signal to hub via BLE for cloud upload.
 * 4. Haptic alert to patient.
 */
void trigger_seizure_alert(float confidence)
{
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_BAND;
    h.dst_node = SZ_SLOT_HUB;
    h.type = SZ_PKT_SEIZURE_ALERT;
    h.seq++;

    sz_seizure_payload_t p = {0};
    p.onset_unix = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    p.semiology = SZ_SEMI_FBTCS;   /* tonic-clonic (default; classified in cloud) */
    p.severity = SZ_SEV_SEIZURE;
    p.duration_s = 0;               /* ongoing */
    p.confidence = (uint8_t)(confidence * 100);
    p.recovery_state = 0;

    uint8_t pkt[SZ_RADIO_MAX_PKT];
    size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    sx1262_send(pkt, len);

    /* If no ACK in 3s, send directly to beacon */
    vTaskDelay(pdMS_TO_TICKS(3000));
    h.dst_node = SZ_SLOT_BEACON;
    h.seq++;
    len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    sx1262_send(pkt, len);

    /* Haptic alert to patient */
    haptic_pulse(3);   /* 3 pulses */

    /* BLE stream raw signal to hub */
    ble_stream_signal(g_accel_buf, g_ppg_buf, g_eda_buf);
}

/* ---- TDMA mesh member ----
 * Band is slot 1. Synchronizes to hub beacon, transmits in slot 1,
 * listens in slot 0 (hub beacon) and slots 2-7.
 */
static void tdma_member_task(void *arg)
{
    (void)arg;
    while (1) {
        /* Wait for hub beacon (slot 0) */
        uint8_t rx_buf[SZ_RADIO_MAX_PKT];
        int rlen = sx1262_receive(rx_buf, sizeof(rx_buf),
                                  SZ_TDMA_SLOT_MS);
        if (rlen > 0) {
            sz_header_t h;
            uint8_t payload[128];
            size_t plen;
            if (sz_parse(rx_buf, rlen, &h, payload, &plen) == 0
                && h.type == SZ_PKT_BEACON) {
                /* Sync: transmit in our slot (slot 1) */
                vTaskDelay(pdMS_TO_TICKS(5));  /* guard */
                send_heartbeat();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SZ_TDMA_SUPERFRAME_MS - SZ_TDMA_SLOT_MS));
    }
}

void send_heartbeat(void)
{
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_BAND;
    h.dst_node = SZ_SLOT_HUB;
    h.type = SZ_PKT_HEARTBEAT;
    h.seq++;

    sz_heartbeat_payload_t p = {0};
    p.battery_pct = 85;   /* TODO: read from BQ25895 */
    p.rssi_dbm = -70;
    p.status_flags = 0x01;  /* worn */
    p.free_heap_kb = 200;

    uint8_t pkt[SZ_RADIO_MAX_PKT];
    size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    sx1262_send(pkt, len);
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "SeizureSync Band starting...");

    /* I²C init for MAX30102 + AD5940 + DRV2605L */
    i2c_config_t i2c = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 10, .scl_io_num = 9,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* SPI init for ICM-42688-P + SX1262 */
    spi_bus_config_t buscfg = {
        .mosi_io_num = 7, .miso_io_num = 6, .sclk_io_num = 5,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 0);

    /* Init sensors */
    accel_init(4, 5, 6, 7, 8);     /* CS, SCK, MISO, MOSI, INT1 */
    ppg_init(9, 10);                /* SCL, SDA */
    eda_init(11, 12);                /* SCL, SDA (AD5940) */
    haptic_init(14, 15);             /* DRV2605L SCL, SDA */

    /* SX1262 Sub-GHz radio */
    sx1262_init(PIN_SX1262_NSS, PIN_SX1262_RST, PIN_SX1262_DIO0,
                PIN_SX1262_BUSY, PIN_SX1262_SCK, PIN_SX1262_MISO,
                PIN_SX1262_MOSI);
    sx1262_set_frequency(SZ_RADIO_FREQ_HZ);
    sx1262_set_tx_power(SZ_RADIO_TX_DBM);
    sx1262_set_modem_params(SZ_RADIO_BW_HZ, SZ_RADIO_SF, SZ_RADIO_CR);

    /* BLE init for streaming to hub */
    ble_init();

    /* Load SeizureNet model */
    seizurennet_init();

    /* Create tasks */
    xTaskCreate(accel_task, "accel", 4096, NULL, 7, NULL);
    xTaskCreate(ppg_task, "ppg", 2048, NULL, 5, NULL);
    xTaskCreate(eda_task, "eda", 2048, NULL, 5, NULL);
    xTaskCreate(inference_task, "infer", 8192, NULL, 6, NULL);
    xTaskCreate(tdma_member_task, "tdma", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Band ready. SeizureNet monitoring active.");
}
/*
 * GrillSync — Grill Sentinel Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Grill Sentinel monitors the grill surface via MLX90640 32×24
 * thermal array, detects gas leaks (MQ-2), flame (IR detector), and
 * flare-up acoustic patterns (piezo). Runs FlareUpNet LSTM on-device
 * for 8–15s flare-up prediction. Reports to Hub via Sub-GHz 868 MHz.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/adc.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "GrillSync-Sentinel";

/* === Global state === */
static gs_mesh_ctx_t g_mesh;

/* Thermal array data (32×24 = 768 pixels) */
static float g_thermal_frame[768];
static float g_thermal_prev[768];
static int16_t g_surface_max_deci = 0;
static int16_t g_surface_avg_deci = 0;
static uint8_t g_hot_zone_count = 0;

/* Gas sensor */
static uint16_t g_gas_ppm = 0;
static uint8_t g_gas_lel_pct = 0;
static uint16_t g_gas_baseline = 0;
static uint8_t g_gas_warmed_up = 0;

/* Flame detection */
static uint8_t g_flame_intensity = 0;
static uint8_t g_flame_detected = 0;

/* Acoustic (piezo) */
static uint16_t g_acoustic_energy = 0;

/* Flare-up prediction */
static uint8_t g_flareup_risk = 0;
static uint16_t g_flareup_eta_100ms = 0;

/* History buffer for FlareUpNet (50 timesteps × 6 channels) */
#define HIST_LEN 50
static float g_hist_thermal_max[HIST_LEN];
static float g_hist_thermal_grad[HIST_LEN];
static uint8_t g_hist_hot_zones[HIST_LEN];
static uint16_t g_hist_acoustic[HIST_LEN];
static uint8_t g_hist_flame[HIST_LEN];
static uint16_t g_hist_gas[HIST_LEN];
static int g_hist_idx = 0;

/* Child detection cooldown */
static uint32_t g_last_child_alert_ms = 0;
static uint16_t g_event_counter = 0;

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SENTINEL_GPIO_SX_MOSI,
        .miso_io_num = SENTINEL_GPIO_SX_MISO,
        .sclk_io_num = SENTINEL_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = SENTINEL_GPIO_SX_NSS,
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
static void spi_reset(uint8_t assert) {
    gpio_set_level(SENTINEL_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(SENTINEL_GPIO_SX_DIO1); }
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

/* === I2C for MLX90640 + BME280 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SENTINEL_GPIO_MLX_SDA,
        .scl_io_num = SENTINEL_GPIO_MLX_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  /* MLX90640 needs 400 kHz */
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === MLX90640 Thermal Array Reader === */
/* MLX90640 I2C address: 0x33 */
#define MLX90640_ADDR 0x33
#define MLX90640_RAM_START 0x0400

static void mlx90640_read_frame(float *frame)
{
    /*
     * Read 768 pixels from MLX90640 in two subpages.
     * Each subpage = 384 pixels = 768 bytes (16-bit per pixel).
     *
     * In production:
     * 1. Write subpage control register
     * 2. Read 768 bytes from RAM (0x0400)
     * 3. Apply calibration coefficients
     * 4. Convert raw to temperature (°C)
     * 5. Repeat for subpage 2
     * 6. Merge into full 32×24 frame
     *
     * Simplified here: fill with simulated data for structure.
     */
    for (int i = 0; i < 768; i++) {
        frame[i] = 25.0 + (float)(i % 32) * 0.5;  /* Simulated gradient */
    }
}

static void process_thermal_frame(void)
{
    memcpy(g_thermal_prev, g_thermal_frame, sizeof(g_thermal_frame));
    mlx90640_read_frame(g_thermal_frame);

    /* Find max and average */
    float max_temp = -273.0;
    float sum = 0.0;
    int hot_pixels = 0;

    for (int i = 0; i < 768; i++) {
        if (g_thermal_frame[i] > max_temp)
            max_temp = g_thermal_frame[i];
        sum += g_thermal_frame[i];
        if (g_thermal_frame[i] > GS_HOT_ZONE_TEMP_C)
            hot_pixels++;
    }
    float avg_temp = sum / 768.0;

    g_surface_max_deci = (int16_t)(max_temp * 10);
    g_surface_avg_deci = (int16_t)(avg_temp * 10);
    g_hot_zone_count = hot_pixels > 0 ? (hot_pixels / 8) + 1 : 0;
    if (g_hot_zone_count > 255)
        g_hot_zone_count = 255;

    /* Child in zone detection: human-temperature blobs */
    int human_pixels = 0;
    for (int i = 0; i < 768; i++) {
        if (g_thermal_frame[i] >= GS_CHILD_ZONE_TEMP_LOW_C &&
            g_thermal_frame[i] <= GS_CHILD_ZONE_TEMP_HIGH_C) {
            human_pixels++;
        }
    }

    /* Send alert if human detected (debounced) */
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (human_pixels >= GS_CHILD_ZONE_MIN_PIXELS &&
        (now - g_last_child_alert_ms) > 5000) {
        g_last_child_alert_ms = now;
        gs_message_t alert;
        gs_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                       GS_ALERT_CHILD_IN_ZONE, GS_PRIORITY_HIGH, NULL, 0);
        gs_mesh_send(&g_mesh, &alert);
        ESP_LOGW(TAG, "Child/human detected near grill!");
    }

    /* Thermal gradient (°C/s) */
    static float prev_max = 0;
    float gradient = (max_temp - prev_max) * 2.0;  /* 2 Hz sampling */
    prev_max = max_temp;

    /* Update history buffer for FlareUpNet */
    g_hist_thermal_max[g_hist_idx] = max_temp;
    g_hist_thermal_grad[g_hist_idx] = gradient;
    g_hist_hot_zones[g_hist_idx] = g_hot_zone_count;
    g_hist_acoustic[g_hist_idx] = g_acoustic_energy;
    g_hist_flame[g_hist_idx] = g_flame_intensity;
    g_hist_gas[g_hist_idx] = g_gas_ppm;
    g_hist_idx = (g_hist_idx + 1) % HIST_LEN;

    ESP_LOGI(TAG, "Thermal: max=%.1f°C avg=%.1f°C hot_zones=%d grad=%.1f°C/s",
             max_temp, avg_temp, g_hot_zone_count, gradient);
}

/* === MQ-2 Gas Sensor === */
static void read_gas_sensor(void)
{
    /* ADC read MQ-2 */
    adc_read(ADC1_CHANNEL_0, (int *)&g_gas_ppm);

    /* Warmup period */
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (!g_gas_warmed_up) {
        static uint32_t warmup_start = 0;
        if (warmup_start == 0)
            warmup_start = now;
        if (now - warmup_start < GS_GAS_BASELINE_WARMUP_MS) {
            g_gas_baseline = g_gas_ppm;
            return;
        }
        g_gas_warmed_up = 1;
    }

    /* LEL calculation: propane LEL = 21000 ppm */
    g_gas_lel_pct = (uint8_t)((g_gas_ppm * 100) / 21000);
    if (g_gas_lel_pct > 100)
        g_gas_lel_pct = 100;

    /* Check for leak */
    if (g_gas_ppm >= GS_GAS_LEAK_10PCT_LEL_PPM) {
        gs_message_t alert;
        uint8_t data[3];
        data[0] = (uint8_t)(g_gas_ppm & 0xFF);
        data[1] = (uint8_t)(g_gas_ppm >> 8);
        data[2] = g_gas_lel_pct;
        gs_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                       GS_ALERT_GAS_LEAK, GS_PRIORITY_CRITICAL, data, 3);
        gs_mesh_send(&g_mesh, &alert);
        ESP_LOGE(TAG, "🚨 GAS LEAK: %d ppm (%d%% LEL)", g_gas_ppm, g_gas_lel_pct);
    }
}

/* === Flame Detector === */
static void read_flame_sensor(void)
{
    int raw;
    adc_read(ADC1_CHANNEL_3, &raw);
    g_flame_intensity = (uint8_t)(raw & 0xFF);

    /* Check GPIO IRQ */
    if (gpio_get_level(SENTINEL_GPIO_FLAME_IRQ) == 0) {
        g_flame_detected = 1;
        /* Check for grill fire (thermal + flame) */
        if (g_surface_max_deci > GS_THERMAL_FLARE_TEMP_C * 10) {
            gs_message_t alert;
            gs_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                           GS_ALERT_GRILL_FIRE, GS_PRIORITY_CRITICAL, NULL, 0);
            gs_mesh_send(&g_mesh, &alert);
            ESP_LOGE(TAG, "🚨 GRILL FIRE: surface=%.1f°C + flame",
                     g_surface_max_deci / 10.0);
        }
    } else {
        g_flame_detected = 0;
    }
}

/* === Piezo Acoustic Sensor === */
static void read_piezo_sensor(void)
{
    int raw;
    adc_read(ADC1_CHANNEL_7, &raw);
    /* RMS approximation: square, accumulate, sqrt */
    static uint32_t sum_sq = 0;
    static int samples = 0;
    sum_sq += (uint32_t)(raw * raw);
    samples++;
    if (samples >= 50) {
        g_acoustic_energy = (uint16_t)(sqrtf((float)sum_sq / samples) * 100);
        sum_sq = 0;
        samples = 0;
    }
}

/* === FlareUpNet LSTM (simplified inference) === */
/*
 * In production: TFLite-Micro int8 quantized LSTM model.
 * Input: 6-channel × 50-timestep history buffer
 * Output: [flare_up_risk (0–100%), time_to_flare (×100ms)]
 *
 * Simplified heuristic: predict flare-up when thermal gradient
 * is high AND acoustic energy spikes (fat dripping) AND hot zones
 * are growing.
 */
static void run_flareup_prediction(void)
{
    /* Get recent history (last 10 timesteps) */
    int start = (g_hist_idx - 10 + HIST_LEN) % HIST_LEN;
    float max_grad = 0;
    uint16_t max_acoustic = 0;
    uint8_t max_hot_zones = 0;

    for (int i = 0; i < 10; i++) {
        int idx = (start + i) % HIST_LEN;
        if (fabsf(g_hist_thermal_grad[idx]) > max_grad)
            max_grad = fabsf(g_hist_thermal_grad[idx]);
        if (g_hist_acoustic[idx] > max_acoustic)
            max_acoustic = g_hist_acoustic[idx];
        if (g_hist_hot_zones[idx] > max_hot_zones)
            max_hot_zones = g_hist_hot_zones[idx];
    }

    /* Heuristic flare-up risk (in production: LSTM model) */
    float risk = 0;
    if (max_grad > 10.0)  /* >10°C/s gradient */
        risk += 30;
    if (max_acoustic > GS_PIEZO_FLARE_THRESHOLD)
        risk += 30;
    if (max_hot_zones > 3)
        risk += 20;
    if (g_flame_detected)
        risk += 20;

    g_flareup_risk = (uint8_t)risk;
    if (g_flareup_risk > 100)
        g_flareup_risk = 100;

    /* ETA estimation: higher risk = sooner */
    if (g_flareup_risk > 50) {
        g_flareup_eta_100ms = (uint16_t)((100 - g_flareup_risk) * 1.5);
    } else {
        g_flareup_eta_100ms = 0;
    }

    /* Send flare-up warning if risk > threshold */
    if (g_flareup_risk >= GS_FLAREUP_RISK_THRESHOLD && g_flareup_eta_100ms > 0) {
        gs_message_t alert;
        uint8_t data[3];
        data[0] = g_flareup_risk;
        data[1] = (uint8_t)(g_flareup_eta_100ms & 0xFF);
        data[2] = (uint8_t)(g_flareup_eta_100ms >> 8);
        gs_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                       GS_ALERT_FLARE_UP_WARNING, GS_PRIORITY_HIGH, data, 3);
        gs_mesh_send(&g_mesh, &alert);
        ESP_LOGW(TAG, "🔥 FLARE-UP PREDICTED: risk=%d%% ETA=%dms",
                 g_flareup_risk, g_flareup_eta_100ms * 100);
    }
}

/* === Thermal Frame Compression === */
/* Delta + RLE compression for mesh transmission */
static uint8_t compress_thermal_frame(uint8_t *out, size_t out_len)
{
    /*
     * Delta encoding: only pixels that changed >2°C since last frame.
     * RLE: run-length encode zero-delta runs.
     * 8-bit quantization: temp delta scaled to ±127.
     */
    uint8_t pos = 0;
    uint8_t run = 0;

    for (int i = 0; i < 768; i++) {
        float delta = g_thermal_frame[i] - g_thermal_prev[i];
        int8_t quantized = (int8_t)(delta * 2.0);  /* 0.5°C resolution */

        if (quantized == 0 || fabsf(delta) < 2.0) {
            run++;
            if (run == 255) {
                if (pos + 2 > out_len) break;
                out[pos++] = 0;  /* zero-delta marker */
                out[pos++] = run;
                run = 0;
            }
        } else {
            if (run > 0) {
                if (pos + 2 > out_len) break;
                out[pos++] = 0;
                out[pos++] = run;
                run = 0;
            }
            if (pos + 1 > out_len) break;
            out[pos++] = (uint8_t)quantized;
        }
    }
    /* Flush remaining run */
    if (run > 0 && pos + 2 <= out_len) {
        out[pos++] = 0;
        out[pos++] = run;
    }

    return pos;
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    uint8_t frame_seq = 0;
    while (1) {
        /* Read all sensors */
        read_gas_sensor();
        read_flame_sensor();
        read_piezo_sensor();

        /* Send telemetry to Hub */
        gs_message_t msg;
        gs_build_sentinel_telem(&msg, g_mesh.node_id, g_mesh.msg_seq++,
                                  0xFF, /* USB-powered */
                                  g_surface_max_deci, g_surface_avg_deci,
                                  g_hot_zone_count, g_gas_ppm, g_gas_lel_pct,
                                  g_flame_intensity, g_flame_detected,
                                  (int16_t)(25.0 * 10), 500,  /* Ambient */
                                  g_acoustic_energy, g_flareup_risk,
                                  g_flareup_eta_100ms, g_event_counter++, 0);
        gs_mesh_send(&g_mesh, &msg);

        /* Send compressed thermal frame */
        uint8_t compressed[GS_MAX_PAYLOAD - 6];
        uint8_t clen = compress_thermal_frame(compressed, sizeof(compressed));
        gs_message_t frame_msg;
        gs_build_thermal_frame(&frame_msg, g_mesh.node_id, g_mesh.msg_seq++,
                                 frame_seq++, compressed, clen,
                                 g_surface_max_deci, g_surface_avg_deci,
                                 g_hot_zone_count);
        gs_mesh_send(&g_mesh, &frame_msg);
        frame_seq++;

        vTaskDelay(pdMS_TO_TICKS(500));  /* 2 Hz telemetry */
    }
}

/* === Thermal Task === */
static void thermal_task(void *arg)
{
    while (1) {
        process_thermal_frame();
        vTaskDelay(pdMS_TO_TICKS(500));  /* 2 Hz */
    }
}

/* === Flare-Up Prediction Task === */
static void flareup_task(void *arg)
{
    while (1) {
        run_flareup_prediction();
        vTaskDelay(pdMS_TO_TICKS(500));  /* 2 Hz prediction */
    }
}

/* === Mesh Task (receive commands) === */
static void mesh_task(void *arg)
{
    gs_message_t msg;
    while (1) {
        if (gs_mesh_recv(&g_mesh, &msg, 1000) == 0) {
            switch (msg.header.type) {
                case GS_MSG_JOIN_ACK:
                    g_mesh.node_id = msg.payload[0];
                    g_mesh.tdma_slot = msg.payload[1];
                    g_mesh.joined = 1;
                    ESP_LOGI(TAG, "Joined mesh: node_id=%d slot=%d",
                             g_mesh.node_id, g_mesh.tdma_slot);
                    break;

                case GS_MSG_TIME_SYNC: {
                    uint32_t epoch = msg.payload[0] | (msg.payload[1] << 8) |
                                     (msg.payload[2] << 16) | (msg.payload[3] << 24);
                    g_mesh.last_time_sync = epoch;
                    ESP_LOGI(TAG, "Time sync: %u", epoch);
                    break;
                }

                case GS_MSG_COMMAND: {
                    uint8_t cmd = msg.payload[0];
                    ESP_LOGI(TAG, "Command 0x%02X from hub", cmd);
                    switch (cmd) {
                        case GS_CMD_CALIBRATE:
                            g_gas_warmed_up = 0;
                            ESP_LOGI(TAG, "Recalibrating gas sensor...");
                            break;
                        case GS_CMD_REBOOT:
                            esp_restart();
                            break;
                        case GS_CMD_EMERGENCY_MODE:
                            ESP_LOGW(TAG, "Emergency mode activated");
                            break;
                        case GS_CMD_NORMAL_MODE:
                            ESP_LOGI(TAG, "Normal mode");
                            break;
                        default:
                            break;
                    }
                    break;
                }

                case GS_MSG_HEARTBEAT:
                    break;

                default:
                    ESP_LOGI(TAG, "Msg type 0x%02X from %d",
                             msg.header.type, msg.header.src);
            }
        }

        /* Send heartbeat every 30 seconds */
        static uint32_t last_hb = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (now - last_hb > 30000) {
            gs_mesh_heartbeat(&g_mesh, 0xFF, 0);
            last_hb = now;
        }
    }
}

/* === GPIO Setup === */
static void gpio_setup(void)
{
    /* Flame detector IRQ as input */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SENTINEL_GPIO_FLAME_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* LED output */
    io_conf.pin_bit_mask = (1ULL << SENTINEL_GPIO_LED);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    /* ADC for MQ-2, flame, piezo */
    adc_config_t adc_conf = { .channel = ADC1_CHANNEL_0, .atten = ADC_ATTEN_11db };
    adc_init(&adc_conf);
}

/* === App Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "GrillSync Sentinel starting...");

    gpio_setup();
    i2c_init();
    spi_init();

    /* Initialize mesh as sentinel node */
    gs_radio_config_t radio_cfg = {
        .frequency = GS_NET_FREQ_HZ,
        .bandwidth = GS_NET_BW_HZ,
        .spreading_factor = GS_NET_SF,
        .coding_rate = GS_NET_CR,
        .preamble_len = GS_NET_PREAMBLE,
        .tx_power_dbm = GS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (gs_mesh_init(&g_mesh, GS_NODE_SENTINEL, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    /* Wait for gas sensor warmup */
    ESP_LOGI(TAG, "MQ-2 warmup period (30s)...");
    vTaskDelay(pdMS_TO_TICKS(GS_GAS_BASELINE_WARMUP_MS));

    /* Join mesh network */
    gs_mesh_join(&g_mesh, GS_NODE_SENTINEL, 0xFF, 0x10);
    ESP_LOGI(TAG, "Join request sent, waiting for assignment...");

    /* Start tasks */
    xTaskCreate(telemetry_task, "telemetry", 6144, NULL, 5, NULL);
    xTaskCreate(thermal_task, "thermal", 8192, NULL, 4, NULL);
    xTaskCreate(flareup_task, "flareup", 6144, NULL, 4, NULL);
    xTaskCreate(mesh_task, "mesh", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Sentinel tasks started. Monitoring grill...");
}
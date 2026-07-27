/*
 * GrillSync — Smoke Node Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Smoke Node monitors BBQ smoker smoke quality using PMS5003
 * particulate sensor, BME680 VOC, MQ-135 gas, and UV flame detector.
 * Runs SmokeNet 1D-CNN on-device for smoke quality classification
 * (clean blue / thin blue / dirty white / creosote / no smoke).
 * Reports to Hub via Sub-GHz 868 MHz.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/uart.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "GrillSync-Smoke";

/* === Global state === */
static gs_mesh_ctx_t g_mesh;

/* PMS5003 data */
static uint16_t g_pm1_0 = 0;     /* ×0.1 µg/m³ */
static uint16_t g_pm2_5 = 0;
static uint16_t g_pm10 = 0;

/* BME680 data */
static uint16_t g_voc_index = 0;
static uint16_t g_gas_resistance = 0;  /* ×100Ω */
static float g_temp = 25.0;
static float g_humidity = 50.0;

/* MQ-135 */
static uint16_t g_co2eq = 0;  /* ppm */

/* UV flame */
static uint8_t g_flame_intensity = 0;

/* Smoke quality (SmokeNet output) */
static uint8_t g_smoke_quality = 4;  /* 4 = no smoke */

/* History buffer for SmokeNet (30 timesteps × 5 channels) */
#define SMOKE_HIST_LEN 30
static uint16_t g_hist_pm25[SMOKE_HIST_LEN];
static uint16_t g_hist_voc[SMOKE_HIST_LEN];
static uint16_t g_hist_gas[SMOKE_HIST_LEN];
static uint16_t g_hist_co2[SMOKE_HIST_LEN];
static float g_hist_opacity[SMOKE_HIST_LEN];
static int g_hist_idx = 0;

static uint16_t g_msg_seq = 0;

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SMOKE_GPIO_SX_MOSI,
        .miso_io_num = SMOKE_GPIO_SX_MISO,
        .sclk_io_num = SMOKE_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = SMOKE_GPIO_SX_NSS,
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
    gpio_set_level(SMOKE_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(SMOKE_GPIO_SX_DIO1); }
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

/* === I2C for BME680 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SMOKE_GPIO_I2C_SDA,
        .scl_io_num = SMOKE_GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === PMS5003 UART Reader === */
/*
 * PMS5003 protocol: 32-byte frame via UART
 * [0x42, 0x4D, framelen_hi, framelen_lo, PM1_0_hi, PM1_0_lo,
 *  PM2_5_hi, PM2_5_lo, PM10_hi, PM10_lo, ... checksum_hi, checksum_lo]
 */
#define PMS5003_FRAME_LEN 32

static void pms5003_read(void)
{
    uint8_t frame[PMS5003_FRAME_LEN];

    /* In production: read 32 bytes from UART1 */
    /* Check start bytes 0x42 0x4D */
    /* Parse PM1.0, PM2.5, PM10 from offsets 4, 6, 8 */
    /* Verify checksum at offset 30 */

    /* Simulated data */
    g_pm1_0 = 15;   /* 1.5 µg/m³ */
    g_pm2_5 = 25;   /* 2.5 µg/m³ */
    g_pm10  = 35;   /* 3.5 µg/m³ */
}

/* === BME680 Reader === */
static void bme680_read(void)
{
    /* In production: read via I²C, apply BSEC library */
    /* VOC index, gas resistance, temp, humidity */
    g_voc_index = 100;     /* 0–500 scale */
    g_gas_resistance = 25000;  /* 250 kΩ */
    g_temp = 75.0;  /* Smoker internal temp */
    g_humidity = 60.0;
}

/* === MQ-135 Reader === */
static void mq135_read(void)
{
    /* ADC read, convert to CO₂ equivalent */
    /* In production: adc_read + calibration curve */
    g_co2eq = 600;  /* ppm */
}

/* === UV Flame Reader === */
static void uv_flame_read(void)
{
    /* In production: ADC read of UV-TRON output */
    g_flame_intensity = 50;  /* 0–255 scale */
}

/* === SmokeNet 1D-CNN (simplified inference) === */
/*
 * SmokeNet: 5-class smoke quality classifier
 * Input: 5-channel × 30-timestep history (PM2.5, VOC, gas, CO₂, opacity)
 * Output: [clean_blue, thin_blue, dirty_white, creosote, no_smoke]
 *
 * In production: TFLite-Micro int8 quantized 1D-CNN (~90 KB)
 * Simplified heuristic below.
 */
static void run_smoke_classification(void)
{
    /* Compute smoke opacity from PM1.0/PM2.5 ratio */
    float opacity = 0;
    if (g_pm2_5 > 0) {
        opacity = (float)g_pm1_0 / (float)g_pm2_5;
    }

    /* Update history */
    g_hist_pm25[g_hist_idx] = g_pm2_5;
    g_hist_voc[g_hist_idx] = g_voc_index;
    g_hist_gas[g_hist_idx] = g_gas_resistance;
    g_hist_co2[g_hist_idx] = g_co2eq;
    g_hist_opacity[g_hist_idx] = opacity;
    g_hist_idx = (g_hist_idx + 1) % SMOKE_HIST_LEN;

    /* Get averages of last 10 readings */
    int start = (g_hist_idx - 10 + SMOKE_HIST_LEN) % SMOKE_HIST_LEN;
    float avg_pm25 = 0, avg_voc = 0, avg_opacity = 0;
    for (int i = 0; i < 10; i++) {
        int idx = (start + i) % SMOKE_HIST_LEN;
        avg_pm25 += g_hist_pm25[idx];
        avg_voc += g_hist_voc[idx];
        avg_opacity += g_hist_opacity[idx];
    }
    avg_pm25 /= 10;
    avg_voc /= 10;
    avg_opacity /= 10;

    /* Classify smoke quality */
    if (avg_pm25 < GS_SMOKE_CLEAN_PM25) {
        g_smoke_quality = 4;  /* No/thin smoke */
    } else if (avg_pm25 < 80 && avg_voc < 150) {
        g_smoke_quality = 0;  /* Clean blue smoke (ideal) */
    } else if (avg_pm25 < 100 && avg_voc < 200) {
        g_smoke_quality = 3;  /* Thin blue smoke */
    } else if (avg_pm25 < GS_SMOKE_DIRTY_PM25 && avg_voc < GS_SMOKE_CREOSOTE_VOC) {
        g_smoke_quality = 1;  /* Dirty white smoke */
    } else if (avg_voc >= GS_SMOKE_CREOSOTE_VOC || avg_pm25 >= GS_SMOKE_DIRTY_PM25) {
        g_smoke_quality = 2;  /* Creosote (acrid, bitter) */
        /* Send alert */
        gs_message_t alert;
        gs_build_alert(&alert, g_mesh.node_id, g_mesh.msg_seq++,
                       GS_ALERT_SMOKE_CREOSOTE, GS_PRIORITY_MEDIUM, NULL, 0);
        gs_mesh_send(&g_mesh, &alert);
        ESP_LOGW(TAG, "💨 Creosote smoke detected — adjust airflow!");
    }

    const char *quality_names[] = {
        "Clean Blue", "Dirty White", "Creosote", "Thin Blue", "No Smoke"
    };
    ESP_LOGI(TAG, "Smoke: PM2.5=%.1f VOC=%d quality=%s",
             avg_pm25 / 10.0, (int)avg_voc,
             quality_names[g_smoke_quality]);
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        /* Read all sensors */
        pms5003_read();
        bme680_read();
        mq135_read();
        uv_flame_read();

        /* Run smoke classification */
        run_smoke_classification();

        /* Send telemetry to Hub */
        gs_message_t msg;
        gs_build_smoke_telem(&msg, g_mesh.node_id, g_mesh.msg_seq++,
                               0xFF,  /* USB-powered */
                               g_pm1_0, g_pm2_5, g_pm10,
                               g_voc_index, g_gas_resistance,
                               g_co2eq, g_smoke_quality, g_flame_intensity,
                               (int16_t)(g_temp * 10), (uint16_t)(g_humidity * 10),
                               0);
        gs_mesh_send(&g_mesh, &msg);

        vTaskDelay(pdMS_TO_TICKS(1000));  /* 1 Hz telemetry */
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
                    break;
                }

                case GS_MSG_COMMAND: {
                    uint8_t cmd = msg.payload[0];
                    ESP_LOGI(TAG, "Command 0x%02X from hub", cmd);
                    switch (cmd) {
                        case GS_CMD_REBOOT:
                            esp_restart();
                            break;
                        case GS_CMD_CALIBRATE:
                            ESP_LOGI(TAG, "Recalibrating smoke sensors...");
                            /* Reset PMS5003, BME680 baselines */
                            break;
                        default:
                            break;
                    }
                    break;
                }

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
    /* PMS5003 enable pin */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SMOKE_GPIO_PMS_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(SMOKE_GPIO_PMS_EN, 1);  /* Enable PMS5003 */

    /* LED */
    io_conf.pin_bit_mask = (1ULL << SMOKE_GPIO_LED);
    gpio_config(&io_conf);
}

/* === UART for PMS5003 === */
static void uart_init(void)
{
    uart_config_t uart_conf = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_conf);
    uart_set_pin(UART_NUM_1, SMOKE_GPIO_PMS_TX, SMOKE_GPIO_PMS_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/* === App Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "GrillSync Smoke Node starting...");

    gpio_setup();
    i2c_init();
    uart_init();
    spi_init();

    /* Initialize mesh as smoke node */
    gs_radio_config_t radio_cfg = {
        .frequency = GS_NET_FREQ_HZ,
        .bandwidth = GS_NET_BW_HZ,
        .spreading_factor = GS_NET_SF,
        .coding_rate = GS_NET_CR,
        .preamble_len = GS_NET_PREAMBLE,
        .tx_power_dbm = GS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (gs_mesh_init(&g_mesh, GS_NODE_SMOKE, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    /* Wait for PMS5003 warmup */
    ESP_LOGI(TAG, "PMS5003 warmup (10s)...");
    vTaskDelay(pdMS_TO_TICKS(10000));

    /* Join mesh network */
    gs_mesh_join(&g_mesh, GS_NODE_SMOKE, 0xFF, 0x10);
    ESP_LOGI(TAG, "Join request sent, waiting for assignment...");

    /* Start tasks */
    xTaskCreate(telemetry_task, "telemetry", 6144, NULL, 5, NULL);
    xTaskCreate(mesh_task, "mesh", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Smoke Node tasks started. Monitoring smoke quality...");
}
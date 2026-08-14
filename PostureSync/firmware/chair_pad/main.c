/*
 * PostureSync Smart Chair Pad Firmware
 * Target: ESP32-S3-MINI-1
 *
 * 16×16 FSR pressure matrix for seated posture detection.
 * Weight distribution, pelvic tilt, slouching, leg crossing detection.
 * MCP23017 I²C GPIO expanders for matrix multiplexing.
 * HX711 load cell amplifier for total weight.
 * SX1262 Sub-GHz 868 MHz to Hub.
 *
 * Battery: 2× AAA, 6-month life (deep sleep between scans)
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
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/adc.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/mesh.h"

static const char *TAG = "CHAIR_PAD";

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA     1
#define PIN_I2C_SCL     2
#define PIN_SPI_CS      4
#define PIN_SPI_SCK     5
#define PIN_SPI_MISO    6
#define PIN_SPI_MOSI    7
#define PIN_SX1262_DIO1 8
#define PIN_SX1262_BUSY 9
#define PIN_SX1262_RST  10
#define PIN_HX711_SCK   11
#define PIN_HX711_DOUT  12
#define PIN_ROW_A       13
#define PIN_ROW_B       14
#define PIN_ROW_C       15
#define PIN_COL_A       16
#define PIN_COL_B       17
#define PIN_COL_C       18
#define PIN_FSR_ANALOG 19
#define PIN_BAT_SENSE   20

#define I2C_PORT I2C_NUM_0
#define MCP23017_ADDR1 0x20
#define MCP23017_ADDR2 0x21

/* 16×16 pressure matrix */
static uint16_t g_pressure[16][16];
static uint16_t g_weight_total = 0;
static uint8_t g_left_pct = 50;
static uint8_t g_right_pct = 50;
static uint8_t g_pelvic_tilt = 0;
static uint8_t g_posture_class = POSTURE_NEUTRAL;
static uint8_t g_ischial_contact = 100;
static uint8_t g_movement_var = 0;

/* Mesh state */
static mesh_state_t g_mesh;
static spi_device_handle_t g_sx1262_spi;

/* ---- I2C MCP23017 ---- */
static void mcp23017_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    i2c_master_write_to_device(I2C_PORT, addr, buf, 2, pdMS_TO_TICKS(10));
}

static uint8_t mcp23017_read(uint8_t addr, uint8_t reg)
{
    uint8_t val;
    i2c_master_write_read_device(I2C_PORT, addr, &reg, 1, &val, 1, pdMS_TO_TICKS(10));
    return val;
}

static void mcp23017_init(void)
{
    /* MCP23017 #1: Rows 0-15 (GPIOA + GPIOB as outputs for row selection) */
    mcp23017_write(MCP23017_ADDR1, 0x00, 0x00); /* IODIRA: all output */
    mcp23017_write(MCP23017_ADDR1, 0x01, 0x00); /* IODIRB: all output */

    /* MCP23017 #2: Columns 0-15 (GPIOA + GPIOB as inputs for column reading) */
    mcp23017_write(MCP23017_ADDR2, 0x00, 0xFF); /* IODIRA: all input */
    mcp23017_write(MCP23017_ADDR2, 0x01, 0xFF); /* IODIRB: all input */

    /* Enable pull-ups on column inputs */
    mcp23017_write(MCP23017_ADDR2, 0x0C, 0xFF); /* GPPUA */
    mcp23017_write(MCP23017_ADDR2, 0x0D, 0xFF); /* GPPUB */
}

/* ---- HX711 load cell ---- */
static long hx711_read(void)
{
    /* Wait for DOUT to go low */
    int timeout = 0;
    while (gpio_get_level(PIN_HX711_DOUT) == 1) {
        if (++timeout > 100000) return 0;
    }

    /* Read 24 bits */
    long val = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(PIN_HX711_SCK, 1);
        esp_rom_delay_us(1);
        val = (val << 1) | gpio_get_level(PIN_HX711_DOUT);
        gpio_set_level(PIN_HX711_SCK, 0);
        esp_rom_delay_us(1);
    }

    /* 25th pulse for channel A, gain 128 */
    gpio_set_level(PIN_HX711_SCK, 1);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_HX711_SCK, 0);

    /* Sign extend */
    if (val & 0x800000) val |= 0xFF000000;

    return val;
}

/* ---- Pressure matrix scan ---- */
static void scan_pressure_matrix(void)
{
    memset(g_pressure, 0, sizeof(g_pressure));

    for (int row = 0; row < 16; row++) {
        /* Select row via MCP23017 #1 */
        uint8_t gpio_val;
        if (row < 8) {
            mcp23017_write(MCP23017_ADDR1, 0x12, 1 << row); /* OLATA */
            mcp23017_write(MCP23017_ADDR1, 0x13, 0x00);     /* OLATB */
        } else {
            mcp23017_write(MCP23017_ADDR1, 0x12, 0x00);    /* OLATA */
            mcp23017_write(MCP23017_ADDR1, 0x13, 1 << (row - 8)); /* OLATB */
        }

        esp_rom_delay_us(100); /* Settle */

        /* Read columns via MCP23017 #2 */
        uint8_t cols_lo = mcp23017_read(MCP23017_ADDR2, 0x12); /* GPIOA */
        uint8_t cols_hi = mcp23017_read(MCP23017_ADDR2, 0x13); /* GPIOB */

        /* For each column, read analog pressure value */
        for (int col = 0; col < 16; col++) {
            /* Select column via 74HC4051 mux */
            gpio_set_level(PIN_COL_A, col & 1);
            gpio_set_level(PIN_COL_B, (col >> 1) & 1);
            gpio_set_level(PIN_COL_C, (col >> 2) & 1);

            esp_rom_delay_us(50);

            /* Read ADC */
            int raw = adc1_get_raw(ADC1_CHANNEL_0); /* GPIO19 = ADC1_CH8 */
            if (raw < 0) raw = 0;
            g_pressure[row][col] = (uint16_t)raw;
        }
    }
}

/* ---- Posture analysis from pressure map ---- */
static void analyze_pressure(void)
{
    /* Total weight from HX711 */
    long raw = hx711_read();
    g_weight_total = (uint16_t)(raw / 100); /* Calibrated conversion */

    /* Left/right weight distribution */
    uint32_t left_sum = 0, right_sum = 0;
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            left_sum += g_pressure[row][col];
        }
        for (int col = 8; col < 16; col++) {
            right_sum += g_pressure[row][col];
        }
    }
    uint32_t total = left_sum + right_sum;
    if (total > 0) {
        g_left_pct = (uint8_t)(left_sum * 100 / total);
        g_right_pct = (uint8_t)(right_sum * 100 / total);
    }

    /* Pelvic tilt: anterior (pressure forward) vs posterior (pressure backward) */
    uint32_t front_sum = 0, back_sum = 0;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 16; col++) {
            front_sum += g_pressure[row][col];
        }
    }
    for (int row = 8; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            back_sum += g_pressure[row][col];
        }
    }
    if (front_sum + back_sum > 0) {
        int tilt = (int)((front_sum - back_sum) * 100 / (front_sum + back_sum));
        g_pelvic_tilt = (uint8_t)(tilt < 0 ? -tilt : tilt);
    }

    /* Ischial vs sacral contact: pressure concentrated on ischial tuberosities (healthy)
     * vs. sacrum (slouching) */
    /* Ischial region: rows 10-13, center columns 5-10 */
    uint32_t ischial = 0;
    for (int row = 10; row < 14; row++) {
        for (int col = 5; col < 11; col++) {
            ischial += g_pressure[row][col];
        }
    }
    /* Sacral region: rows 14-15, all columns */
    uint32_t sacral = 0;
    for (int row = 14; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            sacral += g_pressure[row][col];
        }
    }
    if (ischial + sacral > 0) {
        g_ischial_contact = (uint8_t)(ischial * 100 / (ischial + sacral));
    }

    /* Posture classification from pressure patterns */
    if (g_ischial_contact < 40) {
        g_posture_class = POSTURE_SLOUCHING; /* Sacral sitting */
    } else if (g_left_pct < 40 || g_right_pct < 40) {
        g_posture_class = POSTURE_SCOLIOTIC; /* Asymmetric lean */
    } else if (g_pelvic_tilt > 30) {
        g_posture_class = POSTURE_ANTERIOR_TILT;
    } else if (g_pelvic_tilt > 20 && front_sum > back_sum) {
        g_posture_class = POSTURE_POSTERIOR_TILT;
    } else {
        g_posture_class = POSTURE_NEUTRAL;
    }

    /* Movement variance (active sitting indicator) */
    static uint8_t prev_map[4][4] = {0};
    uint8_t curr_map[4][4] = {0};
    /* Downsample 16×16 to 4×4 */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            uint32_t sum = 0;
            for (int r = i * 4; r < (i + 1) * 4; r++) {
                for (int c = j * 4; c < (j + 1) * 4; c++) {
                    sum += g_pressure[r][c];
                }
            }
            curr_map[i][j] = (uint8_t)(sum / 16);
        }
    }
    uint32_t diff_sum = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int diff = (int)curr_map[i][j] - (int)prev_map[i][j];
            diff_sum += (diff < 0 ? -diff : diff);
            prev_map[i][j] = curr_map[i][j];
        }
    }
    g_movement_var = (uint8_t)(diff_sum / 16);
}

/* ---- SX1262 TX ---- */
static void sx1262_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_sx1262_spi);
}

static void sx1262_tx(const uint8_t *data, uint16_t len)
{
    spi_transaction_t t = {
        .tx_buffer = data,
        .length = len * 8,
    };
    spi_device_transmit(g_sx1262_spi, &t);
}

/* ---- Send data to Hub ---- */
static void send_to_hub(void)
{
    chair_pad_data_t data = {0};
    data.weight_total = g_weight_total;
    data.left_pct = g_left_pct;
    data.right_pct = g_right_pct;
    data.pelvic_tilt = g_pelvic_tilt;
    data.posture_class = g_posture_class;
    data.ischial_contact = g_ischial_contact;
    data.movement_var = g_movement_var;
    data.battery = 80; /* Placeholder */
    data.flags = 0x01;

    /* Downsample pressure map to 4×4 for payload */
    uint8_t map_idx = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            uint32_t sum = 0;
            for (int r = i * 4; r < (i + 1) * 4; r++) {
                for (int c = j * 4; c < (j + 1) * 4; c++) {
                    sum += g_pressure[r][c];
                }
            }
            data.pressure_map[map_idx++] = (uint8_t)(sum / 256);
        }
    }

    postsync_frame_t frame;
    postsync_build_frame(&frame, NODE_ID_CHAIR_PAD, NODE_ID_HUB,
                         MSG_TYPE_SENSOR_DATA, g_mesh.tx_seq++,
                         (uint8_t *)&data, sizeof(data));

    uint8_t buf[FRAME_MAX_LEN];
    memcpy(buf, &frame, sizeof(frame));
    sx1262_tx(buf, sizeof(frame));

    ESP_LOGI(TAG, "TX: weight=%d L=%d%% R=%d%% tilt=%d posture=%d ischial=%d",
             data.weight_total, data.left_pct, data.right_pct,
             data.pelvic_tilt, data.posture_class, data.ischial_contact);
}

/* ---- Main task ---- */
static void chair_pad_task(void *pv)
{
    /* Scan every 10 seconds, transmit every 60 seconds
     * Deep sleep between scans for battery saving */

    uint32_t scan_counter = 0;
    while (1) {
        /* Scan pressure matrix */
        scan_pressure_matrix();
        analyze_pressure();

        /* Print debug */
        ESP_LOGI(TAG, "Scan %d: posture=%d weight=%dg L=%d%% R=%d%%",
                 scan_counter, g_posture_class, g_weight_total,
                 g_left_pct, g_right_pct);

        /* Send to hub every 6th scan (60 seconds) */
        if (scan_counter % 6 == 0) {
            send_to_hub();
        }

        scan_counter++;

        /* Light sleep for 10 seconds */
        esp_sleep_enable_timer_wakeup(10 * 1000000);
        esp_light_sleep_start();
    }
}

/* ---- Join mesh ---- */
static void join_mesh(void)
{
    join_req_t req = {
        .node_type = 0x04, /* Chair Pad */
        .hw_version = 1,
        .fw_version = 1,
        .capabilities = 0x01,
        .device_uid = 0x00000004
    };

    postsync_frame_t frame;
    postsync_build_frame(&frame, NODE_ID_CHAIR_PAD, NODE_ID_HUB,
                         MSG_TYPE_JOIN_REQ, g_mesh.tx_seq++,
                         (uint8_t *)&req, sizeof(req));

    uint8_t buf[FRAME_MAX_LEN];
    memcpy(buf, &frame, sizeof(frame));
    sx1262_tx(buf, sizeof(frame));
}

void app_main(void)
{
    ESP_LOGI(TAG, "PostureSync Chair Pad starting...");

    nvs_flash_init();

    /* I2C */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    /* GPIO */
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << PIN_HX711_SCK) |
                        (1ULL << PIN_ROW_A) | (1ULL << PIN_ROW_B) | (1ULL << PIN_ROW_C) |
                        (1ULL << PIN_COL_A) | (1ULL << PIN_COL_B) | (1ULL << PIN_COL_C),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out_conf);

    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << PIN_HX711_DOUT),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&in_conf);

    /* ADC */
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_8, ADC_ATTEN_11db); /* GPIO19 */

    /* MCP23017 */
    mcp23017_init();

    /* SX1262 */
    sx1262_init();

    /* Mesh */
    mesh_init(&g_mesh, NODE_ID_CHAIR_PAD, false);
    join_mesh();

    /* Main task */
    xTaskCreate(chair_pad_task, "chair_pad", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Chair Pad running");
}
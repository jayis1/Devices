/*
 * AllergySync — Room Sentinel Firmware (ESP32-S3, ESP-IDF)
 *
 * The sensing powerhouse:
 *  - Sensirion SPS30 laser PM sensor (UART) — 0.3–10 µm size bins
 *  - Sensirion SCD41 CO2 sensor (I2C)
 *  - Bosch BME688 VOC sensor (I2C)
 *  - TMP117 high-accuracy temperature (I2C)
 *  - PollenNet: on-device 1D-CNN pollen classifier (tflite-micro)
 *  - Fan-assisted sampling
 *  - Data aggregation: 1-min averages → TX to hub every 5 min
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_timer.h"

#include "common/allergysync_proto.h"
#include "common/as_lr1121.h"
#include "common/as_tdma.h"

/* tflite-micro headers */
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "pollennet_model.h" /* Generated model data */

static const char *TAG = "AllergySync-Sentinel";

/* ---- Pin assignments ---- */
#define PIN_SPS30_TX    8
#define PIN_SPS30_RX    9
#define PIN_SCD41_SDA   10
#define PIN_SCD41_SCL   11
#define PIN_BME688_SDA  12
#define PIN_BME688_SCL  13
#define PIN_TMP117_SDA  10  /* shared I2C bus 1 */
#define PIN_TMP117_SCL  11
#define PIN_FAN_PWM     4
#define PIN_FAN_EN      5
#define PIN_LED         6

/* LR1121 pins */
#define PIN_LR_CS     34
#define PIN_LR_SCLK   35
#define PIN_LR_MISO   36
#define PIN_LR_MOSI   37
#define PIN_LR_DIO0   38
#define PIN_LR_DIO1   39
#define PIN_LR_RESET  40
#define PIN_LR_BUSY   41

/* ---- I2C addresses ---- */
#define SCD41_ADDR     0x62
#define BME688_ADDR    0x77
#define TMP117_ADDR    0x48

/* ---- UART for SPS30 ---- */
#define SPS30_UART_PORT  UART_NUM_1
#define SPS30_BAUD       115200

/* ---- SPS30 command set ---- */
#define SPS30_CMD_START     0x0010
#define SPS30_CMD_READ      0x0200
#define SPS30_CMD_STOP      0x0104
#define SPS30_CMD_RESET     0xD3D3
#define SPS30_CM_READ       0x0300

/* ---- Aggregation state ---- */
typedef struct {
    float pm1_0_sum;
    float pm2_5_sum;
    float pm10_sum;
    float co2_sum;
    float voc_sum;
    float temp_sum;
    float humidity_sum;
    float pressure_sum;
    uint16_t count;
    uint16_t pollen_count_sum;
    uint8_t  pollen_class_votes[AS_POLLEN_COUNT];
} agg_state_t;

static agg_state_t agg = {0};

/* ---- Latest SPS30 reading ---- */
typedef struct {
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm4_0;
    uint16_t pm10;
    uint16_t num_03;  /* particles/cm³ > 0.3 µm */
    uint16_t num_05;
    uint16_t num_10;
    uint16_t typ_pm;  /* typical particle size */
} sps30_data_t;

static sps30_data_t sps30_latest;

/* ---- TFLM (PollenNet) ---- */
static tflite::MicroErrorReporter micro_error_reporter;
static tflite::AllOpsResolver resolver;
static const tflite::Model *model;
static tflite::MicroInterpreter *interpreter = NULL;
static TfLiteTensor *input_tensor;
static TfLiteTensor *output_tensor;

/* PollenNet: input [1, 8] (8 PM size bins), output [1, 6] (6 classes) */
#define POLLENNET_INPUT_LEN   8
#define POLLENNET_OUTPUT_LEN  6
#define TFLM_TENSOR_ARENA_SIZE  (32 * 1024)

static uint8_t tensor_arena[TFLM_TENSOR_ARENA_SIZE];

static void pollennet_init(void)
{
    model = tflite::GetModel(pollennet_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Model schema version mismatch");
        return;
    }

    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, TFLM_TENSOR_ARENA_SIZE,
        &micro_error_reporter);
    interpreter = &static_interpreter;

    interpreter->AllocateTensors();
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    ESP_LOGI(TAG, "PollenNet model loaded, arena used: %u",
             interpreter->arena_used_bytes());
}

static as_pollen_class_t pollennet_classify(const float *pm_bins)
{
    if (!interpreter)
        return AS_POLLEN_NONE;

    /* Fill input tensor (8 values) */
    for (int i = 0; i < POLLENNET_INPUT_LEN; i++)
        input_tensor->data.f[i] = pm_bins[i];

    TfLiteStatus status = interpreter->Invoke();
    if (status != kTfLiteOk) {
        ESP_LOGE(TAG, "PollenNet invoke failed: %d", status);
        return AS_POLLEN_NONE;
    }

    /* Argmax */
    int max_idx = 0;
    float max_val = output_tensor->data.f[0];
    for (int i = 1; i < POLLENNET_OUTPUT_LEN; i++) {
        if (output_tensor->data.f[i] > max_val) {
            max_val = output_tensor->data.f[i];
            max_idx = i;
        }
    }

    return (as_pollen_class_t)(max_idx + 1); /* +1 because NONE=0 */
}

static uint8_t pollennet_confidence(void)
{
    if (!output_tensor)
        return 0;

    /* Softmax → confidence of winning class */
    float exp_vals[POLLENNET_OUTPUT_LEN];
    float sum = 0;
    for (int i = 0; i < POLLENNET_OUTPUT_LEN; i++) {
        exp_vals[i] = expf(output_tensor->data.f[i]);
        sum += exp_vals[i];
    }

    int max_idx = 0;
    for (int i = 1; i < POLLENNET_OUTPUT_LEN; i++)
        if (exp_vals[i] > exp_vals[max_idx])
            max_idx = i;

    return (uint8_t)((exp_vals[max_idx] / sum) * 100);
}

/* ---- SPS30 driver (UART, SHDLC protocol) ---- */
static uint8_t shdlc_checksum(const uint8_t *data, size_t len)
{
    uint8_t cksum = 0;
    for (size_t i = 0; i < len; i++)
        cksum ^= data[i];
    return cksum;
}

static int sps30_send_cmd(uint16_t cmd, uint16_t data)
{
    uint8_t frame[8];
    frame[0] = 0x7E;  /* SHDLC start */
    frame[1] = 0x00;  /* Addr */
    frame[2] = 0x04;  /* Length */
    frame[3] = 0x00;  /* Command MSB */
    frame[4] = cmd >> 8;
    frame[5] = cmd & 0xFF;
    frame[6] = data & 0xFF;
    frame[7] = shdlc_checksum(&frame[1], 6);
    /* TODO: proper SHDLC framing with byte-stuffing */
    uart_write_bytes(SPS30_UART_PORT, frame, 8);
    return 0;
}

static int sps30_read_data(sps30_data_t *data)
{
    uint8_t rx_buf[40];
    int len = uart_read_bytes(SPS30_UART_PORT, rx_buf, sizeof(rx_buf),
                              pdMS_TO_TICKS(100));
    if (len < 40)
        return -1;

    /* Parse SHDLC frame (simplified — real parser needed) */
    /* SPS30 data: 10 values × 2 bytes = 20 bytes + 4 byte CRC */
    /* PM1.0, PM2.5, PM4.0, PM10, num0.3, num0.5, num1.0, num2.5, num4.0, num10 */
    /* Each value is 4 bytes (uint16_t scaled × 10) in big-endian */

    /* For this stub, we parse a simplified version */
    /* Skipping full SHDLC parsing — production code would handle it */
    return 0;
}

static void sps30_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = SPS30_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(SPS30_UART_PORT, &uart_cfg);
    uart_set_pin(SPS30_UART_PORT, PIN_SPS30_TX, PIN_SPS30_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(SPS30_UART_PORT, 256, 256, 0, NULL, 0);

    /* Start measurement */
    sps30_send_cmd(SPS30_CMD_START, 0x0300); /* Mode: typical + number */
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "SPS30 initialized");
}

/* ---- SCD41 driver (I2C) ---- */
static void scd41_init(void)
{
    /* SCD41: write command 0x21B1 → periodic measurement */
    uint8_t cmd[2] = { 0x21, 0xB1 };
    i2c_master_write_to_device(0, SCD41_ADDR, cmd, 2, pdMS_TO_TICKS(100));
}

static int scd41_read(uint16_t *co2, float *temp, float *humidity)
{
    uint8_t data[9];
    esp_err_t ret = i2c_master_read_from_device(0, SCD41_ADDR, data, 9,
                                                pdMS_TO_TICKS(100));
    if (ret != ESP_OK)
        return -1;

    /* CRC8 validation skipped for brevity */
    *co2 = (data[0] << 8) | data[1];
    uint16_t temp_raw = (data[3] << 8) | data[4];
    uint16_t hum_raw = (data[6] << 8) | data[7];
    *temp = -45.0f + 175.0f * temp_raw / 65535.0f;
    *humidity = 100.0f * hum_raw / 65535.0f;
    return 0;
}

/* ---- BME688 driver (I2C) — simplified ---- */
static uint16_t bme688_read_voc(void)
{
    /* In production: BSEC library or raw register reads + bme680 calc */
    /* Stub: read raw gas resistance and compute VOC index 0-500 */
    return 100; /* placeholder */
}

static void bme688_read_thp(float *temp, float *hum, float *press)
{
    /* Stub: production code reads calibration data + raw ADC */
    *temp = 22.0f;
    *hum = 45.0f;
    *press = 1013.0f;
}

/* ---- TMP117 driver (I2C) ---- */
static float tmp117_read_temp(void)
{
    uint8_t reg = 0x00; /* Temperature register */
    uint8_t data[2];
    i2c_master_write_read_device(0, TMP117_ADDR, &reg, 1, data, 2,
                                 pdMS_TO_TICKS(100));
    int16_t raw = (data[0] << 8) | data[1];
    return raw * 0.0078125f; /* 7.8125 mC/LSB */
}

/* ---- Fan control ---- */
static void fan_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .channel = LEDC_CHANNEL_0,
        .duty = 512, /* 50% duty */
        .gpio_num = PIN_FAN_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&ch);

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_FAN_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_FAN_EN, 1); /* Enable fan */
}

/* ---- I2C init ---- */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SCD41_SDA,
        .scl_io_num = PIN_SCD41_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(0, &conf);
    i2c_driver_install(0, conf.mode, 0, 0, 0);
}

/* ---- LR1121 platform port ---- */
static spi_device_handle_t lr_spi;

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
static void lr_reset_assert(bool assert) { gpio_set_level(PIN_LR_RESET, assert ? 0 : 1); }
static bool lr_busy(void) { return gpio_get_level(PIN_LR_BUSY) != 0; }
static void lr_delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static as_lr1121_port_t lr_port = {
    .cs_select = lr_cs_select,
    .cs_release = lr_cs_release,
    .spi_xfer = lr_spi_xfer,
    .reset = lr_reset_assert,
    .busy_read = lr_busy,
    .delay_ms = lr_delay,
};

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
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_LR_CS,
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &lr_spi);
}

/* ---- TDMA ---- */
static as_tdma_node_t tdma;

/* ---- Sampling task (1 Hz) ---- */
static void sampling_task(void *arg)
{
    uint16_t sample_count = 0;
    float pm_bins[8] = {0};

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); /* 1 Hz */

        /* Read SPS30 */
        if (sps30_read_data(&sps30_latest) == 0) {
            agg.pm1_0_sum += sps30_latest.pm1_0;
            agg.pm2_5_sum += sps30_latest.pm2_5;
            agg.pm10_sum  += sps30_latest.pm10;
            agg.pollen_count_sum += sps30_latest.num_03;
        }

        /* Build PM size bins for PollenNet */
        pm_bins[0] = sps30_latest.num_03 / 1.0f;  /* >0.3 µm */
        pm_bins[1] = sps30_latest.num_05 / 1.0f;  /* >0.5 µm */
        pm_bins[2] = sps30_latest.num_10 / 1.0f;  /* >1.0 µm */
        pm_bins[3] = sps30_latest.typ_pm / 1.0f;
        pm_bins[4] = sps30_latest.pm1_0 / 10.0f;
        pm_bins[5] = sps30_latest.pm2_5 / 10.0f;
        pm_bins[6] = sps30_latest.pm4_0 / 10.0f;
        pm_bins[7] = sps30_latest.pm10 / 10.0f;

        /* Classify pollen every 10 samples */
        if (++sample_count % 10 == 0) {
            as_pollen_class_t cls = pollennet_classify(pm_bins);
            uint8_t conf = pollennet_confidence();
            if (cls != AS_POLLEN_NONE) {
                agg.pollen_class_votes[cls]++;
                ESP_LOGI(TAG, "Pollen: class=%d conf=%d%%", cls, conf);
            }
        }

        /* Read SCD41 every 2 minutes */
        if (sample_count % 120 == 0) {
            uint16_t co2;
            float t, h;
            if (scd41_read(&co2, &t, &h) == 0) {
                agg.co2_sum += co2;
                ESP_LOGI(TAG, "CO2=%u ppm, T=%.1f, H=%.1f", co2, t, h);
            }
        }

        /* Read BME688 */
        uint16_t voc = bme688_read_voc();
        agg.voc_sum += voc;
        float bt, bh, bp;
        bme688_read_thp(&bt, &bh, &bp);
        agg.temp_sum += bt;
        agg.humidity_sum += bh;
        agg.pressure_sum += bp;

        /* Read TMP117 */
        float tt = tmp117_read_temp();
        agg.temp_sum += tt;
        agg.count++;
    }
}

/* ---- Telemetry TX task (every 5 min) ---- */
static void telemetry_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(300000)); /* 5 minutes */

        if (agg.count == 0)
            continue;

        /* Compute averages */
        as_telem_sentinel_t telem;
        memset(&telem, 0, sizeof(telem));
        telem.pm1_0 = (uint16_t)((agg.pm1_0_sum / agg.count) * 10);
        telem.pm2_5 = (uint16_t)((agg.pm2_5_sum / agg.count) * 10);
        telem.pm10  = (uint16_t)((agg.pm10_sum / agg.count) * 10);
        telem.co2_ppm = (uint16_t)(agg.co2_sum / (agg.count / 120 + 1));
        telem.voc_index = (uint16_t)(agg.voc_sum / agg.count);
        telem.temp_c = (int16_t)((agg.temp_sum / agg.count) * 100);
        telem.humidity_pct = (uint16_t)((agg.humidity_sum / agg.count) * 10);
        telem.pressure_hpa = (uint16_t)(agg.pressure_sum / agg.count);
        telem.pollen_count = agg.pollen_count_sum / agg.count;

        /* Find most-voted pollen class */
        uint8_t max_votes = 0;
        uint8_t best_class = AS_POLLEN_NONE;
        for (int i = 1; i < AS_POLLEN_COUNT; i++) {
            if (agg.pollen_class_votes[i] > max_votes) {
                max_votes = agg.pollen_class_votes[i];
                best_class = i;
            }
        }
        telem.pollen_class = best_class;
        telem.pollen_conf = max_votes > 0 ?
            (uint8_t)(100 * max_votes / (agg.count / 10)) : 0;
        telem.fan_rpm = 3000;
        telem.battery_pct = 100; /* USB powered */
        telem.flags = 0;

        /* Send via TDMA mesh */
        as_tdma_send(&tdma, AS_MSG_TELEMETRY, 0,
                     (uint8_t *)&telem, sizeof(telem));

        ESP_LOGI(TAG, "Telemetry sent: PM2.5=%.1f, pollen=%d conf=%d%%",
                 telem.pm2_5 / 10.0f, telem.pollen_class, telem.pollen_conf);

        /* Reset aggregation */
        memset(&agg, 0, sizeof(agg));
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "AllergySync Room Sentinel starting...");

    /* Init peripherals */
    i2c_init();
    spi_init();
    sps30_init();
    scd41_init();
    fan_init();
    pollennet_init();

    /* Init LR1121 */
    gpio_config_t io = {0};
    io.pin_bit_mask = (1ULL << PIN_LR_RESET) | (1ULL << PIN_LED);
    io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io);
    io.pin_bit_mask = (1ULL << PIN_LR_BUSY) | (1ULL << PIN_LR_DIO0) |
                      (1ULL << PIN_LR_DIO1);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);
    gpio_set_level(PIN_LR_RESET, 1);

    if (as_lr1121_init(&lr_port) != 0) {
        ESP_LOGE(TAG, "LR1121 init failed!");
        return;
    }
    as_lr1121_set_channel(868100000);
    as_lr1121_set_tx_power(14);
    as_lr1121_set_modem_fsk(50000, 25000, 100000);
    uint8_t sync[] = { 0xA5, 0x1E, 0x9C, 0x47 };
    as_lr1121_set_sync_word(sync, 4);

    /* Init TDMA as node */
    as_tdma_init(&tdma, false, &lr_port);

    /* Join mesh */
    uint8_t dummy_pubkey[64] = {0}; /* Real ECDH key in production */
    as_tdma_join(&tdma, AS_NODE_SENTINEL, dummy_pubkey);

    /* Create tasks */
    xTaskCreate(sampling_task, "sampling", 8192, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telemetry", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Room Sentinel running. Waiting for beacon sync...");
}
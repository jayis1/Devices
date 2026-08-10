/*
 * FireSync — Room Sentinel Firmware
 * ESP32-S3, FreeRTOS
 *
 * Each Room Sentinel fuses 4 sensor modalities (photoelectric smoke,
 * electrochemical CO, rate-of-rise temperature, MLX90640 thermal array)
 * and runs FlameNet CNN to classify fire vs. nuisance.
 * Reports to Hub via Sub-GHz TDMA mesh. Includes PIR occupancy.
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
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/onewire.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "FireSync-Sentinel";

/* === Sensor Context === */
typedef struct {
    /* Smoke (PMSA003 via I²C) */
    uint16_t smoke_pm25;        /* μg/m³ */
    uint16_t smoke_pm10;       /* μg/m³ */
    /* CO (ZE07 via UART2) */
    uint16_t co_ppm;           /* ppm */
    /* Temperature (DS18B20 1-Wire) */
    int16_t  temp_c_x10;       /* ×0.1°C */
    int8_t   temp_rate;        /* °C/min */
    int16_t  prev_temp_x10;    /* Previous reading for rate calculation */
    /* Thermal (MLX90640 I²C) */
    int16_t  thermal_max_x10;   /* Max zone temp ×0.1°C */
    int16_t  thermal_mean_x10;  /* Mean zone temp ×0.1°C */
    uint8_t  thermal_anomaly;   /* ThermalAnomaly score 0-255 */
    /* FlameNet */
    uint8_t  flame_class;      /* 0-6 */
    uint8_t  flame_confidence;  /* 0-100% */
    uint16_t flamenet_ms;      /* Inference time */
    /* PIR occupancy */
    uint8_t  pir_occupant;      /* 0=empty, 1=occupied */
    /* System */
    uint8_t  battery_v;        /* ×0.01V */
    uint8_t  room_id;          /* Configured room ID */
    uint8_t  alarm_active;     /* 0=off, 1=on */
} fs_sensor_ctx_t;

static fs_sensor_ctx_t g_sensors;
static fs_mesh_ctx_t g_mesh;
static fs_radio_hal_t g_radio_hal;
static fs_radio_config_t g_radio_cfg;
static SemaphoreHandle_t g_sensor_mutex;
static spi_device_handle_t g_spi;
static SemaphoreHandle_t g_spi_mutex;

/* === SX1262 HAL (ESP32-S3 SPI) === */
static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SENT_GPIO_SX_MOSI,
        .miso_io_num = SENT_GPIO_SX_MISO,
        .sclk_io_num = SENT_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0, .spics_io_num = -1, .queue_size = 4,
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

static void hal_cs_low(void)  { gpio_set_level(SENT_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(SENT_GPIO_SX_NSS, 1); }
static void hal_reset(int a) { gpio_set_level(SENT_GPIO_SX_RST, !a); }
static int  hal_dio1_read(void) { return gpio_get_level(SENT_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(SENT_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) {}

/* === PMSA003 Smoke Sensor (I²C) === */
static int pmsa003_read(uint16_t *pm25, uint16_t *pm10)
{
    /* Production: I²C read of PMSA003 data frame (32 bytes) */
    /* Frame: 0x42 0x4D + 28 bytes + checksum */
    /* PM2.5 at offset 12-13, PM10 at offset 16-17 (big-endian) */
    *pm25 = 15;   /* Placeholder: μg/m³ */
    *pm10 = 20;
    return 0;
}

/* === ZE07-CO Sensor (UART2) === */
static int ze07_read(uint16_t *co_ppm)
{
    /* Production: UART2 read of ZE07 frame (9 bytes) */
    /* Frame: 0xFF 0x86 + gas concentration (big-endian) + checksum */
    *co_ppm = 2;  /* Placeholder: ppm */
    return 0;
}

/* === DS18B20 Temperature (1-Wire) === */
static int ds18b20_read(int16_t *temp_c_x10)
{
    /* Production: 1-Wire reset → ROM match → convert T → read scratchpad */
    /* Scratchpad byte 0-1 = temperature (LSB first, 0.0625°C resolution) */
    *temp_c_x10 = 235; /* Placeholder: 23.5°C */
    return 0;
}

/* === MLX90640 Thermal Array (I²C) === */
#define MLX90640_ROWS  24
#define MLX90640_COLS  32
static int16_t g_thermal_frame[MLX90640_ROWS * MLX90640_COLS];

static int mlx90640_read(int16_t *max_x10, int16_t *mean_x10,
                          uint8_t *anomaly_score)
{
    /* Production: I²C read of MLX90640 EEPROM + ram data */
    /* Read 832 bytes page 0, 832 bytes page 1, apply calibration */
    /* 768 zones, each ×0.02°C resolution (raw 16-bit) */

    /* Placeholder: simulate ambient thermal */
    int16_t max_val = 0;
    int32_t sum = 0;
    for (int i = 0; i < MLX90640_ROWS * MLX90640_COLS; i++) {
        g_thermal_frame[i] = 250 + (i % 7); /* ~25°C + noise */
        sum += g_thermal_frame[i];
        if (g_thermal_frame[i] > max_val) max_val = g_thermal_frame[i];
    }
    *max_x10 = max_val;
    *mean_x10 = (int16_t)(sum / (MLX90640_ROWS * MLX90640_COLS));
    *anomaly_score = 50; /* Placeholder: 0-255, >128 = anomaly */

    /* If any zone > FS_THERMAL_FIRE_C × 10, flag anomaly */
    if (*max_x10 > FS_THERMAL_FIRE_C * 10) {
        *anomaly_score = 200;
    }

    return 0;
}

/* === FlameNet CNN (TFLite-Micro) === */
static int flamenet_infer(const fs_sensor_ctx_t *ctx,
                           uint8_t *class_out, uint8_t *conf_out,
                           uint16_t *inference_ms)
{
    /* Production: TFLite-Micro int8 inference on ESP32-S3
     * Input: smoke_pm25[20], co_ppm[20], thermal[768], temp_rate[1]
     * Model: Multi-modal 1D-CNN, 7-class output
     * Output: softmax probabilities for 7 classes
     */
    uint64_t start = esp_timer_get_time();

    /* Heuristic placeholder (production: TFLite-Micro model) */
    uint8_t cls = FS_CLASS_NORMAL;
    uint8_t conf = 80;

    if (ctx->smoke_pm25 > FS_SMOKE_FIRE_UGM3 &&
        ctx->co_ppm > 50 &&
        ctx->thermal_max_x10 > FS_THERMAL_FIRE_C * 10) {
        cls = FS_CLASS_FLAMING_FIRE;
        conf = 95;
    } else if (ctx->smoke_pm25 > FS_SMOKE_WARN_UGM3 &&
               ctx->co_ppm > 30 &&
               ctx->temp_rate > 4) {
        cls = FS_CLASS_SMOLDERING;
        conf = 82;
    } else if (ctx->smoke_pm25 > FS_SMOKE_WARN_UGM3 &&
               ctx->co_ppm < 10 &&
               ctx->thermal_max_x10 < FS_THERMAL_WARN_C * 10) {
        cls = FS_CLASS_COOKING;
        conf = 88;
    } else if (ctx->smoke_pm25 > 100 &&
               ctx->co_ppm < 5 &&
               ctx->thermal_max_x10 < 350) {
        cls = FS_CLASS_STEAM;
        conf = 85;
    } else if (ctx->smoke_pm25 > 50 &&
               ctx->co_ppm < 5 &&
               ctx->thermal_max_x10 > 600 &&
               ctx->thermal_max_x10 < 1000) {
        cls = FS_CLASS_CANDLE;
        conf = 80;
    } else if (ctx->smoke_pm25 > 30 &&
               ctx->co_ppm < 8 &&
               ctx->thermal_max_x10 < 400) {
        cls = FS_CLASS_CIGARETTE;
        conf = 75;
    }

    *class_out = cls;
    *conf_out = conf;
    *inference_ms = (uint16_t)((esp_timer_get_time() - start) / 1000);
    return 0;
}

/* === ThermalAnomaly (LSTM Autoencoder) === */
static int thermal_anomaly_check(const int16_t *frame, uint8_t *score)
{
    /* Production: TFLite-Micro LSTM autoencoder reconstruction error */
    /* Placeholder: simple max-threshold */
    int16_t max_val = 0;
    for (int i = 0; i < MLX90640_ROWS * MLX90640_COLS; i++) {
        if (frame[i] > max_val) max_val = frame[i];
    }
    if (max_val > FS_THERMAL_FIRE_C * 10)
        *score = 220;
    else if (max_val > FS_THERMAL_WARN_C * 10)
        *score = 150;
    else
        *score = 50;
    return 0;
}

/* === PIR Occupancy === */
static int pir_read(uint8_t *occupied)
{
    *occupied = gpio_get_level(SENT_GPIO_PIR) ? 1 : 0;
    return 0;
}

/* === Sensor Task === */
static void sensor_task(void *arg)
{
    memset(&g_sensors, 0, sizeof(g_sensors));
    g_sensors.room_id = 0; /* Configured via app */
    g_sensors.prev_temp_x10 = 230;
    g_sensors.battery_v = 420;

    while (1) {
        xSemaphoreTake(g_sensor_mutex, portMAX_DELAY);

        /* Read smoke */
        pmsa003_read(&g_sensors.smoke_pm25, &g_sensors.smoke_pm10);

        /* Read CO */
        ze07_read(&g_sensors.co_ppm);

        /* Read temperature + compute rate of rise */
        int16_t new_temp;
        ds18b20_read(&new_temp);
        int16_t rate = (new_temp - g_sensors.prev_temp_x10) * 6; /* ×0.1°C/2s → °C/min */
        g_sensors.temp_rate = (int8_t)(rate / 10);
        g_sensors.temp_c_x10 = new_temp;
        g_sensors.prev_temp_x10 = new_temp;

        /* Read thermal array */
        mlx90640_read(&g_sensors.thermal_max_x10, &g_sensors.thermal_mean_x10,
                       &g_sensors.thermal_anomaly);

        /* ThermalAnomaly check */
        thermal_anomaly_check(g_thermal_frame, &g_sensors.thermal_anomaly);

        /* FlameNet inference */
        flamenet_infer(&g_sensors, &g_sensors.flame_class,
                       &g_sensors.flame_confidence, &g_sensors.flamenet_ms);

        /* PIR occupancy */
        pir_read(&g_sensors.pir_occupant);

        /* Battery voltage */
        /* Production: ADC read */
        g_sensors.battery_v = 420;

        xSemaphoreGive(g_sensor_mutex);

        /* Check for fire */
        if ((g_sensors.flame_class == FS_CLASS_SMOLDERING ||
             g_sensors.flame_class == FS_CLASS_FLAMING_FIRE) &&
            g_sensors.flame_confidence >= FS_FLAMENET_CONF_FIRE_PCT) {

            ESP_LOGW(TAG, "FIRE DETECTED: room=%d class=%d conf=%d%% smoke=%d CO=%d temp=%.1f°C thermal=%.1f°C",
                     g_sensors.room_id, g_sensors.flame_class,
                     g_sensors.flame_confidence, g_sensors.smoke_pm25,
                     g_sensors.co_ppm,
                     g_sensors.temp_c_x10 / 10.0,
                     g_sensors.thermal_max_x10 / 10.0);

            /* Send FIRE_ALERT (emergency priority) */
            fs_message_t msg;
            fs_build_fire_alert(&msg, g_mesh.node_id, g_mesh.msg_counter++,
                               g_sensors.flame_class,
                               g_sensors.flame_confidence,
                               g_sensors.room_id,
                               g_sensors.smoke_pm25,
                               g_sensors.co_ppm,
                               g_sensors.temp_c_x10,
                               g_sensors.thermal_max_x10,
                               g_sensors.pir_occupant);

            fs_mesh_send_emergency(&g_mesh, &g_radio_hal, &msg);
        }

        /* CO alarm */
        if (g_sensors.co_ppm > FS_CO_DANGER_PPM) {
            ESP_LOGW(TAG, "CO DANGER: %d ppm — evacuating!", g_sensors.co_ppm);
            fs_message_t msg;
            fs_build_fire_alert(&msg, g_mesh.node_id, g_mesh.msg_counter++,
                               FS_CLASS_SMOLDERING, 90,
                               g_sensors.room_id,
                               g_sensors.smoke_pm25,
                               g_sensors.co_ppm,
                               g_sensors.temp_c_x10,
                               g_sensors.thermal_max_x10,
                               g_sensors.pir_occupant);
            fs_mesh_send_emergency(&g_mesh, &g_radio_hal, &msg);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); /* 2 Hz sensor cycle */
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENT_TELEM_INTERVAL_MS));

        xSemaphoreTake(g_sensor_mutex, portMAX_DELAY);
        fs_sentinel_telem_t telem;
        memset(&telem, 0, sizeof(telem));
        telem.subtype        = FS_TELEM_SENTINEL;
        telem.battery_v      = g_sensors.battery_v;
        telem.smoke_pm25     = g_sensors.smoke_pm25;
        telem.co_ppm         = g_sensors.co_ppm;
        telem.temp_c_x10     = g_sensors.temp_c_x10;
        telem.temp_rate      = g_sensors.temp_rate;
        telem.thermal_max_x10= g_sensors.thermal_max_x10;
        telem.thermal_mean_x10= g_sensors.thermal_mean_x10;
        telem.flame_class    = g_sensors.flame_class;
        telem.flame_conf     = g_sensors.flame_confidence;
        telem.pir_occupant   = g_sensors.pir_occupant;
        telem.flamenet_ms    = g_sensors.flamenet_ms;
        telem.thermal_anom   = g_sensors.thermal_anomaly;
        telem.free_heap      = (uint16_t)esp_get_free_heap_size();
        telem.rssi           = g_mesh.last_rssi;
        telem.uptime_min     = 0;
        xSemaphoreGive(g_sensor_mutex);

        fs_message_t msg;
        fs_build_sentinel_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
        fs_mesh_send(&g_mesh, &g_radio_hal, &msg, 0);
    }
}

/* === Radio RX Task (receive commands from Hub) === */
static void radio_rx_task(void *arg)
{
    uint8_t rx_buf[FS_MAX_MSG];
    fs_message_t msg;

    while (1) {
        int rx_len = fs_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   4000, &g_mesh.last_rssi);

        if (rx_len > 0 && fs_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case FS_MSG_ALARM_TRIGGER:
                ESP_LOGW(TAG, "ALARM TRIGGERED — sound buzzer + strobe!");
                g_sensors.alarm_active = 1;
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 200);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
                gpio_set_level(SENT_GPIO_STROBE, 1);
                break;

            case FS_MSG_ALARM_STOP:
                ESP_LOGI(TAG, "Alarm stopped");
                g_sensors.alarm_active = 0;
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
                gpio_set_level(SENT_GPIO_STROBE, 0);
                break;

            case FS_MSG_HVAC_SHUTOFF:
                ESP_LOGI(TAG, "HVAC shutoff command received");
                /* Production: relay control or signal to HVAC */
                break;

            case FS_MSG_FIRE_CONFIRM:
                ESP_LOGW(TAG, "Fire confirmed by Hub — alarm active");
                break;

            case FS_MSG_TEST_ALARM:
                ESP_LOGI(TAG, "Monthly test alarm");
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 150);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
                gpio_set_level(SENT_GPIO_STROBE, 1);
                vTaskDelay(pdMS_TO_TICKS(3000));
                ledc_set_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_CHANNEL, LEDC_CHANNEL_0);
                gpio_set_level(SENT_GPIO_STROBE, 0);
                break;

            case FS_MSG_TIME_SYNC:
                /* Update RTC */
                break;
            }
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "FireSync Room Sentinel starting...");

    /* GPIO init */
    gpio_set_direction(SENT_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENT_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENT_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(SENT_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(SENT_GPIO_PIR, GPIO_MODE_INPUT);
    gpio_set_direction(SENT_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENT_GPIO_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENT_GPIO_STROBE, GPIO_MODE_OUTPUT);
    gpio_set_direction(SENT_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(SENT_GPIO_USB_PWR, GPIO_MODE_INPUT);

    gpio_set_level(SENT_GPIO_SX_NSS, 1);
    gpio_set_level(SENT_GPIO_SX_RST, 1);

    /* I²C init (smoke + thermal) */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SENT_GPIO_SMOKE_SDA,
        .scl_io_num = SENT_GPIO_SMOKE_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000, /* 400 kHz for MLX90640 */
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, i2c_cfg.mode, 0, 0, 0);

    /* I²C bus 2 for MLX90640 (separate bus to avoid contention) */
    /* Production: MLX90640 on I²C bus 1 at 400 kHz */
    i2c_config_t i2c2_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SENT_GPIO_THERMAL_SDA,
        .scl_io_num = SENT_GPIO_THERMAL_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_1, &i2c2_cfg);
    i2c_driver_install(I2C_NUM_1, i2c2_cfg.mode, 0, 0, 0);

    /* UART2 for ZE07-CO */
    uart_config_t uart_cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_2, &uart_cfg);
    uart_set_pin(UART_NUM_2, SENT_GPIO_CO_RX, SENT_GPIO_CO_TX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, 256, 0, 0, NULL, 0);

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
        .gpio_num = SENT_GPIO_BUZZER,
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

    /* Mesh init — node ID assigned during join */
    uint8_t aes_key[16] = {0};
    fs_mesh_init(&g_mesh, 0x01, FS_NODE_SENTINEL, aes_key);

    /* Join network */
    g_mesh.battery_v = 420;
    if (fs_mesh_join(&g_mesh, &g_radio_hal) == 0) {
        ESP_LOGI(TAG, "Joined mesh network, TDMA slot %d", g_mesh.tdma_slot);
    } else {
        ESP_LOGW(TAG, "Failed to join mesh — retrying in background");
    }

    g_sensor_mutex = xSemaphoreCreateMutex();

    /* Tasks */
    xTaskCreate(sensor_task, "sensor", 8192, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telem", 4096, NULL, 3, NULL);
    xTaskCreate(radio_rx_task, "radio_rx", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Room Sentinel ready. Room=%d", g_sensors.room_id);
}
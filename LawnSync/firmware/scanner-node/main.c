/*
 * LawnSync — Lawn Scanner Firmware
 * ESP32-S3, FreeRTOS
 *
 * Captures RGB + NIR images for NDVI computation, runs on-device
 * DiseaseNet (15-class) and WeedSeg (9-class) inference via TFLite-Micro,
 * reports results via Sub-GHz mesh, uploads images via Hub/Wi-Fi.
 *
 * Build: idf.py build with ESP-IDF v5.x + esp-tflite-micro component
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_camera.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "LawnSync-Scanner";

/* === Global state === */
static ls_mesh_ctx_t g_mesh;
static QueueHandle_t g_scan_queue;

/* === SPI Interface for SX1262 === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SCAN_GPIO_SX_MOSI,
        .miso_io_num = SCAN_GPIO_SX_MISO,
        .sclk_io_num = SCAN_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = SCAN_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI3_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t a) { gpio_set_level(SCAN_GPIO_SX_RST, a ? 0 : 1); }
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(SCAN_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int e) { (void)e; }

static const ls_spi_interface_t g_spi_iface = {
    .init = spi_init, .cs_select = spi_cs_select, .cs_release = spi_cs_release,
    .transfer = spi_transfer, .reset = spi_reset, .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read, .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === Camera Configuration (OV5640 DVP) === */
static camera_config_t camera_config = {
    .pin_d0 = SCAN_GPIO_CAM_D0,
    .pin_d1 = SCAN_GPIO_CAM_D1,
    .pin_d2 = SCAN_GPIO_CAM_D2,
    .pin_d3 = SCAN_GPIO_CAM_D3,
    .pin_d4 = SCAN_GPIO_CAM_D4,
    .pin_d5 = SCAN_GPIO_CAM_D5,
    .pin_d6 = SCAN_GPIO_CAM_D6,
    .pin_d7 = SCAN_GPIO_CAM_D7,
    .pin_xclk = SCAN_GPIO_CAM_XCLK,
    .pin_pclk = SCAN_GPIO_CAM_PCLK,
    .pin_vsync = SCAN_GPIO_CAM_VSYNC,
    .pin_href = SCAN_GPIO_CAM_HSYNC,
    .pin_sccb_sda = SCAN_GPIO_CAM_SDA,
    .pin_sccb_scl = SCAN_GPIO_CAM_SCL,
    .pin_pwdn = SCAN_GPIO_CAM_PWDN,
    .pin_reset = SCAN_GPIO_CAM_RST,
    .xclk_freq_hz = 20000000,
    .frame_size = FRAMESIZE_UXGA,     /* 1600×1200 for capture */
    .pixel_format = PIXFORMAT_RGB565,
    .fb_count = 2,                     /* Double buffer for continuous capture */
    .fb_location = CAMERA_FB_IN_PSRAM,
};

/* === LED Control === */
static void led_white_on(void) { gpio_set_level(SCAN_GPIO_WHT_LED, 1); }
static void led_white_off(void) { gpio_set_level(SCAN_GPIO_WHT_LED, 0); }
static void led_nir_on(void) { gpio_set_level(SCAN_GPIO_NIR_LED, 1); }
static void led_nir_off(void) { gpio_set_level(SCAN_GPIO_NIR_LED, 0); }

/* === NDVI Computation === */
/* NDVI = (NIR - Red) / (NIR + Red)
 * Red channel from RGB image, NIR from separate NIR image
 */
static void compute_ndvi(uint8_t *rgb_img, uint8_t *nir_img,
                          int width, int height, float *ndvi_map)
{
    for (int i = 0; i < width * height; i++) {
        /* RGB565: extract red (bits 15-11) and scale to 0-255 */
        uint16_t pixel = (rgb_img[i*2] << 8) | rgb_img[i*2+1];
        float red = (float)((pixel >> 11) & 0x1F) / 31.0 * 255.0;

        /* NIR from NIR image (grayscale) */
        float nir = (float)nir_img[i];

        /* NDVI */
        float denom = nir + red;
        if (denom < 1.0)
            ndvi_map[i] = 0.0;
        else
            ndvi_map[i] = (nir - red) / denom;

        /* Clamp to [-1, 1] */
        if (ndvi_map[i] > 1.0) ndvi_map[i] = 1.0;
        if (ndvi_map[i] < -1.0) ndvi_map[i] = -1.0;
    }
}

/* === TFLite-Micro Inference (stubs) === */
/* In production, include "tensorflow/lite/micro/micro_interpreter.h"
 * and load DiseaseNet int8 model from flash partition.
 */

static const char *disease_names[15] = {
    "Healthy", "Brown Patch", "Dollar Spot", "Rust", "Fairy Ring",
    "Snow Mold", "Pythium Blight", "Necrotic Ring Spot", "Summer Patch",
    "Powdery Mildew", "Slime Mold", "Dog Spot", "Grub Damage",
    "Chinch Bug", "Sod Webworm"
};

typedef struct {
    uint8_t disease_class;    /* 0-14 */
    float confidence;          /* 0.0-1.0 */
    float avg_ndvi;            /* average NDVI of scan area */
    uint8_t weed_coverage;    /* % weed coverage (0-100) */
    uint8_t dominant_weed;    /* 0-8 weed class */
} scan_result_t;

static scan_result_t run_inference(camera_fb_t *rgb_fb, float *ndvi_map,
                                     int width, int height)
{
    scan_result_t result = {0};

    /* In production:
     * 1. Resize RGB image to 224×224
     * 2. Run DiseaseNet TFLite-Micro interpreter
     * 3. Run WeedSeg U-Net-tiny on 512×512 downsampled image
     * 4. Calculate average NDVI from ndvi_map
     * 5. Calculate weed coverage from segmentation mask
     */

    /* Placeholder: simulate inference result */
    result.disease_class = 0; /* Healthy */
    result.confidence = 0.92;
    result.avg_ndvi = 0.0;
    result.weed_coverage = 3; /* 3% weeds */
    result.dominant_weed = 2; /* Clover */

    /* Calculate average NDVI */
    if (ndvi_map) {
        float sum = 0;
        int count = width * height;
        for (int i = 0; i < count && i < 64*64; i++)
            sum += ndvi_map[i];
        result.avg_ndvi = sum / (64*64);
    }

    return result;
}

/* === GPS Reading (NEO-M9N via UART) === */
static void gps_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &cfg);
    uart_set_pin(UART_NUM_1, SCAN_GPIO_GPS_RX, SCAN_GPIO_GPS_TX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
}

static void gps_read(double *lat, double *lon)
{
    uint8_t data[128];
    int len = uart_read_bytes(UART_NUM_1, data, sizeof(data), pdMS_TO_TICKS(1000));
    /* Parse NMEA sentences ($GPGGA, $GPRMC)
     * In production: parse $GPGGA for lat/lon
     */
    *lat = 0.0;
    *lon = 0.0;
    (void)len;
}

/* === Scan Task === */
static void scan_task(void *arg)
{
    /* Initialize camera */
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    /* Initialize mesh */
    ls_radio_config_t radio_cfg = {
        .frequency = LS_NET_FREQ_HZ,
        .bandwidth = LS_NET_BW_HZ,
        .spreading_factor = LS_NET_SF,
        .coding_rate = LS_NET_CR,
        .preamble_len = LS_NET_PREAMBLE,
        .tx_power_dbm = LS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };
    ls_mesh_init(&g_mesh, LS_NODE_SCANNER, &g_spi_iface, &radio_cfg);
    ls_mesh_join(&g_mesh);

    ESP_LOGI(TAG, "Scanner node joined: id=%d slot=%d",
             g_mesh.node_id, g_mesh.tdma_slot);

    while (1) {
        /* Wait for scan command from hub or timer trigger */
        ls_message_t cmd;
        bool do_scan = false;

        if (ls_mesh_recv(&g_mesh, &cmd, 5000) == 0) {
            if (cmd.header.type == LS_MSG_COMMAND &&
                cmd.payload[0] == LS_CMD_SCAN_CAPTURE) {
                do_scan = true;
                ESP_LOGI(TAG, "Scan command received");
            }
        }

        /* Also trigger on daily timer (simplified: every iteration) */
        if (!do_scan) {
            static uint32_t scan_counter = 0;
            if (++scan_counter >= 17280) { /* ~24h at 5s loop */
                do_scan = true;
                scan_counter = 0;
            }
        }

        if (!do_scan) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Starting scan sequence...");

        /* Get GPS position */
        double lat, lon;
        gps_read(&lat, lon);

        /* === Capture RGB image === */
        led_white_on();
        vTaskDelay(pdMS_TO_TICKS(200)); /* Stabilize */
        camera_fb_t *rgb_fb = esp_camera_fb_get();
        led_white_off();

        if (!rgb_fb) {
            ESP_LOGE(TAG, "RGB capture failed");
            continue;
        }
        ESP_LOGI(TAG, "RGB captured: %dx%d, %d bytes",
                 rgb_fb->width, rgb_fb->height, rgb_fb->len);

        /* === Capture NIR image === */
        led_nir_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        camera_fb_t *nir_fb = esp_camera_fb_get();
        led_nir_off();

        if (!nir_fb) {
            ESP_LOGE(TAG, "NIR capture failed");
            esp_camera_fb_return(rgb_fb);
            continue;
        }
        ESP_LOGI(TAG, "NIR captured: %dx%d, %d bytes",
                 nir_fb->width, nir_fb->height, nir_fb->len);

        /* === Compute NDVI === */
        int w = (rgb_fb->width < 64) ? rgb_fb->width : 64;
        int h = (rgb_fb->height < 64) ? rgb_fb->height : 64;
        float ndvi_map[64 * 64];
        compute_ndvi(rgb_fb->buf, nir_fb->buf, w, h, ndvi_map);

        /* === Run ML inference === */
        scan_result_t result = run_inference(rgb_fb, ndvi_map, w, h);

        ESP_LOGI(TAG, "Scan result: disease=%s (%.0f%% conf), NDVI=%.2f, "
                 "weeds=%d%% (%s)",
                 disease_names[result.disease_class],
                 result.confidence * 100, result.avg_ndvi,
                 result.weed_coverage,
                 result.dominant_weed < 9 ? disease_names[result.dominant_weed] : "N/A");

        /* === Send results via mesh === */
        ls_message_t scan_msg;
        memset(&scan_msg, 0, sizeof(scan_msg));
        scan_msg.header.sync[0] = LS_SYNC0;
        scan_msg.header.sync[1] = LS_SYNC1;
        scan_msg.header.src = g_mesh.node_id;
        scan_msg.header.dst = LS_HUB_NODE_ID;
        scan_msg.header.type = LS_MSG_SCAN_RESULT;
        scan_msg.header.msg_id = g_mesh.msg_seq++;

        /* Payload: disease_class(1) + confidence(1, ×0.01) +
         *          avg_ndvi(2, ×100 signed) + weed_coverage(1) +
         *          dominant_weed(1) + lat(4) + lon(4) */
        scan_msg.payload[0] = result.disease_class;
        scan_msg.payload[1] = (uint8_t)(result.confidence * 100);
        int16_t ndvi_i = (int16_t)(result.avg_ndvi * 100);
        scan_msg.payload[2] = (uint8_t)(ndvi_i & 0xFF);
        scan_msg.payload[3] = (uint8_t)(ndvi_i >> 8);
        scan_msg.payload[4] = result.weed_coverage;
        scan_msg.payload[5] = result.dominant_weed;
        /* GPS: simplified to 4 bytes each (scaled) */
        int32_t lat_i = (int32_t)(lat * 1e6);
        int32_t lon_i = (int32_t)(lon * 1e6);
        memcpy(&scan_msg.payload[6], &lat_i, 4);
        memcpy(&scan_msg.payload[10], &lon_i, 4);
        scan_msg.payload_len = 14;

        ls_mesh_send(&g_mesh, &scan_msg);

        /* === Free camera buffers === */
        esp_camera_fb_return(rgb_fb);
        esp_camera_fb_return(nir_fb);

        /* In production: store images to SD/SPIFFS for cloud upload via Wi-Fi */
        ESP_LOGI(TAG, "Scan complete");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "LawnSync Scanner starting...");

    /* Initialize GPIOs */
    gpio_set_direction(SCAN_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(SCAN_GPIO_SX_RST, 1);
    gpio_set_direction(SCAN_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(SCAN_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCAN_GPIO_NIR_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(SCAN_GPIO_WHT_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(SCAN_GPIO_NIR_LED, 0);
    gpio_set_level(SCAN_GPIO_WHT_LED, 0);

    /* Initialize SPI */
    spi_init();

    /* Initialize GPS */
    gps_init();

    /* Create scan queue and task */
    g_scan_queue = xQueueCreate(4, sizeof(ls_message_t));
    xTaskCreate(scan_task, "scan", 16384, NULL, 5, NULL);

    ESP_LOGI(TAG, "Scanner running");
}
/*
 * MenoSync — Climate Node Firmware
 * ESP32-C3, ESP-IDF v5.x / FreeRTOS
 *
 * The Climate Node is a room-mounted environmental sensor + actuator
 * that monitors ambient conditions and controls HVAC + smart shades for
 * pre-emptive cooling during menopause hot flashes.
 *   - BME280: ambient temperature ±1°C, humidity ±3% RH, pressure
 *   - MLX90640: 32×24 thermal IR array for radiant temperature mapping
 *     (detects uneven cooling, drafts, sunlight hotspots)
 *   - 2× relay outputs: HVAC control + smart shade/curtain motor control
 *   - RFM69HCW 868 MHz: Sub-GHz TDMA mesh to Hub
 *   - USB-C 5V or solar powered
 *
 * Build: idf.py build with ESP-IDF v5.x (ESP32-C3)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "../common/protocol.h"
#include "../common/config.h"

static const char *TAG = "MenoSync-Climate";

/* === Global state === */
static uint8_t g_node_id = 0x10;  /* Climate Node ID (0x10+ for climate nodes) */
static uint8_t g_seq = 0;

/* Latest sensor readings */
static int16_t  g_ambient_temp_cd = 2300;   /* 23.0°C */
static uint16_t g_humidity_pct = 45;
static uint16_t g_pressure_hpa = 1013;
static int16_t  g_radiant_temp_cd = 2400;   /* 24.0°C radiant temp */
static uint8_t  g_hvac_state = 0;           /* 0=off, 1=cooling, 2=heating, 3=fan */
static uint8_t  g_shade_pct = 0;            /* 0=open, 100=closed */

/* MLX90640 32×24 thermal image (768 pixels, centi-degrees) */
#define MLX_WIDTH  32
#define MLX_HEIGHT 24
#define MLX_PIXELS (MLX_WIDTH * MLX_HEIGHT)
static int16_t g_thermal_image[MLX_PIXELS];

/* Cooling command state */
static uint8_t  g_cooling_active = 0;
static int16_t  g_target_temp_cd = 2200;    /* 22.0°C default cooling target */

/* === I²C init (ESP32-C3) === */
static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CN_GPIO_I2C_SDA,
        .scl_io_num = CN_GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
        .clk_flags = 0,
    };
    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === BME280 read (ambient temp/humidity/pressure) === */
static void read_bme280(void)
{
    /* In production: read BME280 registers via I²C at 0x76
     * Temp: registers 0xFA-0xFB (raw → centi-degrees)
     * Humidity: registers 0xFD-0xFE (raw → %RH)
     * Pressure: registers 0xF7-0xF9 (raw → hPa)
     * Apply BME280 compensation algorithm.
     */
    /* Simulated with realistic room values */
    g_ambient_temp_cd = 2200 + (esp_timer_get_time() / 1000000 % 400);
    g_humidity_pct = 40 + (esp_timer_get_time() / 1000000 % 15);
    g_pressure_hpa = 1010 + (esp_timer_get_time() / 1000000 % 6);
}

/* === MLX90640 read (32×24 thermal IR array) === */
static void read_mlx90640(void)
{
    /* In production: read MLX90640 via I²C at 0x33
     * Read 832 words (0x0400-0x07FF), extract 768 pixel temps,
     * apply calibration. Temperature range: -40 to 300°C.
     * Refresh rate: 0.5-8 Hz (we use 0.5 Hz = every 2s, but report 0.02 Hz).
     *
     * For this stub, simulate a thermal image:
     * - Base: ambient temp
     * - Add gradient (ceiling warmer than floor)
     * - Add hotspot if sunlight (shade open) or HVAC running
     */
    int16_t base = g_ambient_temp_cd;

    for (int y = 0; y < MLX_HEIGHT; y++) {
        for (int x = 0; x < MLX_WIDTH; x++) {
            /* Vertical gradient: top of room slightly warmer */
            int16_t temp = base + (int16_t)((MLX_HEIGHT - y) * 2);

            /* Sunlight hotspot on one side if shade is open */
            if (g_shade_pct < 50 && x > MLX_WIDTH * 3 / 4) {
                temp += 50 + (int16_t)(50 * (1.0f - (float)g_shade_pct / 100.0f));
            }

            /* HVAC cooling effect near vent (top corner) */
            if (g_hvac_state == 1 && y < 6 && x < 8) {
                temp -= 80;
            }

            g_thermal_image[y * MLX_WIDTH + x] = temp;
        }
    }

    /* Calculate average radiant temp from full image */
    int32_t sum = 0;
    for (int i = 0; i < MLX_PIXELS; i++) {
        sum += g_thermal_image[i];
    }
    g_radiant_temp_cd = (int16_t)(sum / MLX_PIXELS);
}

/* === RFM69HCW Sub-GHz communication === */
/* In production: use SPI to communicate with RFM69HCW at 868 MHz.
 * The Hub sends cooling commands via Sub-GHz; this node receives them.
 * TDMA: each climate node has a time slot in the 8-second superframe.
 */
static void rfm69_init(void)
{
    /* In production: initialize RFM69HCW via SPI
     * - Set frequency to 868 MHz
     * - Set TX power to +20 dBm
     * - Configure packet format with AES-128 encryption
     * - Set node address (g_node_id)
     * - Configure TDMA slot timing
     */
    ESP_LOGI(TAG, "RFM69HCW initialized at 868 MHz (node ID 0x%02X)", g_node_id);
}

static int rfm69_receive(uint8_t *buf, size_t buf_len, uint32_t timeout_ms)
{
    /* In production: poll SPI for received packet, check CRC,
     * decrypt AES-128, return payload.
     * For this stub, return -1 (no packet) most of the time.
     */
    return -1;
}

static int rfm69_send(uint8_t *data, size_t len)
{
    /* In production: load packet into RFM69 FIFO, transmit,
     * wait for ACK from Hub.
     */
    return 0;
}

/* === Control HVAC relay === */
static void set_hvac(uint8_t mode)
{
    g_hvac_state = mode;
    /* In production: drive CN_GPIO_RELAY_HVAC */
    const char *mode_str[] = {"OFF", "COOLING", "HEATING", "FAN"};
    ESP_LOGI(TAG, "HVAC → %s", mode_str[mode < 4 ? mode : 0]);
}

/* === Control smart shade relay === */
static void set_shade(uint8_t pct)
{
    g_shade_pct = pct > 100 ? 100 : pct;
    /* In production: drive CN_GPIO_RELAY_SHADE with PWM or time-based control
     * for shade motor. 0% = fully open, 100% = fully closed.
     */
    ESP_LOGI(TAG, "Shade → %d%% closed", g_shade_pct);
}

/* === Process cooling command from Hub === */
static void process_cooling_cmd(const ms_cooling_cmd_t *cmd)
{
    ESP_LOGI(TAG, "Cooling command received: action=%d target=%.1f°C HVAC=%d shade=%d%%",
             cmd->action, cmd->target_temp_cd / 100.0f, cmd->hvac_mode, cmd->shade_pct);

    g_target_temp_cd = cmd->target_temp_cd;

    switch (cmd->action) {
        case 0:  /* Stop cooling */
            set_hvac(0);
            set_shade(0);
            g_cooling_active = 0;
            break;
        case 1:  /* Start pre-emptive cooling */
            set_hvac(cmd->hvac_mode);
            set_shade(cmd->shade_pct);
            g_cooling_active = 1;
            ESP_LOGI(TAG, "Pre-emptive cooling activated — target %.1f°C",
                     cmd->target_temp_cd / 100.0f);
            break;
        case 2:  /* Set HVAC temperature */
            set_hvac(cmd->hvac_mode);
            break;
        case 3:  /* Set shade position */
            set_shade(cmd->shade_pct);
            break;
    }
}

/* === Send ambient data via Sub-GHz to Hub === */
static void send_ambient_subghz(void)
{
    ms_ambient_t ambient = {
        .ambient_temp_cd = g_ambient_temp_cd,
        .humidity_pct = g_humidity_pct,
        .pressure_hpa = g_pressure_hpa,
        .radiant_temp_cd = g_radiant_temp_cd,
        .hvac_state = g_hvac_state,
        .shade_pct = g_shade_pct,
    };

    uint8_t msg[MS_MAX_MSG];
    size_t len = ms_encode(msg, sizeof(msg),
                           g_node_id, 0x01,
                           MS_MSG_AMBIENT_DATA, MS_TELEM_CLIMATE,
                           g_seq++, (uint8_t *)&ambient, sizeof(ambient));
    if (len > 0) {
        rfm69_send(msg, len);
        ESP_LOGI(TAG, "Sub-GHz → Hub: ambient=%.1f°C RH=%d%% radiant=%.1f°C HVAC=%d shade=%d%%",
                 g_ambient_temp_cd / 100.0f, g_humidity_pct,
                 g_radiant_temp_cd / 100.0f, g_hvac_state, g_shade_pct);
    }
}

/* === Ambient Monitoring Task (0.1 Hz = every 10s) === */
static void ambient_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        read_bme280();
        send_ambient_subghz();
    }
}

/* === Radiant Temperature Task (0.02 Hz = every 50s) === */
static void radiant_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50000));
        read_mlx90640();
        ESP_LOGI(TAG, "MLX90640: radiant avg=%.1f°C", g_radiant_temp_cd / 100.0f);
    }
}

/* === Sub-GHz Receiver Task === */
static void subghz_rx_task(void *arg)
{
    uint8_t rx_buf[MS_MAX_MSG];

    while (1) {
        int len = rfm69_receive(rx_buf, sizeof(rx_buf), 1000);
        if (len > 0) {
            ms_msg_header_t hdr;
            uint8_t payload[MS_MAX_PAYLOAD];

            int payload_len = ms_decode(rx_buf, len, &hdr, payload, sizeof(payload));
            if (payload_len > 0) {
                if (hdr.msg_type == MS_MSG_COOLING_CMD) {
                    if (payload_len >= (int)sizeof(ms_cooling_cmd_t)) {
                        process_cooling_cmd((ms_cooling_cmd_t *)payload);
                    }
                } else if (hdr.msg_type == MS_MSG_COMMAND) {
                    ESP_LOGI(TAG, "Command received: subtype=%d", hdr.subtype);
                    if (hdr.subtype == MS_CMD_REBOOT) {
                        esp_restart();
                    }
                }
            }
        }
    }
}

/* === Heartbeat Task (every 30s) === */
static void heartbeat_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MS_HEARTBEAT_INTERVAL_S * 1000));
        uint8_t msg[MS_MAX_MSG];
        uint8_t payload[4] = {g_hvac_state, g_shade_pct, 100, 0};  /* HVAC, shade, batt, rsv */
        size_t len = ms_encode(msg, sizeof(msg),
                               g_node_id, 0x01,
                               MS_MSG_HEARTBEAT, MS_TELEM_CLIMATE,
                               g_seq++, payload, 4);
        if (len > 0) {
            rfm69_send(msg, len);
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "MenoSync Climate Node starting (node ID 0x%02X)", g_node_id);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    init_i2c();

    /* Initialize GPIO for relays */
    gpio_set_direction(CN_GPIO_RELAY_HVAC, GPIO_MODE_OUTPUT);
    gpio_set_direction(CN_GPIO_RELAY_SHADE, GPIO_MODE_OUTPUT);
    gpio_set_direction(CN_GPIO_LED, GPIO_MODE_OUTPUT);

    /* Initialize RFM69HCW Sub-GHz radio */
    rfm69_init();

    /* Initial sensor readings */
    read_bme280();
    read_mlx90640();
    ESP_LOGI(TAG, "Initial: ambient=%.1f°C RH=%d%% radiant=%.1f°C",
             g_ambient_temp_cd / 100.0f, g_humidity_pct,
             g_radiant_temp_cd / 100.0f);

    /* Create tasks */
    xTaskCreate(ambient_task, "ambient", 2048, NULL, 5, NULL);
    xTaskCreate(radiant_task, "radiant", 4096, NULL, 4, NULL);
    xTaskCreate(subghz_rx_task, "subghz_rx", 4096, NULL, 6, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Climate Node ready — monitoring room + listening for cooling commands");
}
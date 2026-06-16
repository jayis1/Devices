/**
 * FreshKeep — Pantry Node Firmware (ESP32-S3)
 * 
 * Pantry/cabinet monitoring: barcode scanner, camera, weight shelves,
 * temp/humidity sensors. Tracks inventory, detects low stock, and
 * manages the lazy susan for full camera coverage.
 * 
 * TDMA mesh: transmits in Slot 2 (100ms window every 500ms)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi.h"
#include "driver/uart.h"
#include "driver/adc.h"
#include "esp_wifi.h"
#include "esp_camera.h"

static const char *TAG = "PantryNode";

/* ── Pin Definitions (ESP32-S3) ────────────────────────────────────── */
#define PIN_RADIO_SDA    0
#define PIN_RADIO_SCL    1
#define PIN_BARCODE_TX   19
#define PIN_BARCODE_RX   20
#define PIN_SERVO_PWM    21
#define PIN_HX711_CLK_0 35
#define PIN_HX711_DAT_0 41
#define PIN_HX711_CLK_1 36
#define PIN_HX711_DAT_1 42
#define PIN_HX711_CLK_2 37
#define PIN_HX711_DAT_2 43
#define PIN_HX711_CLK_3 38
#define PIN_HX711_DAT_3 44
#define PIN_HX711_CLK_4 39
#define PIN_HX711_DAT_4 45
#define PIN_HX711_CLK_5 40
#define PIN_HX711_DAT_5 46
#define PIN_OLED_SDA     47
#define PIN_OLED_SCL     48
#define PIN_LIGHT_INT    16
#define PIN_LED_STRIP    38
#define PIN_BUZZER       39

/* ── Barcode Scanner ───────────────────────────────────────────────── */
#define BARCODE_UART_NUM   UART_NUM_1
#define BARCODE_BUF_SIZE   256

/* ── Constants ──────────────────────────────────────────────────────── */
#define NUM_SHELVES         6
#define LOW_STOCK_THRESHOLD_G 100.0f  /* Alert if shelf weight < 100g */
#define DOOR_OPEN_DEBOUNCE_MS 500
#define CAMERA_CAPTURES     3        /* Photos per door-open event */
#define SCAN_ROTATION_DEG   120      /* Lazy susan rotation per photo */
#define MAX_BARCODE_LEN     32

/* ── Pantry State ───────────────────────────────────────────────────── */
typedef struct {
    /* Shelf weights */
    float    shelf_weight_kg[NUM_SHELVES];
    float    shelf_weight_baseline_kg[NUM_SHELVES]; /* Calibrated empty weights */
    
    /* Temp/humidity per shelf */
    float    shelf_temp_c[NUM_SHELVES];
    float    shelf_humidity_pct[NUM_SHELVES];
    
    /* Door state */
    uint8_t  door_open;
    uint32_t door_open_count;
    
    /* Barcode */
    char     last_barcode[MAX_BARCODE_LEN];
    uint8_t  barcode_ready;
    uint32_t last_barcode_time_ms;
    
    /* Inventory */
    uint8_t  items_count;           /* Estimated total items */
    uint8_t  low_stock_shelves;      /* Bitmask of shelves below threshold */
    
    /* Camera */
    uint8_t  image_ready;
    
    /* Battery */
    uint8_t  battery_pct;
    
    /* Radio */
    uint32_t last_tx_ms;
} pantry_state_t;

static pantry_state_t g_pantry;
static QueueHandle_t g_barcode_queue;

/* ── HX711 Load Cell Reader ─────────────────────────────────────────── */
static int32_t hx711_read(int clk_pin, int dat_pin) {
    int32_t value = 0;
    
    /* Wait for data ready (DOUT goes low) */
    int timeout = 1000;
    while (gpio_get_level(dat_pin) && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (timeout <= 0) return -1; /* Timeout */
    
    /* Pulse CLK 24 times to read 24 bits */
    for (int i = 0; i < 24; i++) {
        gpio_set_level(clk_pin, 1);
        ets_delay_us(1);
        value = (value << 1) | gpio_get_level(dat_pin);
        gpio_set_level(clk_pin, 0);
        ets_delay_us(1);
    }
    
    /* 25th pulse for channel A, gain 128 */
    gpio_set_level(clk_pin, 1);
    ets_delay_us(1);
    gpio_set_level(clk_pin, 0);
    ets_delay_us(1);
    
    return value;
}

static float hx711_to_grams(int32_t raw, float calibration_factor) {
    if (raw < 0) return 0.0f;
    /* Convert raw HX711 reading to grams */
    /* calibration_factor determined during calibration */
    return (float)raw / calibration_factor;
}

/* ── Barcode Scanner Task ───────────────────────────────────────────── */
static void barcode_task(void *pvParameters) {
    uint8_t data[BARCODE_BUF_SIZE];
    int len = 0;
    
    while (1) {
        /* Read from barcode scanner UART */
        int received = uart_read_bytes(BARCODE_UART_NUM, data + len, 
                                        BARCODE_BUF_SIZE - len - 1, 
                                        pdMS_TO_TICKS(100));
        if (received > 0) {
            len += received;
            
            /* Check for complete barcode (ends with \r or \n) */
            for (int i = 0; i < len; i++) {
                if (data[i] == '\r' || data[i] == '\n') {
                    data[i] = '\0';
                    
                    /* Valid barcode received */
                    if (strlen((char*)data) > 0) {
                        strncpy(g_pantry.last_barcode, (char*)data, MAX_BARCODE_LEN - 1);
                        g_pantry.last_barcode[MAX_BARCODE_LEN - 1] = '\0';
                        g_pantry.barcode_ready = 1;
                        g_pantry.last_barcode_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        
                        ESP_LOGI(TAG, "Barcode scanned: %s", g_pantry.last_barcode);
                        
                        /* Beep confirmation */
                        gpio_set_level(PIN_BUZZER, 1);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        gpio_set_level(PIN_BUZZER, 0);
                    }
                    
                    /* Reset buffer */
                    len = 0;
                    break;
                }
            }
        }
    }
}

/* ── Lazy Susan Rotation ────────────────────────────────────────────── */
static void rotate_lazy_susan(float degrees) {
    /* MG90S servo: 50Hz PWM, 0.5-2.5ms pulse = 0-180° */
    /* Convert degrees to pulse width */
    uint32_t pulse_us = (uint32_t)(500 + (degrees / 180.0f) * 2000);
    
    /* Send PWM pulses (simplified — would use LEDC in production) */
    for (int i = 0; i < 50; i++) { /* 50 pulses = 1 second at 50Hz */
        gpio_set_level(PIN_SERVO_PWM, 1);
        ets_delay_us(pulse_us);
        gpio_set_level(PIN_SERVO_PWM, 0);
        ets_delay_us(20000 - pulse_us);
    }
}

static void scan_pantry_shelves(void) {
    /* Capture images from multiple angles by rotating lazy susan */
    for (int angle = 0; angle < 360; angle += SCAN_ROTATION_DEG) {
        /* Rotate to next position */
        rotate_lazy_susan(angle);
        vTaskDelay(pdMS_TO_TICKS(500)); /* Wait for rotation to settle */
        
        /* Capture image */
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            /* Process image (would send to hub for cloud inference) */
            ESP_LOGI(TAG, "Captured pantry image: %d bytes at %d°", fb->len, angle);
            esp_camera_fb_return(fb);
            g_pantry.image_ready = 1;
        }
    }
    
    /* Return to home position */
    rotate_lazy_susan(0);
}

/* ── SHT40 Temp/Humidity Reader ──────────────────────────────────────── */
static esp_err_t sht40_read(int i2c_port, uint8_t addr, float *temp, float *humidity) {
    uint8_t cmd[2] = {0xFD, 0x00}; /* High precision measurement */
    uint8_t data[6];
    
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, addr << 1, true);
    i2c_master_write(cmd_handle, cmd, 2, true);
    i2c_master_stop(cmd_handle);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);
    
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (addr << 1) | 1, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);
    ret = i2c_master_cmd_begin(i2c_port, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);
    
    if (ret != ESP_OK) return ret;
    
    uint16_t temp_raw = (data[0] << 8) | data[1];
    uint16_t humid_raw = (data[3] << 8) | data[4];
    
    *temp = 175.0f * temp_raw / 65535.0f - 45.0f;
    *humidity = 100.0f * humid_raw / 65535.0f;
    
    return ESP_OK;
}

/* ── Weight Monitoring ──────────────────────────────────────────────── */
static void read_all_weights(void) {
    /* HX711 calibration factors (determined during initial calibration) */
    static const float cal_factors[NUM_SHELVES] = {
        420.0f, 420.0f, 420.0f, 420.0f, 420.0f, 420.0f
    };
    
    int clk_pins[NUM_SHELVES] = {
        PIN_HX711_CLK_0, PIN_HX711_CLK_1, PIN_HX711_CLK_2,
        PIN_HX711_CLK_3, PIN_HX711_CLK_4, PIN_HX711_CLK_5
    };
    int dat_pins[NUM_SHELVES] = {
        PIN_HX711_DAT_0, PIN_HX711_DAT_1, PIN_HX711_DAT_2,
        PIN_HX711_DAT_3, PIN_HX711_DAT_4, PIN_HX711_DAT_5
    };
    
    g_pantry.low_stock_shelves = 0;
    g_pantry.items_count = 0;
    
    for (int i = 0; i < NUM_SHELVES; i++) {
        int32_t raw = hx711_read(clk_pins[i], dat_pins[i]);
        if (raw > 0) {
            float grams = hx711_to_grams(raw, cal_factors[i]);
            g_pantry.shelf_weight_kg[i] = grams / 1000.0f;
            
            /* Net weight = current - baseline (empty shelf) */
            float net_weight = g_pantry.shelf_weight_kg[i] - g_pantry.shelf_weight_baseline_kg[i];
            if (net_weight < 0) net_weight = 0;
            
            /* Low stock detection */
            if (net_weight < LOW_STOCK_THRESHOLD_G / 1000.0f && net_weight > 0) {
                g_pantry.low_stock_shelves |= (1 << i);
            }
            
            /* Rough item count estimate (assume 200g per item on average) */
            g_pantry.items_count += (uint8_t)(net_weight * 1000.0f / 200.0f);
        }
    }
}

/* ── Door Open Detection ────────────────────────────────────────────── */
static void check_door_state(void) {
    static uint8_t prev_door = 0;
    
    /* Use light sensor (TSL2591) — high lux = door open */
    /* Simplified: assume we read lux from sensor */
    float lux = 0; /* tsl2591_read_lux(); */
    uint8_t door_open = (lux > 50.0f) ? 1 : 0;
    
    if (door_open && !prev_door) {
        /* Door just opened */
        g_pantry.door_open_count++;
        
        /* Turn on shelf lighting */
        gpio_set_level(PIN_LED_STRIP, 1);
        
        /* Capture images */
        scan_pantry_shelves();
        
        /* Read weights after door opens (items may have been added/removed) */
        read_all_weights();
    } else if (!door_open && prev_door) {
        /* Door just closed */
        gpio_set_level(PIN_LED_STRIP, 0);
        
        /* Read weights again (user may have moved items) */
        read_all_weights();
    }
    
    g_pantry.door_open = door_open;
    prev_door = door_open;
}

/* ── Main Task ───────────────────────────────────────────────────────── */
static void pantry_main_task(void *pvParameters) {
    ESP_LOGI(TAG, "FreshKeep Pantry Node starting...");
    
    memset(&g_pantry, 0, sizeof(g_pantry));
    
    /* Initialize radio */
    radio_handle_t radio = {
        .config = RADIO_CONFIG_DEFAULT,
        .pin_nss = 17,
        .pin_busy = 14,
        .pin_irq = 15,
        .pin_reset = 16,
    };
    radio_init(&radio, &RADIO_CONFIG_DEFAULT);
    
    /* Calibrate weight baselines (empty shelf weights) */
    ESP_LOGI(TAG, "Calibrating weight baselines...");
    read_all_weights();
    memcpy(g_pantry.shelf_weight_baseline_kg, g_pantry.shelf_weight_kg, 
           sizeof(g_pantry.shelf_weight_baseline_kg));
    
    /* Start barcode scanner task */
    g_barcode_queue = xQueueCreate(10, MAX_BARCODE_LEN);
    xTaskCreate(barcode_task, "barcode", 4096, NULL, 5, NULL);
    
    uint32_t last_weight_read = 0;
    uint32_t last_temp_read = 0;
    uint32_t last_tx = 0;
    
    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        /* Read weights every 30 seconds */
        if (now - last_weight_read > 30000) {
            read_all_weights();
            last_weight_read = now;
        }
        
        /* Read temp/humidity every 60 seconds */
        if (now - last_temp_read > 60000) {
            for (int i = 0; i < NUM_SHELVES; i++) {
                sht40_read(0, 0x44 + i, 
                          &g_pantry.shelf_temp_c[i],
                          &g_pantry.shelf_humidity_pct[i]);
            }
            last_temp_read = now;
        }
        
        /* Check door state continuously */
        check_door_state();
        
        /* TDMA Slot 2: Transmit pantry data */
        uint32_t slot_time = now % 500; /* 500ms frame */
        if (slot_time >= 200 && slot_time < 300 && (now - last_tx > 400)) {
            pantry_data_t data = {
                .temp_c_x10 = (uint16_t)(g_pantry.shelf_temp_c[0] * 10),
                .humidity_x10 = (uint16_t)(g_pantry.shelf_humidity_pct[0] * 10),
                .door_state = g_pantry.door_open,
                .barcode_ready = g_pantry.barcode_ready,
                .image_ready = g_pantry.image_ready,
                .items_count = g_pantry.items_count,
                .battery_pct = 100, /* Mains powered */
            };
            for (int i = 0; i < 6; i++) {
                data.weight_mg[i] = (uint32_t)(g_pantry.shelf_weight_kg[i] * 1000000);
            }
            
            packet_t pkt;
            pkt_init(&pkt, ADDR_PANTRY, ADDR_HUB, PKT_PANTRY_DATA);
            pkt_add_payload(&pkt, (uint8_t*)&data, sizeof(data));
            pkt_finalize(&pkt);
            radio_send(&radio, pkt.data, pkt.len);
            
            /* If barcode was scanned, also send inventory update */
            if (g_pantry.barcode_ready) {
                inventory_update_t inv = {
                    .action = 0, /* Added */
                    .location = 1, /* Pantry */
                    .barcode = atoi(g_pantry.last_barcode),
                    .name_len = 0,
                    .weight_mg = (uint16_t)(g_pantry.shelf_weight_kg[0] * 1000),
                    .expiry_days = 30, /* Default, will be updated from barcode DB */
                    .category = 5, /* Other */
                };
                strncpy(inv.name, g_pantry.last_barcode, 16);
                inv.name_len = strlen(inv.name);
                
                pkt_init(&pkt, ADDR_PANTRY, ADDR_HUB, PKT_INVENTORY_UPDATE);
                pkt_add_payload(&pkt, (uint8_t*)&inv, sizeof(inv));
                pkt_finalize(&pkt);
                radio_send(&radio, pkt.data, pkt.len);
                
                g_pantry.barcode_ready = 0;
            }
            
            last_tx = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "FreshKeep Pantry Node v1.0");
    xTaskCreate(pantry_main_task, "pantry_main", 8192, NULL, 10, NULL);
}
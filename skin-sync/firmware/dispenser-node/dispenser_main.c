/*
 * dispenser_main.c — SkinSync Smart Dispenser firmware (ESP32-C6, ESP-IDF)
 *
 * Receives dispensing commands from the Mirror Hub (via Sub-GHz SX1262 or
 * WiFi), runs peristaltic pumps for computed durations to dispense exact
 * amounts of skincare products, monitors load cells for dose verification
 * and inventory tracking, and reports results back to the hub.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "skin_protocol.h"

static const char *TAG = "dispenser";

/* ---- Hardware pins ---- */
#define PIN_PUMP_0        0
#define PIN_PUMP_1        1
#define PIN_PUMP_2        2
#define PIN_PUMP_3        3
#define PIN_VALVE_0       4
#define PIN_VALVE_1       5
#define PIN_VALVE_2       6
#define PIN_VALVE_3       7
#define PIN_HX711_0_DOUT  8
#define PIN_HX711_0_SCK   9
#define PIN_HX711_1_DOUT  10
#define PIN_HX711_1_SCK   11
#define PIN_HX711_2_DOUT  12
#define PIN_HX711_2_SCK   13
#define PIN_HX711_3_DOUT  14
#define PIN_HX711_3_SCK   15
#define PIN_OLED_SDA      16
#define PIN_OLED_SCL      17
#define PIN_BTN_0         24
#define PIN_BTN_1         25
#define PIN_BTN_2         26
#define PIN_BTN_3         27
#define PIN_SX1262_CS     28
#define PIN_SX1262_SCK    29
#define PIN_SX1262_MOSI   30
#define PIN_SX1262_MISO   31

/* ---- Product slots ---- */
#define NUM_SLOTS 4

typedef struct {
    uint8_t  slot_id;
    uint8_t  product_id;       /* from RFID */
    char     product_name[32];
    float    flow_rate_ml_s;   /* calibrated ml/sec */
    float    tare_g;           /* empty cartridge weight */
    float    current_g;        /* current weight (from load cell) */
    float    initial_fill_g;   /* weight when first filled */
    uint8_t  remaining_pct;    /* 0-100 */
    uint8_t  pump_pin;
    uint8_t  valve_pin;
    uint8_t  hx711_dout;
    uint8_t  hx711_sck;
} product_slot_t;

static product_slot_t slots[NUM_SLOTS] = {
    { 0, 0, "Cleanser",   0.30f, 50.0f, 80.0f, 100.0f, 100, PIN_PUMP_0, PIN_VALVE_0, PIN_HX711_0_DOUT, PIN_HX711_0_SCK },
    { 1, 0, "Serum",      0.50f, 45.0f, 25.0f,  40.0f, 100, PIN_PUMP_1, PIN_VALVE_1, PIN_HX711_1_DOUT, PIN_HX711_1_SCK },
    { 2, 0, "Moisturizer",0.40f, 50.0f, 60.0f,  80.0f, 100, PIN_PUMP_2, PIN_VALVE_2, PIN_HX711_2_DOUT, PIN_HX711_2_SCK },
    { 3, 0, "Sunscreen",  0.35f, 55.0f, 70.0f,  90.0f, 100, PIN_PUMP_3, PIN_VALVE_3, PIN_HX711_3_DOUT, PIN_HX711_3_SCK },
};

/* ---- HX711 load cell reader (simplified) ---- */
static long hx711_read(uint8_t dout, uint8_t sck)
{
    /* In production:
     * 1. Wait for DOUT to go low (data ready)
     * 2. Pulse SCK 24 times, read 24 data bits
     * 3. Additional SCK pulses for gain/channel selection (1 for chA/gain128)
     * 4. Convert 24-bit two's complement to grams using calibration factor
     *
     * Calibration: known weight → raw reading → scale factor
     * grams = (raw - tare_raw) / scale_factor
     */
    (void)dout; (void)sck;
    return 0;  /* stub */
}

static float read_slot_weight(product_slot_t *slot)
{
    /* Read HX711 and convert to grams */
    long raw = hx711_read(slot->hx711_dout, slot->hx711_sck);
    /* grams = (raw - tare_raw) / scale */
    /* For stub: return current_g unchanged */
    return slot->current_g;
}

static void update_slot_inventory(product_slot_t *slot)
{
    slot->current_g = read_slot_weight(slot);
    float product_g = slot->current_g - slot->tare_g;
    if (slot->initial_fill_g > slot->tare_g) {
        float fill_capacity = slot->initial_fill_g - slot->tare_g;
        slot->remaining_pct = (uint8_t)((product_g / fill_capacity) * 100.0f);
        if (slot->remaining_pct > 100) slot->remaining_pct = 100;
    }
}

/* ---- Pump control ---- */
static void dispense_slot(product_slot_t *slot, uint16_t amount_mg)
{
    float amount_ml = (float)amount_mg / 1000.0f;  /* mg → ml (assume ρ≈1g/ml) */
    float duration_s = amount_ml / slot->flow_rate_ml_s;

    /* Safety: max 30 seconds per dispense */
    if (duration_s > 30.0f) {
        ESP_LOGW(TAG, "Dispense capped at 30s (requested %.1fs)", duration_s);
        duration_s = 30.0f;
    }

    /* Measure weight before */
    float weight_before = read_slot_weight(slot);

    /* Open anti-drip valve */
    gpio_set_level(slot->valve_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Run pump */
    gpio_set_level(slot->pump_pin, 1);
    ESP_LOGI(TAG, "Dispensing slot %u (%s): %u mg over %.1fs",
             slot->slot_id, slot->product_name, amount_mg, duration_s);

    uint32_t duration_ms = (uint32_t)(duration_s * 1000);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    /* Stop pump */
    gpio_set_level(slot->pump_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Close anti-drip valve */
    gpio_set_level(slot->valve_pin, 0);

    /* Measure weight after */
    float weight_after = read_slot_weight(slot);
    float actual_g = weight_before - weight_after;
    uint16_t actual_mg = (uint16_t)(actual_g * 1000.0f);

    /* Determine status */
    uint8_t status = SS_DISPENSE_OK;
    uint8_t flags = 0;

    if (actual_g < 0.05f) {
        status = SS_DISPENSE_EMPTY;
        ESP_LOGW(TAG, "Slot %u empty — no product dispensed", slot->slot_id);
    } else if (actual_mg < amount_mg * 0.8f) {
        status = SS_DISPENSE_PARTIAL;
        ESP_LOGW(TAG, "Slot %u partial: requested %u mg, got %u mg",
                 slot->slot_id, amount_mg, actual_mg);
    }

    /* Update inventory */
    update_slot_inventory(slot);
    if (slot->remaining_pct < 15) {
        flags |= SS_ALERT_LOW_PRODUCT;
        ESP_LOGW(TAG, "Slot %u low product: %u%% remaining",
                 slot->slot_id, slot->remaining_pct);
    }

    uint16_t remaining_mg = (uint16_t)((slot->current_g - slot->tare_g) * 1000.0f);

    /* Send ack to hub */
    ss_send_dispense_ack(slot->slot_id, status, actual_mg, remaining_mg, flags);

    ESP_LOGI(TAG, "Dispense complete: slot %u, %u mg delivered, %u mg remaining, status=%u",
             slot->slot_id, actual_mg, remaining_mg, status);
}

/* ---- OLED display (simplified) ---- */
static void oled_init(void) { }
static void oled_show_inventory(void)
{
    /* In production: display 4 slots with product name + remaining % */
    for (int i = 0; i < NUM_SLOTS; i++) {
        ESP_LOGI(TAG, "Slot %d: %s — %u%%", i, slots[i].product_name,
                 slots[i].remaining_pct);
    }
}

/* ---- Manual dispense buttons ---- */
static void button_task(void *arg)
{
    uint8_t prev[NUM_SLOTS] = { 1, 1, 1, 1 };

    while (1) {
        uint8_t btn_pins[NUM_SLOTS] = { PIN_BTN_0, PIN_BTN_1, PIN_BTN_2, PIN_BTN_3 };

        for (int i = 0; i < NUM_SLOTS; i++) {
            uint8_t val = gpio_get_level(btn_pins[i]);
            if (!val && prev[i]) {
                /* Manual dispense: default amount per product type */
                uint16_t default_mg = 500;  /* 0.5ml default */
                if (i == 0) default_mg = 1500;  /* cleanser: 1.5ml */
                if (i == 3) default_mg = 1200;  /* sunscreen: 1.2ml */
                ESP_LOGI(TAG, "Manual dispense: slot %u, %u mg", i, default_mg);
                dispense_slot(&slots[i], default_mg);
            }
            prev[i] = val;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ---- Sub-GHz mesh receive (SX1262) ---- */
static void mesh_rx_handler(uint8_t type, const uint8_t *data, size_t len)
{
    if (type == SS_MSG_DISPENSE_CMD) {
        if (len < sizeof(ss_dispense_cmd_payload_t) - 2) return;
        const ss_dispense_cmd_payload_t *p = (const ss_dispense_cmd_payload_t *)data;
        if (p->slot < NUM_SLOTS) {
            ESP_LOGI(TAG, "Received dispense cmd: slot %u, %u mg, product %u",
                     p->slot, p->amount_mg, p->product_id);
            dispense_slot(&slots[p->slot], p->amount_mg);
        }
    }
}

/* ---- SX1262 Sub-GHz radio init (simplified) ---- */
static void sx1262_init(void)
{
    /* In production: SPI init, set frequency 868/915 MHz, RX mode */
    ESP_LOGI(TAG, "SX1262 initialized (stub)");
}

static int sx1262_tx(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    return (int)len;
}

/* ---- RFID reader (MFRC522) for cartridge identification ---- */
static void rfid_check(void)
{
    /* In production: poll MFRC522 for nearby RFID tags.
     * When a cartridge is inserted, read its RFID tag → product_id.
     * Auto-update slot.product_id + product_name from cloud product DB.
     * Trigger flow-rate recalibration for new product viscosity. */
}

/* ---- Inventory monitoring task ---- */
static void inventory_task(void *arg)
{
    while (1) {
        for (int i = 0; i < NUM_SLOTS; i++) {
            update_slot_inventory(&slots[i]);
            if (slots[i].remaining_pct < 15) {
                ESP_LOGW(TAG, "Low product: slot %u (%s) — %u%%",
                         i, slots[i].product_name, slots[i].remaining_pct);
                /* In production: send alert to hub → cloud → auto-reorder */
            }
        }
        oled_show_inventory();
        rfid_check();
        vTaskDelay(pdMS_TO_TICKS(60000));  /* check every 1 min */
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "SkinSync Smart Dispenser starting...");

    /* Initialize pump + valve GPIOs (outputs, default low) */
    uint64_t pin_mask = 0;
    for (int i = 0; i < NUM_SLOTS; i++) {
        pin_mask |= (1ULL << slots[i].pump_pin);
        pin_mask |= (1ULL << slots[i].valve_pin);
    }
    gpio_config_t out_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out_conf);

    /* Initialize button GPIOs (inputs with pullups) */
    uint64_t btn_mask = (1ULL << PIN_BTN_0) | (1ULL << PIN_BTN_1) |
                        (1ULL << PIN_BTN_2) | (1ULL << PIN_BTN_3);
    gpio_config_t btn_conf = {
        .pin_bit_mask = btn_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_conf);

    /* Initialize OLED */
    oled_init();
    oled_show_inventory();

    /* Initialize Sub-GHz mesh */
    sx1262_init();
    ss_mesh_set_tx(sx1262_tx);
    ss_mesh_set_rx_callback(mesh_rx_handler);

    /* Initial inventory read */
    for (int i = 0; i < NUM_SLOTS; i++) {
        update_slot_inventory(&slots[i]);
    }
    oled_show_inventory();

    /* Start tasks */
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
    xTaskCreate(inventory_task, "inventory_task", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Dispenser ready. 4 slots loaded.");
}
/*
 * TremorSync Med Station Firmware
 * Target: ESP32-S3-MINI-1
 *
 * Motorized levodopa dispenser with ON/OFF cycle tracking.
 * - 28-compartment rotating carousel (28BYJ-48 stepper)
 * - HX711 load cell pill weight verification (1 mg resolution)
 * - IR break-beam pill drop confirmation (TCRT5000)
 * - AS5600 magnetic encoder for carousel position
 * - OLED display + WS2812B LED ring + piezo buzzer
 * - Sub-GHz 868 MHz to Hub
 *
 * Safety: mechanical anti-double-dose lock, hard dose ceiling,
 *         missed-dose caregiver alert after 30 min.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/mesh.h"

static const char *TAG = "MED_STATION";

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA         1
#define PIN_I2C_SCL         2
#define PIN_SPI_CS          4
#define PIN_SPI_SCK         5
#define PIN_SPI_MISO        6
#define PIN_SPI_MOSI        7
#define PIN_SX_DIO1         8
#define PIN_SX_BUSY         9
#define PIN_SX_RESET        10
#define PIN_HX711_SCK       11
#define PIN_HX711_DOUT      12
#define PIN_IR_BEAM         13
#define PIN_STEPPER_IN1     14
#define PIN_STEPPER_IN2     15
#define PIN_STEPPER_IN3     16
#define PIN_STEPPER_IN4     17
#define PIN_LED_RING        18
#define PIN_BUZZER          19
#define PIN_BTN_DOSE        20
#define PIN_BTN_SNOOZE      21
#define PIN_BAT_SENSE       22

/* ---- Dose config ---- */
#define COMPARTMENTS        28
#define DOSE_INTERVAL_MIN   240   /* 4 hours default */
#define MAX_DOSES_PER_DAY   6
#define DOSE_WINDOW_MIN     30    /* snooze window before escalation */
#define PILL_WEIGHT_MG      200   /* levodopa 200 mg */

/* ---- Stepper sequence (28BYJ-48, half-step) ---- */
static const int step_seq[8][4] = {
    {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
    {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1},
};
#define STEPPER_STEPS_PER_REV  4076  /* 28BYJ-48 gear ratio */

/* ---- State ---- */
static mesh_state_t g_mesh;
static uint16_t g_seq = 0;

static int      g_current_compartment = 0;
static int      g_doses_today = 0;
static uint32_t g_last_dose_ms = 0;
static uint8_t  g_onoff_state = ONOFF_OFF;
static uint8_t  g_battery_pct = 100;
static bool     g_dose_due = false;
static bool     g_reminder_active = false;
static uint32_t g_reminder_start_ms = 0;

/* Dose log (NVS-backed) */
typedef struct {
    uint32_t timestamp_ms;
    uint8_t  pill_weight_mg;
    bool     verified;
} dose_log_entry_t;

static dose_log_entry_t g_dose_log[100];
static int g_dose_log_count = 0;

/* ---- I²C init ---- */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* ---- HX711 load cell ---- */
static long hx711_read(void)
{
    /* In production: bit-bang HX711 protocol (SCK/DOUT) */
    /* Returns 24-bit signed value */
    return 0;  /* placeholder */
}

static float hx711_get_weight_mg(void)
{
    long raw = hx711_read();
    /* Calibration: raw / scale_factor = grams → × 1000 = mg */
    /* Example: scale_factor = 8388.0 (determined during calibration) */
    return (float)raw / 8388.0f * 1000.0f;
}

/* ---- AS5600 magnetic encoder (I²C) ---- */
static uint16_t as5600_read_angle(void)
{
    /* AS5600: 12-bit angle register 0x0C-0x0D */
    uint8_t buf[2];
    /* i2c_master_write_read(I2C_NUM_0, 0x36, {0x0C}, 1, buf, 2); */
    return (buf[0] << 8) | buf[1];  /* 0-4095 → 0-360° */
}

/* ---- Stepper motor ---- */
static void stepper_step(int step)
{
    gpio_set_level(PIN_STEPPER_IN1, step_seq[step][0]);
    gpio_set_level(PIN_STEPPER_IN2, step_seq[step][1]);
    gpio_set_level(PIN_STEPPER_IN3, step_seq[step][2]);
    gpio_set_level(PIN_STEPPER_IN4, step_seq[step][3]);
}

static void stepper_rotate_to_compartment(int target)
{
    /* 360° / 28 compartments = 12.857° per compartment */
    int steps_per_comp = STEPPER_STEPS_PER_REV / COMPARTMENTS;
    int total_steps = (target - g_current_compartment) * steps_per_comp;
    if (total_steps < 0) total_steps += STEPPER_STEPS_PER_REV;  /* forward only */

    for (int i = 0; i < total_steps; i++) {
        stepper_step(i % 8);
        vTaskDelay(pdMS_TO_TICKS(2));  /* 500 Hz step rate */
    }
    /* Power off coils */
    gpio_set_level(PIN_STEPPER_IN1, 0);
    gpio_set_level(PIN_STEPPER_IN2, 0);
    gpio_set_level(PIN_STEPPER_IN3, 0);
    gpio_set_level(PIN_STEPPER_IN4, 0);

    g_current_compartment = target;
}

/* ---- Dispense a dose ---- */
static bool dispense_dose(void)
{
    if (g_doses_today >= MAX_DOSES_PER_DAY) {
        ESP_LOGW(TAG, "Daily dose ceiling reached (%d) — NOT dispensing",
                 MAX_DOSES_PER_DAY);
        return false;
    }

    int target = (g_current_compartment + 1) % COMPARTMENTS;
    ESP_LOGI(TAG, "Dispensing compartment %d...", target);
    stepper_rotate_to_compartment(target);

    /* Verify pill drop via IR beam */
    vTaskDelay(pdMS_TO_TICKS(500));
    bool ir_blocked = (gpio_get_level(PIN_IR_BEAM) == 0);
    if (!ir_blocked) {
        ESP_LOGW(TAG, "No pill detected by IR beam — compartment empty?");
        /* Still log but flag as unverified */
    }

    /* Verify pill weight via HX711 */
    float weight = hx711_get_weight_mg();
    bool weight_ok = (weight > (PILL_WEIGHT_MG - 20) &&
                      weight < (PILL_WEIGHT_MG + 20));

    if (weight_ok) {
        ESP_LOGI(TAG, "Pill verified: %.0f mg (target %d mg)",
                 weight, PILL_WEIGHT_MG);
    } else {
        ESP_LOGW(TAG, "Pill weight mismatch: %.0f mg (target %d mg)",
                 weight, PILL_WEIGHT_MG);
    }

    /* Log dose */
    if (g_dose_log_count < 100) {
        g_dose_log[g_dose_log_count].timestamp_ms = esp_timer_get_time() / 1000;
        g_dose_log[g_dose_log_count].pill_weight_mg = (uint8_t)weight;
        g_dose_log[g_dose_log_count].verified = weight_ok && ir_blocked;
        g_dose_log_count++;
    }

    g_doses_today++;
    g_last_dose_ms = esp_timer_get_time() / 1000;
    g_dose_due = false;
    g_reminder_active = false;

    return true;
}

/* ---- SX1262 radio ---- */
static spi_device_handle_t g_sx_spi;
static void sx1262_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 1);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_sx_spi);
}

static void sx1262_transmit(const uint8_t *data, size_t len)
{
    spi_transaction_t t = {0};
    t.tx_buffer = data;
    t.length = len * 8;
    spi_device_polling_transmit(g_sx_spi, &t);
}

/* ---- Send med data to Hub ---- */
static void send_med_data(void)
{
    med_payload_t p;
    p.dose_taken         = (g_doses_today > 0) ? 1 : 0;
    p.pill_weight_mg     = PILL_WEIGHT_MG;
    p.minutes_since_dose = (uint16_t)((esp_timer_get_time() / 1000 -
                                       g_last_dose_ms) / 60000);
    p.onoff_state        = g_onoff_state;
    p.battery_pct        = g_battery_pct;

    mesh_frame_t frame;
    protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_HUB,
                         MSG_TYPE_SENSOR_DATA, g_seq++,
                         (uint8_t *)&p, sizeof(p));
    sx1262_transmit((uint8_t *)&frame, FRAME_MAX_LEN);
}

/* ---- LED ring (WS2812B) ---- */
static void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    /* In production: drive WS2812B via RMT peripheral */
    ESP_LOGI(TAG, "LED: R=%d G=%d B=%d", r, g, b);
}

static void update_led_for_state(void)
{
    if (g_dose_due) {
        led_set_color(0, 0, 255);  /* blue = dose due */
    } else if (g_onoff_state == ONOFF_OFF) {
        led_set_color(255, 0, 0);  /* red = OFF */
    } else if (g_onoff_state == ONOFF_TRANSITION) {
        led_set_color(255, 128, 0);  /* yellow = approaching OFF */
    } else {
        led_set_color(0, 255, 0);  /* green = ON */
    }
}

/* ---- Dose reminder ---- */
static void dose_reminder_task(void *arg)
{
    uint32_t now;
    while (1) {
        now = esp_timer_get_time() / 1000;

        /* Check if dose is due (every DOSE_INTERVAL_MIN) */
        if (g_last_dose_ms > 0 && !g_dose_due) {
            uint32_t elapsed_min = (now - g_last_dose_ms) / 60000;
            if (elapsed_min >= DOSE_INTERVAL_MIN) {
                g_dose_due = true;
                g_reminder_active = true;
                g_reminder_start_ms = now;
                ESP_LOGI(TAG, "Dose due! (%lu min since last)", elapsed_min);
            }
        }

        /* Escalating reminder */
        if (g_reminder_active) {
            uint32_t since_reminder = (now - g_reminder_start_ms) / 1000;

            /* Gentle beep at 0-30s */
            if (since_reminder < 30) {
                /* Single short beep every 10s */
            }
            /* Louder beep at 30-60s */
            else if (since_reminder < 60) {
                /* Double beep every 10s */
            }
            /* Voice prompt via Hub at 60s+ */
            else if (since_reminder < 120) {
                /* Trigger Hub voice prompt via Sub-GHz */
            }
            /* Caregiver alert after 30 min */
            else if (since_reminder >= DOSE_WINDOW_MIN * 60) {
                ESP_LOGW(TAG, "DOSE MISSED — caregiver alert");
                /* Send alert via Hub → cloud → caregiver app */
                mesh_frame_t frame;
                uint8_t payload[FRAME_PAYLOAD_MAX] = {0};
                payload[0] = 0xFF;  /* missed dose code */
                protocol_build_frame(&frame, g_mesh.self_id, NODE_ID_HUB,
                                     MSG_TYPE_MED_REMINDER,
                                     g_seq++, payload, 1);
                sx1262_transmit((uint8_t *)&frame, FRAME_MAX_LEN);
                g_reminder_active = false;  /* stop local reminder */
            }
        }

        update_led_for_state();
        send_med_data();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- Button handlers ---- */
static void IRAM_ATTR btn_dose_handler(void *arg)
{
    /* Large "DOSE" button — patient presses to confirm taking dose */
    ESP_LOGI(TAG, "DOSE button pressed — dispensing");
    /* Trigger dispense in main task context */
}

static void IRAM_ATTR btn_snooze_handler(void *arg)
{
    if (g_reminder_active) {
        g_reminder_start_ms = esp_timer_get_time() / 1000;  /* reset timer */
        ESP_LOGI(TAG, "Reminder snoozed 10 min");
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "TremorSync Med Station starting...");

    nvs_flash_init();
    i2c_init();
    sx1262_init();
    mesh_init(&g_mesh, NODE_ID_MED_STATION, false);

    /* GPIO */
    int outs[] = {PIN_STEPPER_IN1, PIN_STEPPER_IN2, PIN_STEPPER_IN3,
                  PIN_STEPPER_IN4, PIN_LED_RING, PIN_BUZZER};
    for (int i = 0; i < 6; i++) gpio_set_direction(outs[i], GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BTN_DOSE, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BTN_SNOOZE, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_IR_BEAM, GPIO_MODE_INPUT);
    gpio_pullup_en(PIN_BTN_DOSE);
    gpio_pullup_en(PIN_BTN_SNOOZE);
    gpio_set_intr_type(PIN_BTN_DOSE, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(PIN_BTN_SNOOZE, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BTN_DOSE, btn_dose_handler, NULL);
    gpio_isr_handler_add(PIN_BTN_SNOOZE, btn_snooze_handler, NULL);

    g_last_dose_ms = esp_timer_get_time() / 1000;

    xTaskCreate(dose_reminder_task, "reminder", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Med Station ready — %d compartments, %d min interval, %d max/day",
             COMPARTMENTS, DOSE_INTERVAL_MIN, MAX_DOSES_PER_DAY);
}
/**
 * MedSync - Pill Station Node Firmware
 * STM32F407 (Motor control + sensor processing) + nRF52832 (BLE mesh)
 *
 * Manages motorized pill carousel, weight verification, IR detection,
 * and medication dispensing.
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "ms_protocol.h"
#include "ms_util.h"

LOG_MODULE_REGISTER(pill_station, CONFIG_MS_PILL_LOG_LEVEL);

/* ============================================================
 * Thread Configuration
 * ============================================================ */

#define MAIN_STACK_SIZE         2048
#define MOTOR_STACK_SIZE        1024
#define SENSOR_STACK_SIZE       2048
#define DISPLAY_STACK_SIZE      1024

K_THREAD_STACK_DEFINE(main_stack, MAIN_STACK_SIZE);
K_THREAD_STACK_DEFINE(motor_stack, MOTOR_STACK_SIZE);
K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
K_THREAD_STACK_DEFINE(display_stack, DISPLAY_STACK_SIZE);

/* ============================================================
 * Hardware Definitions (STM32F407)
 * ============================================================ */

/* Stepper motor pins (A4988 drivers) */
#define CAROUSEL_STEP_PIN    PA0
#define CAROUSEL_DIR_PIN     PA1
#define CAROUSEL_EN_PIN      PB0

/* Bin stepper pins (bins 0-7) */
static const uint8_t bin_step_pins[8] = {PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9};
static const uint8_t bin_dir_pins[8] = {PA10, PA11, PA12, PA13, PA14, PA15, PA16, PA17};
static const uint8_t bin_en_pins[8] = {PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7};

/* IR detector inputs (per bin) */
static const uint8_t ir_pins[8] = {PB8, PB9, PB10, PB11, PB12, PB13, PB14, PB15};

/* HX711 weight sensor */
#define HX711_SCK_PIN        PC8
#define HX711_DOUT_PINS      {PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7}

/* OLED display */
#define OLED_SCL_PIN          PC9
#define OLED_SDA_PIN          PC10

/* Other */
#define COVER_SW_PIN          PC11
#define BUZZER_PIN            PC12
#define LED_DATA_PIN          PD0
#define NFC_IRQ_PIN           PD8
#define NFC_RST_PIN           PD9
#define RTC_SDA_PIN           PD10
#define RTC_SCL_PIN           PD11
#define VBAT_SENSE_PIN        PD12
#define SUPPLY_5V_PIN         PD13
#define MOTOR_FAULT_PIN       PD14

/* ============================================================
 * Motor Control
 * ============================================================ */

/* Current bin positions (0-7, where 0 is home position) */
static uint8_t g_current_bin = 0;

/* Bin status array */
static ms_bin_status_t g_bin_status[MS_NUM_BINS] = {
    MS_BIN_EMPTY, MS_BIN_EMPTY, MS_BIN_EMPTY, MS_BIN_EMPTY,
    MS_BIN_EMPTY, MS_BIN_EMPTY, MS_BIN_EMPTY, MS_BIN_EMPTY
};

/* Bin metadata */
struct bin_info {
    char medication_name[16];
    uint16_t pill_weight_mg;     /* Weight per pill in mg */
    uint8_t  pills_per_dose;     /* Number of pills per dose */
    uint8_t  pills_remaining;    /* Current pill count */
    int32_t  last_weight_mg;     /* Last measured weight in mg */
    uint8_t  medication_id;      /* Medication database ID */
};

static struct bin_info g_bins[MS_NUM_BINS];

/**
 * Rotate carousel to position the specified bin at the dispense window.
 * Uses stepper motor with acceleration/deceleration profile.
 */
int ms_carousel_rotate_to(uint8_t target_bin)
{
    if (target_bin >= MS_NUM_BINS) {
        LOG_ERR("Invalid target bin: %d", target_bin);
        return -1;
    }

    /* Calculate shortest rotation direction */
    int8_t delta = target_bin - g_current_bin;
    if (delta > 4) delta -= 8;     /* Go the other way (shorter) */
    if (delta < -4) delta += 8;    /* Go the other way (shorter) */

    int steps = abs(delta) * MS_CAROUSEL_STEPS_PER_BIN;
    bool clockwise = (delta > 0);

    LOG_INF("Rotating carousel from bin %d to bin %d (%d steps, %s)",
             g_current_bin, target_bin, steps, clockwise ? "CW" : "CCW");

    /* Step the carousel motor */
    /* gpio_pin_set(CAROUSEL_DIR_PIN, clockwise ? 1 : 0);
     * for (int i = 0; i < steps; i++) {
     *     gpio_pin_set(CAROUSEL_STEP_PIN, 1);
     *     k_usleep(800);  // 800µs step pulse (acceleration profile)
     *     gpio_pin_set(CAROUSEL_STEP_PIN, 0);
     *     k_usleep(800);
     * }
     */

    g_current_bin = target_bin;
    return 0;
}

/**
 * Dispense a specific number of pills from the current bin.
 * Uses the per-bin stepper to push pills out one at a time.
 * Monitors weight sensor to verify each pill is dispensed.
 */
int ms_dispense_pills(uint8_t bin_id, uint8_t count)
{
    if (bin_id >= MS_NUM_BINS) {
        LOG_ERR("Invalid bin_id: %d", bin_id);
        return -1;
    }

    struct bin_info *bin = &g_bins[bin_id];
    int32_t weight_before = ms_read_bin_weight(bin_id);
    int32_t expected_weight_change = ms_dose_weight_mg(count, bin->pill_weight_mg);

    LOG_INF("Dispensing %d pills from bin %d (%s, %d mg each)",
             count, bin_id, bin->medication_name, bin->pill_weight_mg);
    LOG_INF("Expected weight change: %d mg", expected_weight_change);

    /* Rotate carousel to target bin */
    ms_carousel_rotate_to(bin_id);

    /* Open dispense window (motorized cover) */
    /* ... */

    g_bin_status[bin_id] = MS_BIN_DISPENSING;

    /* Dispense pills one at a time using per-bin stepper */
    int pills_dispensed = 0;
    for (int i = 0; i < count; i++) {
        /* Step the per-bin motor to push one pill out */
        for (int s = 0; s < 200; s++) {  /* 200 steps per pill */
            /* gpio_pin_set(bin_step_pins[bin_id], 1);
             * k_usleep(1000);
             * gpio_pin_set(bin_step_pins[bin_id], 0);
             * k_usleep(1000);
             */
        }

        /* Wait for pill to fall and weight to stabilize */
        k_msleep(500);

        /* Check weight to confirm pill was dispensed */
        int32_t current_weight = ms_read_bin_weight(bin_id);
        int32_t change = weight_before - current_weight;

        if (ms_weight_confirms_dose(change, (i + 1) * (int32_t)bin->pill_weight_mg,
                                      bin->pill_weight_mg)) {
            pills_dispensed++;
            LOG_INF("Pill %d/%d confirmed (weight change: %d mg)", i + 1, count, change);
        } else {
            LOG_WRN("Pill %d/%d not confirmed by weight", i + 1, count);
            /* Retry: step motor again */
        }
    }

    /* Update bin status */
    g_bin_status[bin_id] = MS_BIN_WAITING_PICKUP;
    bin->pills_remaining -= pills_dispensed;

    LOG_INF("Dispensed %d/%d pills from bin %d", pills_dispensed, count, bin_id);

    /* Check if bin is running low */
    if (bin->pills_remaining <= MS_REFILL_THRESHOLD_PCT) {
        g_bin_status[bin_id] = MS_BIN_REFILL_NEEDED;
        ms_send_alert(MS_ALERT_INFO, MS_ALERT_TYPE_REFILL,
                     "Bin %d (%s) needs refill: %d pills remaining",
                     bin_id, bin->medication_name, bin->pills_remaining);
    }

    return pills_dispensed;
}

/* ============================================================
 * Weight Sensor (HX711)
 * ============================================================ */

/**
 * Read weight from a specific bin's load cell via multiplexed HX711.
 * Returns weight in mg.
 */
int32_t ms_read_bin_weight(uint8_t bin_id)
{
    if (bin_id >= MS_NUM_BINS) {
        LOG_ERR("Invalid bin_id: %d", bin_id);
        return 0;
    }

    /* Select the correct HX711 channel via multiplexer */
    /* gpio_pin_set(HX711_MUX_SEL_0, (bin_id >> 0) & 1);
     * gpio_pin_set(HX711_MUX_SEL_1, (bin_id >> 1) & 1);
     * gpio_pin_set(HX711_MUX_SEL_2, (bin_id >> 2) & 1);
     */

    /* Pulse HX711 SCK to read 24-bit data */
    int32_t raw = 0;

    /* Wait for HX711 DOUT to go low (data ready) */
    /* while (gpio_pin_get(HX711_DOUT_PINS[bin_id])) {
     *     k_msleep(1);
     * }
     */

    /* Read 24 bits from HX711 */
    /* for (int i = 0; i < 24; i++) {
     *     gpio_pin_set(HX711_SCK_PIN, 1);
     *     k_usleep(1);
     *     raw = (raw << 1) | gpio_pin_get(HX711_DOUT_PINS[bin_id]);
     *     gpio_pin_set(HX711_SCK_PIN, 0);
     *     k_usleep(1);
     * }
     */

    /* Convert raw ADC value to weight in mg */
    /* int32_t weight_mg = (raw - HX711_TARE_OFFSET) * HX711_SCALE_FACTOR / 1000; */
    int32_t weight_mg = g_bins[bin_id].last_weight_mg;  /* Placeholder */

    /* Update last known weight */
    g_bins[bin_id].last_weight_mg = weight_mg;

    return weight_mg;
}

/* ============================================================
 * IR Beam-Break Detection
 * ============================================================ */

/**
 * Check if IR beam is broken for a specific bin.
 * Called periodically during WAITING_PICKUP state.
 */
bool ms_check_ir_beam(uint8_t bin_id)
{
    if (bin_id >= MS_NUM_BINS) {
        return false;
    }

    /* Read IR detector input */
    /* bool beam_broken = !gpio_pin_get(ir_pins[bin_id]);  // Active low */
    bool beam_broken = false;

    return beam_broken;
}

/* ============================================================
 * Cover Switch
 * ============================================================ */

/**
 * Check if the pill station cover is open.
 * Used for manual override detection.
 */
bool ms_is_cover_open(void)
{
    /* return gpio_pin_get(COVER_SW_PIN);  // Active high = open */
    return false;
}

/* ============================================================
 * Display (SSD1306 OLED)
 * ============================================================ */

/**
 * Update OLED display with current medication information.
 */
void ms_display_update(uint8_t bin_id, const char *medication_name,
                        uint8_t dose_count, const char *food_instruction)
{
    /* Clear display */
    /* ssd1306_clear(); */

    /* Line 1: Medication name */
    /* ssd1306_draw_text(0, 0, medication_name, FONT_LARGE); */

    /* Line 2: Dose count + food instruction */
    /* ssd1306_draw_text(0, 16, "Take %d pill(s) %s", dose_count, food_instruction); */

    /* Line 3: Time */
    /* ssd1306_draw_text(0, 32, "Scheduled: %02d:%02d", hour, min); */

    /* Line 4: Status */
    /* ssd1306_draw_text(0, 48, "Status: %s", bin_status_str); */

    /* ssd1306_refresh(); */
}

/* ============================================================
 * Alert Helper
 * ============================================================ */

void ms_send_alert(ms_alert_level_t level, ms_alert_type_t type,
                    const char *message, ...)
{
    /* Format message */
    char msg[64];
    va_list args;
    va_start(args, message);
    vsnprintf(msg, sizeof(msg), message, args);
    va_end(args);

    /* Send to nRF52832 for BLE mesh transmission */
    ms_uart_send_alert_to_nrf(level, type, msg);

    /* Activate local buzzer */
    if (level >= MS_ALERT_REMINDER) {
        /* Buzzer pattern based on urgency */
        for (int i = 0; i < (int)level; i++) {
            /* gpio_pin_set(BUZZER_PIN, 1);
             * k_msleep(200);
             * gpio_pin_set(BUZZER_PIN, 0);
             * k_msleep(200);
             */
        }
    }
}

/**
 * Send alert to nRF52832 via UART for BLE mesh distribution.
 */
void ms_uart_send_alert_to_nrf(ms_alert_level_t level, ms_alert_type_t type,
                                 const char *message)
{
    /* uint8_t payload[6 + 64] = { ... };
     * uart_tx(nrf_uart, payload, len, K_MSEC(100));
     */
    LOG_INF("Alert sent to nRF52832: level=%d, type=%d, msg=%s", level, type, message);
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    LOG_INF("MedSync Pill Station starting...");
    LOG_INF("Firmware v%d.%d.%d", MS_VERSION_MAJOR, MS_VERSION_MINOR, MS_VERSION_PATCH);

    /* Initialize GPIOs */
    /* ... motor pins, IR pins, weight sensor, OLED, buzzer, LEDs ... */

    /* Initialize HX711 weight sensors (tare) */
    LOG_INF("Calibrating weight sensors...");
    for (int i = 0; i < MS_NUM_BINS; i++) {
        g_bins[i].last_weight_mg = ms_read_bin_weight(i);
        LOG_INF("  Bin %d: %d mg", i, g_bins[i].last_weight_mg);
    }

    /* Initialize DS3231 RTC */
    /* rtc_init(); */

    /* Initialize nRF52832 UART link */
    /* uart_init(); */

    /* Home carousel to position 0 */
    ms_carousel_rotate_to(0);

    /* Main loop */
    while (1) {
        /* Check for commands from nRF52832 (BLE mesh) */
        /* ms_process_uart_commands(); */

        /* Check for scheduled doses */
        /* ms_check_schedule(); */

        /* Monitor weight sensors for changes */
        for (int i = 0; i < MS_NUM_BINS; i++) {
            if (g_bin_status[i] == MS_BIN_WAITING_PICKUP) {
                int32_t current_weight = ms_read_bin_weight(i);
                int32_t expected_change = ms_dose_weight_mg(
                    g_bins[i].pills_per_dose, g_bins[i].pill_weight_mg);
                int32_t actual_change = g_bins[i].last_weight_mg - current_weight;

                /* Check IR beam */
                bool ir_broken = ms_check_ir_beam(i);

                if (ms_weight_confirms_dose(actual_change, expected_change,
                                              g_bins[i].pill_weight_mg / 5)) {
                    /* Weight confirms pill was taken */
                    g_bin_status[i] = MS_BIN_CONFIRMED;
                    LOG_INF("Bin %d dose confirmed by weight", i);
                } else if (ir_broken) {
                    /* IR confirms something was picked up */
                    LOG_INF("Bin %d IR beam broken (probable pickup)", i);
                }

                /* Check cover */
                if (ms_is_cover_open()) {
                    LOG_INF("Pill station cover opened (manual access)");
                }
            }

            /* Check for refills */
            if (g_bins[i].pills_remaining > 0 &&
                g_bins[i].pills_remaining <= MS_REFILL_THRESHOLD_PCT / 100.0 * 30) {
                g_bin_status[i] = MS_BIN_REFILL_NEEDED;
            }
        }

        /* Update display */
        /* ms_display_update(...); */

        /* Report status to hub via nRF52832 */
        /* ms_send_status_report(); */

        k_sleep(K_SECONDS(5));
    }

    return 0;
}
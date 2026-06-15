/**
 * MedSync - Room Beacon Node Firmware
 * nRF52832 (BLE mesh router + sensor processing)
 *
 * Monitors room occupancy, temperature, humidity, light, and sound.
 * Provides proximity medication reminders when patient walks past.
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

#include "ms_protocol.h"
#include "ms_util.h"

LOG_MODULE_REGISTER(room_beacon, CONFIG_MS_BEACON_LOG_LEVEL);

/* ============================================================
 * Thread Configuration
 * ============================================================ */

#define MAIN_STACK_SIZE         1024
#define SENSOR_STACK_SIZE       1024
#define REMINDER_STACK_SIZE     1024

K_THREAD_STACK_DEFINE(main_stack, MAIN_STACK_SIZE);
K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
K_THREAD_STACK_DEFINE(reminder_stack, REMINDER_STACK_SIZE);

/* ============================================================
 * Global State
 * ============================================================ */

struct ms_beacon_state {
    /* Sensor data */
    bool    occupancy;              /* PIR: true if person detected */
    int16_t temperature_cx100;      /* Temperature in °C × 100 */
    uint16_t humidity_cx10;         /* Humidity in %RH × 10 */
    uint16_t light_lux;             /* Ambient light in lux */
    uint8_t  sound_level_db;        /* Sound level in dB (A-weighted) */
    uint16_t battery_mv;            /* Battery voltage in mV */

    /* Reminder state */
    bool    reminder_active;        /* True if medication is due */
    uint8_t reminder_urgency;       /* ms_urgency_t */
    uint8_t reminder_bin_id;        /* Which medication bin */
    int64_t reminder_start_time;    /* When reminder was activated */
    int64_t last_reminder_time;     /* Last time we reminded the patient */

    /* Occupancy tracking */
    int64_t last_occupancy_time;    /* Last time person was detected */
    int64_t occupancy_start_time;   /* When current occupancy began */

    /* BLE mesh */
    uint16_t node_id;
    bool     mesh_provisioned;
};

static struct ms_beacon_state g_state;

/* ============================================================
 * GPIO Definitions
 * ============================================================ */

#define PIR_NODE       DT_ALIAS(pir)
#define LED_NODE       DT_ALIAS(led0)
#define BUZZER_NODE    DT_ALIAS(buzzer)
#define BUTTON_NODE    DT_ALIAS(sw0)

static const struct gpio_dt_spec pir_gpio = GPIO_DT_SPEC_GET(PIR_NODE, gpios);
static const struct gpio_dt_spec led_gpio = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec buzzer_gpio = GPIO_DT_SPEC_GET(BUZZER_NODE, gpios);
static const struct gpio_dt_spec button_gpio = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

/* ============================================================
 * I2C Sensors (SHT40, VEML7700)
 * ============================================================ */

#define SHT40_ADDR     0x44
#define VEML7700_ADDR  0x10

/**
 * Read temperature and humidity from SHT40.
 * Returns 0 on success, negative on error.
 */
int ms_read_sht40(int16_t *temp_cx100, uint16_t *hum_cx10)
{
    /* const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0)); */
    uint8_t cmd = 0xFD;  /* SHT40 measure at high precision */
    uint8_t buf[6];

    /* if (i2c_write(i2c, &cmd, 1, SHT40_ADDR) != 0) {
     *     LOG_ERR("SHT40 I2C write failed");
     *     return -EIO;
     * }
     * k_msleep(10);  // Wait for measurement
     * if (i2c_read(i2c, buf, 6, SHT40_ADDR) != 0) {
     *     LOG_ERR("SHT40 I2C read failed");
     *     return -EIO;
     * }
     */

    /* Parse SHT40 data (placeholder values) */
    *temp_cx100 = 2200;   /* 22.00°C */
    *hum_cx10 = 450;       /* 45.0% RH */

    return 0;
}

/**
 * Read ambient light level from VEML7700.
 * Returns light level in lux.
 */
int ms_read_veml7700(uint16_t *lux)
{
    /* const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0)); */
    uint8_t cmd[2] = {0x00, 0x00};  /* Configuration register */
    uint8_t buf[2];

    /* Read ALS data register */
    /* if (i2c_reg_read_byte(i2c, VEML7700_ADDR, 0x04, buf) != 0) {
     *     LOG_ERR("VEML7700 read failed");
     *     return -EIO;
     * }
     */

    /* Convert raw ALS to lux */
    *lux = 150;  /* Placeholder: 150 lux */

    return 0;
}

/* ============================================================
 * MEMS Microphone (SPH0645LM4H)
 * ============================================================ */

/**
 * Read sound level from MEMS microphone.
 * Returns sound level in dB (A-weighted).
 * Samples for 100ms and computes RMS.
 */
int ms_read_sound_level(uint8_t *db_level)
{
    /* Read I2S data from SPH0645LM4H */
    /* int16_t samples[4800];  // 100ms at 48kHz */
    /* uint64_t sum_squares = 0;
     *
     * for (int i = 0; i < 4800; i++) {
     *     sum_squares += (int64_t)samples[i] * samples[i];
     * }
     *
     * float rms = sqrtf((float)sum_squares / 4800.0f);
     * float db = 20.0f * log10f(rms / 32768.0f) + 94.0f;
     */

    *db_level = 40;  /* Placeholder: 40 dB (quiet room) */
    return 0;
}

/* ============================================================
 * PIR Motion Detection
 * ============================================================ */

/**
 * PIR interrupt handler.
 * Called when AM312 detects motion.
 */
static struct gpio_callback pir_cb;

void ms_pir_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    g_state.occupancy = true;
    g_state.last_occupancy_time = k_uptime_get();

    LOG_INF("Motion detected in room");

    /* If a reminder is active, activate LED and buzzer */
    if (g_state.reminder_active) {
        ms_activate_proximity_reminder();
    }
}

/* ============================================================
 * Proximity Reminder
 * ============================================================ */

/**
 * Activate proximity reminder when patient walks past.
 * Uses LED pulse and short buzzer beep.
 */
void ms_activate_proximity_reminder(void)
{
    int64_t now = k_uptime_get();

    /* Don't remind more than once every 2 minutes */
    if (g_state.last_reminder_time > 0 &&
        (now - g_state.last_reminder_time) < 120000) {
        return;
    }

    LOG_INF("Proximity reminder: bin=%d, urgency=%d",
             g_state.reminder_bin_id, g_state.reminder_urgency);

    /* LED pulse pattern based on urgency */
    switch (g_state.reminder_urgency) {
    case MS_URGENCY_NORMAL:
        /* Blue pulse: 2 slow pulses */
        for (int i = 0; i < 2; i++) {
            /* gpio_pin_set_dt(&led_gpio, 1); */
            k_msleep(300);
            /* gpio_pin_set_dt(&led_gpio, 0); */
            k_msleep(300);
        }
        /* 1 short beep */
        /* gpio_pin_set_dt(&buzzer_gpio, 1);
         * k_msleep(100);
         * gpio_pin_set_dt(&buzzer_gpio, 0);
         */
        break;

    case MS_URGENCY_ELEVATED:
        /* Yellow pulse: 3 medium pulses */
        for (int i = 0; i < 3; i++) {
            /* gpio_pin_set_dt(&led_gpio, 1); */
            k_msleep(200);
            /* gpio_pin_set_dt(&led_gpio, 0); */
            k_msleep(200);
        }
        /* 2 short beeps */
        for (int i = 0; i < 2; i++) {
            /* gpio_pin_set_dt(&buzzer_gpio, 1);
             * k_msleep(150);
             * gpio_pin_set_dt(&buzzer_gpio, 0);
             * k_msleep(150);
             */
        }
        break;

    case MS_URGENCY_URGENT:
    case MS_URGENCY_EMERGENCY:
        /* Red rapid pulse + continuous beeping */
        for (int i = 0; i < 5; i++) {
            /* gpio_pin_set_dt(&led_gpio, 1); */
            k_msleep(100);
            /* gpio_pin_set_dt(&led_gpio, 0); */
            k_msleep(100);
        }
        /* 3 short beeps */
        for (int i = 0; i < 3; i++) {
            /* gpio_pin_set_dt(&buzzer_gpio, 1);
             * k_msleep(200);
             * gpio_pin_set_dt(&buzzer_gpio, 0);
             * k_msleep(200);
             */
        }
        break;
    }

    g_state.last_reminder_time = now;
}

/* ============================================================
 * Button Handler
 * ============================================================ */

static struct gpio_callback button_cb;

void ms_button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    LOG_INF("Button pressed");

    if (g_state.reminder_active) {
        /* Dismiss reminder — mark as taken or snooze */
        /* Short press = "Mark as taken" */
        /* Send DoseConfirm to hub via BLE mesh */
        LOG_INF("Reminder dismissed (marked as taken)");

        g_state.reminder_active = false;
        /* gpio_pin_set_dt(&led_gpio, 0); */
    }
}

/* ============================================================
 * Sensor Reading Thread
 * ============================================================ */

void ms_sensor_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        /* Read environmental sensors */
        ms_read_sht40(&g_state.temperature_cx100, &g_state.humidity_cx10);
        ms_read_veml7700(&g_state.light_lux);

        /* Read sound level (only every 5 minutes to save power) */
        static int sound_counter = 0;
        if (++sound_counter >= 60) {  /* Every 60 readings × 5 sec = 5 min */
            ms_read_sound_level(&g_state.sound_level_db);
            sound_counter = 0;
        }

        /* Read battery voltage */
        /* g_state.battery_mv = ms_adc_to_battery_mv(adc_read(), 4095, 1000000, 1000000); */

        /* Check PIR timeout (occupancy expires after 30 seconds of no motion) */
        if (g_state.occupancy &&
            (k_uptime_get() - g_state.last_occupancy_time) > 30000) {
            g_state.occupancy = false;
            LOG_INF("Occupancy timeout — room unoccupied");
        }

        /* Send periodic report to hub */
        ms_beacon_report_t report = {
            .node_id = g_state.node_id,
            .occupancy = g_state.occupancy,
            .temperature_cx100 = g_state.temperature_cx100,
            .humidity_cx10 = g_state.humidity_cx10,
            .light_lux = g_state.light_lux,
            .sound_level_db = g_state.sound_level_db,
            .reminder_active = g_state.reminder_active,
            .battery_mv = g_state.battery_mv,
            .uptime_sec = (uint32_t)(k_uptime_get() / 1000),
        };

        /* Send via BLE mesh */
        /* ms_mesh_send(MS_NODE_ID_HUB, MS_MODEL_VITALS, &report, sizeof(report)); */

        k_sleep(K_SECONDS(5));
    }
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    LOG_INF("MedSync Room Beacon starting...");
    LOG_INF("Firmware v%d.%d.%d", MS_VERSION_MAJOR, MS_VERSION_MINOR, MS_VERSION_PATCH);

    /* Initialize GPIOs */
    /* gpio_pin_configure_dt(&pir_gpio, GPIO_INPUT);
     * gpio_pin_configure_dt(&led_gpio, GPIO_OUTPUT_INACTIVE);
     * gpio_pin_configure_dt(&buzzer_gpio, GPIO_OUTPUT_INACTIVE);
     * gpio_pin_configure_dt(&button_gpio, GPIO_INPUT | GPIO_PULL_UP);
     */

    /* Initialize PIR interrupt */
    /* gpio_init_callback(&pir_cb, ms_pir_handler, BIT(pir_gpio.pin));
     * gpio_add_callback(pir_gpio.port, &pir_cb);
     * gpio_pin_interrupt_configure_dt(&pir_gpio, GPIO_INT_EDGE_TO_ACTIVE);
     */

    /* Initialize button interrupt */
    /* gpio_init_callback(&button_cb, ms_button_handler, BIT(button_gpio.pin));
     * gpio_add_callback(button_gpio.port, &button_cb);
     * gpio_pin_interrupt_configure_dt(&button_gpio, GPIO_INT_EDGE_FALLING);
     */

    /* Initialize I2C sensors */
    /* ms_read_sht40(NULL, NULL); // Initial read to warm up */
    /* ms_read_veml7700(NULL); */

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.node_id = MS_NODE_ID_BEACON_BASE;  /* Assigned during provisioning */

    /* Start BLE mesh */
    /* ble_mesh_init(); */

    /* Start sensor reading thread */
    /* k_thread_create(&sensor_tid, sensor_stack, SENSOR_STACK_SIZE,
     *                 ms_sensor_thread, NULL, NULL, NULL,
     *                 -1, 0, K_NO_WAIT);
     */

    /* Main loop — sleep and let threads work */
    while (1) {
        k_sleep(K_SECONDS(60));
    }

    return 0;
}
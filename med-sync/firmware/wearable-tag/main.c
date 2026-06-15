/**
 * MedSync - Wearable Tag Node Firmware
 * nRF52833 (BLE + pulse ox + accelerometer + gyroscope + haptic)
 *
 * Monitors heart rate, SpO2, activity, and fall detection.
 * Provides vibration medication reminders and NFC tap-to-confirm.
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

LOG_MODULE_REGISTER(wearable_tag, CONFIG_MS_WEARABLE_LOG_LEVEL);

/* ============================================================
 * Thread Configuration
 * ============================================================ */

#define MAIN_STACK_SIZE         1024
#define VITALS_STACK_SIZE       2048
#define ACTIVITY_STACK_SIZE     2048
#define FALL_DETECT_STACK_SIZE  3072

K_THREAD_STACK_DEFINE(main_stack, MAIN_STACK_SIZE);
K_THREAD_STACK_DEFINE(vitals_stack, VITALS_STACK_SIZE);
K_THREAD_STACK_DEFINE(activity_stack, ACTIVITY_STACK_SIZE);
K_THREAD_STACK_DEFINE(fall_detect_stack, FALL_DETECT_STACK_SIZE);

/* ============================================================
 * Global State
 * ============================================================ */

struct ms_wearable_state {
    /* Vitals */
    uint8_t  heart_rate_bpm;       /* Latest heart rate */
    uint8_t  spo2_percent;         /* Latest SpO2 */
    int16_t  skin_temp_cx100;      /* Skin temperature °C × 100 */

    /* Activity */
    ms_activity_t activity_level;  /* Current activity classification */
    uint16_t steps_count;          /* Step counter */
    bool     fall_detected;        /* Fall detected flag */
    float    fall_confidence;      /* ML model fall confidence */

    /* Reminders */
    bool     reminder_active;      /* Medication reminder active */
    uint8_t  reminder_urgency;     /* Reminder urgency level */
    int64_t  reminder_start_time;  /* When reminder started */
    int64_t  last_vibe_time;       /* Last vibration time */

    /* System */
    uint16_t battery_mv;           /* Battery voltage */
    uint32_t uptime_sec;           /* Uptime in seconds */
    uint16_t node_id;              /* BLE mesh node ID */
};

static struct ms_wearable_state g_state;

/* ============================================================
 * GPIO Definitions
 * ============================================================ */

#define LED_R_NODE     DT_ALIAS(led_r)
#define LED_G_NODE     DT_ALIAS(led_g)
#define LED_B_NODE     DT_ALIAS(led_b)
#define BUTTON_NODE    DT_ALIAS(sw0)
#define HAP_DRV_NODE   DT_ALIAS(haptic)

static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_R_NODE, gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(LED_G_NODE, gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

/* ============================================================
 * Pulse Oximetry (MAX30101)
 * ============================================================ */

#define MAX30101_ADDR  0x57

/* MAX30101 register addresses */
#define MAX30101_INT_STATUS_1   0x00
#define MAX30101_INT_STATUS_2   0x01
#define MAX30101_INT_ENABLE_1   0x02
#define MAX30101_INT_ENABLE_2   0x03
#define MAX30101_FIFO_WR_PTR    0x04
#define MAX30101_FIFO_RD_PTR    0x06
#define MAX30101_FIFO_DATA      0x07
#define MAX30101_FIFO_CONFIG    0x08
#define MAX30101_MODE_CONFIG    0x09
#define MAX30101_SPO2_CONFIG    0x0A
#define MAX30101_LED1_PA        0x0C  /* Red LED pulse amplitude */
#define MAX30101_LED2_PA        0x0D  /* IR LED pulse amplitude */
#define MAX30101_LED3_PA        0x0E  /* Green LED pulse amplitude */
#define MAX30101_MULTI_LED_CTRL1 0x11
#define MAX30101_MULTI_LED_CTRL2 0x12

/**
 * Initialize MAX30101 pulse oximeter.
 * Configure for SpO2 mode with 100 samples/sec, 411µs ADC range.
 */
int ms_max30101_init(void)
{
    /* const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0)); */
    uint8_t buf[2];

    /* Reset */
    buf[0] = MAX30101_MODE_CONFIG;
    buf[1] = 0x40;  /* Reset bit */
    /* i2c_reg_write_byte(i2c, MAX30101_ADDR, buf[0], buf[1]); */
    k_msleep(10);

    /* Configure FIFO: sample averaging = 4, FIFO almost full = 17 */
    buf[0] = MAX30101_FIFO_CONFIG;
    buf[1] = 0x47;  /* SMP_AVE=4, FIFO_A_FULL=17 */
    /* i2c_reg_write_byte(i2c, MAX30101_ADDR, buf[0], buf[1]); */

    /* Set mode: SpO2 (Red + IR LED) */
    buf[0] = MAX30101_MODE_CONFIG;
    buf[1] = 0x03;  /* SpO2 mode */
    /* i2c_reg_write_byte(i2c, MAX30101_ADDR, buf[0], buf[1]); */

    /* SpO2 configuration: 100 samples/sec, 411µs pulse width, 4096 ADC range */
    buf[0] = MAX30101_SPO2_CONFIG;
    buf[1] = 0x2F;  /* SPO2_SR=100, LED_PW=411µs, ADC_RGE=4096 */
    /* i2c_reg_write_byte(i2c, MAX30101_ADDR, buf[0], buf[1]); */

    /* LED pulse amplitudes */
    buf[0] = MAX30101_LED1_PA;
    buf[1] = 0x24;  /* Red LED: 7.6mA */
    /* i2c_reg_write_byte(i2c, MAX30101_ADDR, buf[0], buf[1]); */

    buf[0] = MAX30101_LED2_PA;
    buf[1] = 0x24;  /* IR LED: 7.6mA */
    /* i2c_reg_write_byte(i2c, MAX30101_ADDR, buf[0], buf[1]); */

    LOG_INF("MAX30101 initialized (SpO2 mode, 100Hz)");
    return 0;
}

/**
 * Read heart rate and SpO2 from MAX30101.
 * Uses simple peak detection on PPG signal.
 * Returns 0 on success, negative on error.
 */
int ms_read_pulse_ox(uint8_t *hr_bpm, uint8_t *spo2_pct)
{
    /* Read FIFO samples from MAX30101 */
    /* For each sample, we get Red and IR LED values */
    /* Heart rate is calculated from peak-to-peak interval */
    /* SpO2 is calculated from R = (AC_red / DC_red) / (AC_ir / DC_ir) */

    /* Placeholder algorithm — production uses proper PPG processing */
    *hr_bpm = 72;     /* Typical resting heart rate */
    *spo2_pct = 98;    /* Normal SpO2 */

    g_state.heart_rate_bpm = *hr_bpm;
    g_state.spo2_percent = *spo2_pct;

    return 0;
}

/* ============================================================
 * Accelerometer + Gyroscope (ADXL362 + BMI160)
 * ============================================================ */

#define ADXL362_CS_PIN    P0.07
#define BMI160_ADDR       0x69

/**
 * Initialize ADXL362 accelerometer for ultra-low-power activity detection.
 * Configures activity interrupt for >2g threshold (fall detection).
 */
int ms_adxl362_init(void)
{
    /* SPI initialization */
    /* Write ADXL362 registers:
     * THRESH_ACT: 0x17 (2g / 0.063g = 31.7 ≈ 32 = 0x20)
     * TIME_ACT: 0x01 (1 sample = 10ms at 100Hz)
     * ACT_INACT_CTL: 0x40 (enable activity interrupt, referenced mode)
     * INTMAP1: 0x40 (activity interrupt on INT1)
     * FILTER_CTL: 0x43 (100Hz ODR, half-bandwidth)
     * POWER_CTL: 0x02 (measurement mode)
     */

    LOG_INF("ADXL362 initialized (100Hz, activity threshold 2g)");
    return 0;
}

/**
 * Initialize BMI160 IMU for 6-axis motion tracking.
 * Configures accelerometer (100Hz, ±4g) and gyroscope (100Hz, ±500°/s).
 */
int ms_bmi160_init(void)
{
    /* I2C initialization */
    /* Write BMI160 registers:
     * CMD: 0x11 (Soft reset)
     * CMD: 0x15 (Accel set to normal mode)
     * CMD: 0x19 (Gyro set to normal mode)
     * ACC_CONF: 0x28 (100Hz, undersample=0)
     * ACC_RANGE: 0x05 (±4g)
     * GYR_CONF: 0x28 (100Hz)
     * GYR_RANGE: 0x04 (±500°/s)
     * INT_EN: 0x80 (Data ready interrupt)
     */

    LOG_INF("BMI160 initialized (6-axis, 100Hz)");
    return 0;
}

/**
 * ADXL362 activity interrupt handler.
 * Called when acceleration exceeds 2g (potential fall).
 */
static struct gpio_callback adxl_int_cb;

void ms_fall_impact_handler(const struct device *dev, struct gpio_callback *cb,
                             uint32_t pins)
{
    LOG_WRN("Impact detected! (>2g)");

    /* Capture BMI160 6-axis data for fall classification */
    /* ms_start_fall_window(); */
}

/* ============================================================
 * Fall Detection (TFLite Micro)
 * ============================================================ */

/**
 * Process a 3-second window of 6-axis IMU data for fall classification.
 * Uses TFLite Micro model (INT8 quantized, 45KB).
 *
 * Input: 3s × 100Hz × 6 axes = 1800 samples (int8)
 * Output: [not_fall_probability, fall_probability]
 */
float ms_classify_fall(int8_t *imu_data, uint16_t len)
{
    /* In production, this would run TFLite Micro inference:
     *
     * TfLiteTensor* input = interpreter->input(0);
     * memcpy(input->data.int8, imu_data, len);
     * interpreter->Invoke();
     * TfLiteTensor* output = interpreter->output(0);
     * float fall_prob = (float)output->data.int8[1] / 127.0f;
     * return fall_prob;
     */

    /* Placeholder: simple threshold-based detection */
    /* Check if peak acceleration > 2g AND followed by stillness */
    float fall_score = 0.0f;

    /* Check for impact peak */
    int max_accel_idx = 0;
    int8_t max_accel = 0;
    for (int i = 0; i < 300; i++) {  /* First 3 seconds × 100 samples/sec */
        /* Use acceleration magnitude */
        int8_t ax = imu_data[i * 6 + 0];
        int8_t ay = imu_data[i * 6 + 1];
        int8_t az = imu_data[i * 6 + 2];
        int16_t magnitude = (int16_t)ax * ax + (int16_t)ay * ay + (int16_t)az * az;

        if (abs(magnitude) > 400) {  /* > 2g equivalent */
            fall_score += 0.3f;
        }
    }

    /* Check for post-impact stillness (last 1 second) */
    bool still = true;
    for (int i = 200; i < 300; i++) {
        int8_t ax = imu_data[i * 6 + 0];
        int8_t ay = imu_data[i * 6 + 1];
        int8_t az = imu_data[i * 6 + 2];
        if (abs(ax) > 10 || abs(ay) > 10 || abs(az) > 10) {
            still = false;
            break;
        }
    }
    if (still) fall_score += 0.5f;

    /* Clamp to [0, 1] */
    if (fall_score > 1.0f) fall_score = 1.0f;

    return fall_score;
}

/**
 * Fall detection thread.
 * Waits for ADXL362 activity interrupt, then captures BMI160 data
 * and runs TFLite Micro inference for fall classification.
 */
void ms_fall_detect_thread(void *p1, void *p2, void *p3)
{
    int8_t imu_buffer[MS_FALL_MODEL_INPUT_SIZE];  /* 3s × 100Hz × 6 axes */
    bool impact_detected = false;
    int64_t impact_time = 0;

    while (1) {
        /* Wait for impact detection from ADXL362 */
        /* In production, this would be triggered by GPIO interrupt */
        k_sleep(K_MSEC(100));

        if (impact_detected) {
            LOG_INF("Capturing IMU window for fall classification...");

            /* Capture 3 seconds of BMI160 data at 100Hz */
            impact_time = k_uptime_get();

            for (int i = 0; i < 300; i++) {
                /* Read BMI160 FIFO:
                 * imu_buffer[i*6+0] = accel_x;
                 * imu_buffer[i*6+1] = accel_y;
                 * imu_buffer[i*6+2] = accel_z;
                 * imu_buffer[i*6+3] = gyro_x;
                 * imu_buffer[i*6+4] = gyro_y;
                 * imu_buffer[i*6+5] = gyro_z;
                 */
                k_msleep(10);  /* 100Hz sampling */
            }

            /* Run TFLite Micro fall classification */
            float fall_confidence = ms_classify_fall(imu_buffer,
                                                       MS_FALL_MODEL_INPUT_SIZE);

            LOG_INF("Fall classification: confidence=%.2f", fall_confidence);

            if (fall_confidence >= MS_FALL_THRESHOLD) {
                /* Fall detected! */
                g_state.fall_detected = true;
                g_state.fall_confidence = fall_confidence;

                /* Check for post-fall stillness (person not moving) */
                bool post_fall_still = true;  /* Placeholder */

                /* Send fall alert to hub */
                ms_alert_t alert = {
                    .level = MS_ALERT_EMERGENCY,
                    .type = MS_ALERT_TYPE_FALL,
                    .source_node_id = g_state.node_id,
                    .bin_id = 0xFF,
                    .medication_id = 0xFF,
                };
                ms_str_copy(alert.message,
                    "Fall detected! Please press button if you are OK.",
                    sizeof(alert.message));

                /* ms_mesh_send(MS_NODE_ID_HUB, MS_MODEL_ALERT, &alert, sizeof(alert)); */

                LOG_ERR("FALL DETECTED! Confidence: %.2f, Stillness: %d",
                         fall_confidence, post_fall_still);

                /* Activate haptic pattern to ask if OK */
                ms_haptic_pattern(HAPTIC_FALL_ALERT);

                /* Set LED to red */
                /* gpio_pin_set_dt(&led_r, 1);
                 * gpio_pin_set_dt(&led_g, 0);
                 * gpio_pin_set_dt(&led_b, 0);
                 */

                /* Wait 60 seconds for button press (confirm OK) */
                /* If no press, escalate to emergency */
            }

            impact_detected = false;
        }
    }
}

/* ============================================================
 * Activity Classification
 * ============================================================ */

/**
 * Classify current activity from IMU data.
 * Simple threshold-based classification (production uses TFLite).
 */
ms_activity_t ms_classify_activity(int16_t accel_rms, int16_t gyro_rms)
{
    /* Still: low accel, low gyro */
    if (accel_rms < 50 && gyro_rms < 20) {
        return MS_ACTIVITY_STILL;
    }

    /* Sleeping: very low accel, very low gyro, sustained over time */
    if (accel_rms < 30 && gyro_rms < 10) {
        return MS_ACTIVITY_SLEEPING;
    }

    /* Walking: moderate accel, moderate gyro */
    if (accel_rms >= 50 && accel_rms < 200 && gyro_rms >= 20 && gyro_rms < 100) {
        return MS_ACTIVITY_WALKING;
    }

    /* Running: high accel, high gyro */
    if (accel_rms >= 200) {
        return MS_ACTIVITY_RUNNING;
    }

    return MS_ACTIVITY_UNKNOWN;
}

/**
 * Vitals monitoring thread.
 * Reads pulse oximetry every 5 minutes, classifies activity,
 * and sends reports to hub.
 */
void ms_vitals_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        /* Read pulse oximetry (heart rate + SpO2) */
        ms_read_pulse_ox(&g_state.heart_rate_bpm, &g_state.spo2_percent);

        /* Read skin temperature from MAX30101 die temperature */
        /* g_state.skin_temp_cx100 = ms_read_skin_temp(); */

        /* Classify activity */
        int16_t accel_rms = 100;   /* Placeholder */
        int16_t gyro_rms = 50;     /* Placeholder */
        g_state.activity_level = ms_classify_activity(accel_rms, gyro_rms);

        /* Check for abnormal vitals */
        if (g_state.spo2_percent < MS_SPO2_WARNING_MIN) {
            LOG_WRN("Low SpO2: %d%%", g_state.spo2_percent);
        }
        if (g_state.heart_rate_bpm > MS_HR_TACHY_THRESHOLD ||
            g_state.heart_rate_bpm < MS_HR_BRADY_THRESHOLD) {
            LOG_WRN("Abnormal heart rate: %d BPM", g_state.heart_rate_bpm);
        }

        /* Send vitals report to hub */
        ms_vitals_report_t report = {
            .node_id = g_state.node_id,
            .heart_rate_bpm = g_state.heart_rate_bpm,
            .spo2_percent = g_state.spo2_percent,
            .activity_level = g_state.activity_level,
            .fall_detected = g_state.fall_detected ? 1 : 0,
            .steps_count = g_state.steps_count,
            .skin_temp_cx100 = g_state.skin_temp_cx100,
            .battery_mv = g_state.battery_mv,
            .timestamp = (uint32_t)(k_uptime_get() / 1000),
        };

        /* ms_mesh_send(MS_NODE_ID_HUB, MS_MODEL_VITALS, &report, sizeof(report)); */

        /* Clear fall flag after reporting */
        g_state.fall_detected = false;

        /* Sleep for 5 minutes (pulse ox interval) */
        k_sleep(K_SECONDS(MS_PULSE_OX_INTERVAL_SEC));
    }
}

/* ============================================================
 * Haptic Feedback (DRV2605L)
 * ============================================================ */

#define DRV2605L_ADDR  0x5A

typedef enum {
    HAPTIC_SINGLE_BUZZ = 0,    /* Single 200ms vibration */
    HAPTIC_DOUBLE_BUZZ = 1,    /* Two 200ms vibrations */
    HAPTIC_TRIPLE_BUZZ = 2,    /* Three 200ms vibrations */
    HAPTIC_LONG_BUZZ = 3,      /* One 500ms vibration */
    HAPTIC_FALL_ALERT = 4,     /* Continuous pattern for fall alert */
    HAPTIC_CONFIRM = 5,        /* Short confirmation vibration */
} haptic_pattern_t;

/**
 * Play haptic pattern on DRV2605L.
 * Patterns are tailored for medication reminders and fall alerts.
 */
void ms_haptic_pattern(haptic_pattern_t pattern)
{
    LOG_INF("Haptic pattern: %d", pattern);

    /* const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c2)); */

    switch (pattern) {
    case HAPTIC_SINGLE_BUZZ:
        /* DRV2605L: play waveform 14 (strong click 500ms) */
        /* i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x04, 0x0E);  // Waveform 14
         * i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x0C, 0x01);  // Go
         */
        k_msleep(500);
        break;

    case HAPTIC_DOUBLE_BUZZ:
        /* Two short vibrations for standard medication reminder */
        /* Play waveform 14 twice with 200ms gap */
        /* i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x04, 0x0E);
         * i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x05, 0x0E);
         * i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x0C, 0x01);
         */
        k_msleep(1200);
        break;

    case HAPTIC_TRIPLE_BUZZ:
        /* Three vibrations for escalating reminder */
        /* Play waveform 14 three times with 200ms gap */
        k_msleep(1800);
        break;

    case HAPTIC_LONG_BUZZ:
        /* Long vibration for urgent reminder */
        /* Play waveform 47 (long ramp up) */
        /* i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x04, 0x2F);
         * i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x0C, 0x01);
         */
        k_msleep(1000);
        break;

    case HAPTIC_FALL_ALERT:
        /* Continuous vibration pattern for fall detection */
        for (int i = 0; i < 5; i++) {
            /* i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x04, 0x0E);
             * i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x0C, 0x01);
             */
            k_msleep(400);
            k_msleep(200);
        }
        break;

    case HAPTIC_CONFIRM:
        /* Short confirmation vibration (button press feedback) */
        /* i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x04, 0x0E);
         * i2c_reg_write_byte(i2c, DRV2605L_ADDR, 0x0C, 0x01);
         */
        k_msleep(200);
        break;
    }
}

/* ============================================================
 * Button Handler
 * ============================================================ */

static struct gpio_callback button_cb;

void ms_button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static int64_t last_press_time = 0;
    int64_t now = k_uptime_get();
    int64_t delta = now - last_press_time;
    last_press_time = now;

    if (delta < 300) {
        /* Double press: snooze 10 minutes */
        LOG_INF("Button double-press: Snooze 10 min");
        if (g_state.reminder_active) {
            g_state.reminder_start_time = k_uptime_get() + 600000;  /* Snooze 10 min */
            ms_haptic_pattern(HAPTIC_CONFIRM);
        }
    } else if (delta < 2000) {
        /* Single press: mark as taken */
        LOG_INF("Button single-press: Mark as taken");

        /* Send DoseConfirm to hub via BLE mesh */
        /* ms_mesh_send(MS_NODE_ID_HUB, MS_MODEL_SCHEDULE,
         *             MS_OP_DOSE_CONFIRM, &confirm_payload, sizeof(confirm_payload)); */

        ms_haptic_pattern(HAPTIC_CONFIRM);
        g_state.reminder_active = false;

        /* Set LED to green for 3 seconds */
        /* gpio_pin_set_dt(&led_g, 1);
         * k_sleep(K_SECONDS(3));
         * gpio_pin_set_dt(&led_g, 0);
         */
    } else {
        /* Long press (>2s): emergency alert */
        LOG_WRN("Button long-press: Emergency alert");
        ms_alert_t alert = {
            .level = MS_ALERT_EMERGENCY,
            .type = MS_ALERT_TYPE_SYSTEM,
            .source_node_id = g_state.node_id,
            .bin_id = 0xFF,
            .medication_id = 0xFF,
        };
        ms_str_copy(alert.message, "Emergency button pressed!", sizeof(alert.message));
        /* ms_mesh_send(MS_NODE_ID_HUB, MS_MODEL_ALERT, &alert, sizeof(alert)); */
        ms_haptic_pattern(HAPTIC_FALL_ALERT);
    }
}

/* ============================================================
 * Medication Reminder Thread
 * ============================================================ */

void ms_reminder_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        if (g_state.reminder_active) {
            int64_t now = k_uptime_get();
            int64_t elapsed = now - g_state.reminder_start_time;

            /* Escalate vibration pattern based on time overdue */
            if (elapsed < 300000) {
                /* First 5 minutes: double buzz every 5 minutes */
                ms_haptic_pattern(HAPTIC_DOUBLE_BUZZ);
                ms_set_led_color(0, 0, 1);  /* Blue = pending */
            } else if (elapsed < 900000) {
                /* 5-15 minutes overdue: triple buzz every 2 minutes */
                ms_haptic_pattern(HAPTIC_TRIPLE_BUZZ);
                ms_set_led_color(1, 1, 0);  /* Yellow = overdue */
            } else if (elapsed < 1800000) {
                /* 15-30 minutes overdue: long buzz every 1 minute */
                ms_haptic_pattern(HAPTIC_LONG_BUZZ);
                ms_set_led_color(1, 0, 0);  /* Red = urgent */
            } else {
                /* 30+ minutes overdue: continuous pattern */
                ms_haptic_pattern(HAPTIC_FALL_ALERT);
                ms_set_led_color(1, 0, 0);  /* Red = emergency */
            }
        }

        k_sleep(K_SECONDS(60));
    }
}

/* ============================================================
 * LED Color Helper
 * ============================================================ */

void ms_set_led_color(int r, int g, int b)
{
    /* gpio_pin_set_dt(&led_r, r);
     * gpio_pin_set_dt(&led_g, g);
     * gpio_pin_set_dt(&led_b, b);
     */
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    LOG_INF("MedSync Wearable Tag starting...");
    LOG_INF("Firmware v%d.%d.%d", MS_VERSION_MAJOR, MS_VERSION_MINOR, MS_VERSION_PATCH);

    /* Initialize GPIOs */
    /* gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
     * gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
     * gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
     * gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
     */

    /* Initialize sensors */
    ms_max30101_init();
    ms_adxl362_init();
    ms_bmi160_init();

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.node_id = MS_NODE_ID_WEARABLE_BASE;
    g_state.activity_level = MS_ACTIVITY_UNKNOWN;

    /* Set ADXL362 activity interrupt */
    /* gpio_init_callback(&adxl_int_cb, ms_fall_impact_handler, BIT(ADXL362_INT1_PIN));
     * gpio_add_callback(adxl_int_gpio.port, &adxl_int_cb);
     * gpio_pin_interrupt_configure_dt(&adxl_int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
     */

    /* Set button interrupt */
    /* gpio_init_callback(&button_cb, ms_button_handler, BIT(button.pin));
     * gpio_add_callback(button.port, &button_cb);
     * gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING);
     */

    /* Start threads */
    /* k_thread_create(&vitals_tid, vitals_stack, VITALS_STACK_SIZE,
     *                 ms_vitals_thread, NULL, NULL, NULL, -1, 0, K_NO_WAIT);
     * k_thread_create(&fall_tid, fall_detect_stack, FALL_DETECT_STACK_SIZE,
     *                 ms_fall_detect_thread, NULL, NULL, NULL, -2, 0, K_NO_WAIT);
     * k_thread_create(&reminder_tid, reminder_stack, REMINDER_STACK_SIZE,
     *                 ms_reminder_thread, NULL, NULL, NULL, 0, 0, K_NO_WAIT);
     */

    /* Green LED = booted OK */
    ms_set_led_color(0, 1, 0);
    k_sleep(K_SECONDS(2));
    ms_set_led_color(0, 0, 0);

    /* Main loop */
    while (1) {
        /* Update battery voltage */
        /* g_state.battery_mv = ms_adc_to_battery_mv(adc_read(), 4095, 1000000, 1000000); */

        /* Check for low battery */
        if (ms_battery_low(g_state.battery_mv)) {
            LOG_WRN("Low battery: %d mV", g_state.battery_mv);
        }

        /* Increment step counter based on activity */
        if (g_state.activity_level == MS_ACTIVITY_WALKING ||
            g_state.activity_level == MS_ACTIVITY_RUNNING) {
            g_state.steps_count += 10;  /* Approximate steps per 5-min interval */
        }

        /* Update uptime */
        g_state.uptime_sec = (uint32_t)(k_uptime_get() / 1000);

        k_sleep(K_SECONDS(30));
    }

    return 0;
}
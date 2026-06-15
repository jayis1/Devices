/*
 * ErgoFlow — Desk Controller Node Main
 * STM32G070CB based motorized desk + lighting controller
 *
 * Responsibilities:
 *   - Closed-loop PID desk height control
 *   - Monitor tilt servo control
 *   - WS2812B ambient lighting (circadian + manual)
 *   - Hall sensor position feedback
 *   - Motor current monitoring (stall detection)
 *   - Endstop safety (hardware interrupt)
 *   - BLE mesh command receiver
 *   - Manual button control (hardware override)
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

#include "common/ble_mesh/mesh_config.h"
#include "common/ble_mesh/mesh_handler.h"
#include "common/ble_mesh/protocol.h"
#include "common/sensors/i2c_bus.h"
#include "common/sensors/ina219.h"

LOG_MODULE_REGISTER(desk_main, CONFIG_ERGO_LOG_LEVEL);

/* ── Configuration ──────────────────────────────────────────────────── */
#define DESK_HEIGHT_MIN_MM      650    /* Minimum desk height */
#define DESK_HEIGHT_MAX_MM      1250   /* Maximum desk height */
#define DESK_SPEED_MAX_MM_S     40     /* Max speed: 40mm/s */
#define DESK_SPEED_DEFAULT_PCT  70     /* Default speed: 70% */
#define DESK_TOLERANCE_MM       5      /* Height tolerance for target */
#define PID_KP                  2.0f
#define PID_KI                  0.5f
#define PID_KD                  0.1f
#define CONTROL_LOOP_MS         20     /* 50 Hz control loop */
#define HEARTBEAT_MS            60000
#define LIGHTING_UPDATE_MS      100   /* 10 Hz LED update */
#define MOTOR_STALL_CURRENT_MA  3000   /* Stall detection threshold */
#define MONITOR_TILT_MAX        15     /* ±15 degrees */
#define MONITOR_TILT_DEFAULT    0      /* Default tilt: level */

/* ── Preset heights ─────────────────────────────────────────────────── */
#define PRESET_SIT_MM           720    /* Sitting height */
#define PRESET_STAND_MM         1100   /* Standing height */
#define PRESET_CUSTOM_MM        900    /* Custom height (user-defined) */

/* ── Motor states ───────────────────────────────────────────────────── */
#define MOTOR_IDLE      0
#define MOTOR_MOVING_UP 1
#define MOTOR_MOVING_DOWN 2
#define MOTOR_ERROR     3

/* ── PID controller ─────────────────────────────────────────────────── */
typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
    float output;
    uint32_t last_time;
} pid_controller_t;

/* ── Global state ──────────────────────────────────────────────────── */
static struct {
    uint16_t current_height_mm;
    uint16_t target_height_mm;
    uint8_t motor_state;
    uint16_t motor_current_ma;
    int8_t monitor_tilt_deg;
    uint8_t speed_pct;
    uint8_t lighting_r, lighting_g, lighting_b, lighting_w;
    uint8_t lighting_brightness;
    uint8_t lighting_mode;
    uint8_t battery_pct;
    bool endstop_top;
    bool endstop_bottom;
    bool manual_override;
    pid_controller_t pid;
} desk_state = {
    .current_height_mm = 750,
    .target_height_mm = 750,
    .motor_state = MOTOR_IDLE,
    .motor_current_ma = 0,
    .monitor_tilt_deg = 0,
    .speed_pct = DESK_SPEED_DEFAULT_PCT,
    .lighting_r = 220, .lighting_g = 220, .lighting_b = 255,
    .lighting_w = 170, .lighting_brightness = 80,
    .lighting_mode = 1,
    .battery_pct = 100,
    .endstop_top = false,
    .endstop_bottom = false,
    .manual_override = false,
};

static const struct device *gpio_dev;

/* ── PID controller ─────────────────────────────────────────────────── */
static void pid_init(pid_controller_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output = 0.0f;
    pid->last_time = k_uptime_get_32();
}

static float pid_update(pid_controller_t *pid, float error)
{
    uint32_t now = k_uptime_get_32();
    float dt = (float)(now - pid->last_time) / 1000.0f;
    if (dt < 0.001f) dt = 0.001f;
    pid->last_time = now;

    pid->integral += error * dt;
    /* Anti-windup: clamp integral */
    if (pid->integral > 1000.0f) pid->integral = 1000.0f;
    if (pid->integral < -1000.0f) pid->integral = -1000.0f;

    float derivative = (error - pid->prev_error) / dt;
    pid->prev_error = error;

    pid->output = (pid->kp * error) + (pid->ki * pid->integral) + (pid->kd * derivative);
    return pid->output;
}

/* ── Hall sensor position reading ────────────────────────────────────── */
static uint16_t read_hall_position(void)
{
    /* In production: read ADC from two Hall effect sensors
     * that track a magnetic strip on the desk column.
     * Interpolate between sensors for precise height.
     * For now: return simulated position that tracks target */
    return desk_state.current_height_mm;
}

/* ── Motor control ──────────────────────────────────────────────────── */
static void motor_set_direction(int direction)
{
    /* direction: 0=stop, 1=up, -1=down */
    if (direction == 0) {
        /* Stop motor */
        gpio_pin_set(gpio_dev, 10, 0);  /* DRV8871 IN1 */
        gpio_pin_set(gpio_dev, 11, 0);  /* DRV8871 IN2 */
        desk_state.motor_state = MOTOR_IDLE;
    } else if (direction > 0) {
        /* Move up */
        if (!desk_state.endstop_top) {
            gpio_pin_set(gpio_dev, 10, 1);  /* IN1 high */
            gpio_pin_set(gpio_dev, 11, 0);  /* IN2 low */
            desk_state.motor_state = MOTOR_MOVING_UP;
        }
    } else {
        /* Move down */
        if (!desk_state.endstop_bottom) {
            gpio_pin_set(gpio_dev, 10, 0);  /* IN1 low */
            gpio_pin_set(gpio_dev, 11, 1);  /* IN2 high */
            desk_state.motor_state = MOTOR_MOVING_DOWN;
        }
    }
}

static void motor_stop(void)
{
    motor_set_direction(0);
}

/* ── Endstop interrupts ─────────────────────────────────────────────── */
static void endstop_top_isr(const struct device *dev, gpio_pin_t pin, void *data)
{
    desk_state.endstop_top = true;
    motor_stop();  /* Emergency stop */
    LOG_WRN("Top endstop hit!");
}

static void endstop_bottom_isr(const struct device *dev, gpio_pin_t pin, void *data)
{
    desk_state.endstop_bottom = true;
    motor_stop();  /* Emergency stop */
    LOG_WRN("Bottom endstop hit!");
}

static void button_up_isr(const struct device *dev, gpio_pin_t pin, void *data)
{
    desk_state.manual_override = true;
    desk_state.target_height_mm = DESK_HEIGHT_MAX_MM - 50;  /* Near top */
}

static void button_down_isr(const struct device *dev, gpio_pin_t pin, void *data)
{
    desk_state.manual_override = true;
    desk_state.target_height_mm = DESK_HEIGHT_MIN_MM + 50;  /* Near bottom */
}

/* ── WS2812B lighting ──────────────────────────────────────────────── */
static void update_lighting(void)
{
    /* In production: drive WS2812B strip via SPI or bitbang
     * For now: placeholder for LED strip update */
    /* Convert RGBW + brightness to actual LED values */
    float b = desk_state.lighting_brightness / 100.0f;
    /* 60 LEDs in 1m strip */
    LOG_DBG("Lighting: R=%d G=%d B=%d W=%d brightness=%d%%",
            desk_state.lighting_r, desk_state.lighting_g,
            desk_state.lighting_b, desk_state.lighting_w,
            desk_state.lighting_brightness);
}

/* ── Monitor tilt servo ─────────────────────────────────────────────── */
static void set_monitor_tilt(int8_t degrees)
{
    if (degrees < -MONITOR_TILT_MAX) degrees = -MONITOR_TILT_MAX;
    if (degrees > MONITOR_TILT_MAX) degrees = MONITOR_TILT_MAX;
    desk_state.monitor_tilt_deg = degrees;

    /* In production: drive PCA9685 PWM to servo
     * Servo range: -15° to +15° maps to 0.5ms-2.5ms pulse */
    float pulse_ms = 1.5f + (degrees / 15.0f) * 1.0f;
    (void)pulse_ms;  /* Used in production PWM driver */
}

/* ── BLE mesh message callback ──────────────────────────────────────── */
static void desk_mesh_callback(uint16_t opcode, const uint8_t *data,
                                uint16_t len, uint16_t src_addr, void *user_data)
{
    switch (opcode) {
        case ERGO_OP_DESK_COMMAND: {
            ergo_desk_command_t cmd;
            if (ergo_unpack_desk_command(data, len, &cmd) != 0) return;

            switch (cmd.cmd) {
                case ERGO_DESK_CMD_HEIGHT:
                    desk_state.target_height_mm = cmd.target_mm;
                    desk_state.speed_pct = cmd.speed_pct;
                    desk_state.manual_override = false;
                    LOG_INF("Desk height command: %dmm at %d%%",
                            cmd.target_mm, cmd.speed_pct);
                    break;
                case ERGO_DESK_CMD_PRESET:
                    switch (cmd.target_mm) {
                        case ERGO_DESK_PRESET_SIT:
                            desk_state.target_height_mm = PRESET_SIT_MM;
                            break;
                        case ERGO_DESK_PRESET_STAND:
                            desk_state.target_height_mm = PRESET_STAND_MM;
                            break;
                        case ERGO_DESK_PRESET_CUSTOM:
                            desk_state.target_height_mm = PRESET_CUSTOM_MM;
                            break;
                    }
                    desk_state.speed_pct = cmd.speed_pct;
                    desk_state.manual_override = false;
                    LOG_INF("Desk preset: %d at %d%%", cmd.target_mm, cmd.speed_pct);
                    break;
                case ERGO_DESK_CMD_STOP:
                    motor_stop();
                    desk_state.target_height_mm = desk_state.current_height_mm;
                    LOG_INF("Desk stopped");
                    break;
            }
            break;
        }
        case ERGO_OP_LIGHTING_CMD: {
            ergo_lighting_cmd_t cmd;
            if (ergo_unpack_lighting_cmd(data, len, &cmd) != 0) return;
            desk_state.lighting_r = cmd.r;
            desk_state.lighting_g = cmd.g;
            desk_state.lighting_b = cmd.b;
            desk_state.lighting_w = cmd.w;
            desk_state.lighting_brightness = cmd.brightness_pct;
            desk_state.lighting_mode = cmd.mode;
            LOG_INF("Lighting: R=%d G=%d B=%d W=%d bri=%d%% mode=%d",
                    cmd.r, cmd.g, cmd.b, cmd.w, cmd.brightness_pct, cmd.mode);
            break;
        }
        case ERGO_OP_MONITOR_TILT: {
            ergo_monitor_tilt_t tilt;
            if (ergo_unpack_monitor_tilt(data, len, &tilt) != 0) return;
            set_monitor_tilt(tilt.tilt_degrees);
            LOG_INF("Monitor tilt: %d degrees", tilt.tilt_degrees);
            break;
        }
        case ERGO_OP_CALIBRATION: {
            ergo_calibration_t cal;
            if (ergo_unpack_calibration(data, len, &cal) != 0) return;
            if (cal.target == 2) {  /* Desk calibration */
                /* Calibrate current position as reference */
                desk_state.current_height_mm = cal.param1;
                LOG_INF("Desk calibrated: height=%dmm", cal.param1);
            }
            break;
        }
        case ERGO_OP_FACTORY_RESET:
            motor_stop();
            desk_state.target_height_mm = PRESET_SIT_MM;
            set_monitor_tilt(0);
            LOG_INF("Factory reset");
            break;
    }
}

/* ── Control loop thread ────────────────────────────────────────────── */
static void control_loop(void *p1, void *p2, void *p3)
{
    LOG_INF("Desk control loop started");
    pid_init(&desk_state.pid, PID_KP, PID_KI, PID_KD);

    while (1) {
        /* Read current height from Hall sensors */
        desk_state.current_height_mm = read_hall_position();

        /* Read motor current */
        float current;
        if (ina219_read_current(&current) == 0) {
            desk_state.motor_current_ma = (uint16_t)(current * 1000);
        }

        /* Check for motor stall */
        if (desk_state.motor_current_ma > MOTOR_STALL_CURRENT_MA) {
            motor_stop();
            desk_state.motor_state = MOTOR_ERROR;
            LOG_ERR("Motor stall detected! Current: %dmA", desk_state.motor_current_ma);
        }

        /* Clear endstops if not at limit */
        if (desk_state.current_height_mm > DESK_HEIGHT_MIN_MM + 20) {
            desk_state.endstop_bottom = false;
        }
        if (desk_state.current_height_mm < DESK_HEIGHT_MAX_MM - 20) {
            desk_state.endstop_top = false;
        }

        /* PID control for height */
        if (desk_state.motor_state != MOTOR_ERROR) {
            float error = (float)desk_state.target_height_mm - (float)desk_state.current_height_mm;

            if (fabsf(error) > DESK_TOLERANCE_MM) {
                float pid_out = pid_update(&desk_state.pid, error);
                int direction = (pid_out > 0) ? 1 : -1;
                motor_set_direction(direction);
            } else {
                motor_stop();
                pid_init(&desk_state.pid, PID_KP, PID_KI, PID_KD);
            }
        }

        /* Update position (simulate movement for now) */
        if (desk_state.motor_state == MOTOR_MOVING_UP) {
            desk_state.current_height_mm += (DESK_SPEED_MAX_MM_S * desk_state.speed_pct / 100)
                                            * (CONTROL_LOOP_MS / 1000);
            if (desk_state.current_height_mm > DESK_HEIGHT_MAX_MM)
                desk_state.current_height_mm = DESK_HEIGHT_MAX_MM;
        } else if (desk_state.motor_state == MOTOR_MOVING_DOWN) {
            desk_state.current_height_mm -= (DESK_SPEED_MAX_MM_S * desk_state.speed_pct / 100)
                                            * (CONTROL_LOOP_MS / 1000);
            if (desk_state.current_height_mm < DESK_HEIGHT_MIN_MM)
                desk_state.current_height_mm = DESK_HEIGHT_MIN_MM;
        }

        k_msleep(CONTROL_LOOP_MS);
    }
}

/* ── Status transmit thread ─────────────────────────────────────────── */
static void status_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        k_msleep(2000);  /* 0.5 Hz status */

        ergo_desk_status_t status = {
            .height_mm = desk_state.current_height_mm,
            .motor_state = desk_state.motor_state,
            .current_ma = desk_state.motor_current_ma,
        };
        uint8_t buf[8];
        uint16_t len;
        ergo_pack_desk_status(&status, buf, &len);
        mesh_handler_send(ERGO_OP_DESK_STATUS, buf, len, ERGO_ADDR_HUB);

        /* Heartbeat */
        ergo_node_heartbeat_t hb = {
            .battery_pct = desk_state.battery_pct,
            .state = ERGO_STATE_RUNNING,
            .uptime_min = (uint16_t)(k_uptime_get() / 60000),
        };
        ergo_pack_node_heartbeat(&hb, buf, &len);
        mesh_handler_send(ERGO_OP_NODE_HEARTBEAT, buf, len, ERGO_ADDR_HUB);
    }
}

/* ── Lighting thread ───────────────────────────────────────────────── */
static void lighting_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        update_lighting();
        k_msleep(LIGHTING_UPDATE_MS);
    }
}

/* ── Main ───────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("ErgoFlow Desk Controller starting...");

    gpio_dev = device_get_binding("GPIO_0");

    /* Initialize I2C */
    i2c_bus_init();
    ina219_init(INA219_ADDR_GND);

    /* Initialize mesh */
    mesh_handler_init();
    mesh_handler_register_callback(0xFFFF, desk_mesh_callback, NULL);

    /* Configure GPIO */
    /* Motor control pins */
    gpio_pin_configure(gpio_dev, 10, GPIO_OUTPUT_LOW);  /* DRV8871 IN1 */
    gpio_pin_configure(gpio_dev, 11, GPIO_OUTPUT_LOW);  /* DRV8871 IN2 */
    gpio_pin_configure(gpio_dev, 22, GPIO_OUTPUT_LOW);  /* Motor enable */

    /* Endstop inputs with interrupts */
    gpio_pin_configure(gpio_dev, 2, GPIO_INPUT | GPIO_PULL_UP);  /* Top endstop */
    gpio_pin_configure(gpio_dev, 3, GPIO_INPUT | GPIO_PULL_UP);  /* Bottom endstop */
    gpio_pin_interrupt_configure(gpio_dev, 2, GPIO_INT_EDGE_FALLING);
    gpio_pin_interrupt_configure(gpio_dev, 3, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&endstop_top_cb, endstop_top_isr, BIT(2));
    gpio_init_callback(&endstop_bottom_cb, endstop_bottom_isr, BIT(3));
    gpio_add_callback(gpio_dev, &endstop_top_cb);
    gpio_add_callback(gpio_dev, &endstop_bottom_cb);

    /* Manual buttons with interrupts */
    gpio_pin_configure(gpio_dev, 16, GPIO_INPUT | GPIO_PULL_UP);  /* Up button */
    gpio_pin_configure(gpio_dev, 17, GPIO_INPUT | GPIO_PULL_UP);  /* Down button */
    gpio_pin_interrupt_configure(gpio_dev, 16, GPIO_INT_EDGE_FALLING);
    gpio_pin_interrupt_configure(gpio_dev, 17, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_up_cb, button_up_isr, BIT(16));
    gpio_init_callback(&button_down_cb, button_down_isr, BIT(17));
    gpio_add_callback(gpio_dev, &button_up_cb);
    gpio_add_callback(gpio_dev, &button_down_cb);

    /* Start threads */
    K_THREAD_DEFINE(control_tid, 2048, control_loop, NULL, NULL, NULL, -1, 0, 0);
    K_THREAD_DEFINE(status_tid, 1024, status_thread, NULL, NULL, NULL, 3, 0, 0);
    K_THREAD_DEFINE(lighting_tid, 1024, lighting_thread, NULL, NULL, NULL, 5, 0, 0);

    LOG_INF("Desk Controller running");

    while (1) {
        k_msleep(10000);
    }

    return 0;
}
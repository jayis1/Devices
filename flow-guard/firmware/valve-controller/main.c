/**
 * FlowGuard - Valve Controller Firmware
 * nRF52832 (Zigbee 3.0 router + motorized valve control)
 *
 * Safety-critical node: Controls main water shutoff valve.
 * Failsafe design: valve spring-returns to CLOSED on power loss.
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>

#include "fg_protocol.h"
#include "fg_util.h"

LOG_MODULE_REGISTER(valve_ctrl, CONFIG_FG_VALVE_LOG_LEVEL);

/* ============================================================
 * GPIO Pin Definitions
 * ============================================================ */

/* Motor driver (DRV8871) */
#define MOTOR_A_PIN     DT_ALIAS(motor_a)   /* P0.08 - DRV8871 IN1 */
#define MOTOR_B_PIN     DT_ALIAS(motor_b)   /* P0.09 - DRV8871 IN2 */
#define MOTOR_EN_PIN    DT_ALIAS(motor_en)   /* P0.10 - DRV8871 EN */
#define MOTOR_FAULT_PIN DT_ALIAS(motor_fault) /* P0.11 - DRV8871 nFAULT */

/* Valve limit switches */
#define VALVE_OPEN_PIN   DT_ALIAS(valve_open)   /* P0.14 - Open limit switch */
#define VALVE_CLOSE_PIN DT_ALIAS(valve_closed)  /* P0.15 - Closed limit switch */

/* Sensors */
#define FLOW_PIN        DT_ALIAS(flow_pulse)  /* P0.04 - YF-S201 flow meter */
#define PRESSURE_PIN    DT_ALIAS(pressure)    /* P0.02 - MPX5700DP analog */
#define DIFF_PRESS_PIN  DT_ALIAS(diff_pressure) /* P0.03 - XGZP6847A analog */
#define TEMP_PIN        DT_ALIAS(temp_1wire)  /* P0.05 - DS18B20 */
#define HEATER_TEMP_PIN DT_ALIAS(heater_temp)  /* P0.13 - DS18B20 (heat trace) */

/* Heater control */
#define HEATER_PWM_PIN  DT_ALIAS(heater_pwm)  /* P0.12 - Heat trace PWM */

/* User interface */
#define MANUAL_BTN_PIN  DT_ALIAS(manual_btn)  /* P0.16 - Manual override */
#define LED_R_PIN       DT_ALIAS(led_r)      /* P0.17 */
#define LED_G_PIN       DT_ALIAS(led_g)      /* P0.18 */
#define LED_B_PIN       DT_ALIAS(led_b)      /* P0.19 */
#define BUZZER_PIN      DT_ALIAS(buzzer)     /* P0.22 */

/* Power monitoring */
#define VBAT_PIN        DT_ALIAS(vbat_sense) /* P0.20 - Battery voltage */
#define V12_PIN          DT_ALIAS(v12_sense)  /* P0.21 - 12V supply monitor */

static const struct gpio_dt_spec motor_a = GPIO_DT_SPEC_GET(MOTOR_A_PIN, gpios);
static const struct gpio_dt_spec motor_b = GPIO_DT_SPEC_GET(MOTOR_B_PIN, gpios);
static const struct gpio_dt_spec motor_en = GPIO_DT_SPEC_GET(MOTOR_EN_PIN, gpios);
static const struct gpio_dt_spec valve_open_sw = GPIO_DT_SPEC_GET(VALVE_OPEN_PIN, gpios);
static const struct gpio_dt_spec valve_close_sw = GPIO_DT_SPEC_GET(VALVE_CLOSE_PIN, gpios);
static const struct gpio_dt_spec manual_btn = GPIO_DT_SPEC_GET(MANUAL_BTN_PIN, gpios);
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_R_PIN, gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(LED_G_PIN, gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(LED_B_PIN, gpios);
static const struct gpio_dt_spec buzzer = GPIO_DT_SPEC_GET(BUZZER_PIN, gpios);

/* ============================================================
 * Valve State Machine
 * ============================================================ */

typedef enum {
    VALVE_STATE_CLOSED,
    VALVE_STATE_OPENING,
    VALVE_STATE_OPEN,
    VALVE_STATE_CLOSING,
    VALVE_STATE_ERROR_MOTOR,
    VALVE_STATE_ERROR_TIMEOUT,
} valve_state_machine_t;

static valve_state_machine_t valve_state = VALVE_STATE_CLOSED;
static fg_valve_reason_t close_reason = FG_REASON_USER_MANUAL;
static int64_t valve_operation_start_time = 0;
static uint32_t motor_current_samples = 0;
static uint32_t motor_current_sum = 0;

/* ============================================================
 * Flow Meter (YF-S201)
 * ============================================================ */

static volatile uint32_t flow_pulse_count = 0;
static volatile int64_t flow_last_pulse_time = 0;
static uint32_t flow_total_pulses = 0;

void flow_pulse_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    flow_pulse_count++;
    flow_last_pulse_time = k_uptime_get();
}

static struct gpio_callback flow_cb;

/* ============================================================
 * Valve Motor Control
 * ============================================================ */

/**
 * Drive valve motor to open position.
 * Uses DRV8871 H-bridge: IN1=high, IN2=low = forward (open)
 */
static int valve_motor_open(void)
{
    gpio_pin_set_dt(&motor_a, 1);
    gpio_pin_set_dt(&motor_b, 0);
    gpio_pin_set_dt(&motor_en, 1);
    valve_state = VALVE_STATE_OPENING;
    valve_operation_start_time = k_uptime_get();
    LOG_INF("Valve motor: OPENING");
    return 0;
}

/**
 * Drive valve motor to close position.
 * Uses DRV8871 H-bridge: IN1=low, IN2=high = reverse (close)
 */
static int valve_motor_close(void)
{
    gpio_pin_set_dt(&motor_a, 0);
    gpio_pin_set_dt(&motor_b, 1);
    gpio_pin_set_dt(&motor_en, 1);
    valve_state = VALVE_STATE_CLOSING;
    valve_operation_start_time = k_uptime_get();
    LOG_INF("Valve motor: CLOSING");
    return 0;
}

/**
 * Stop valve motor (coast / high-impedance).
 */
static void valve_motor_stop(void)
{
    gpio_pin_set_dt(&motor_en, 0);
    gpio_pin_set_dt(&motor_a, 0);
    gpio_pin_set_dt(&motor_b, 0);
    LOG_INF("Valve motor: STOPPED");
}

/**
 * Emergency brake (slow decay / braking).
 * DRV8871: IN1=low, IN2=low, EN=high = brake
 */
static void valve_motor_brake(void)
{
    gpio_pin_set_dt(&motor_a, 0);
    gpio_pin_set_dt(&motor_b, 0);
    gpio_pin_set_dt(&motor_en, 1);
    k_sleep(K_MSEC(100));  /* Brake for 100ms */
    valve_motor_stop();
}

/* ============================================================
 * Manual Override Button Handler
 * ============================================================ */

static struct gpio_callback manual_btn_cb;
static int64_t manual_btn_press_time = 0;

void manual_btn_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (gpio_pin_get_dt(&manual_btn)) {
        /* Button pressed */
        manual_btn_press_time = k_uptime_get();
    } else {
        /* Button released */
        int64_t press_duration = k_uptime_get() - manual_btn_press_time;

        if (press_duration > 3000) {
            /* Long press (>3s): Emergency close */
            LOG_WRN("Manual override: EMERGENCY CLOSE");
            valve_motor_close();
            close_reason = FG_REASON_USER_MANUAL;
        } else {
            /* Short press: Toggle valve */
            if (valve_state == VALVE_STATE_OPEN) {
                LOG_INF("Manual override: CLOSING valve");
                valve_motor_close();
                close_reason = FG_REASON_USER_MANUAL;
            } else if (valve_state == VALVE_STATE_CLOSED) {
                LOG_INF("Manual override: OPENING valve");
                valve_motor_open();
            }
        }
    }
}

/* ============================================================
 * Valve State Machine Update
 * ============================================================ */

void valve_state_update(void)
{
    int64_t elapsed = k_uptime_get() - valve_operation_start_time;

    switch (valve_state) {
    case VALVE_STATE_OPENING:
        /* Check if open limit switch is hit */
        if (gpio_pin_get_dt(&valve_open_sw)) {
            valve_motor_brake();
            valve_state = VALVE_STATE_OPEN;
            LOG_INF("Valve: OPEN (limit switch)");
            fg_update_led(VALVE_STATE_OPEN);
        }
        /* Timeout: motor should complete within 5 seconds */
        else if (elapsed > FG_VALVE_OPERATION_TIME_MS) {
            valve_motor_brake();
            valve_state = VALVE_STATE_ERROR_TIMEOUT;
            LOG_ERR("Valve: OPEN TIMEOUT - motor may be stuck");
            fg_update_led(VALVE_STATE_ERROR_MOTOR);
        }
        break;

    case VALVE_STATE_CLOSING:
        /* Check if closed limit switch is hit */
        if (gpio_pin_get_dt(&valve_close_sw)) {
            valve_motor_brake();
            valve_state = VALVE_STATE_CLOSED;
            LOG_INF("Valve: CLOSED (limit switch, reason: %d)", close_reason);
            fg_update_led(VALVE_STATE_CLOSED);
        }
        /* Timeout: motor should complete within 5 seconds */
        else if (elapsed > FG_VALVE_EMERGENCY_TIMEOUT_MS) {
            valve_motor_brake();
            valve_state = VALVE_STATE_ERROR_TIMEOUT;
            LOG_ERR("Valve: CLOSE TIMEOUT - motor may be stuck!");
            fg_update_led(VALVE_STATE_ERROR_MOTOR);
            /* CRITICAL: Valve failed to close!
             * Spring-return should have closed it on power loss,
             * but if the limit switch isn't triggered, something is wrong.
             * Send emergency alert immediately. */
            fg_send_emergency_alert("Valve failed to close!");
        }
        break;

    case VALVE_STATE_ERROR_MOTOR:
    case VALVE_STATE_ERROR_TIMEOUT:
        /* Stay in error state until manual intervention */
        /* Blink red LED rapidly */
        gpio_pin_set_dt(&led_r, (k_uptime_get() / 250) % 2);
        break;

    default:
        break;
    }
}

/* ============================================================
 * LED Status Update
 * ============================================================ */

void fg_update_led(valve_state_machine_t state)
{
    switch (state) {
    case VALVE_STATE_OPEN:
        gpio_pin_set_dt(&led_r, 0);
        gpio_pin_set_dt(&led_g, 1);
        gpio_pin_set_dt(&led_b, 0);
        break;
    case VALVE_STATE_CLOSED:
        gpio_pin_set_dt(&led_r, 1);
        gpio_pin_set_dt(&led_g, 0);
        gpio_pin_set_dt(&led_b, 0);
        break;
    case VALVE_STATE_OPENING:
    case VALVE_STATE_CLOSING:
        gpio_pin_set_dt(&led_r, 0);
        gpio_pin_set_dt(&led_g, 0);
        gpio_pin_set_dt(&led_b, 1);  /* Blue = moving */
        break;
    case VALVE_STATE_ERROR_MOTOR:
    case VALVE_STATE_ERROR_TIMEOUT:
        gpio_pin_set_dt(&led_r, 1);
        gpio_pin_set_dt(&led_g, 0);
        gpio_pin_set_dt(&led_b, 1);  /* Purple = error */
        break;
    }
}

/* ============================================================
 * Pressure and Flow Monitoring
 * ============================================================ */

static int16_t last_pressure_kpa_x10 = 0;

void fg_monitor_pressure_and_flow(void)
{
    /* Read pressure from MPX5700DP */
    int16_t pressure_kpa_x10 = fg_mpx5700_to_kpa_x10(
        adc_read_channel(pressure_pin), 3300);

    /* Read flow meter */
    uint32_t current_pulses = flow_pulse_count;
    flow_pulse_count = 0;
    float flow_hz = (float)current_pulses;  /* Pulses per monitoring interval */
    uint16_t flow_ml_min = fg_pulse_to_flow_ml_min(flow_hz);

    /* Detect rapid pressure drop (burst pipe) */
    int16_t pressure_delta = pressure_kpa_x10 - last_pressure_kpa_x10;
    if (pressure_delta < -1380) {  /* >20 PSI (138 kPa) drop */
        LOG_ERR("PRESSURE DROP: %d kPa in 30s! Possible burst pipe!", -pressure_delta / 10);
        /* Emergency close valve — no hub processing needed */
        valve_motor_close();
        close_reason = FG_REASON_PRESSURE_ANOMALY;
    }

    /* Detect very high flow with no known source */
    if (flow_ml_min > 30000) {  /* >30 L/min = emergency flow rate */
        LOG_ERR("EMERGENCY FLOW: %d mL/min! Shutting valve!", flow_ml_min);
        valve_motor_close();
        close_reason = FG_REASON_LEAK_DETECTED;
    }

    last_pressure_kpa_x10 = pressure_kpa_x10;

    /* Accumulate total gallons */
    flow_total_pulses += current_pulses;
}

/* ============================================================
 * Zigbee Incoming Command Handler
 * ============================================================ */

void fg_zigbee_command_handler(zb_bufid_t bufid)
{
    zb_apsde_data_ind_t *ind = ZB_BUF_GET_PARAM(bufid, zb_apsde_data_ind_t);
    uint8_t *payload = zb_buf_begin(bufid);
    uint16_t payload_len = zb_buf_len(bufid);

    if (payload_len < 1) {
        LOG_WRN("Empty command received");
        zb_buf_free(bufid);
        return;
    }

    uint8_t cmd = payload[0];

    switch (cmd) {
    case FG_CMD_VALVE_CLOSE: {
        if (payload_len < 6) {
            LOG_ERR("Valve close command too short");
            break;
        }
        /* Verify auth token */
        uint8_t *token = &payload[1];
        /* TODO: Verify auth token against stored token */
        fg_valve_reason_t reason = payload[5];
        LOG_INF("Received VALVE_CLOSE command (reason: %d)", reason);
        close_reason = reason;
        valve_motor_close();
        break;
    }

    case FG_CMD_VALVE_OPEN: {
        if (payload_len < 6) {
            LOG_ERR("Valve open command too short");
            break;
        }
        uint8_t *token = &payload[1];
        /* TODO: Verify auth token — 2FA required for opening */
        fg_valve_reason_t reason = payload[5];
        LOG_INF("Received VALVE_OPEN command (reason: %d)", reason);
        valve_motor_open();
        break;
    }

    case FG_CMD_EMERGENCY_SHUTDOWN: {
        LOG_ERR("EMERGENCY SHUTDOWN received!");
        valve_motor_close();
        close_reason = FG_REASON_EMERGENCY;
        /* Activate buzzer continuously */
        gpio_pin_set_dt(&buzzer, 1);
        break;
    }

    default:
        LOG_WRN("Unknown command: 0x%02x", cmd);
        break;
    }

    zb_buf_free(bufid);
}

/* Placeholder for emergency alert */
void fg_send_emergency_alert(const char *msg)
{
    LOG_ERR("EMERGENCY: %s", msg);
    /* This would send via Zigbee to hub */
}

/* Placeholder for ADC read */
uint16_t adc_read_channel(const struct gpio_dt_spec *pin)
{
    /* Would use Zephyr ADC API to read */
    return 2048;  /* Midpoint placeholder */
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    int ret;

    LOG_INF("FlowGuard Valve Controller starting...");
    LOG_INF("Firmware v%d.%d.%d", FG_VERSION_MAJOR, FG_VERSION_MINOR, FG_VERSION_PATCH);

    /* Initialize GPIOs */
    gpio_pin_configure_dt(&motor_a, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&motor_b, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&motor_en, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&valve_open_sw, GPIO_INPUT);
    gpio_pin_configure_dt(&valve_close_sw, GPIO_INPUT);
    gpio_pin_configure_dt(&manual_btn, GPIO_INPUT | GPIO_PULL_UP);

    /* Set initial state: closed (safe default) */
    valve_state = VALVE_STATE_CLOSED;
    fg_update_led(VALVE_STATE_CLOSED);

    /* Setup flow meter interrupt */
    gpio_pin_configure_dt(&GPIO_DT_SPEC_GET(FLOW_PIN, gpios), GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&GPIO_DT_SPEC_GET(FLOW_PIN, gpios), GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&flow_cb, flow_pulse_handler, BIT(GPIO_DT_SPEC_GET(FLOW_PIN, gpios).pin));
    gpio_add_callback(GPIO_DT_SPEC_GET(FLOW_PIN, gpios).port, &flow_cb);

    /* Setup manual override button interrupt */
    gpio_pin_interrupt_configure_dt(&manual_btn, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&manual_btn_cb, manual_btn_handler, BIT(manual_btn.pin));
    gpio_add_callback(manual_btn.port, &manual_btn_cb);

    /* Start Zigbee */
    zigbee_enable();

    LOG_INF("Valve Controller initialized. Valve state: CLOSED (safe default)");

    /* Main loop */
    while (1) {
        /* Update valve state machine */
        valve_state_update();

        /* Monitor pressure and flow every 30 seconds */
        if (k_uptime_get() % 30000 < 10) {
            fg_monitor_pressure_and_flow();
        }

        /* Update status LEDs */
        if (valve_state != VALVE_STATE_ERROR_MOTOR &&
            valve_state != VALVE_STATE_ERROR_TIMEOUT) {
            /* Slow heartbeat on green LED when open */
            if (valve_state == VALVE_STATE_OPEN && (k_uptime_get() / 1000) % 2 == 0) {
                gpio_pin_set_dt(&led_g, 1);
            } else if (valve_state == VALVE_STATE_OPEN) {
                gpio_pin_set_dt(&led_g, 0);
            }
        }

        k_sleep(K_MSEC(100));
    }

    return 0;
}
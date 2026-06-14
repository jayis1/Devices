/*
 * climate_main.c — SleepSync Climate Node (ESP32-C3-MINI-1)
 *
 * Wall-mounted bedroom climate controller:
 * - High-accuracy temperature + humidity sensing (SHT40)
 * - PID temperature control (heating + cooling)
 * - PID humidity control (humidify + dehumidify)
 * - IR blaster for existing HVAC control
 * - Dual relay for portable heater + humidifier
 * - Triac dimming for resistive heater elements
 * - OLED display for current conditions
 * - Receives setpoints from hub via BLE mesh
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "mesh_protocol.h"

/* ---- Pin Definitions ---- */
#define PIN_I2C0_SDA      0
#define PIN_I2C0_SCL      1
#define PIN_I2C1_SDA      2
#define PIN_I2C1_SCL      3
#define PIN_IR_LED         4
#define PIN_RELAY_1        5    /* Heater/fan relay */
#define PIN_RELAY_2        6    /* Humidifier/dehumidifier relay */
#define PIN_TRIAC_PWM      7    /* Triac gate (BTA16) */
#define PIN_ZERO_CROSS    8    /* AC zero-crossing detector */
#define PIN_BTN_MODE       9
#define PIN_LED_STATUS     10

/* ---- PID Controller ---- */

typedef struct {
    float kp;
    float ki;
    float kd;
    float setpoint;
    float integral;
    float prev_error;
    float output_min;
    float output_max;
    float output;
} pid_controller_t;

static void pid_init(pid_controller_t *pid, float kp, float ki, float kd,
                     float setpoint, float out_min, float out_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->setpoint = setpoint;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output_min = out_min;
    pid->output_max = out_max;
    pid->output = 0.0f;
}

static float pid_update(pid_controller_t *pid, float measurement, float dt)
{
    float error = pid->setpoint - measurement;

    /* Proportional */
    float p_term = pid->kp * error;

    /* Integral (with anti-windup) */
    pid->integral += error * dt;
    float i_term = pid->ki * pid->integral;
    if (i_term > pid->output_max) {
        i_term = pid->output_max;
        pid->integral = i_term / pid->ki;
    } else if (i_term < pid->output_min) {
        i_term = pid->output_min;
        pid->integral = i_term / pid->ki;
    }

    /* Derivative */
    float d_term = pid->kd * (error - pid->prev_error) / dt;
    pid->prev_error = error;

    /* Output */
    pid->output = p_term + i_term + d_term;
    if (pid->output > pid->output_max) pid->output = pid->output_max;
    if (pid->output < pid->output_min) pid->output = pid->output_min;

    return pid->output;
}

/* ---- SHT40 Temperature + Humidity Sensor ---- */

#define SHT40_ADDR  0x44

typedef struct {
    float temperature;  /* °C */
    float humidity;     /* %RH */
} sht40_data_t;

static void sht40_init(void)
{
    /* I2C0 initialization */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C0_SDA,
        .scl_io_num = PIN_I2C0_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    /* i2c_param_config(I2C_NUM_0, &conf); */
    /* i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0); */
    printf("[SHT40] Initialized on I2C0\n");
}

static sht40_data_t sht40_read(void)
{
    sht40_data_t data = {20.0f, 45.0f}; /* stub defaults */

    /* In production:
     * 1. Send measurement command 0xFD (high precision) to SHT40
     * 2. Wait ~10ms for measurement
     * 3. Read 6 bytes: temp_msb, temp_lsb, temp_crc, hum_msb, hum_lsb, hum_crc
     * 4. Convert: temp = -45 + 175 * (raw / 65535)
     *            humidity = -6 + 125 * (raw / 65535)
     */

    return data;
}

/* ---- IR Blaster ---- */

static void ir_blaster_init(void)
{
    gpio_set_direction(PIN_IR_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_IR_LED, 0);
    printf("[IR] Blaster initialized\n");
}

static void ir_send_raw(const uint16_t *timings, uint16_t count)
{
    /*
     * In production: modulate IR LED at 38kHz carrier
     * timings[] = mark/space durations in microseconds
     * Even index = mark (LED on), Odd index = space (LED off)
     */
    for (uint16_t i = 0; i < count; i++) {
        if (i % 2 == 0) {
            /* Mark: 38kHz carrier burst */
            gpio_set_level(PIN_IR_LED, 1);
        } else {
            /* Space: LED off */
            gpio_set_level(PIN_IR_LED, 0);
        }
        /* delay_us(timings[i]); */
    }
    gpio_set_level(PIN_IR_LED, 0);
}

/* Pre-learned IR codes (populated during setup) */
#define MAX_IR_CODES 20
typedef struct {
    uint8_t  code_id;       /* 0=power, 1=temp_up, 2=temp_down, 3=cool, 4=heat, 5=fan_low, 6=fan_high */
    uint16_t timings[200];  /* Raw mark/space timings */
    uint16_t timing_count;
} ir_code_t;

static ir_code_t ir_codes[MAX_IR_CODES];
static uint8_t num_ir_codes = 0;

/* ---- Relay Control ---- */

static void relay_init(void)
{
    gpio_set_direction(PIN_RELAY_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RELAY_2, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RELAY_1, 0);
    gpio_set_level(PIN_RELAY_2, 0);
    printf("[RELAY] 2× relays initialized\n");
}

static void relay_set(uint8_t relay, bool on)
{
    uint8_t pin = (relay == 1) ? PIN_RELAY_1 : PIN_RELAY_2;
    gpio_set_level(pin, on ? 1 : 0);
    printf("[RELAY] Relay %d: %s\n", relay, on ? "ON" : "OFF");
}

/* ---- Triac Dimmer (Phase-angle control) ---- */

static void triac_init(void)
{
    gpio_set_direction(PIN_TRIAC_PWM, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_ZERO_CROSS, GPIO_MODE_INPUT);
    printf("[TRIAC] BTA16 triac dimmer initialized\n");
}

static void triac_set_power(uint8_t percent)
{
    /*
     * In production: use zero-crossing interrupt + timer delay
     * - Detect AC zero crossing on PIN_ZERO_CROSS
     * - Wait for phase angle delay (0-100% maps to 0-8.3ms for 60Hz)
     * - Fire triac gate pulse (PIN_TRIAC_PWM)
     * - Higher percent = earlier firing = more power
     */
    printf("[TRIAC] Power set to %d%%\n", percent);
}

/* ---- Safety Limits ---- */

#define TEMP_MAX_LIMIT       28.0f  /* Never heat above 28°C for sleep */
#define TEMP_MIN_LIMIT       14.0f  /* Never cool below 14°C */
#define HUM_MAX_LIMIT        70.0f  /* Never humidify above 70% */
#define HUM_MIN_LIMIT        25.0f  /* Never dehumidify below 25% */
#define SENSOR_FAIL_TIMEOUT  60000  /* 60s without valid reading → safety shutoff */

/* ---- OLED Display (SH1106) ---- */

static void oled_init(void)
{
    /* I2C1 at 0x3C */
    printf("[OLED] SH1106 initialized on I2C1\n");
}

static void oled_display(float temp, float hum, float temp_sp, float hum_sp)
{
    /* In production: render current temp/humidity + setpoints on 128×64 OLED */
    printf("[OLED] %.1f°C/%.1f%%RH (target: %.1f°C/%.1f%%RH)\n",
           temp, hum, temp_sp, hum_sp);
}

/* ---- BLE Mesh Client ---- */

static env_data_payload_t latest_env = {0};
static float target_temp = 19.0f;
static float target_hum = 45.0f;

static void ble_mesh_init(void)
{
    printf("[BLE] Mesh client initialized\n");
}

static void ble_send_env_data(void)
{
    latest_env.temperature = (int16_t)(latest_env.temperature);
    latest_env.humidity = (uint16_t)(latest_env.humidity);

    uint8_t msg[64];
    uint16_t msg_len = mesh_build_message(
        NODE_ID_CLIMATE, NODE_ID_HUB, MSG_ENV_DATA,
        (uint8_t *)&latest_env, sizeof(latest_env),
        msg, sizeof(msg));

    /* In production: send via BLE mesh vendor model */
    printf("[BLE] TX env data: %.2f°C, %.2f%%RH\n",
           latest_env.temperature / 100.0f,
           latest_env.humidity / 100.0f);
}

static void process_hub_command(const hub_command_payload_t *cmd)
{
    switch (cmd->cmd_type) {
    case CMD_CLIMATE_SETPOINT: {
        int16_t temp_q;
        uint16_t hum_q;
        memcpy(&temp_q, &cmd->params[0], 2);
        memcpy(&hum_q, &cmd->params[2], 2);
        target_temp = temp_q / 100.0f;
        target_hum = hum_q / 100.0f;

        /* Clamp to safety limits */
        if (target_temp > TEMP_MAX_LIMIT) target_temp = TEMP_MAX_LIMIT;
        if (target_temp < TEMP_MIN_LIMIT) target_temp = TEMP_MIN_LIMIT;
        if (target_hum > HUM_MAX_LIMIT) target_hum = HUM_MAX_LIMIT;
        if (target_hum < HUM_MIN_LIMIT) target_hum = HUM_MIN_LIMIT;

        printf("[CMD] New setpoints: %.1f°C, %.1f%%RH\n", target_temp, target_hum);
        break;
    }
    case CMD_CLIMATE_HVAC: {
        uint8_t mode = cmd->params[0];
        /* Send learned IR code for HVAC mode */
        if (mode <= num_ir_codes) {
            ir_send_raw(ir_codes[mode].timings, ir_codes[mode].timing_count);
        }
        printf("[CMD] HVAC mode: %d\n", mode);
        break;
    }
    default:
        break;
    }
}

/* ---- Main Control Task ---- */

static pid_controller_t temp_pid, hum_pid;

static void climate_control_task(void *arg)
{
    /* Initialize PID controllers */
    pid_init(&temp_pid, 0.8f, 0.05f, 0.2f, target_temp, 0.0f, 100.0f);
    pid_init(&hum_pid, 0.5f, 0.02f, 0.1f, target_hum, 0.0f, 100.0f);

    float dt = 30.0f; /* 30 second control interval */
    int64_t last_reading_ms = 0;

    while (1) {
        /* Read sensors */
        sht40_data_t sht = sht40_read();

        /* Safety: sensor validation */
        if (sht.temperature < -40.0f || sht.temperature > 80.0f ||
            sht.humidity < 0.0f || sht.humidity > 100.0f) {
            /* Invalid reading — shut off outputs */
            relay_set(1, false);
            relay_set(2, false);
            triac_set_power(0);
            latest_env.errors |= 0x01; /* sensor fail bit */
            printf("[SAFETY] Invalid sensor reading — all outputs off\n");
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }
        latest_env.errors &= ~0x01; /* clear sensor fail bit */

        /* Update latest environment data */
        latest_env.temperature = (int16_t)(sht.temperature * 100);
        latest_env.humidity = (uint16_t)(sht.humidity * 100);

        /* Update PID setpoints */
        temp_pid.setpoint = target_temp;
        hum_pid.setpoint = target_hum;

        /* Run PID */
        float temp_output = pid_update(&temp_pid, sht.temperature, dt);
        float hum_output = pid_update(&hum_pid, sht.humidity, dt);

        /* Apply temperature control */
        if (temp_output > 5.0f) {
            /* Need heating */
            if (temp_output > 50.0f) {
                relay_set(1, true);  /* Full power relay */
                triac_set_power(100);
            } else {
                relay_set(1, false);
                triac_set_power((uint8_t)temp_output);  /* Dimmed heater */
            }
            latest_env.heater_state = 2; /* triac dimming */
        } else if (temp_output < -5.0f) {
            /* Need cooling — use IR blaster for HVAC */
            if (num_ir_codes > 4) {
                ir_send_raw(ir_codes[3].timings, ir_codes[3].timing_count); /* cool mode */
            }
            relay_set(1, false);
            latest_env.hvac_state |= 0x01; /* cooling bit */
            latest_env.heater_state = 0;
        } else {
            /* Temperature OK — all off */
            relay_set(1, false);
            triac_set_power(0);
            latest_env.heater_state = 0;
            latest_env.hvac_state = 0;
        }

        /* Apply humidity control */
        if (hum_output > 10.0f) {
            /* Need humidification */
            relay_set(2, true);
            latest_env.humidifier_state = 1;
        } else if (hum_output < -10.0f) {
            /* Need dehumidification */
            relay_set(2, true);
            latest_env.humidifier_state = 2;
        } else {
            relay_set(2, false);
            latest_env.humidifier_state = 0;
        }

        /* Update display */
        oled_display(sht.temperature, sht.humidity, target_temp, target_hum);

        /* Send environment data to hub */
        ble_send_env_data();

        vTaskDelay(pdMS_TO_TICKS(30000)); /* 30s control loop */
    }
}

/* ---- Main Entry ---- */

void app_main(void)
{
    printf("=== SleepSync Climate Node v1.0 ===\n");
    printf("ESP32-C3-MINI-1\n");

    /* Initialize hardware */
    sht40_init();
    ir_blaster_init();
    relay_init();
    triac_init();
    oled_init();
    ble_mesh_init();

    /* Configure mode button */
    gpio_set_direction(PIN_BTN_MODE, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_LED_STATUS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED_STATUS, 1);

    /* Start climate control task */
    xTaskCreate(climate_control_task, "climate_ctrl", 4096, NULL, 3, NULL);

    printf("Climate node initialized. Waiting for hub setpoints...\n");
}
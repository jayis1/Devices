/*
 * zone_actuator_main.c — ThermoGrid Zone Actuator Node (ESP32-C3 + SX1261)
 *
 * Responsibilities:
 * - Control one heating/cooling zone (radiator valve / HVAC damper / relay)
 * - PID control loop on room temperature (feedback from room sensor via hub)
 * - Receive zone setpoints from hub over Sub-GHz
 * - Motorized radiator valve (Danfoss RA2) or servo damper (MG996R) or relay
 * - Measure pipe/floor temperature (DS18B20) for control feedback
 * - Optional flow meter (YF-S201) for energy accounting (hydronic systems)
 * - Failsafe: revert to last setpoint or frost protection if hub offline >10min
 * - Energy reporting (BTU/kWh per zone)
 *
 * ESP32-C3: RISC-V, WiFi (optional fallback) + SX1261 Sub-GHz (primary)
 */

#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_sleep.h"
#include "mesh_protocol.h"

static const char *TAG = "ZONE_ACT";

/* ---- Pin Definitions (ESP32-C3) ---- */
#define PIN_SX1261_SCK    4
#define PIN_SX1261_MOSI   5
#define PIN_SX1261_MISO   6
#define PIN_SX1261_CS     7
#define PIN_SX1261_BUSY   8
#define PIN_SX1261_IRQ    9
#define PIN_SX1261_NRST   10

#define PIN_DS18B20       2   /* OneWire for pipe/floor temp */

/* Motorized radiator valve (Danfoss RA2) — PWM-positioned actuator */
#define PIN_VALVE_PWM     3   /* PWM output to valve motor driver */
#define PIN_VALVE_DIR_A   0   /* H-bridge A (open direction) */
#define PIN_VALVE_DIR_B   1   /* H-bridge B (close direction) */

/* Damper servo (MG996R) for forced-air zone dampers */
#define PIN_DAMPER_PWM    3   /* PWM to servo (shared with valve — pick one) */

/* Relay outputs (for radiant zone valves / electric heat / boiler) */
#define PIN_RELAY_1       20  /* Zone valve relay */
#define PIN_RELAY_2       21  /* Boiler / heat-pump relay */

/* Flow sensor (YF-S201) — interrupt pin */
#define PIN_FLOW_SENSOR   19  /* pulse counter */

/* ---- Node identity ---- */
#define MY_NODE_ID       0x20   /* configured at enrollment */
#define MY_ZONE_ID        0     /* which zone */

/* ---- PID Controller ---- */
typedef struct {
    float setpoint;       /* target temperature °C */
    float kp, ki, kd;     /* PID gains */
    float integral;       /* integral term */
    float prev_error;     /* previous error for derivative */
    float output_min;     /* 0% */
    float output_max;     /* 100% */
    uint32_t last_time;   /* last update time ms */
} pid_controller_t;

static pid_controller_t valve_pid = {
    .setpoint = 21.0f,
    .kp = 2.0f,
    .ki = 0.1f,
    .kd = 0.5f,
    .integral = 0.0f,
    .prev_error = 0.0f,
    .output_min = 0.0f,
    .output_max = 100.0f,
    .last_time = 0,
};

static float pid_compute(pid_controller_t *pid, float input, uint32_t now_ms)
{
    float error = pid->setpoint - input;
    float dt = (now_ms - pid->last_time) / 1000.0f;
    if (dt <= 0) dt = 1.0f;

    /* Proportional */
    float p = pid->kp * error;

    /* Integral (with anti-windup) */
    pid->integral += error * dt;
    float i = pid->ki * pid->integral;
    /* Clamp integral */
    if (i > pid->output_max) { i = pid->output_max; pid->integral = i / pid->ki; }
    if (i < pid->output_min) { i = pid->output_min; pid->integral = i / pid->ki; }

    /* Derivative */
    float d = pid->kd * (error - pid->prev_error) / dt;
    pid->prev_error = error;
    pid->last_time = now_ms;

    float output = p + i + d;
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    return output;
}

/* ---- Valve/Damper/Relay control ---- */

typedef enum {
    ACTUATOR_TYPE_VALVE = 0,   /* motorized radiator valve (PWM H-bridge) */
    ACTUATOR_TYPE_DAMPER = 1,   /* servo damper (PWM position) */
    ACTUATOR_TYPE_RELAY = 2,    /* on/off relay (bang-bang with hysteresis) */
} actuator_type_t;

static actuator_type_t actuator_type = ACTUATOR_TYPE_VALVE;

/* Current valve/damper position (0-100%) */
static uint8_t current_valve_pos = 0;
static uint8_t target_valve_pos = 0;

/* Relay bang-bang state */
static bool relay_on = false;
static float relay_hysteresis = 0.5f; /* ±0.5°C */

static void valve_set_position(uint8_t pos)
{
    pos = pos > 100 ? 100 : pos;

    if (actuator_type == ACTUATOR_TYPE_VALVE) {
        /* Motorized radiator valve: drive H-bridge
         * - If pos > current: drive open (DIR_A high, DIR_B low) for proportional time
         * - If pos < current: drive close (DIR_A low, DIR_B high) for proportional time
         * - PWM duty cycle determines speed (slow for precision)
         *
         * In production: drive motor for (delta_pos / 100) * full_stroke_time_ms
         * Danfoss RA2 full stroke ≈ 60s
         */
        int delta = (int)pos - (int)current_valve_pos;
        if (delta > 0) {
            gpio_set_level(PIN_VALVE_DIR_A, 1);
            gpio_set_level(PIN_VALVE_DIR_B, 0);
            /* PWM at 50% for slow, precise movement */
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ESP_LOGI(TAG, "Valve OPENING to %d%% (delta=%d)", pos, delta);
        } else if (delta < 0) {
            gpio_set_level(PIN_VALVE_DIR_A, 0);
            gpio_set_level(PIN_VALVE_DIR_B, 1);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ESP_LOGI(TAG, "Valve CLOSING to %d%% (delta=%d)", pos, delta);
        } else {
            /* Stop motor */
            gpio_set_level(PIN_VALVE_DIR_A, 0);
            gpio_set_level(PIN_VALVE_DIR_B, 0);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }
        /* In production: wait for motor to reach position, then stop */
        vTaskDelay(pdMS_TO_TICKS(abs(delta) * 600)); /* 6s per 1% */
        gpio_set_level(PIN_VALVE_DIR_A, 0);
        gpio_set_level(PIN_VALVE_DIR_B, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
    else if (actuator_type == ACTUATOR_TYPE_DAMPER) {
        /* Servo damper: PWM position
         * 0% = 0° (closed), 100% = 90° (fully open)
         * Servo: 1ms = 0°, 2ms = 90° (50Hz PWM)
         * Duty = 5% (1ms/20ms) to 10% (2ms/20ms)
         */
        uint32_t duty = 50 + (pos * 50 / 100); /* 5% to 10% */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        ESP_LOGI(TAG, "Damper set to %d%% (duty=%d)", pos, duty);
    }
    else if (actuator_type == ACTUATOR_TYPE_RELAY) {
        /* Relay: bang-bang with hysteresis
         * If room temp < setpoint - hysteresis: ON
         * If room temp > setpoint + hysteresis: OFF
         * (handled in control loop, not here)
         */
        relay_on = (pos > 50);
        gpio_set_level(PIN_RELAY_1, relay_on ? 1 : 0);
        ESP_LOGI(TAG, "Relay %s", relay_on ? "ON" : "OFF");
    }

    current_valve_pos = pos;
}

/* ---- DS18B20 pipe/floor temperature ---- */

static float ds18b20_read(void)
{
    /* In production: OneWire reset, send 0x44 (convert T), wait 750ms,
     * read 9 bytes (scratchpad), compute CRC, temp = raw / 16.0
     *
     * Stub: return 35°C (typical hydronic pipe temp) */
    return 35.0f;
}

/* ---- Flow sensor (YF-S201) ---- */

static uint16_t flow_mlmin = 0;
static volatile uint32_t flow_pulses = 0;

/* Flow pulse counter ISR (in production: GPIO interrupt on falling edge) */
static void IRAM_ATTR flow_isr_handler(void *arg)
{
    flow_pulses++;
}

/* YF-S201: F = 7.5 * pulses_per_second (Hz → L/min)
 * ml/min = Hz * 7500 / 60 = Hz * 125 */
static uint16_t read_flow_mlmin(void)
{
    uint32_t pulses_start = flow_pulses;
    vTaskDelay(pdMS_TO_TICKS(1000)); /* 1s sample */
    uint32_t pulses_delta = flow_pulses - pulses_start;
    float hz = pulses_delta; /* pulses per second */
    flow_mlmin = (uint16_t)(hz * 125.0f); /* L/min * 1000 / 60 * 7.5 */
    return flow_mlmin;
}

/* ---- Energy accounting (hydronic) ---- */

/* Energy = mass_flow * specific_heat * delta_T
 * Q (J) = m (kg) * 4186 (J/kg·K) * (T_supply - T_return)
 * Approximate: pipe_temp is supply, room_temp is return proxy
 * Wh = Q / 3600
 */
static uint32_t accumulated_energy_wh_x10 = 0;

static void update_energy(float pipe_temp, float room_temp, uint16_t flow)
{
    float delta_t = pipe_temp - room_temp;
    if (delta_t < 0) delta_t = 0;
    /* flow in ml/min → kg/min = ml/1000 (water density 1 kg/L)
     * Q (J/min) = flow_kg_min * 4186 * delta_t
     * Wh = Q / 3600
     */
    float flow_kg_min = flow / 1000.0f;
    float power_w = flow_kg_min * 4186.0f * delta_t / 60.0f;
    /* Accumulate (called every 30s = 0.5 min) */
    accumulated_energy_wh_x10 += (uint32_t)(power_w * 0.5 / 6.0f); /* Wh×10 per 30s */
}

/* ---- Sub-GHz Radio (SX1261) ---- */

static void radio_init(void)
{
    /* In production: SPI init, SX1261 reset, register config
     * Frequency: 915 MHz (US) / 868 MHz (EU)
     * Modulation: LoRa SF7 (telemetry) / SF9 (commands)
     * TX power: +14 dBm
     */
    ESP_LOGI(TAG, "SX1261 initialized (915MHz, LoRa SF7)");
}

static void radio_send(const uint8_t *data, uint16_t len)
{
    ESP_LOGI(TAG, "TX %d bytes", len);
}

static int16_t radio_receive(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms)
{
    return 0; /* stub */
}

/* ---- Zone state ---- */

static int16_t current_setpoint_cx100 = 2100; /* 21.0°C default */
static uint8_t current_mode = MODE_HEATING;
static uint8_t target_mode = MODE_HEATING;
static uint8_t boost_minutes = 0;
static float room_temp_from_sensor = 21.0f; /* received from hub */
static int16_t boost_delta_cx100 = 0;

/* Last setpoint update from hub (for failsafe) */
static uint32_t last_hub_contact_ms = 0;
static bool hub_online = false;

/* ---- Process hub commands ---- */

static void process_setpoint(const zone_setpoint_payload_t *sp)
{
    if (sp->zone_id != MY_ZONE_ID)
        return;

    current_setpoint_cx100 = sp->setpoint_cx100;
    target_mode = sp->mode;
    boost_minutes = sp->boost_minutes;

    if (sp->valve_pos_override != 255) {
        /* Direct valve position override (e.g., freeze protection: 100%) */
        valve_set_position(sp->valve_pos_override);
        ESP_LOGI(TAG, "Valve override: %d%%", sp->valve_pos_override);
    }

    hub_online = true;
    last_hub_contact_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    ESP_LOGI(TAG, "Setpoint: %.2f°C mode=%d boost=%dmin source=%d",
             current_setpoint_cx100 / 100.0f, sp->mode, sp->boost_minutes, sp->source);
}

static void process_packet(mesh_packet_t *pkt)
{
    switch (pkt->pkt_type) {
    case PKT_ZONE_SETPOINT: {
        zone_setpoint_payload_t sp;
        memcpy(&sp, pkt->payload, sizeof(sp));
        process_setpoint(&sp);
        break;
    }
    case PKT_SENSOR_DATA: {
        /* If this is our zone's sensor, extract room temp for PID */
        sensor_data_payload_t sd;
        memcpy(&sd, pkt->payload, sizeof(sd));
        /* In production: match by zone_id, not node_id */
        /* Hub relays sensor data relevant to our zone */
        room_temp_from_sensor = sd.air_temp_cx100 / 100.0f;
        ESP_LOGI(TAG, "Room temp from sensor 0x%02X: %.1f°C",
                 pkt->src_id, room_temp_from_sensor);
        break;
    }
    case PKT_FREEZE_ALERT: {
        freeze_alert_payload_t fa;
        memcpy(&fa, pkt->payload, sizeof(fa));
        ESP_LOGW(TAG, "FREEZE ALERT! T=%.1f°C — opening valve to 100%%",
                 fa.temp_cx100 / 100.0f);
        /* Emergency: force valve fully open */
        valve_set_position(100);
        current_mode = MODE_FROST;
        /* If we control a boiler relay, turn it on */
        gpio_set_level(PIN_RELAY_2, 1);
        break;
    }
    case PKT_SOLAR_STATUS: {
        solar_status_payload_t ss;
        memcpy(&ss, pkt->payload, sizeof(ss));
        ESP_LOGI(TAG, "Solar: %dW surplus, boost=%s",
                 ss.surplus_w, ss.boost_recommended ? "YES" : "no");
        break;
    }
    case PKT_TOU_SCHEDULE: {
        tou_schedule_payload_t tou;
        memcpy(&tou, pkt->payload, sizeof(tou));
        ESP_LOGI(TAG, "TOU: period=%d rate=%d.%d¢ next=%d in %dmin",
                 tou.current_period,
                 tou.rate_cents_x10 / 10, tou.rate_cents_x10 % 10,
                 tou.next_period, tou.next_change_min);
        break;
    }
    case PKT_HEARTBEAT:
        hub_online = true;
        last_hub_contact_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        break;
    default:
        ESP_LOGI(TAG, "Unknown pkt type 0x%02X", pkt->pkt_type);
        break;
    }
}

/* ---- Control loop task ---- */

static void control_loop_task(void *pvParameters)
{
    uint32_t control_period_ms = 30000; /* 30s */
    uint32_t last_report = 0;

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Read pipe/floor temperature */
        float pipe_temp = ds18b20_read();

        /* Check failsafe: if hub offline >10min, go to frost protection */
        if (hub_online && (now - last_hub_contact_ms) > 600000) {
            ESP_LOGW(TAG, "Hub offline >10min — failsafe: frost protect");
            hub_online = false;
            current_mode = MODE_FROST;
            current_setpoint_cx100 = 500; /* 5°C minimum */
            valve_pid.setpoint = 5.0f;
        }

        /* PID or bang-bang control based on actuator type */
        if (actuator_type == ACTUATOR_TYPE_RELAY) {
            /* Bang-bang with hysteresis */
            float sp = current_setpoint_cx100 / 100.0f;
            if (!relay_on && room_temp_from_sensor < sp - relay_hysteresis) {
                valve_set_position(100); /* relay ON */
                ESP_LOGI(TAG, "Relay ON (T=%.1f < %.1f-%.1f)",
                         room_temp_from_sensor, sp, relay_hysteresis);
            } else if (relay_on && room_temp_from_sensor > sp + relay_hysteresis) {
                valve_set_position(0); /* relay OFF */
                ESP_LOGI(TAG, "Relay OFF (T=%.1f > %.1f+%.1f)",
                         room_temp_from_sensor, sp, relay_hysteresis);
            }
        } else {
            /* PID control for valve/damper */
            valve_pid.setpoint = current_setpoint_cx100 / 100.0f;
            float pid_output = pid_compute(&valve_pid, room_temp_from_sensor, now);
            target_valve_pos = (uint8_t)pid_output;

            /* Only move valve if change is significant (>5%) to avoid motor wear */
            if (abs((int)target_valve_pos - (int)current_valve_pos) > 5) {
                valve_set_position(target_valve_pos);
            }
        }

        /* Read flow and update energy */
        uint16_t flow = read_flow_mlmin();
        update_energy(pipe_temp, room_temp_from_sensor, flow);

        /* Listen for hub packets */
        uint8_t rx_buf[128];
        int16_t rx_len = radio_receive(rx_buf, sizeof(rx_buf), 100);
        if (rx_len > 0) {
            mesh_packet_t pkt;
            if (mesh_parse_packet(rx_buf, rx_len, &pkt) == 0) {
                if (pkt.dst_id == MY_NODE_ID || pkt.dst_id == NODE_ID_BROADCAST) {
                    process_packet(&pkt);
                }
            }
        }

        /* Send telemetry to hub every 30s */
        if (now - last_report > control_period_ms) {
            actuator_data_payload_t data;
            memset(&data, 0, sizeof(data));
            data.valve_pos = current_valve_pos;
            data.valve_target = target_valve_pos;
            data.pipe_temp_cx100 = (int16_t)(pipe_temp * 100);
            data.flow_mlmin = flow;
            data.energy_btu_x10 = (uint16_t)(accumulated_energy_wh_x10 * 3412 / 10000);
            data.zone_mode = current_mode;
            data.relay_state = relay_on ? 0x01 : 0x00;
            data.fault_flags = 0;
            data.battery_pct = 255; /* 255 = wired power */
            data.power_source = 0; /* 24VAC wired */
            data.signal_rssi = -70;
            data.pipe_target_cx100 = (int16_t)(valve_pid.setpoint * 100);
            data.zone_id = MY_ZONE_ID;
            data.pid_active = (current_mode == MODE_HEATING ||
                              current_mode == MODE_SOLAR_BOOST) ? 1 : 0;
            data.seq_num = 0;

            mesh_packet_t pkt;
            uint16_t pkt_len = mesh_build_packet(
                MY_NODE_ID, NODE_ID_HUB, PKT_ACTUATOR_DATA,
                (uint8_t *)&data, sizeof(data), &pkt);
            radio_send((uint8_t *)&pkt, pkt_len);

            /* Also send energy report */
            energy_report_payload_t er;
            memset(&er, 0, sizeof(er));
            er.zone_id = MY_ZONE_ID;
            er.energy_wh_x10 = accumulated_energy_wh_x10;
            er.flow_total_l = 0;
            er.uptime_minutes = (uint16_t)((now / 1000) / 60);
            er.avg_pipe_temp_cx100 = (int16_t)(pipe_temp * 100);
            er.avg_room_temp_cx100 = (int16_t)(room_temp_from_sensor * 100);
            er.cost_cents = 0;
            er.tariff_period = 0;

            pkt_len = mesh_build_packet(
                MY_NODE_ID, NODE_ID_HUB, PKT_ENERGY_REPORT,
                (uint8_t *)&er, sizeof(er), &pkt);
            radio_send((uint8_t *)&pkt, pkt_len);

            accumulated_energy_wh_x10 = 0; /* reset per report */
            last_report = now;

            ESP_LOGI(TAG, "Telemetry: valve=%d%% pipe=%.1f°C flow=%dml/min T=%.1f°C sp=%.1f°C mode=%d",
                     current_valve_pos, pipe_temp, flow,
                     room_temp_from_sensor, valve_pid.setpoint, current_mode);
        }

        /* Boost countdown */
        if (boost_minutes > 0) {
            static uint32_t last_boost_tick = 0;
            if (now - last_boost_tick > 60000) { /* 1 min */
                boost_minutes--;
                last_boost_tick = now;
                if (boost_minutes == 0) {
                    ESP_LOGI(TAG, "Boost expired");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); /* 1s loop, but control every 30s */
    }
}

/* ---- Main ---- */

void app_main(void)
{
    ESP_LOGI(TAG, "\n=== ThermoGrid Zone Actuator v1.0 (ID=0x%02X zone=%d) ===",
             MY_NODE_ID, MY_ZONE_ID);

    /* GPIO init */
    gpio_set_direction(PIN_VALVE_DIR_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_VALVE_DIR_B, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RELAY_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RELAY_2, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_VALVE_DIR_A, 0);
    gpio_set_level(PIN_VALVE_DIR_B, 0);
    gpio_set_level(PIN_RELAY_1, 0);
    gpio_set_level(PIN_RELAY_2, 0);

    /* PWM init for valve motor / damper servo */
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 50, /* 50Hz for servo, or 1kHz for motor */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t chan_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = PIN_VALVE_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&chan_conf);

    /* Flow sensor interrupt */
    gpio_set_direction(PIN_FLOW_SENSOR, GPIO_MODE_INPUT);
    gpio_isr_handler_add(PIN_FLOW_SENSOR, flow_isr_handler, NULL);
    gpio_intr_enable(PIN_FLOW_SENSOR);

    /* Radio init */
    radio_init();

    /* Start control loop */
    xTaskCreate(control_loop_task, "control", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Zone actuator running (type=%d)", actuator_type);
}
/*
 * washer_main.c — WashWise Washer Node (ESP32-S3)
 *
 * Responsibilities:
 * - Cycle phase detection (fill/wash/rinse/spin/done) via current + vibration + flow
 * - Automatic detergent dispensing (peristaltic pump, calibrated by load cell)
 * - Load imbalance detection (vibration >0.4g during spin)
 * - Fabric type classification from wash vibration signature
 * - Water usage tracking per cycle
 * - Washer leak detection (humidity spike + flow anomaly)
 * - Detergent reservoir monitoring (load cell)
 * - Reports telemetry to hub every 15 seconds
 * - Receives dosing commands from hub (based on scan results)
 *
 * Sensors/Actuators:
 * - ADXL313: vibration (I2C) — load imbalance + fabric type
 * - YF-S201: water flow (hall-effect, interrupt)
 * - DS18B20: wash water temp (1-Wire)
 * - SHT40: ambient humidity (I2C) — leak detection
 * - ACS712: current clamp on washer power (analog)
 * - HX711 + 5kg load cell: detergent reservoir weight
 * - Kamoer NKP peristaltic pump: detergent dispensing
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "esp_timer.h"

#include "mesh_protocol.h"

static const char *TAG = "WASHER";

/* ---- Pin Definitions (ESP32-S3) ---- */
#define PIN_SX1261_SCK    12
#define PIN_SX1261_MOSI   13
#define PIN_SX1261_MISO   14
#define PIN_SX1261_CS     15
#define PIN_SX1261_BUSY   16
#define PIN_SX1261_IRQ    17
#define PIN_SX1261_NRST   18

/* I2C for ADXL313 + SHT40 */
#define I2C_PORT           I2C_NUM_0
#define PIN_I2C_SDA        8
#define PIN_I2C_SCL        9
#define SHT40_ADDR         0x44
#define ADXL313_ADDR       0x1D

/* ADC channels */
#define ADC_CURRENT        ADC1_CHANNEL_0  /* GPIO1 — ACS712 */
#define ADC_BATTERY        ADC1_CHANNEL_3  /* GPIO4 — battery */

/* Flow sensor (interrupt) */
#define PIN_FLOW_SENSOR    5
#define FLOW_PULSE_PER_L   450  /* YF-S201: 450 pulses per liter */

/* DS18B20 (1-Wire) */
#define PIN_DS18B20        6

/* HX711 load cell (detergent reservoir) */
#define PIN_HX711_DOUT     7
#define PIN_HX711_SCK      10

/* Peristaltic pump (PWM via LEDC) */
#define PIN_PUMP_PWM       4
#define PUMP_PWM_CHANNEL   LEDC_CHANNEL_0

/* HX711 gain/offset (calibration) */
#define HX711_SCALE_FACTOR  -19.8f   /* grams per raw unit (calibrate) */
#define HX711_TARE_OFFSET   8500000  /* raw tare value (calibrate) */
#define DETERGENT_RESERVOIR_EMPTY_G  20.0f  /* empty reservoir weight */

/* Detergent dosing */
#define PUMP_RATE_ML_PER_S  1.2f  /* Kamoer NKP flow rate */
#define DOSE_TOLERANCE_ML   0.1f

/* Vibration thresholds */
#define VIBRATION_IMBALANCE_MG  400.0f  /* 0.4g */
#define VIBRATION_SEVERE_MG    700.0f  /* 0.7g */

/* ---- State ---- */
typedef enum {
    CYCLE_IDLE = 0,
    CYCLE_FILL,
    CYCLE_WASH,
    CYCLE_RINSE,
    CYCLE_SPIN,
    CYCLE_DONE
} cycle_phase_t;

static volatile uint32_t flow_pulse_count = 0;
static uint16_t total_water_ml = 0;
static cycle_phase_t current_phase = CYCLE_IDLE;
static uint16_t detergent_mg_dispensed = 0;
static uint8_t fabric_type_detected = 0;
static QueueHandle_t cmd_queue;

/* ---- Flow sensor ISR ---- */
static void IRAM_ATTR flow_isr_handler(void *arg)
{
    flow_pulse_count++;
}

/* ---- Sensor Reading Stubs ---- */

static float read_vibration_rms(void)
{
    /* ADXL313: read x/y/z acceleration, compute RMS
     * Sample at 100 Hz for 1 second, compute RMS */
    return 150.0f;  /* milli-g normal */
}

static uint16_t read_flow_rate_mlmin(void)
{
    /* YF-S201: flow_pulse_count / FLOW_PULSE_PER_L * 60000
     * Returns mL/min */
    static uint32_t last_count = 0;
    uint32_t current = flow_pulse_count;
    uint32_t delta = current - last_count;
    last_count = current;
    /* Approximate: pulses in last period → mL/min */
    return (uint16_t)((delta * 60000) / (FLOW_PULSE_PER_L * 15));  /* 15s reporting */
}

static float read_water_temp(void)
{
    /* DS18B20: 1-Wire temperature */
    return 25.0f;  /* °C */
}

static float read_ambient_humidity(void)
{
    /* SHT40: I2C */
    return 45.0f;  /* % */
}

static uint16_t read_current_ma(void)
{
    /* ACS712: analog, 100mV/A, offset at VCC/2
     * Washer: 0 (off) / 0.5-2A (wash) / 3-8A (spin/heating) */
    return 0;
}

static float read_detergent_weight_g(void)
{
    /* HX711: 24-bit ADC, read load cell
     * weight = (raw - tare) / scale_factor */
    /* Stub: return simulated value */
    return 450.0f;  /* 450g of detergent remaining */
}

static uint8_t read_battery_pct(void)
{
    return 90;
}

/* ---- SX1261 Radio (stub) ---- */
static void sx1261_init(void)
{
    ESP_LOGI(TAG, "SX1261 initialized (868MHz LoRa)");
}

static void sx1261_send(const uint8_t *data, uint16_t len)
{
    /* In production: SPI write FIFO, TX mode */
}

static int16_t sx1261_receive(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms)
{
    /* In production: RX mode, wait for IRQ */
    return 0;
}

/* ---- Cycle Phase Detection ---- */

static cycle_phase_t detect_phase(uint16_t current_ma, float vibration,
                                   uint16_t flow_rate, float water_temp)
{
    /* State machine based on current draw + vibration + flow:
     * IDLE: current < 50mA, low vibration, no flow
     * FILL: current 200-500mA (valve), flow > 0
     * WASH: current 500-2000mA, moderate vibration, water present
     * RINSE: current 200-2000mA, flow present again, temp may differ
     * SPIN: current > 2000mA, high vibration (>300mg)
     * DONE: current drops to idle after spin */
    if (current_ma < 50 && vibration < 100 && flow_rate == 0) {
        if (current_phase == CYCLE_SPIN)
            return CYCLE_DONE;
        return CYCLE_IDLE;
    }
    if (flow_rate > 100 && current_ma < 1000)
        return CYCLE_FILL;
    if (current_ma > 2000 && vibration > 300)
        return CYCLE_SPIN;
    if (current_ma > 200 && flow_rate == 0 && vibration < 300)
        return CYCLE_RINSE;
    if (current_ma > 200 && vibration > 100)
        return CYCLE_WASH;
    return current_phase;
}

/* ---- Fabric Type Classification (stub) ---- */

static uint8_t classify_fabric(float vibration_rms, float water_temp)
{
    /* In production: TFLite Micro model on vibration signature
     * during wash phase. Different fabrics produce different
     * vibration patterns (heavy denim vs light silk).
     *
     * Stub: simple heuristic */
    if (vibration_rms > 500) return 5;  /* denim (heavy vibration) */
    if (vibration_rms > 300) return 1;  /* cotton */
    if (vibration_rms > 200) return 6;  /* blend */
    if (vibration_rms > 150) return 2;  /* polyester */
    if (vibration_rms > 100) return 7;  /* linen */
    return 3;  /* wool (low vibration, gentle) */
}

/* ---- Imbalance Detection ---- */

static uint8_t check_imbalance(float vibration_rms, cycle_phase_t phase)
{
    if (phase != CYCLE_SPIN)
        return 0;
    if (vibration_rms > VIBRATION_SEVERE_MG) return 2;  /* severe */
    if (vibration_rms > VIBRATION_IMBALANCE_MG) return 1; /* warning */
    return 0; /* ok */
}

/* ---- Leak Detection ---- */

static bool detect_leak(float humidity, uint16_t flow_rate, cycle_phase_t phase)
{
    /* If humidity spikes above 85% during non-fill phase,
     * or flow detected when washer should be idle → possible leak */
    if (phase != CYCLE_FILL && humidity > 85.0f)
        return true;
    if (phase == CYCLE_IDLE && flow_rate > 50)
        return true;
    return false;
}

/* ---- Detergent Dosing ---- */

static void dose_detergent(uint8_t ml)
{
    ESP_LOGI(TAG, "Dosing %d mL detergent via peristaltic pump", ml);

    /* Calculate pump run time */
    uint32_t run_ms = (uint32_t)(ml / PUMP_RATE_ML_PER_S * 1000);

    /* Start pump */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PUMP_PWM_CHANNEL, 128);  /* 50% duty */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PUMP_PWM_CHANNEL);

    /* Run for calculated time */
    vTaskDelay(pdMS_TO_TICKS(run_ms));

    /* Stop pump */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PUMP_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PUMP_PWM_CHANNEL);

    /* Update dispensed amount (detergent density ~1.1 g/mL) */
    detergent_mg_dispensed += (uint16_t)(ml * 1.1f * 1000);

    ESP_LOGI(TAG, "Dosed %d mL, total this cycle: %d mg",
             ml, detergent_mg_dispensed);
}

/* ---- Send Telemetry ---- */

static void send_washer_data(const washer_data_payload_t *data)
{
    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(
        NODE_ID_WASHER, NODE_ID_HUB, PKT_WASHER_DATA,
        (const uint8_t *)data, sizeof(*data), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
}

/* ---- Main Task ---- */

void washer_task(void *arg)
{
    ESP_LOGI(TAG, "WashWise Washer Node starting");

    washer_data_payload_t wdata;
    memset(&wdata, 0, sizeof(wdata));

    bool cycle_in_progress = false;
    uint32_t cycle_start_time = 0;
    uint16_t pending_dose_ml = 0;

    while (1) {
        /* Check for commands from hub */
        command_payload_t cmd;
        if (xQueueReceive(cmd_queue, &cmd, 0) == pdTRUE) {
            if (cmd.cmd_type == CMD_DOSE && cmd.param_len >= 1) {
                pending_dose_ml = cmd.params[0];
                ESP_LOGI(TAG, "Received dose command: %d mL", pending_dose_ml);
            } else if (cmd.cmd_type == CMD_CYCLE_SELECT && cmd.param_len >= 3) {
                uint8_t cycle = cmd.params[0];
                int16_t temp_x10 = (cmd.params[1] << 8) | cmd.params[2];
                ESP_LOGI(TAG, "Cycle select: type=%d temp=%.1fC", cycle, temp_x10 / 10.0f);
            }
        }

        /* Read sensors */
        float vib_rms = read_vibration_rms();
        uint16_t flow_rate = read_flow_rate_mlmin();
        float water_temp = read_water_temp();
        float amb_hum = read_ambient_humidity();
        uint16_t current = read_current_ma();
        float det_weight = read_detergent_weight_g();
        uint8_t batt = read_battery_pct();

        /* Detect cycle phase */
        cycle_phase_t new_phase = detect_phase(current, vib_rms, flow_rate, water_temp);

        /* Track cycle start/end */
        if (new_phase != CYCLE_IDLE && new_phase != CYCLE_DONE && !cycle_in_progress) {
            cycle_in_progress = true;
            cycle_start_time = esp_timer_get_time() / 1000;
            detergent_mg_dispensed = 0;
            total_water_ml = 0;
            ESP_LOGI(TAG, "Wash cycle started, phase=%d", new_phase);

            /* Auto-dose if command received */
            if (pending_dose_ml > 0) {
                /* Wait for fill phase to dose */
                if (new_phase == CYCLE_FILL) {
                    vTaskDelay(pdMS_TO_TICKS(5000));  /* let water fill */
                    dose_detergent(pending_dose_ml);
                    pending_dose_ml = 0;
                }
            }
        }
        if (new_phase == CYCLE_DONE && cycle_in_progress) {
            cycle_in_progress = false;
            uint32_t duration = (esp_timer_get_time() / 1000) - cycle_start_time;
            ESP_LOGI(TAG, "Cycle complete: %d min %d s, water=%d mL, detergent=%d mg",
                     duration / 60000, (duration / 1000) % 60,
                     total_water_ml, detergent_mg_dispensed);
        }

        current_phase = new_phase;

        /* Classify fabric during wash phase */
        if (current_phase == CYCLE_WASH) {
            fabric_type_detected = classify_fabric(vib_rms, water_temp);
        }

        /* Check imbalance during spin */
        uint8_t imbalance = check_imbalance(vib_rms, current_phase);
        if (imbalance >= 1) {
            ESP_LOGW(TAG, "Load imbalance detected: level=%d, vibration=%.0f mg",
                     imbalance, vib_rms);
        }

        /* Check for leaks */
        bool leak = detect_leak(amb_hum, flow_rate, current_phase);
        if (leak) {
            ESP_LOGE(TAG, "Possible washer leak detected! humidity=%.1f%% flow=%d",
                     amb_hum, flow_rate);
        }

        /* Track total water */
        total_water_ml += (flow_rate * 15) / 60;  /* 15s reporting interval */

        /* Build telemetry payload */
        wdata.cycle_phase        = current_phase;
        wdata.vibration_rms_x10  = (uint16_t)(vib_rms * 10);
        wdata.flow_rate_mlmin    = flow_rate;
        wdata.total_water_ml     = total_water_ml;
        wdata.water_temp_c_x10   = (int16_t)(water_temp * 10);
        wdata.ambient_hum_x10   = (uint16_t)(amb_hum * 10);
        wdata.motor_state       = (current > 2000) ? 2 : (current > 50 ? 1 : 0);
        wdata.current_ma         = current;
        wdata.detergent_mg       = detergent_mg_dispensed;
        wdata.reservoir_g_x10    = (uint16_t)(det_weight * 10);
        wdata.fabric_type        = fabric_type_detected;
        wdata.imbalance_flag     = imbalance;
        wdata.leak_flag          = leak ? 1 : 0;
        wdata.battery_pct        = batt;
        wdata.signal_rssi        = 0;
        wdata.reserved           = 0;

        /* Check detergent low */
        if (det_weight < DETERGENT_RESERVOIR_EMPTY_G + 50.0f) {
            ESP_LOGW(TAG, "Detergent low: %.0fg remaining", det_weight);
        }

        /* Send to hub */
        send_washer_data(&wdata);

        ESP_LOGI(TAG, "phase=%d vib=%.0fmg flow=%d temp=%.1fC det=%.0fg fab=%d",
                 current_phase, vib_rms, flow_rate, water_temp, det_weight,
                 fabric_type_detected);

        /* Report every 15 seconds */
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

/* ---- Command Receiver Task (listens for hub commands in slot 4) ---- */

void cmd_rx_task(void *arg)
{
    while (1) {
        /* In production: listen during TDMA slot 4 for commands from hub */
        uint8_t rx_buf[64];
        int16_t rx_len = sx1261_receive(rx_buf, sizeof(rx_buf), 100);
        if (rx_len > 0) {
            mesh_packet_t pkt;
            if (mesh_parse_packet(rx_buf, rx_len, &pkt) == 0) {
                if (pkt.dst_id == NODE_ID_WASHER && pkt.pkt_type == PKT_COMMAND) {
                    command_payload_t cmd;
                    memcpy(&cmd, pkt.payload, sizeof(cmd));
                    xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));  /* check every TDMA frame */
    }
}

void app_main(void)
{
    /* Initialize I2C */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_PORT, &i2c_cfg);
    i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);

    /* Initialize ADC */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CURRENT, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_BATTERY, ADC_ATTEN_DB_11);

    /* Initialize flow sensor interrupt */
    gpio_config_t flow_cfg = {
        .pin_bit_mask = (1ULL << PIN_FLOW_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&flow_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_FLOW_SENSOR, flow_isr_handler, NULL);

    /* Initialize pump PWM (LEDC) */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .channel = PUMP_PWM_CHANNEL,
        .duty = 0,
        .gpio_num = PIN_PUMP_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&ch_cfg);

    /* Initialize HX711 pins */
    gpio_config_t hx711_cfg = {
        .pin_bit_mask = (1ULL << PIN_HX711_DOUT) | (1ULL << PIN_HX711_SCK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&hx711_cfg);
    gpio_set_direction(PIN_HX711_SCK, GPIO_MODE_OUTPUT);

    sx1261_init();

    /* Create command queue */
    cmd_queue = xQueueCreate(4, sizeof(command_payload_t));

    /* Start tasks */
    xTaskCreate(washer_task, "washer_task", 8192, NULL, 5, NULL);
    xTaskCreate(cmd_rx_task, "cmd_rx_task", 4096, NULL, 4, NULL);
}
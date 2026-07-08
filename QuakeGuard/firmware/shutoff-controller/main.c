/*
 * quakeguard_shutoff.c — QuakeGuard Auto-Shutoff Controller firmware (ESP32-C3)
 *
 * Receives SHUTOFF_NOW from Hub, drives motorized ball valves to close
 * gas and water mains, trips equipment relays, samples gas sensors
 * post-shutoff, and sends ACK with valve states + gas readings.
 *
 * Also handles monthly valve self-test (VALVE_TEST msg from Hub).
 *
 * License: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

#include "common/quakeguard_protocol.h"
#include "common/cc1101.h"

static const char *TAG = "QG_SHUTOFF";

/* ── Pin Definitions (ESP32-C3) ─────────────────────────────── */
#define PIN_MQ8 analog  GPIO_NUM_1   /* ADC1_CH1 */
#define PIN_MQ4_analog   GPIO_NUM_2   /* ADC1_CH2 */
#define PIN_DS18B20      GPIO_NUM_3
#define PIN_GAS_STEP     GPIO_NUM_4
#define PIN_GAS_DIR      GPIO_NUM_5
#define PIN_WATER_PWM    GPIO_NUM_6   /* DRV8871 IN1 */
#define PIN_WATER_IN2    GPIO_NUM_7   /* DRV8871 IN2 */
#define PIN_RELAY1       GPIO_NUM_8   /* elevator drop */
#define PIN_REED_GAS     GPIO_NUM_9
#define PIN_REED_WATER   GPIO_NUM_10
#define PIN_SPI_CLK      GPIO_NUM_18
#define PIN_SPI_MISO     GPIO_NUM_19
#define PIN_SPI_MOSI     GPIO_NUM_20
#define PIN_CC1101_CS    GPIO_NUM_21
#define PIN_CC1101_GD0   GPIO_NUM_22

/* ── Constants ──────────────────────────────────────────────── */
#define STEPPER_STEPS_TO_CLOSE  300    /* 1.5 rev × 200 steps/rev */
#define STEPPER_SPEED_SPS       1000   /* steps per second */
#define WATER_VALVE_TIME_MS     1500   /* DC motor close time */
#define GAS_LEAK_H2_THRESHOLD   100    /* ppm */
#define GAS_LEAK_CH4_THRESHOLD  100    /* ppm */
#define POST_SHUTOFF_MONITOR_S  600    /* 10 min post-shutoff gas monitoring */

/* ── Global State ───────────────────────────────────────────── */
static cc1101_t radio;
static uint8_t seq_num = 0;
static volatile uint8_t gas_valve_closed = 0;
static volatile uint8_t water_valve_closed = 0;

/* ── Stepper Motor: Gas Valve ────────────────────────────────── */
static void gas_valve_close(void)
{
    ESP_LOGW(TAG, "Closing gas valve (stepper)...");

    gpio_set_level(PIN_GAS_DIR, 1);  /* direction = close */

    for (int i = 0; i < STEPPER_STEPS_TO_CLOSE; i++) {
        gpio_set_level(PIN_GAS_STEP, 1);
        esp_rom_delay_us(500);  /* 500 µs high pulse */
        gpio_set_level(PIN_GAS_STEP, 0);
        esp_rom_delay_us(500);  /* 500 µs low pulse = 1000 steps/s */
    }

    /* Verify with reed switch */
    vTaskDelay(pdMS_TO_TICKS(50));
    gas_valve_closed = gpio_get_level(PIN_REED_GAS);

    ESP_LOGI(TAG, "Gas valve closed (reed=%d)", gas_valve_closed);
}

static void gas_valve_open(void)
{
    ESP_LOGI(TAG, "Opening gas valve (stepper)...");

    gpio_set_level(PIN_GAS_DIR, 0);  /* direction = open */

    for (int i = 0; i < STEPPER_STEPS_TO_CLOSE; i++) {
        gpio_set_level(PIN_GAS_STEP, 1);
        esp_rom_delay_us(500);
        gpio_set_level(PIN_GAS_STEP, 0);
        esp_rom_delay_us(500);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    gas_valve_closed = gpio_get_level(PIN_REED_GAS);
}

/* ── DC Motor: Water Valve ──────────────────────────────────── */
static void water_valve_close(void)
{
    ESP_LOGW(TAG, "Closing water valve (DC motor)...");

    /* DRV8871: IN1=PWM (close), IN2=low */
    /* Soft start: ramp PWM from 0 to 100% over 200 ms */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    gpio_set_level(PIN_WATER_IN2, 0);

    for (int duty = 0; duty <= 1023; duty += 51) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    /* Hold for 1.5 s to complete 90° turn */
    vTaskDelay(pdMS_TO_TICKS(WATER_VALVE_TIME_MS));

    /* Stop motor (both inputs low = coast) */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    gpio_set_level(PIN_WATER_IN2, 0);

    /* Verify with reed switch */
    vTaskDelay(pdMS_TO_TICKS(50));
    water_valve_closed = gpio_get_level(PIN_REED_WATER);

    ESP_LOGI(TAG, "Water valve closed (reed=%d)", water_valve_closed);
}

static void water_valve_open(void)
{
    ESP_LOGI(TAG, "Opening water valve (DC motor)...");

    /* DRV8871: IN1=low, IN2=PWM (open / reverse) */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    gpio_set_level(PIN_WATER_IN2, 1);  /* reverse */

    /* Use a second LEDC channel or GPIO PWM for IN2 in production */
    /* Simplified: full speed reverse */
    vTaskDelay(pdMS_TO_TICKS(WATER_VALVE_TIME_MS));

    gpio_set_level(PIN_WATER_IN2, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    water_valve_closed = gpio_get_level(PIN_REED_WATER);
}

/* ── Equipment Relays ───────────────────────────────────────── */
static void trip_relays(uint8_t relay_mask)
{
    if (relay_mask & QG_ACT_RELAY_1) {
        gpio_set_level(PIN_RELAY1, 1);  /* elevator drop */
        ESP_LOGW(TAG, "Relay 1 tripped (elevator drop)");
    }
    /* Relays 2-4 via PCF8574 I2C expander in production */
}

/* ── Gas Sensor Reading ─────────────────────────────────────── */
static uint16_t read_h2_ppm(void)
{
    /* MQ-8 H2 sensor: analog read
     * ADC1_CH1 (GPIO1)
     * Rs/R0 ratio → ppm (logarithmic calibration curve)
     * Simplified: return raw ADC value as proxy
     */
    adc1_config_width(ADC_WIDTH_BIT_12);
    int raw = adc1_get_raw(ADC1_CHANNEL_1);
    /* Convert: ppm = a * (Rs/R0)^b
     * For MQ-8: a = 980, b = -2.2 (approximate)
     * Rs/R0 = (Vcc/Vout - 1) * RL / R0
     * Simplified mapping for demo:
     */
    if (raw < 400) return 0;
    return (uint16_t)((raw - 400) * 2);  /* rough ppm estimate */
}

static uint16_t read_ch4_ppm(void)
{
    /* MQ-4 CH4 sensor: analog read on ADC1_CH2 (GPIO2) */
    adc1_config_width(ADC_WIDTH_BIT_12);
    int raw = adc1_get_raw(ADC1_CHANNEL_2);
    if (raw < 400) return 0;
    return (uint16_t)((raw - 400) * 5);  /* rough ppm estimate */
}

/* ── Send ACK to Hub ────────────────────────────────────────── */
static void send_shutoff_ack(void)
{
    shutoff_ack_payload_t ack = {
        .gas_valve_closed = gas_valve_closed,
        .water_valve_closed = water_valve_closed,
        .h2_ppm = read_h2_ppm(),
        .ch4_ppm = read_ch4_ppm(),
        .temperature_c = 250,  /* DS18B20 reading in production */
        .relay_states = 0x0F,  /* all relays tripped */
    };

    qg_frame_t frame;
    size_t frame_len = qg_build_frame(&frame,
        MSG_SHUTOFF_ACK, QG_ADDR_SHUTOFF, QG_ADDR_HUB,
        seq_num++, (uint8_t *)&ack, sizeof(ack));

    cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                frame_len - QG_PREAMBLE_LEN);

    ESP_LOGI(TAG, "ACK sent: gas=%d water=%d H2=%d CH4=%d",
             ack.gas_valve_closed, ack.water_valve_closed,
             ack.h2_ppm, ack.ch4_ppm);
}

/* ── Post-Shutoff Gas Monitor ────────────────────────────────── */
static void post_shutoff_monitor(void)
{
    ESP_LOGI(TAG, "Post-shutoff gas monitoring for %d s", POST_SHUTOFF_MONITOR_S);

    for (int i = 0; i < POST_SHUTOFF_MONITOR_S / 5; i++) {
        uint16_t h2 = read_h2_ppm();
        uint16_t ch4 = read_ch4_ppm();

        if (h2 > GAS_LEAK_H2_THRESHOLD || ch4 > GAS_LEAK_CH4_THRESHOLD) {
            ESP_LOGE(TAG, "GAS LEAK! H2=%d ppm CH4=%d ppm", h2, ch4);
            /* In production: send GAS_LEAK_ALERT to Hub */
            /* Hub will trigger full siren + push notification */
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ── Execute Shutoff ───────────────────────────────────────── */
static void execute_shutoff(uint8_t action_flags, uint8_t urgency)
{
    ESP_LOGE(TAG, "EXECUTING SHUTOFF: flags=0x%02X urgency=%d",
             action_flags, urgency);

    if (action_flags & QG_ACT_GAS_VALVE) {
        gas_valve_close();
    }
    if (action_flags & QG_ACT_WATER_VALVE) {
        water_valve_close();
    }
    if (action_flags & (QG_ACT_RELAY_1 | QG_ACT_RELAY_2 |
                         QG_ACT_RELAY_3 | QG_ACT_RELAY_4)) {
        trip_relays(action_flags);
    }

    /* Send ACK */
    send_shutoff_ack();

    /* Start post-shutoff gas monitoring */
    post_shutoff_monitor();
}

/* ── Sub-GHz RX Task ────────────────────────────────────────── */
static void rx_task(void *arg)
{
    uint8_t rx_buf[128];
    uint8_t rx_len;
    int8_t rssi;

    while (1) {
        if (cc1101_recv(&radio, rx_buf, &rx_len, &rssi) == 0) {
            qg_frame_t frame;
            if (qg_parse_frame(rx_buf, rx_len, &frame) != 0)
                continue;

            switch (frame.header.msg_type) {
            case MSG_SHUTOFF_NOW: {
                shutoff_now_payload_t *sp = (shutoff_now_payload_t *)frame.payload;
                ESP_LOGE(TAG, "SHUTDOWN_NOW received!");
                execute_shutoff(sp->action_flags, sp->urgency);
                break;
            }
            case MSG_VALVE_TEST: {
                shutoff_now_payload_t *sp = (shutoff_now_payload_t *)frame.payload;
                ESP_LOGI(TAG, "VALVE_TEST received (flags=0x%02X)", sp->action_flags);

                /* Monthly self-test: cycle valves open→close→open */
                if (sp->action_flags & QG_ACT_GAS_VALVE) {
                    gas_valve_close();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    gas_valve_open();
                }
                if (sp->action_flags & QG_ACT_WATER_VALVE) {
                    water_valve_close();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    water_valve_open();
                }

                /* Send test results */
                shutoff_ack_payload_t result = {
                    .gas_valve_closed = gas_valve_closed,
                    .water_valve_closed = water_valve_closed,
                    .h2_ppm = read_h2_ppm(),
                    .ch4_ppm = read_ch4_ppm(),
                    .temperature_c = 250,
                    .relay_states = 0,
                };
                qg_frame_t ack_frame;
                size_t ack_len = qg_build_frame(&ack_frame,
                    MSG_TEST_RESULT, QG_ADDR_SHUTOFF, QG_ADDR_HUB,
                    seq_num++, (uint8_t *)&result, sizeof(result));
                cc1101_send(&radio, (uint8_t *)&ack_frame + QG_PREAMBLE_LEN,
                            ack_len - QG_PREAMBLE_LEN);
                break;
            }
            case MSG_SEISMIC_CONFIRMED: {
                /* Seismic event confirmed by Hub — prepare for shutoff command */
                seismic_confirmed_payload_t *scp = (seismic_confirmed_payload_t *)frame.payload;
                ESP_LOGW(TAG, "SEISMIC_CONFIRMED: severity=%d mag=%d.%d",
                         scp->severity, scp->magnitude_x10 / 10,
                         scp->magnitude_x10 % 10);
                break;
            }
            default:
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── Main ───────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "QuakeGuard Shutoff Controller starting...");

    /* Initialize SPI bus */
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    /* Initialize CC1101 */
    cc1101_init(&radio, SPI2_HOST, PIN_CC1101_CS,
                PIN_CC1101_GD0, -1, QG_ADDR_SHUTOFF);

    /* Configure GPIO */
    gpio_set_direction(PIN_GAS_STEP, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_GAS_DIR, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_WATER_IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RELAY1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_REED_GAS, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_REED_WATER, GPIO_MODE_INPUT);

    gpio_set_level(PIN_GAS_STEP, 0);
    gpio_set_level(PIN_GAS_DIR, 0);
    gpio_set_level(PIN_WATER_IN2, 0);
    gpio_set_level(PIN_RELAY1, 0);

    /* Configure PWM for water valve (LEDC) */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,  /* 1 kHz PWM */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PIN_WATER_PWM,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);

    /* Configure ADC for gas sensors */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_11);  /* GPIO1 */
    adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_11);  /* GPIO2 */

    /* Read initial valve states */
    gas_valve_closed = gpio_get_level(PIN_REED_GAS);
    water_valve_closed = gpio_get_level(PIN_REED_WATER);

    ESP_LOGI(TAG, "Initial states: gas=%d water=%d",
             gas_valve_closed, water_valve_closed);

    /* Start RX task */
    xTaskCreate(rx_task, "rx_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "Shutoff Controller ready. Waiting for commands...");

    /* Main loop: heartbeat */
    uint8_t hb_seq = 0;
    while (1) {
        heartbeat_payload_t hb = {
            .battery_pct = 100,
            .temperature_c = 250,
            .status_flags = 0x01,
            .uptime_hours = 0,
            .rssi_db = 0xFFFF,
        };

        qg_frame_t frame;
        size_t frame_len = qg_build_frame(&frame,
            MSG_HEARTBEAT, QG_ADDR_SHUTOFF, QG_ADDR_HUB,
            hb_seq++, (uint8_t *)&hb, sizeof(hb));
        cc1101_send(&radio, (uint8_t *)&frame + QG_PREAMBLE_LEN,
                    frame_len - QG_PREAMBLE_LEN);

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
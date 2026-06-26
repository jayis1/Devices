/*
 * HiveSync — Smart Feeder Firmware
 * MCU: nRF52840
 * Peripherals: HX711 load cell, MG996R servo, 28BYJ-48 stepper, SHT40, CC1101
 * Dispenses syrup on command from gateway, monitors consumption
 */

#include <string.h>
#include "nrf_drv_gpiote.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "cc1101_nrf.h"
#include "hx711_nrf.h"
#include "sht40_nrf.h"
#include "hivesync_proto.h"

#define FEEDER_NODE_ID    0x0020
#define SYRUP_DENSITY     1.33f   /* g/mL for 1:1 sugar syrup */
#define PATTY_MM_PER_STEP 0.1f    /* mm advance per stepper step */

typedef enum {
    FEEDER_IDLE,
    FEEDER_DISPENSING_SYRUP,
    FEEDER_ADVANCING_PATTY,
    FEEDER_ERROR
} feeder_state_t;

typedef struct {
    uint16_t node_id;
    float current_weight_kg;
    float last_dispense_weight_kg;
    float temperature_c;
    float humidity_pct;
    uint16_t dispense_count;
    feeder_state_t state;
    uint8_t valve_open;
    uint8_t clog_detected;
    float battery_mv;
} feeder_data_t;

static feeder_data_t g_feeder;
static cc1101_nrf_t g_radio;
static hx711_dev_t  g_load;
static sht40_dev_t  g_sht;

/* ---- Servo Control (MG996R) ---- */
/* Servo: 50 Hz PWM, 0.5-2.5ms pulse for 0-180° */
static void servo_set_angle(uint8_t angle) {
    /* Using TIMER1 for PWM generation */
    /* Valve closed = 0°, Valve open = 90° */
    uint32_t pulse_us = 500 + (angle * 2000 / 180);
    nrf_timer_cc_set(NRF_TIMER1, NRF_TIMER_CC_CHANNEL0, pulse_us * 64 / 1000);
}

static void valve_open(void) {
    servo_set_angle(90);
    g_feeder.valve_open = 1;
}

static void valve_close(void) {
    servo_set_angle(0);
    g_feeder.valve_open = 0;
}

/* ---- Stepper Motor (28BYJ-48 via ULN2003) ---- */
static const uint8_t step_seq[8] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09};
static uint8_t step_pos = 0;

static void stepper_step(bool forward) {
    if (forward) step_pos = (step_pos + 1) & 0x07;
    else step_pos = (step_pos - 1) & 0x07;

    /* Set ULN2003 outputs IN1-IN4 on P0.11-P0.14 */
    uint32_t out = 0;
    if (step_seq[step_pos] & 0x01) out |= (1 << 11);
    if (step_seq[step_pos] & 0x02) out |= (1 << 12);
    if (step_seq[step_pos] & 0x04) out |= (1 << 13);
    if (step_seq[step_pos] & 0x08) out |= (1 << 14);
    NRF_P0->OUT = (NRF_P0->OUT & ~0x7800) | (out << 11);
    nrf_delay_ms(2);
}

static void advance_patty_mm(float mm) {
    int steps = (int)(mm / PATTY_MM_PER_STEP);
    for (int i = 0; i < steps; i++) {
        stepper_step(true);
    }
}

/* ---- Dispense Syrup ---- */
static feeder_cmd_result_t dispense_syrup(uint16_t ml) {
    float target_delta = ml * SYRUP_DENSITY / 1000.0f;
    float start_weight = hx711_read_kg(&g_load);

    valve_open();
    g_feeder.state = FEEDER_DISPENSING_SYRUP;

    /* Monitor weight until target reached or timeout */
    int timeout_ms = ml * 200;  /* ~200ms per mL expected */
    int elapsed = 0;
    float current_weight;

    while (elapsed < timeout_ms) {
        nrf_delay_ms(100);
        elapsed += 100;
        current_weight = hx711_read_kg(&g_load);
        float dispensed = start_weight - current_weight;

        if (dispensed >= target_delta) {
            valve_close();
            g_feeder.last_dispense_weight_kg = current_weight;
            g_feeder.state = FEEDER_IDLE;
            g_feeder.dispense_count++;
            return CMD_OK;
        }
    }

    /* Timeout — check for clog */
    valve_close();
    current_weight = hx711_read_kg(&g_load);
    float actual_delta = start_weight - current_weight;
    float error_pct = (target_delta > 0) ? fabsf(actual_delta - target_delta) / target_delta * 100.0f : 100.0f;

    if (error_pct > 20.0f) {
        g_feeder.clog_detected = 1;
        g_feeder.state = FEEDER_ERROR;
        return CMD_CLOG_DETECTED;
    }

    g_feeder.state = FEEDER_IDLE;
    return CMD_PARTIAL;
}

/* ---- Sample & Transmit ---- */
static void sample_and_report(void) {
    g_feeder.current_weight_kg = hx711_read_kg(&g_load);
    sht40_read(&g_sht, &g_feeder.temperature_c, &g_feeder.humidity_pct);
    g_feeder.battery_mv = adc_read_vbat();

    uint8_t tx_buf[64];
    uint16_t len = hivesync_pack_feeder(tx_buf,
        g_feeder.node_id,
        g_feeder.current_weight_kg,
        g_feeder.temperature_c,
        g_feeder.humidity_pct,
        g_feeder.state,
        g_feeder.valve_open,
        g_feeder.clog_detected,
        g_feeder.battery_mv,
        g_feeder.dispense_count);

    cc1101_tx(&g_radio, tx_buf, len);
}

/* ---- Main ---- */
int main(void) {
    /* Init peripherals */
    hx711_nrf_init(&g_load, 7, 8, 2280.0f); /* SCK=P0.7, DOUT=P0.8 */
    sht40_nrf_init(&g_sht, NRF_DRV_TWI_INSTANCE_0);
    cc1101_nrf_init(&g_radio, SPI_INSTANCE_0, 5, 6); /* CS=P0.5, IRQ=P0.6 */
    cc1101_set_frequency(&g_radio, 868000000);

    g_feeder.node_id = FEEDER_NODE_ID;
    g_feeder.state = FEEDER_IDLE;

    while (1) {
        /* Check for incoming command */
        uint8_t rx_buf[64];
        uint16_t rx_len;
        if (cc1101_rx(&g_radio, rx_buf, &rx_len, 5000)) {
            hivesync_msg_t msg;
            if (hivesync_parse(rx_buf, rx_len, &msg) == PARSE_OK) {
                if (msg.msg_type == MSG_COMMAND) {
                    switch (msg.cmd.cmd_id) {
                        case CMD_DISPENSE_SYRUP:
                            dispense_syrup(msg.cmd.param_u16);
                            break;
                        case CMD_ADVANCE_PATTY:
                            advance_patty_mm(msg.cmd.param_f32);
                            break;
                        case CMD_VALVE_OPEN:
                            valve_open();
                            break;
                        case CMD_VALVE_CLOSE:
                            valve_close();
                            break;
                    }
                }
            }
        }

        /* Periodic sample + transmit */
        sample_and_report();

        /* Deep sleep for 60 seconds */
        sd_power_system_off();
    }
}
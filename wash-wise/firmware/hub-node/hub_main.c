/*
 * hub_main.c — WashWise Hub Node (RP2040 + ESP32-C6)
 *
 * Responsibilities:
 * - Sub-GHz mesh coordinator (TDMA scheduler, 5 slots)
 * - Data aggregation from washer + dryer + scanner nodes
 * - WiFi uplink to MQTT broker
 * - BLE GATT server for mobile app
 * - TFT dashboard: lint risk gauge, cycle progress, detergent level
 * - Local alarm triggers (piezo — 85 dB fire alarm)
 * - OTA update distribution
 * - TFLite Micro fire risk inference
 *
 * RP2040 Core 0: mesh TDMA + ML + display (hard real-time)
 * RP2040 Core 1: ESP32-C6 UART bridge (soft real-time)
 *
 * SAFETY: Dryer fire alerts override normal TDMA. Hub halts normal
 * scheduling, relays FIRE_ALERT to app + cloud, activates piezo alarm.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "pico/time.h"

#include "mesh_protocol.h"

/* ---- Pin Definitions (RP2040) ---- */
#define PIN_SX1262_SPI   spi1
#define PIN_SX1262_SCK   18
#define PIN_SX1262_MOSI  19
#define PIN_SX1262_MISO  20
#define PIN_SX1262_CS    17
#define PIN_SX1262_BUSY  14
#define PIN_SX1262_IRQ   15
#define PIN_SX1262_NRST  16

#define PIN_ES32_UART   uart0
#define PIN_ES32_TX     0
#define PIN_ES32_RX     1

#define PIN_TFT_SPI     spi0
#define PIN_TFT_SCK     6
#define PIN_TFT_MOSI    7
#define PIN_TFT_MISO    8
#define PIN_TFT_CS      10
#define PIN_TFT_DC      11
#define PIN_TFT_RST     12
#define PIN_TFT_BL      13

#define PIN_SD_CS        9

#define PIN_PIEZO       22
#define PIN_USER_BTN    23
#define PIN_LED_R       24
#define PIN_LED_G       25
#define PIN_LED_B       26

/* ---- Mesh State ---- */
#define MAX_NODES       3
#define NODE_TIMEOUT_S  60

typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;     /* 0=unknown, 1=washer, 2=dryer, 3=scanner */
    bool     active;
    absolute_time_t last_seen;
    washer_data_payload_t  washer_data;
    dryer_data_payload_t   dryer_data;
    scan_result_payload_t  scan_data;
    uint8_t  fire_risk_score;
} node_state_t;

static node_state_t nodes[MAX_NODES];
static uint8_t num_active_nodes = 0;

/* ---- Fire alarm state ---- */
static bool fire_alarm_active = false;
static absolute_time_t fire_alarm_time;
static fire_alert_payload_t last_fire_alert;

/* ---- SX1262 Radio Interface (stub for build) ---- */

static void sx1262_init(void)
{
    spi_init(PIN_SX1262_SPI, 1000000);
    gpio_set_function(PIN_SX1262_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_SX1262_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SX1262_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_SX1262_CS);
    gpio_set_dir(PIN_SX1262_CS, GPIO_OUT);
    gpio_put(PIN_SX1262_CS, 1);

    gpio_init(PIN_SX1262_BUSY);
    gpio_set_dir(PIN_SX1262_BUSY, GPIO_IN);

    gpio_init(PIN_SX1262_IRQ);
    gpio_set_dir(PIN_SX1262_IRQ, GPIO_IN);

    gpio_init(PIN_SX1262_NRST);
    gpio_set_dir(PIN_SX1262_NRST, GPIO_OUT);
    gpio_put(PIN_SX1262_NRST, 1);

    /* Reset SX1262 */
    gpio_put(PIN_SX1262_NRST, 0);
    sleep_ms(10);
    gpio_put(PIN_SX1262_NRST, 1);
    sleep_ms(50);

    printf("[SX1262] Initialized on SPI1\n");
    /* In production: write SX1262 registers for LoRa SF7, 868MHz, 125kHz BW */
}

static void sx1262_send(const uint8_t *data, uint16_t len)
{
    /* In production: configure TX, write FIFO, set TX mode, wait for TxDone IRQ */
    printf("[SX1262] TX %d bytes\n", len);
}

static int16_t sx1262_receive(uint8_t *buf, uint16_t max_len)
{
    /* In production: configure RX, wait for RxDone IRQ, read FIFO */
    return 0; /* stub: no data */
}

/* ---- Fire alarm (85 dB piezo) ---- */

static void fire_alarm_start(void)
{
    fire_alarm_active = true;
    fire_alarm_time = get_absolute_time();
    /* Continuous high-volume alarm pattern */
    for (int i = 0; i < 60; i++) {  /* 60 cycles = 12 seconds, then re-check */
        pwm_set_gpio_level(PIN_PIEZO, 200);
        sleep_ms(100);
        pwm_set_gpio_level(PIN_PIEZO, 0);
        sleep_ms(50);
    }
}

static void fire_alarm_stop(void)
{
    fire_alarm_active = false;
    pwm_set_gpio_level(PIN_PIEZO, 0);
}

/* ---- TDMA Coordinator ---- */

static void tdma_run_frame(void)
{
    mesh_packet_t tx_pkt, rx_pkt;

    /* Slot 0: Hub broadcast — sync + commands */
    uint8_t heartbeat_payload[4] = {0};
    uint16_t pkt_len = mesh_build_packet(
        NODE_ID_HUB, NODE_ID_BROADCAST, PKT_HEARTBEAT,
        heartbeat_payload, 0, &tx_pkt);
    sx1262_send((uint8_t *)&tx_pkt, pkt_len);

    sleep_ms(TDMA_SLOT_MS);

    /* Slot 1: Receive from washer node */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                if (rx_pkt.src_id == NODE_ID_WASHER && rx_pkt.pkt_type == PKT_WASHER_DATA) {
                    memcpy(&nodes[0].washer_data, rx_pkt.payload,
                           sizeof(washer_data_payload_t));
                    nodes[0].node_type = 1;
                    nodes[0].active = true;
                    nodes[0].last_seen = get_absolute_time();
                }
            }
        }
        sleep_until(slot_end);
    }

    /* Slot 2: Receive from dryer node (SAFETY PRIORITY) */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                if (rx_pkt.src_id == NODE_ID_DRYER) {
                    if (rx_pkt.pkt_type == PKT_DRYER_DATA) {
                        memcpy(&nodes[1].dryer_data, rx_pkt.payload,
                               sizeof(dryer_data_payload_t));
                        nodes[1].node_type = 2;
                        nodes[1].active = true;
                        nodes[1].last_seen = get_absolute_time();
                        nodes[1].fire_risk_score = nodes[1].dryer_data.fire_risk_score;
                    } else if (rx_pkt.pkt_type == PKT_FIRE_ALERT) {
                        /* EMERGENCY: Fire alert override */
                        memcpy(&last_fire_alert, rx_pkt.payload,
                               sizeof(fire_alert_payload_t));
                        printf("[ALERT] FIRE ALERT from dryer! Risk=%d/255\n",
                               last_fire_alert.fire_risk_score);
                        fire_alarm_start();
                    }
                }
            }
        }
        sleep_until(slot_end);
    }

    /* Slot 3: Receive from scanner node */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                if (rx_pkt.src_id == NODE_ID_SCANNER && rx_pkt.pkt_type == PKT_SCAN_RESULT) {
                    memcpy(&nodes[2].scan_data, rx_pkt.payload,
                           sizeof(scan_result_payload_t));
                    nodes[2].node_type = 3;
                    nodes[2].active = true;
                    nodes[2].last_seen = get_absolute_time();
                }
            }
        }
        sleep_until(slot_end);
    }

    /* Slot 4: Control / ACK / retransmit */
    sleep_ms(TDMA_SLOT_MS);
}

/* ---- ESP32-C6 UART Bridge ---- */

static void esp32_bridge_send(const char *msg)
{
    uart_puts(PIN_ES32_UART, msg);
    uart_putc_raw(PIN_ES32_UART, '\n');
}

static void esp32_bridge_send_dryer_telemetry(void)
{
    if (!nodes[1].active)
        return;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "D:%d,EXH_T:%d,AMB_T:%d,PRES:%d,HUM:%d,VIB:%d,CUR:%d,ST:%d,HEAT:%d,RISK:%d,LINT:%d,DRY:%d,AL:%d",
             2,  /* dryer node id */
             nodes[1].dryer_data.exhaust_temp_c_x10,
             nodes[1].dryer_data.ambient_temp_c_x10,
             nodes[1].dryer_data.diff_pressure_pa,
             nodes[1].dryer_data.exhaust_hum_x10,
             nodes[1].dryer_data.vibration_rms_x10,
             nodes[1].dryer_data.current_ma,
             nodes[1].dryer_data.dryer_state,
             nodes[1].dryer_data.heating_on,
             nodes[1].dryer_data.fire_risk_score,
             nodes[1].dryer_data.lint_clog_level,
             nodes[1].dryer_data.dryness_level,
             nodes[1].dryer_data.alert_level);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_washer_telemetry(void)
{
    if (!nodes[0].active)
        return;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "W:%d,PH:%d,VIB:%d,FLOW:%d,WATER:%d,TEMP:%d,HUM:%d,MOT:%d,CUR:%d,DET:%d,RES:%d,FAB:%d,IMB:%d,LEAK:%d",
             1,
             nodes[0].washer_data.cycle_phase,
             nodes[0].washer_data.vibration_rms_x10,
             nodes[0].washer_data.flow_rate_mlmin,
             nodes[0].washer_data.total_water_ml,
             nodes[0].washer_data.water_temp_c_x10,
             nodes[0].washer_data.ambient_hum_x10,
             nodes[0].washer_data.motor_state,
             nodes[0].washer_data.current_ma,
             nodes[0].washer_data.detergent_mg,
             nodes[0].washer_data.reservoir_g_x10,
             nodes[0].washer_data.fabric_type,
             nodes[0].washer_data.imbalance_flag,
             nodes[0].washer_data.leak_flag);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_fire_alert(const fire_alert_payload_t *fa)
{
    char msg[256];
    snprintf(msg, sizeof(msg),
             "FIRE_ALERT:LEVEL:%d,RISK:%d,EXH_T:%d,PRES:%d,LINT:%d,HEAT:%d,NODE:%d",
             fa->alert_level,
             fa->fire_risk_score,
             fa->exhaust_temp_c_x10,
             fa->diff_pressure_pa,
             fa->lint_clog_level,
             fa->heating_on,
             fa->source_node);
    esp32_bridge_send(msg);
}

/* ---- TFT Display (stub) ---- */

static void tft_init(void)
{
    /* ILI9341 initialization via SPI */
    printf("[TFT] ILI9341 initialized\n");
}

static void tft_draw_dashboard(void)
{
    /* In production: draw lint risk gauge, cycle progress, detergent level */
    printf("[TFT] Dashboard: %d active nodes", num_active_nodes);
    if (nodes[1].active) {
        printf(" | Dryer: risk=%d/255 lint=%d exhaust=%.1fC",
               nodes[1].dryer_data.fire_risk_score,
               nodes[1].dryer_data.lint_clog_level,
               nodes[1].dryer_data.exhaust_temp_c_x10 / 10.0f);
    }
    if (nodes[0].active) {
        printf(" | Washer: phase=%d detergent=%dg",
               nodes[0].washer_data.cycle_phase,
               nodes[0].washer_data.reservoir_g_x10 / 10);
    }
    if (fire_alarm_active) {
        printf(" | !!! FIRE ALARM ACTIVE !!!");
    }
    printf("\n");
}

/* ---- Fire Risk ML Inference (stub) ---- */

static uint8_t compute_fire_risk(const dryer_data_payload_t *data)
{
    /* In production: run TFLite Micro 1D-CNN+LSTM model
     * Inputs: exhaust temp, diff pressure, humidity, ambient temp, current, vibration
     * Output: fire risk score 0-255 (0.0-1.0)
     *
     * Stub: rule-based heuristic */
    float score = 0.0f;

    float exh_temp = data->exhaust_temp_c_x10 / 10.0f;
    float diff_pres = data->diff_pressure_pa;
    float hum = data->exhaust_hum_x10 / 10.0f;

    /* Exhaust temperature is the primary indicator */
    if (exh_temp > 95.0f)       score += 0.7f;
    else if (exh_temp > 85.0f)  score += 0.4f;
    else if (exh_temp > 75.0f)  score += 0.15f;

    /* High backpressure = lint clog = fire risk amplifier */
    if (diff_pres > 200)       score += 0.3f;
    else if (diff_pres > 120)  score += 0.15f;

    /* Low humidity during heating = very dry lint = more flammable */
    if (data->heating_on && hum < 20.0f)  score += 0.15f;

    if (score > 1.0f) score = 1.0f;
    return (uint8_t)(score * 255.0f);
}

/* ---- Safety check ---- */

static void check_dryer_safety(void)
{
    if (!nodes[1].active)
        return;

    /* Run ML fire risk inference */
    uint8_t ml_risk = compute_fire_risk(&nodes[1].dryer_data);
    nodes[1].fire_risk_score = ml_risk;
    nodes[1].dryer_data.fire_risk_score = ml_risk;

    float risk = ml_risk / 255.0f;
    float exh_temp = nodes[1].dryer_data.exhaust_temp_c_x10 / 10.0f;

    if (risk > 0.95f || exh_temp > 95.0f) {
        /* EMERGENCY: immediate alarm */
        printf("[SAFETY] EMERGENCY: Fire risk %.2f, exhaust %.1fC\n", risk, exh_temp);
        if (!fire_alarm_active) {
            fire_alert_payload_t fa = {0};
            fa.alert_level = ALERT_EMERGENCY;
            fa.fire_risk_score = ml_risk;
            fa.exhaust_temp_c_x10 = nodes[1].dryer_data.exhaust_temp_c_x10;
            fa.diff_pressure_pa = nodes[1].dryer_data.diff_pressure_pa;
            fa.lint_clog_level = nodes[1].dryer_data.lint_clog_level;
            fa.heating_on = nodes[1].dryer_data.heating_on;
            fa.source_node = NODE_ID_DRYER;
            memcpy(&last_fire_alert, &fa, sizeof(fa));
            esp32_bridge_send_fire_alert(&fa);
            fire_alarm_start();
        }
    } else if (risk > 0.8f) {
        /* CRITICAL: push notification + advisory to shutoff dryer */
        printf("[SAFETY] CRITICAL: Fire risk %.2f — clean lint trap NOW\n", risk);
        char msg[128];
        snprintf(msg, sizeof(msg), "SAFETY_CRITICAL:RISK:%d", ml_risk);
        esp32_bridge_send(msg);
        nodes[1].dryer_data.alert_level = ALERT_CRITICAL;
    } else if (risk > 0.6f) {
        /* WARNING: clean lint trap soon */
        printf("[SAFETY] WARNING: Fire risk %.2f — clean lint trap soon\n", risk);
        nodes[1].dryer_data.alert_level = ALERT_WARNING;
    } else {
        nodes[1].dryer_data.alert_level = ALERT_OK;
    }
}

/* ---- Main Loop (Core 0) ---- */

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    printf("=== WashWise Hub Node v1.0 ===\n");
    printf("RP2040 + ESP32-C6\n");

    /* Initialize hardware */
    uart_init(PIN_ES32_UART, 115200);
    gpio_set_function(PIN_ES32_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ES32_RX, GPIO_FUNC_UART);

    /* Piezo PWM for alarm */
    gpio_set_function(PIN_PIEZO, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_PIEZO);
    pwm_set_wrap(slice, 250);
    pwm_set_enabled(slice, true);

    /* Status LEDs */
    gpio_init(PIN_LED_R); gpio_set_dir(PIN_LED_R, GPIO_OUT);
    gpio_init(PIN_LED_G); gpio_set_dir(PIN_LED_G, GPIO_OUT);
    gpio_init(PIN_LED_B); gpio_set_dir(PIN_LED_B, GPIO_OUT);

    sx1262_init();
    tft_init();

    /* Initialize node state */
    memset(nodes, 0, sizeof(nodes));
    nodes[0].node_id = NODE_ID_WASHER;
    nodes[1].node_id = NODE_ID_DRYER;
    nodes[2].node_id = NODE_ID_SCANNER;
    for (int i = 0; i < MAX_NODES; i++)
        nodes[i].active = false;

    printf("Hub initialized. Starting TDMA mesh coordinator.\n");

    /* Main loop */
    uint32_t frame_count = 0;
    while (true) {
        /* Run one TDMA frame */
        tdma_run_frame();

        /* Count active nodes + check timeouts */
        num_active_nodes = 0;
        for (int i = 0; i < MAX_NODES; i++) {
            if (nodes[i].active) {
                int64_t age = absolute_time_diff_us(nodes[i].last_seen, get_absolute_time());
                if (age > NODE_TIMEOUT_S * 1000000) {
                    nodes[i].active = false;
                } else {
                    num_active_nodes++;
                }
            }
        }

        /* Safety checks every frame (fire safety is highest priority) */
        check_dryer_safety();

        /* Check if fire alarm should continue */
        if (fire_alarm_active) {
            int64_t alarm_age = absolute_time_diff_us(fire_alarm_time, get_absolute_time());
            /* Auto-stop after 30s if risk dropped */
            if (alarm_age > 30 * 1000000) {
                uint8_t current_risk = compute_fire_risk(&nodes[1].dryer_data);
                if (current_risk < 200) { /* < 0.78 */
                    fire_alarm_stop();
                    printf("[ALARM] Fire alarm auto-cleared (risk dropped)\n");
                } else {
                    fire_alarm_time = get_absolute_time(); /* extend */
                }
            }
        }

        /* Update TFT every 4 frames (2 seconds) */
        if (frame_count % 4 == 0) {
            tft_draw_dashboard();
        }

        /* Forward telemetry to ESP32-C6 for WiFi/BLE bridge */
        esp32_bridge_send_dryer_telemetry();
        if (frame_count % 2 == 0)
            esp32_bridge_send_washer_telemetry();

        /* Status LED: green=ok, red=alarm, blue=scanning */
        if (fire_alarm_active) {
            gpio_put(PIN_LED_R, 1);
            gpio_put(PIN_LED_G, 0);
            gpio_put(PIN_LED_B, 0);
        } else if (nodes[1].active && nodes[1].fire_risk_score > 150) {
            gpio_put(PIN_LED_R, 1);
            gpio_put(PIN_LED_G, 0);
            gpio_put(PIN_LED_B, 0);
        } else {
            gpio_put(PIN_LED_R, 0);
            gpio_put(PIN_LED_G, 1);
            gpio_put(PIN_LED_B, 0);
        }

        frame_count++;
    }

    return 0;
}
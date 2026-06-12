/*
 * hub_main.c — Aqua Guard Hub Node (RP2040 + ESP32-C6)
 *
 * Responsibilities:
 * - Sub-GHz mesh coordinator (TDMA scheduler)
 * - Data aggregation from sensor + feeder nodes
 * - WiFi uplink to MQTT broker
 * - BLE GATT server for mobile app
 * - TFT dashboard display
 * - Local alarm triggers
 * - OTA update distribution
 * - TFLite Micro anomaly detection
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

/* ---- Pin Definitions ---- */
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

#define PIN_SD_CS       9

#define PIN_PIEZO       22
#define PIN_USER_BTN    23
#define PIN_LED_R       24
#define PIN_LED_G       25
#define PIN_LED_B       26

/* ---- Mesh State ---- */
#define MAX_NODES       8
#define NODE_TIMEOUT_S  30

typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;     /* 0=unknown, 1=sensor, 2=feeder */
    bool     active;
    absolute_time_t last_seen;
    sensor_data_payload_t  sensor_data;
    feeder_status_payload_t feeder_data;
    float    anomaly_score;
} node_state_t;

static node_state_t nodes[MAX_NODES];
static uint8_t num_active_nodes = 0;

/* ---- SX1262 Radio Interface (stub) ---- */

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

/* ---- TDMA Coordinator ---- */

static void tdma_run_frame(void)
{
    /* Slot 0: Hub broadcast — sync + commands */
    uint8_t cmd_payload[16] = {0};
    mesh_packet_t tx_pkt;
    uint16_t pkt_len = mesh_build_packet(
        NODE_ID_HUB, NODE_ID_BROADCAST, PKT_HEARTBEAT,
        cmd_payload, 0, &tx_pkt);
    sx1262_send((uint8_t *)&tx_pkt, pkt_len);

    sleep_ms(TDMA_SLOT_MS);

    /* Slots 1-7: Receive from sensor nodes */
    for (int slot = 1; slot <= 7; slot++) {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        
        int16_t rx_len = sx1262_receive((uint8_t *)&tx_pkt, sizeof(tx_pkt));
        if (rx_len > 0) {
            mesh_packet_t rx_pkt;
            if (mesh_parse_packet((uint8_t *)&tx_pkt, rx_len, &rx_pkt) == 0) {
                if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                    /* Process received data */
                    uint8_t nid = rx_pkt.src_id;
                    if (nid >= NODE_ID_SENSOR_MIN && nid <= NODE_ID_SENSOR_MAX) {
                        int idx = nid - 1;
                        if (idx < MAX_NODES) {
                            nodes[idx].node_id = nid;
                            nodes[idx].node_type = 1;
                            nodes[idx].active = true;
                            nodes[idx].last_seen = get_absolute_time();
                            if (rx_pkt.pkt_type == PKT_SENSOR_DATA) {
                                memcpy(&nodes[idx].sensor_data, rx_pkt.payload,
                                       sizeof(sensor_data_payload_t));
                            }
                        }
                    } else if (nid == NODE_ID_FEEDER) {
                        nodes[7].node_id = nid;
                        nodes[7].node_type = 2;
                        nodes[7].active = true;
                        nodes[7].last_seen = get_absolute_time();
                        if (rx_pkt.pkt_type == PKT_FEEDER_STATUS) {
                            memcpy(&nodes[7].feeder_data, rx_pkt.payload,
                                   sizeof(feeder_status_payload_t));
                        }
                    }
                }
            }
        }
        
        sleep_until(slot_end);
    }

    /* Slot 8: Receive from feeder node (handled above if in slots 1-7) */
    /* Slot 9: Control/retransmit slot */
    sleep_ms(TDMA_SLOT_MS * 2);
}

/* ---- ESP32-C6 UART Bridge ---- */

static void esp32_bridge_send(const char *msg)
{
    uart_puts(PIN_ES32_UART, msg);
    uart_putc_raw(PIN_ES32_UART, '\n');
}

/* ---- TFT Display (stub) ---- */

static void tft_init(void)
{
    /* ILI9341 initialization via SPI */
    printf("[TFT] ILI9341 initialized\n");
}

static void tft_draw_dashboard(void)
{
    /* In production: draw sensor values, node status, alerts */
    /* Stub: render basic status */
    printf("[TFT] Dashboard: %d active nodes\n", num_active_nodes);
}

/* ---- Anomaly Detection (stub) ---- */

static float compute_anomaly(const sensor_data_payload_t *data)
{
    /* In production: run TFLite Micro 1D-CNN+LSTM model */
    /* Stub: simple rule-based check */
    float score = 0.0f;
    if (data->ph < 6.0f || data->ph > 8.5f) score += 0.4f;
    if (data->ammonia > 0.5f) score += 0.5f;
    if (data->nitrite > 0.5f) score += 0.3f;
    if (data->dissolved_o2 < 3.0f) score += 0.6f;
    if (data->temperature < 18.0f || data->temperature > 32.0f) score += 0.4f;
    return score > 1.0f ? 1.0f : score;
}

/* ---- Alarm System ---- */

static void trigger_alarm(uint8_t level, const char *message)
{
    /* Piezo buzzer patterns */
    if (level >= 2) {
        /* Warning: 2 beeps */
        for (int i = 0; i < 2; i++) {
            pwm_set_gpio_level(PIN_PIEZO, 128);
            sleep_ms(200);
            pwm_set_gpio_level(PIN_PIEZO, 0);
            sleep_ms(200);
        }
    }
    if (level >= 3) {
        /* Critical: continuous alarm */
        for (int i = 0; i < 10; i++) {
            pwm_set_gpio_level(PIN_PIEZO, 200);
            sleep_ms(100);
            pwm_set_gpio_level(PIN_PIEZO, 0);
            sleep_ms(100);
        }
    }
    printf("[ALARM] Level %d: %s\n", level, message);
}

/* ---- Main Loop (Core 0) ---- */

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    printf("=== Aqua Guard Hub Node v1.0 ===\n");
    printf("RP2040 + ESP32-C6\n");

    /* Initialize hardware */
    uart_init(PIN_ES32_UART, 115200);
    gpio_set_function(PIN_ES32_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ES32_RX, GPIO_FUNC_UART);

    sx1262_init();
    tft_init();

    /* Initialize node state */
    memset(nodes, 0, sizeof(nodes));
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].node_id = i + 1;
        nodes[i].active = false;
    }

    printf("Hub initialized. Starting TDMA mesh coordinator.\n");

    /* Main loop */
    uint32_t frame_count = 0;
    while (true) {
        /* Run one TDMA frame */
        tdma_run_frame();

        /* Count active nodes */
        num_active_nodes = 0;
        for (int i = 0; i < MAX_NODES; i++) {
            if (nodes[i].active) {
                /* Check for timeout */
                int64_t age = absolute_time_diff_us(nodes[i].last_seen, get_absolute_time());
                if (age > NODE_TIMEOUT_S * 1000000) {
                    nodes[i].active = false;
                } else {
                    num_active_nodes++;
                }
            }
        }

        /* Run anomaly detection on all sensor nodes */
        for (int i = 0; i < 7; i++) {
            if (nodes[i].active && nodes[i].node_type == 1) {
                nodes[i].anomaly_score = compute_anomaly(&nodes[i].sensor_data);

                if (nodes[i].anomaly_score > 0.9f) {
                    trigger_alarm(3, "CRITICAL water quality anomaly detected!");
                } else if (nodes[i].anomaly_score > 0.7f) {
                    trigger_alarm(2, "WARNING: water parameters trending out of range");
                }
            }
        }

        /* Update TFT every 5 frames (5 seconds) */
        if (frame_count % 5 == 0) {
            tft_draw_dashboard();
        }

        /* Forward data to ESP32-C6 for WiFi/BLE bridge every frame */
        char bridge_msg[128];
        snprintf(bridge_msg, sizeof(bridge_msg),
                 "D:%d,N:%d,A:%.2f",
                 frame_count, num_active_nodes,
                 num_active_nodes > 0 ? nodes[0].anomaly_score : 0.0f);
        esp32_bridge_send(bridge_msg);

        frame_count++;
    }

    return 0;
}
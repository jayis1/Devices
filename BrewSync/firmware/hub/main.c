/*
 * BrewSync Hub - Main Firmware
 * RP2040 (protocol coordinator) + ESP32-C3 (Wi-Fi/MQTT bridge)
 * RP2040 runs this code; ESP32-C3 runs ESP-IDF bridge
 *
 * Coordinates all Sub-GHz nodes, manages fermentation state machine,
 * drives LCD display, controls relays, bridges to cloud via ESP32-C3
 *
 * Copyright (c) 2025 BrewSync. MIT License.
 */

#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "bsmp.h"
#include "bsmp_sensors.h"

/* ---- Pin definitions ---- */
#define PIN_SX1262_CS      5   /* SPI0 CS */
#define PIN_SX1262_BUSY    6
#define PIN_SX1262_DIO1    7
#define PIN_SX1262_RST     8
#define PIN_RELAY_HEAT     10  /* Relay 1: heating */
#define PIN_RELAY_COOL     11  /* Relay 2: cooling */
#define PIN_BUZZER         12
#define PIN_LED_STATUS     13
#define PIN_LCD_CS         17
#define PIN_LCD_DC         20
#define PIN_LCD_RST        21
#define PIN_SHT40_SDA      26
#define PIN_SHT40_SCL      27
#define PIN_BMP390_SDA     26  /* Shared I2C0 bus */
#define PIN_BMP390_SCL     27
#define PIN_UART_TX        0   /* UART to ESP32-C3 */
#define PIN_UART_RX        1

/* ---- Configuration ---- */
#define FW_VERSION          "1.2.0"
#define UART_BAUD           115200
#define MAX_NODES           16
#define BEACON_INTERVAL_MS  60000   /* Send beacon every 60s */
#define NODE_TIMEOUT_MS     300000  /* 5 min node timeout */

/* ---- Node tracking ---- */
typedef struct {
    uint16_t addr;
    uint8_t  type;         /* BSMP_NODE_FERMENTER, etc. */
    bool     active;
    uint32_t last_seen_ms;
    uint16_t battery_mv;
    int8_t   rssi_dbm;
    /* Latest telemetry (if fermenter) */
    float    sg;
    float    temp_c;
    float    co2_ppm;
    float    pressure_bar;
    float    ph;
    uint8_t  flags;
    uint8_t  sensor_status;
} node_entry_t;

/* ---- Fermentation batch tracking ---- */
typedef struct {
    bool     active;
    uint16_t fermenter_addr;
    float    target_og;
    float    target_fg;
    float    target_temp_c;
    float    current_sg;
    float    current_temp_c;
    uint32_t start_time_ms;
    ferment_state_t state;
    /* Temperature PID */
    float    temp_integral;
    float    temp_prev_error;
    uint32_t temp_pid_interval_ms;
} batch_t;

/* ---- Globals ---- */
static struct {
    node_entry_t  nodes[MAX_NODES];
    uint8_t       node_count;
    batch_t       batch;
    uint8_t       seq;
    uint32_t      beacon_timer;
    uint32_t      pid_timer;
    bool          wifi_connected;
    uint8_t       aes_key[16];
} g;

static bsmp_hal_t hal;

/* ---- LCD display ---- */
/* Simplified IPS LCD driver - actual implementation uses SPI graphics */
static void lcd_init(void) {
    /* Initialize 3.5" 480x320 IPS LCD over SPI */
    gpio_init(PIN_LCD_CS); gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_init(PIN_LCD_DC); gpio_set_dir(PIN_LCD_DC, GPIO_OUT);
    gpio_init(PIN_LCD_RST); gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
    /* SPI init + LCD reset sequence would go here */
}

static void lcd_show_status(const batch_t *batch, const node_entry_t *nodes, uint8_t count) {
    /* Render status screen: batch state, SG, temp, CO2, node list */
    /* Simplified - actual implementation uses framebuffer graphics */
    (void)batch; (void)nodes; (void)count;
}

/* ---- Temperature PID controller ---- */
/* Controls relay for heating/cooling to maintain target fermentation temp */
#define PID_KP    2.0f
#define PID_KI    0.05f
#define PID_KD    0.5f
#define PID_MAX_INTEGRAL  50.0f
#define TEMP_HYSTERESIS   0.5f

static void temp_pid_update(void) {
    if (!g.batch.active) {
        gpio_put(PIN_RELAY_HEAT, 0);
        gpio_put(PIN_RELAY_COOL, 0);
        return;
    }

    float target = g.batch.target_temp_c;
    float current = g.batch.current_temp_c;
    float error = target - current;

    /* Proportional */
    float p_term = PID_KP * error;

    /* Integral */
    g.batch.temp_integral += error * (g.batch.temp_pid_interval_ms / 1000.0f);
    if (g.batch.temp_integral > PID_MAX_INTEGRAL)
        g.batch.temp_integral = PID_MAX_INTEGRAL;
    if (g.batch.temp_integral < -PID_MAX_INTEGRAL)
        g.batch.temp_integral = -PID_MAX_INTEGRAL;
    float i_term = PID_KI * g.batch.temp_integral;

    /* Derivative */
    float d_term = PID_KD * (error - g.batch.temp_prev_error);
    g.batch.temp_prev_error = error;

    float output = p_term + i_term + d_term;

    /* Relay control with hysteresis */
    if (output > TEMP_HYSTERESIS) {
        gpio_put(PIN_RELAY_HEAT, 1);  /* Heat ON */
        gpio_put(PIN_RELAY_COOL, 0);
    } else if (output < -TEMP_HYSTERESIS) {
        gpio_put(PIN_RELAY_HEAT, 0);
        gpio_put(PIN_RELAY_COOL, 1);  /* Cool ON */
    } else {
        gpio_put(PIN_RELAY_HEAT, 0);  /* Both OFF - within hysteresis */
        gpio_put(PIN_RELAY_COOL, 0);
    }
}

/* ---- ESP32-C3 UART bridge ---- */
/* Commands sent to ESP32-C3 over UART for Wi-Fi/MQTT bridge */
#define ESP_CMD_CONNECT_WIFI    0x01
#define ESP_CMD_MQTT_PUBLISH    0x02
#define ESP_CMD_MQTT_SUBSCRIBE  0x03
#define ESP_CMD_OTA_CHECK       0x04

static void esp_send_cmd(uint8_t cmd, const uint8_t *data, uint16_t len) {
    uint8_t header[4] = {0xAA, 0x55, cmd, (uint8_t)(len & 0xFF)};
    uart_write_blocking(uart1, header, 4);
    if (len > 0 && data) {
        uart_write_blocking(uart1, data, len);
    }
}

/* ---- Node management ---- */
static node_entry_t *find_node(uint16_t addr) {
    for (uint8_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].addr == addr) return &g.nodes[i];
    }
    return NULL;
}

static node_entry_t *add_node(uint16_t addr, uint8_t type) {
    node_entry_t *n = find_node(addr);
    if (n) {
        n->last_seen_ms = to_ms_since_boot(get_absolute_time());
        return n;
    }
    if (g.node_count >= MAX_NODES) return NULL;
    n = &g.nodes[g.node_count++];
    n->addr = addr;
    n->type = type;
    n->active = true;
    n->last_seen_ms = to_ms_since_boot(get_absolute_time());
    return n;
}

/* ---- Process received frame ---- */
static void process_frame(const bsmp_frame_t *frame) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    switch (frame->type) {
        case BSMP_TYPE_TELEMETRY: {
            if (frame->len < sizeof(bsmp_fermenter_telem_t)) break;

            bsmp_fermenter_telem_t telem;
            memcpy(&telem, frame->payload, sizeof(telem));

            node_entry_t *node = add_node(frame->addr, BSMP_NODE_FERMENTER);
            if (!node) break;

            node->sg           = telem.sg;
            node->temp_c       = telem.temp_c;
            node->co2_ppm      = telem.co2_ppm;
            node->pressure_bar = telem.pressure_bar;
            node->ph           = telem.ph;
            node->battery_mv   = telem.battery_mv;
            node->flags        = telem.flags;
            node->sensor_status = telem.sensor_status;
            node->last_seen_ms = now;

            /* Update batch if this is our fermenter */
            if (g.batch.active && g.batch.fermenter_addr == frame->addr) {
                g.batch.current_sg = telem.sg;
                g.batch.current_temp_c = telem.temp_c;
            }

            /* Forward to cloud via ESP32-C3 */
            esp_send_cmd(ESP_CMD_MQTT_PUBLISH, frame->payload, frame->len);

            /* Check for alerts */
            if (telem.flags & BSMP_FLAG_TEMP_ALARM) {
                gpio_put(PIN_BUZZER, 1);
                sleep_ms(200);
                gpio_put(PIN_BUZZER, 0);
            }
            break;
        }

        case BSMP_TYPE_PAIR_REQ: {
            /* Handle pairing request - assign address */
            uint8_t assigned_addr = 0x0001 + g.node_count;
            /* Build PAIR_RESP frame and send */
            uint8_t resp_payload[36];
            resp_payload[0] = (assigned_addr >> 8) & 0xFF;
            resp_payload[1] = assigned_addr & 0xFF;
            /* In production: X25519 ECDH key exchange here */
            uint8_t resp_buf[64];
            int len = bsmp_encode(frame->addr, g.seq++, BSMP_TYPE_PAIR_RESP,
                                  resp_payload, 2, resp_buf, sizeof(resp_buf));
            if (len > 0) {
                sx1262_send(&hal, resp_buf, (uint8_t)len);
            }
            break;
        }

        case BSMP_TYPE_HEARTBEAT: {
            node_entry_t *node = find_node(frame->addr);
            if (node) node->last_seen_ms = now;
            break;
        }

        default:
            break;
    }
}

/* ---- Main ---- */
int main(void) {
    stdio_init_all();

    /* Init GPIO */
    gpio_init(PIN_RELAY_HEAT); gpio_set_dir(PIN_RELAY_HEAT, GPIO_OUT);
    gpio_init(PIN_RELAY_COOL); gpio_set_dir(PIN_RELAY_COOL, GPIO_OUT);
    gpio_init(PIN_BUZZER);     gpio_set_dir(PIN_BUZZER, GPIO_OUT);
    gpio_init(PIN_LED_STATUS); gpio_set_dir(PIN_LED_STATUS, GPIO_OUT);

    /* Init UART to ESP32-C3 */
    uart_init(uart1, UART_BAUD);
    gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);

    /* Init I2C for SHT40/BMP390 */
    i2c_init(i2c0, 100000);

    /* Init SPI for SX1262 + LCD */
    spi_init(spi0, 4000000);

    /* Init sensors */
    sht40_init(&hal, 0);
    bmp390_init(&hal, 0);

    /* Init radio */
    sx1262_init(&hal, 0, PIN_SX1262_CS, PIN_SX1262_BUSY,
                PIN_SX1262_DIO1, PIN_SX1262_RST);
    sx1262_rx_info_t rx_info;
    sx1262_config(&rx_info, 7, 125, 5, 868000000, 14);

    /* Init LCD */
    lcd_init();

    /* Init ESP32-C3 bridge */
    esp_send_cmd(ESP_CMD_CONNECT_WIFI, NULL, 0);

    /* Defaults */
    memset(&g, 0, sizeof(g));
    g.node_count = 0;
    g.seq = 0;

    uint32_t last_beacon = 0;
    uint32_t last_pid = 0;

    /* Main loop */
    while (1) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        /* Receive from Sub-GHz */
        bsmp_frame_t frame;
        int rc = sx1262_receive(&hal, (uint8_t *)&frame, sizeof(frame),
                                &rx_info, 50);
        if (rc > 0 && rx_info.crc_ok) {
            process_frame(&frame);
        }

        /* Send beacon periodically */
        if ((now - last_beacon) >= BEACON_INTERVAL_MS) {
            uint8_t beacon_payload[8];
            uint32_t net_id = 0x42525359; /* "BRSY" */
            memcpy(beacon_payload, &net_id, 4);
            beacon_payload[4] = g.node_count;
            beacon_payload[5] = 7; /* SF7 */
            uint16_t uptime = (uint16_t)(now / 1000);
            memcpy(&beacon_payload[6], &uptime, 2);

            uint8_t buf[64];
            int len = bsmp_encode(BSMP_ADDR_BROADCAST, g.seq++,
                                  BSMP_TYPE_BEACON, beacon_payload, 8,
                                  buf, sizeof(buf));
            if (len > 0) sx1262_send(&hal, buf, (uint8_t)len);
            last_beacon = now;
        }

        /* Temperature PID control */
        if (g.batch.active && (now - last_pid) >= 5000) { /* 5s interval */
            temp_pid_update();
            last_pid = now;
        }

        /* Update LCD */
        lcd_show_status(&g.batch, g.nodes, g.node_count);

        /* Check node timeouts */
        for (uint8_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].active && (now - g.nodes[i].last_seen_ms) > NODE_TIMEOUT_MS) {
                g.nodes[i].active = false;
                /* Alert: node offline */
            }
        }

        /* Blink status LED */
        gpio_put(PIN_LED_STATUS, 1);
        sleep_ms(50);
        gpio_put(PIN_LED_STATUS, 0);

        sleep_ms(50); /* Yield */
    }
}
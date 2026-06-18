/*
 * hub_main.c — PorchGuard Hub Node (RP2040 + ESP32-C6)
 *
 * Responsibilities:
 * - Sub-GHz mesh coordinator (TDMA scheduler, 5 slots)
 * - Data aggregation from porch-camera + mailbox + lock nodes
 * - WiFi uplink to MQTT broker
 * - BLE GATT server for mobile app
 * - TFT dashboard: porch status, parcel count, last event, siren state
 * - Local siren triggers (100 dB piezo for pirate/tamper alerts)
 * - OTA update distribution
 * - TFLite Micro pirate behavior inference
 *
 * RP2040 Core 0: mesh TDMA + ML + display + siren (hard real-time)
 * RP2040 Core 1: ESP32-C6 UART bridge (soft real-time)
 *
 * SAFETY: Pirate/tamper alerts override normal TDMA. Hub halts normal
 * scheduling, activates 100 dB siren, relays clip ref to app + cloud.
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

#define PIN_SIREN       22   /* 100 dB piezo siren via MOSFET */
#define PIN_USER_BTN    23
#define PIN_LED_R       24
#define PIN_LED_G       25
#define PIN_LED_B       26

/* ---- Mesh State ---- */
#define MAX_NODES       3
#define NODE_TIMEOUT_S  120

typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;     /* 0=unknown,1=camera,2=mailbox,3=lock */
    bool     active;
    absolute_time_t last_seen;
    camera_data_payload_t  camera_data;
    mailbox_data_payload_t mailbox_data;
    lock_data_payload_t    lock_data;
    uint8_t  pirate_risk;
} node_state_t;

static node_state_t nodes[MAX_NODES];
static uint8_t num_active_nodes = 0;

/* ---- Alarm state ---- */
static bool siren_active = false;
static absolute_time_t siren_time;
static pirate_alert_payload_t last_pirate_alert;
static tamper_alert_payload_t last_tamper_alert;
static bool armed = true;

/* ---- Event log (circular, 64 entries) ---- */
#define EVENT_LOG_SIZE 64
typedef struct {
    uint8_t  event_type;   /* 0=delivery,1=mail,2=pirate,3=tamper,4=visitor,5=door */
    uint8_t  severity;
    uint16_t timestamp_s;
    uint8_t  source_node;
    uint8_t  reserved[3];
} event_log_entry_t;
static event_log_entry_t event_log[EVENT_LOG_SIZE];
static uint8_t event_log_head = 0;

static void log_event(uint8_t type, uint8_t severity, uint8_t source)
{
    event_log[event_log_head].event_type  = type;
    event_log[event_log_head].severity   = severity;
    event_log[event_log_head].timestamp_s = (uint16_t)(to_us_since_boot(get_absolute_time()) / 1000000);
    event_log[event_log_head].source_node = source;
    event_log_head = (event_log_head + 1) % EVENT_LOG_SIZE;
}

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

    printf("[SX1262] Initialized on SPI1 (915MHz, +20dBm)\n");
    /* In production: write SX1262 registers for LoRa SF7, 915MHz, 125kHz BW */
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

/* ---- 100 dB siren ---- */

static void siren_start(uint8_t duration_s)
{
    siren_active = true;
    siren_time = get_absolute_time();
    printf("[SIREN] ACTIVATED for %ds — pirate/tamper!\n", duration_s);
    /* Pulsed 3.3kHz square wave via MOSFET for 100 dB piezo */
    for (int cycle = 0; cycle < duration_s * 10; cycle++) {
        if (!siren_active) break;
        for (int i = 0; i < 100; i++) {
            pwm_set_gpio_level(PIN_SIREN, 200);
            sleep_us(150);
            pwm_set_gpio_level(PIN_SIREN, 0);
            sleep_us(150);
        }
        /* Brief off-period between pulses for attention-grabbing pattern */
        sleep_ms(50);
    }
    siren_active = false;
    pwm_set_gpio_level(PIN_SIREN, 0);
}

static void siren_stop(void)
{
    siren_active = false;
    pwm_set_gpio_level(PIN_SIREN, 0);
    printf("[SIREN] Stopped\n");
}

/* ---- TDMA Coordinator ---- */

static void tdma_run_frame(void)
{
    mesh_packet_t tx_pkt, rx_pkt;

    /* Slot 0: Hub broadcast — sync + commands (heartbeat carries armed state) */
    uint8_t hb_payload[2] = { armed ? 1 : 0, siren_active ? 1 : 0 };
    uint16_t pkt_len = mesh_build_packet(
        NODE_ID_HUB, NODE_ID_BROADCAST, PKT_HEARTBEAT,
        hb_payload, sizeof(hb_payload), &tx_pkt);
    sx1262_send((uint8_t *)&tx_pkt, pkt_len);

    sleep_ms(TDMA_SLOT_MS);

    /* Slot 1: Receive from porch camera (primary sensor — parcel + pirate) */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                if (rx_pkt.src_id == NODE_ID_CAMERA) {
                    if (rx_pkt.pkt_type == PKT_CAMERA_DATA) {
                        memcpy(&nodes[0].camera_data, rx_pkt.payload,
                               sizeof(camera_data_payload_t));
                        nodes[0].node_type = 1;
                        nodes[0].active = true;
                        nodes[0].last_seen = get_absolute_time();
                        nodes[0].pirate_risk = nodes[0].camera_data.pirate_risk;
                    } else if (rx_pkt.pkt_type == PKT_PIRATE_ALERT) {
                        /* CRITICAL: Pirate behavior detected */
                        memcpy(&last_pirate_alert, rx_pkt.payload,
                               sizeof(pirate_alert_payload_t));
                        printf("[ALERT] PIRATE ALERT from camera! Risk=%d/255\n",
                               last_pirate_alert.pirate_risk);
                        log_event(2, last_pirate_alert.alert_level, NODE_ID_CAMERA);
                        if (last_pirate_alert.siren_requested && armed)
                            siren_start(15);
                    } else if (rx_pkt.pkt_type == PKT_DELIVERY_EVENT) {
                        delivery_event_payload_t dev;
                        memcpy(&dev, rx_pkt.payload, sizeof(dev));
                        printf("[EVENT] Delivery: parcel_class=%d courier=%d\n",
                               dev.parcel_class, dev.courier_id);
                        log_event(0, ALERT_INFO, NODE_ID_CAMERA);
                    } else if (rx_pkt.pkt_type == PKT_TAMPER_ALERT) {
                        memcpy(&last_tamper_alert, rx_pkt.payload,
                               sizeof(tamper_alert_payload_t));
                        printf("[ALERT] Camera TAMPER type=%d\n",
                               last_tamper_alert.tamper_type);
                        log_event(3, last_tamper_alert.severity, NODE_ID_CAMERA);
                    }
                }
            }
        }
        sleep_until(slot_end);
    }

    /* Slot 2: Receive from mailbox node (long-range, polled less often) */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                if (rx_pkt.src_id == NODE_ID_MAILBOX) {
                    if (rx_pkt.pkt_type == PKT_MAILBOX_DATA) {
                        memcpy(&nodes[1].mailbox_data, rx_pkt.payload,
                               sizeof(mailbox_data_payload_t));
                        nodes[1].node_type = 2;
                        nodes[1].active = true;
                        nodes[1].last_seen = get_absolute_time();
                        if (nodes[1].mailbox_data.last_event == 1) {
                            printf("[EVENT] Mail arrived: class=%d weight=%dmg\n",
                                   nodes[1].mailbox_data.mail_class,
                                   nodes[1].mailbox_data.weight_mg);
                            log_event(1, ALERT_INFO, NODE_ID_MAILBOX);
                        }
                    } else if (rx_pkt.pkt_type == PKT_TAMPER_ALERT) {
                        memcpy(&last_tamper_alert, rx_pkt.payload,
                               sizeof(tamper_alert_payload_t));
                        printf("[ALERT] Mailbox TAMPER type=%d\n",
                               last_tamper_alert.tamper_type);
                        log_event(3, last_tamper_alert.severity, NODE_ID_MAILBOX);
                        if (armed && last_tamper_alert.severity >= 2)
                            siren_start(10);
                    }
                }
            }
        }
        sleep_until(slot_end);
    }

    /* Slot 3: Receive from lock node (usually BLE; Sub-GHz on fallback) */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                if (rx_pkt.src_id == NODE_ID_LOCK) {
                    if (rx_pkt.pkt_type == PKT_LOCK_DATA) {
                        memcpy(&nodes[2].lock_data, rx_pkt.payload,
                               sizeof(lock_data_payload_t));
                        nodes[2].node_type = 3;
                        nodes[2].active = true;
                        nodes[2].last_seen = get_absolute_time();
                        if (nodes[2].lock_data.tamper_flag >= 1) {
                            printf("[ALERT] Lock tamper flag=%d\n",
                                   nodes[2].lock_data.tamper_flag);
                            log_event(3, ALERT_CRITICAL, NODE_ID_LOCK);
                            if (armed)
                                siren_start(12);
                        }
                        if (nodes[2].lock_data.door_open_s > 120) {
                            printf("[WARN] Door open %ds\n",
                                   nodes[2].lock_data.door_open_s);
                            log_event(5, ALERT_WARNING, NODE_ID_LOCK);
                        }
                    } else if (rx_pkt.pkt_type == PKT_TAMPER_ALERT) {
                        memcpy(&last_tamper_alert, rx_pkt.payload,
                               sizeof(tamper_alert_payload_t));
                        printf("[ALERT] Lock forced entry!\n");
                        log_event(3, ALERT_EMERGENCY, NODE_ID_LOCK);
                        if (armed)
                            siren_start(20);
                    }
                }
            }
        }
        sleep_until(slot_end);
    }

    /* Slot 4: Control / ACK / retransmit */
    sleep_ms(TDMA_SLOT_MS);
}

/* ---- ESP32-C6 UART Bridge (WiFi/BLE uplink) ---- */

static void esp32_bridge_send(const char *msg)
{
    uart_puts(PIN_ES32_UART, msg);
    uart_putc_raw(PIN_ES32_UART, '\n');
}

static void esp32_bridge_send_camera_telemetry(void)
{
    if (!nodes[0].active)
        return;
    camera_data_payload_t *c = &nodes[0].camera_data;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "C:PRES:%d,PID:%d,PCONF:%d,PARC:%d,PARCONF:%d,RISK:%d,DIST:%d,ARMED:%d,TAMP:%d,KNOCK:%d,CLIP:%d,BAT:%d,WIFI:%d,PWR:%d",
             c->presence_state, c->person_id, c->person_conf,
             c->parcel_class, c->parcel_conf, c->pirate_risk,
             c->mmwave_dist_cm, c->armed, c->tamper_flag,
             c->knock_detected, c->clip_id, c->battery_pct,
             c->wifi_up, c->power_lost);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_mailbox_telemetry(void)
{
    if (!nodes[1].active)
        return;
    mailbox_data_payload_t *m = &nodes[1].mailbox_data;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "M:DOOR:%d,MAIL:%d,WT:%d,TEMP:%d,LUX:%d,TAMP:%d,BAT:%d,SOL:%d,RSSI:%d,EVT:%d,AGE:%d",
             m->door_state, m->mail_class, m->weight_mg,
             m->temp_c_x10, m->light_lux, m->tamper_flag,
             m->battery_pct, m->solar_mv, m->signal_rssi,
             m->last_event, m->event_age_s);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_lock_telemetry(void)
{
    if (!nodes[2].active)
        return;
    lock_data_payload_t *l = &nodes[2].lock_data;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "L:LOCK:%d,DOOR:%d,SRC:%d,CODE:%d,TAMP:%d,BAT:%d,AUTO:%d,OPEN:%d,GAR:%d,CODES:%d,KP:%d",
             l->lock_state, l->door_state, l->last_unlock_src,
             l->last_code_id, l->tamper_flag, l->battery_pct,
             l->auto_lock_enabled, l->door_open_s,
             l->garage_relay_on, l->codes_active, l->keypad_active);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_pirate_alert(const pirate_alert_payload_t *pa)
{
    char msg[256];
    snprintf(msg, sizeof(msg),
             "PIRATE_ALERT:LEVEL:%d,RISK:%d,PID:%d,PARC:%d,CLIP:%d,SIREN:%d,NODE:%d",
             pa->alert_level, pa->pirate_risk, pa->person_id,
             pa->parcel_class, pa->clip_id, pa->siren_requested,
             pa->source_node);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_tamper_alert(const tamper_alert_payload_t *ta)
{
    char msg[256];
    snprintf(msg, sizeof(msg),
             "TAMPER_ALERT:LEVEL:%d,TYPE:%d,NODE:%d,SEV:%d",
             ta->alert_level, ta->tamper_type, ta->source_node, ta->severity);
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
    /* In production: draw porch status, parcel count, last event, siren state */
    printf("[TFT] Dashboard: %d active nodes | armed=%d | siren=%d",
           num_active_nodes, armed, siren_active);
    if (nodes[0].active) {
        printf(" | Cam: pres=%d parcel=%d risk=%d/255",
               nodes[0].camera_data.presence_state,
               nodes[0].camera_data.parcel_class,
               nodes[0].camera_data.pirate_risk);
    }
    if (nodes[1].active) {
        printf(" | Mail: class=%d w=%dmg",
               nodes[1].mailbox_data.mail_class,
               nodes[1].mailbox_data.weight_mg);
    }
    if (nodes[2].active) {
        printf(" | Lock: %s door %s",
               nodes[2].lock_data.lock_state ? "UNLOCKED" : "LOCKED",
               nodes[2].lock_data.door_state ? "OPEN" : "closed");
    }
    if (siren_active) {
        printf(" | !!! SIREN ACTIVE !!!");
    }
    printf("\n");
}

/* ---- Pirate Risk ML Inference (stub — rule-based heuristic) ---- */

static uint8_t compute_pirate_risk(const camera_data_payload_t *data)
{
    /* In production: run TFLite Micro 1D-CNN+LSTM temporal model on the
     * rolling window of presence/parcel/person events.
     * Inputs: mmwave distance, parcel-present, person-present, person-id, motion
     * Output: pirate risk score 0-255 (0.0-1.0)
     *
     * Stub: rule-based heuristic using current telemetry only. */
    float score = 0.0f;

    /* Unknown stranger present on porch is the key precondition */
    if (data->person_id == PERSON_UNKNOWN)
        score += 0.3f;
    else if (data->person_id == PERSON_LOITERING)
        score += 0.5f;

    /* Parcel was present but now gone, with a stranger around — classic theft */
    if (data->parcel_class == PARCEL_NONE && data->person_id >= PERSON_UNKNOWN)
        score += 0.4f;

    /* mmWave shows close approach to porch (<50cm) */
    if (data->mmwave_dist_cm > 0 && data->mmwave_dist_cm < 50)
        score += 0.2f;

    /* Person not a resident and armed */
    if (data->armed && data->person_id != PERSON_RESIDENT &&
        data->person_id != PERSON_NONE)
        score += 0.1f;

    if (score > 1.0f) score = 1.0f;
    return (uint8_t)(score * 255.0f);
}

/* ---- Safety check ---- */

static void check_porch_safety(void)
{
    if (!nodes[0].active)
        return;

    /* Run ML pirate risk inference (hub runs temporal model; camera runs detection) */
    uint8_t ml_risk = compute_pirate_risk(&nodes[0].camera_data);
    nodes[0].pirate_risk = ml_risk;
    nodes[0].camera_data.pirate_risk = ml_risk;

    float risk = ml_risk / 255.0f;

    if (risk > 0.95f) {
        /* EMERGENCY: immediate siren + clip + cloud */
        printf("[SAFETY] EMERGENCY: Pirate risk %.2f\n", risk);
        if (!siren_active && armed) {
            pirate_alert_payload_t pa = {0};
            pa.alert_level = ALERT_EMERGENCY;
            pa.pirate_risk = ml_risk;
            pa.person_id = nodes[0].camera_data.person_id;
            pa.parcel_class = nodes[0].camera_data.parcel_class;
            pa.has_clip = nodes[0].camera_data.clip_ready;
            pa.clip_id = nodes[0].camera_data.clip_id;
            pa.siren_requested = 1;
            pa.source_node = NODE_ID_CAMERA;
            memcpy(&last_pirate_alert, &pa, sizeof(pa));
            esp32_bridge_send_pirate_alert(&pa);
            siren_start(20);
        }
    } else if (risk > 0.8f) {
        /* CRITICAL: push notification + clip + chime */
        printf("[SAFETY] CRITICAL: Pirate risk %.2f — possible theft\n", risk);
        char msg[128];
        snprintf(msg, sizeof(msg), "SAFETY_CRITICAL:RISK:%d", ml_risk);
        esp32_bridge_send(msg);
    } else if (risk > 0.6f) {
        /* WARNING: chime + app notification */
        printf("[SAFETY] WARNING: Pirate risk %.2f — suspicious behavior\n", risk);
        esp32_bridge_send("SAFETY_WARNING");
    }
}

/* ---- Main Loop (Core 0) ---- */

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    printf("=== PorchGuard Hub Node v1.0 ===\n");
    printf("RP2040 + ESP32-C6\n");

    /* Initialize hardware */
    uart_init(PIN_ES32_UART, 115200);
    gpio_set_function(PIN_ES32_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ES32_RX, GPIO_FUNC_UART);

    /* Siren PWM (piezo via MOSFET for 100 dB) */
    gpio_set_function(PIN_SIREN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_SIREN);
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
    nodes[0].node_id = NODE_ID_CAMERA;
    nodes[1].node_id = NODE_ID_MAILBOX;
    nodes[2].node_id = NODE_ID_LOCK;
    for (int i = 0; i < MAX_NODES; i++)
        nodes[i].active = false;

    printf("Hub initialized. Starting TDMA mesh coordinator (915MHz).\n");

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
                    printf("[MESH] Node %d timed out\n", nodes[i].node_id);
                    log_event(5, ALERT_WARNING, nodes[i].node_id);
                } else {
                    num_active_nodes++;
                }
            }
        }

        /* Safety checks every frame (porch piracy is highest priority) */
        check_porch_safety();

        /* Check if siren should auto-stop (handled in siren_start loop, but belt+braces) */
        if (siren_active) {
            int64_t alarm_age = absolute_time_diff_us(siren_time, get_absolute_time());
            /* Auto-stop after 60s max */
            if (alarm_age > 60 * 1000000) {
                siren_stop();
                printf("[SIREN] Auto-stopped after 60s\n");
            }
        }

        /* Update TFT every 4 frames (2 seconds) */
        if (frame_count % 4 == 0) {
            tft_draw_dashboard();
        }

        /* Forward telemetry to ESP32-C6 for WiFi/BLE bridge */
        esp32_bridge_send_camera_telemetry();
        if (frame_count % 4 == 0)  /* mailbox polled less often */
            esp32_bridge_send_mailbox_telemetry();
        if (frame_count % 2 == 0)
            esp32_bridge_send_lock_telemetry();

        /* Status LED: green=ok-armed, blue=disarmed, red=alarm */
        if (siren_active) {
            gpio_put(PIN_LED_R, 1);
            gpio_put(PIN_LED_G, 0);
            gpio_put(PIN_LED_B, 0);
        } else if (!armed) {
            gpio_put(PIN_LED_R, 0);
            gpio_put(PIN_LED_G, 0);
            gpio_put(PIN_LED_B, 1);
        } else if (nodes[0].active && nodes[0].pirate_risk > 150) {
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
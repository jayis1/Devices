/**
 * HearthKeep Hub Node Firmware
 * 
 * MCU: nRF5340 (application core) + ESP32-C6 (WiFi/BLE bridge)
 * Radio: SX1262 (868MHz Sub-GHz LoRa mesh coordinator)
 * Display: 3.2" IPS TFT (ILI9341)
 * Audio: MAX98357A (speaker) + SPH0645LM4H (microphone)
 * 
 * Responsibilities:
 * - Mesh network coordinator (TDMA scheduler)
 * - Data aggregation from all room monitors and bed mat
 * - Local fall detection verification and alert escalation
 * - WiFi uplink to MQTT broker (QoS 1, TLS)
 * - BLE GATT server for wearable tags and mobile app
 * - TFT dashboard rendering
 * - Two-way voice (speaker + mic)
 * - Emergency alarm + voice prompt
 * - OTA update distribution
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <stdio.h>
#include <string.h>

#include "../common/mesh_protocol.h"

LOG_MODULE_REGISTER(hk_hub, LOG_LEVEL_INF);

/* ========================================================================
 * HARDWARE DEFINITIONS
 * ======================================================================== */

/* LEDs */
#define LED_R_NODE  DT_ALIAS(led0)
#define LED_G_NODE  DT_ALIAS(led1)
#define LED_B_NODE  DT_ALIAS(led2)
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_R_NODE, gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(LED_G_NODE, gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);

/* Zone LEDs */
static const struct gpio_dt_spec zone1 = GPIO_DT_SPEC_GET(DT_ALIAS(zone1), gpios);
static const struct gpio_dt_spec zone2 = GPIO_DT_SPEC_GET(DT_ALIAS(zone2), gpios);
static const struct gpio_dt_spec zone3 = GPIO_DT_SPEC_GET(DT_ALIAS(zone3), gpios);
static const struct gpio_dt_spec zone4 = GPIO_DT_SPEC_GET(DT_ALIAS(zone4), gpios);

/* Emergency Button */
#define EMERGENCY_BTN_NODE DT_ALIAS(emergency_btn)
static const struct gpio_dt_spec emergency_btn = GPIO_DT_SPEC_GET(EMERGENCY_BTN_NODE, gpios);
static struct gpio_callback emergency_btn_cb;

/* SX1262 Radio SPI */
#define RADIO_SPI_DEV DT_NODELABEL(spi1)
#define RADIO_NSS    DT_ALIAS(radio_nss)
#define RADIO_BUSY   DT_ALIAS(radio_busy)
#define RADIO_IRQ    DT_ALIAS(radio_irq)
#define RADIO_NRST   DT_ALIAS(radio_nrst)

/* I2C Bus */
#define I2C0_DEV DT_NODELABEL(i2c0)
#define I2C1_DEV DT_NODELABEL(i2c1)

/* Audio */
#define I2S_DEV DT_NODELABEL(i2s0)
#define SPEAKER_AMP_DT DT_NODELABEL(max98357a)
#define MIC_DT DT_NODELABEL(sph0645)

/* Inter-MCU UART (nRF5340 <-> ESP32-C6) */
#define UART_ESP32 DT_NODELABEL(uart1)

/* SD Card SPI */
#define SD_SPI_DEV DT_NODELABEL(spi0)

/* TFT Display SPI */
#define TFT_SPI_DEV DT_NODELABEL(spi1)

/* ========================================================================
 * SYSTEM STATE
 * ======================================================================== */

typedef struct {
    uint8_t node_id;
    hk_node_type_t type;
    bool online;
    uint8_t battery_pct;
    int8_t rssi;
    uint32_t last_seen_ms;
    uint8_t tdma_slot;
    /* Room monitor specific */
    hk_radar_data_t radar;
    hk_env_data_t env;
    hk_position_class_t last_position;
    /* Bed mat specific */
    hk_bed_vitals_t vitals;
    /* Wearable tag specific */
    uint8_t tag_battery;
    bool panic_active;
} hk_node_state_t;

typedef struct {
    hk_node_state_t nodes[HK_MAX_NODES];
    uint8_t num_nodes;
    uint8_t hub_id;
    uint16_t frame_counter;
    uint32_t uptime_ms;
    bool wifi_connected;
    bool mqtt_connected;
    bool emergency_active;
    uint8_t emergency_room;
    uint32_t emergency_start_ms;
    /* Alert escalation state */
    hk_alert_level_t alert_level;
    uint32_t alert_start_ms;
    uint8_t alert_room;
    /* Audio state */
    bool voice_active;
    bool speaker_muted;
    /* Statistics */
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t alerts_fall;
    uint32_t alerts_panic;
} hk_hub_state_t;

static hk_hub_state_t g_state;

/* K-thread stacks */
#define STACK_SIZE 4096
K_THREAD_STACK_DEFINE(radio_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(mqtt_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(ble_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(display_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(alert_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(audio_stack, STACK_SIZE);

static struct k_thread radio_thread;
static struct k_thread mqtt_thread;
static struct k_thread ble_thread;
static struct k_thread display_thread;
static struct k_thread alert_thread;
static struct k_thread audio_thread;

/* Message queues */
K_MSGQ_DEFINE(radio_rx_queue, sizeof(hk_packet_t), 32, 4);
K_MSGQ_DEFINE(radio_tx_queue, sizeof(hk_packet_t), 32, 4);
K_MSGQ_DEFINE(alert_queue, sizeof(hk_alert_level_t), 8, 4);

/* Timers */
static struct k_timer tdma_timer;
static struct k_timer heartbeat_timer;
static struct k_timer watchdog_timer;

/* ========================================================================
 * SX1262 RADIO DRIVER (Simplified)
 * ======================================================================== */

typedef struct {
    const struct device *spi_dev;
    struct spi_config spi_cfg;
    const struct gpio_dt_spec nss;
    const struct gpio_dt_spec busy;
    const struct gpio_dt_spec irq;
    const struct gpio_dt_spec nrst;
    bool initialized;
    uint8_t rx_buffer[HK_MAX_PACKET_LEN + 10];
} hk_radio_t;

static hk_radio_t g_radio;

static int radio_init(hk_radio_t *radio)
{
    LOG_INF("Initializing SX1262 radio...");
    
    /* Configure GPIOs */
    gpio_pin_configure_dt(&radio->nss, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&radio->busy, GPIO_INPUT);
    gpio_pin_configure_dt(&radio->irq, GPIO_INPUT);
    gpio_pin_configure_dt(&radio->nrst, GPIO_OUTPUT_ACTIVE);
    
    /* Reset the radio */
    gpio_pin_set_dt(&radio->nrst, 0);
    k_msleep(10);
    gpio_pin_set_dt(&radio->nrst, 1);
    k_msleep(50);
    
    /* Configure SPI: CPOL=0, CPHA=0, MSB first, 8-bit, 10MHz */
    radio->spi_cfg.frequency = 10000000;
    radio->spi_cfg.operation = SPI_WORD_SET(8);
    radio->spi_cfg.cs = NULL;  /* Manual CS */
    
    radio->initialized = true;
    LOG_INF("SX1262 initialized successfully");
    return 0;
}

static int radio_transmit(hk_radio_t *radio, const uint8_t *data, uint16_t len)
{
    /* Set SX1262 to TX mode and transmit packet */
    gpio_pin_set_dt(&radio->nss, 0);  /* CS low */
    
    /* In a real implementation, we would:
     * 1. Set SX1262 to standby mode
     * 2. Write packet to FIFO
     * 3. Set TX parameters (power, frequency, SF)
     * 4. Set SX1262 to TX mode
     * 5. Wait for TX done interrupt
     */
    
    gpio_pin_set_dt(&radio->nss, 1);  /* CS high */
    
    g_state.packets_tx++;
    return 0;
}

static int radio_receive(hk_radio_t *radio, uint8_t *data, uint16_t *len, int32_t timeout_ms)
{
    /* Set SX1262 to RX mode and wait for packet */
    /* In a real implementation, we would:
     * 1. Set SX1262 to RX mode with timeout
     * 2. Wait for RX done interrupt or timeout
     * 3. Read received packet from FIFO
     */
    *len = 0;
    return -1;  /* No packet received (placeholder) */
}

/* ========================================================================
 * TDMA MESH COORDINATOR
 * ======================================================================== */

static void tdma_timer_handler(struct k_timer *timer)
{
    /* Called every 50ms — advance to next TDMA slot */
    g_state.frame_counter++;
    
    uint8_t current_slot = g_state.frame_counter % HK_SLOTS_PER_FRAME;
    
    if (current_slot == HK_SLOT_HUB) {
        /* Hub's turn: broadcast sync + commands */
        hk_packet_t pkt;
        hk_packet_init(&pkt, g_state.hub_id, HK_NODE_ID_BROADCAST, HK_TYPE_HEARTBEAT);
        
        hk_heartbeat_t hb;
        hb.node_type = HK_NODE_HUB;
        hb.battery_pct = 100;  /* Hub always on mains */
        hb.signal_rssi = 0;
        hb.uptime_min = g_state.uptime_ms / 60000;
        hb.fault_flags = 0;
        hb.firmware_version = 1;
        
        memcpy(pkt.payload, &hb, sizeof(hb));
        pkt.length = HK_HEADER_LEN + sizeof(hb) + HK_CRC_LEN;
        pkt.seq_num = g_state.frame_counter;
        
        /* Transmit hub sync packet */
        uint8_t tx_buf[HK_MAX_PACKET_LEN];
        uint16_t tx_len = hk_packet_serialize(&pkt, tx_buf, sizeof(tx_buf));
        if (tx_len > 0) {
            radio_transmit(&g_radio, tx_buf, tx_len);
        }
    }
}

static void tdma_start(void)
{
    LOG_INF("Starting TDMA coordinator (18 slots, 50ms each)");
    k_timer_start(&tdma_timer, K_MSEC(HK_SLOT_DURATION_MS), 
                  K_MSEC(HK_SLOT_DURATION_MS));
}

/* ========================================================================
 * EMERGENCY BUTTON HANDLER
 * ======================================================================== */

static void emergency_btn_handler(const struct device *dev, 
                                   struct gpio_callback *cb, 
                                   uint32_t pins)
{
    LOG_WRN("EMERGENCY BUTTON PRESSED");
    
    g_state.emergency_active = true;
    g_state.emergency_start_ms = k_uptime_get_32();
    
    /* Send panic alert to all nodes and cloud */
    hk_packet_t pkt;
    hk_packet_init(&pkt, g_state.hub_id, HK_NODE_ID_BROADCAST, HK_TYPE_PANIC_ALERT);
    
    hk_panic_alert_t panic;
    panic.tag_id = g_state.hub_id;
    panic.battery_pct = 100;
    panic.timestamp = k_uptime_get_32() / 1000;
    panic.button_hold_time = 0;
    
    memcpy(pkt.payload, &panic, sizeof(panic));
    pkt.length = HK_HEADER_LEN + sizeof(panic) + HK_CRC_LEN;
    
    /* Queue for immediate transmission */
    k_msgq_put(&radio_tx_queue, &pkt, K_NO_WAIT);
    
    /* Trigger alert escalation */
    hk_alert_level_t level = HK_ALERT_EMERGENCY;
    k_msgq_put(&alert_queue, &level, K_NO_WAIT);
}

/* ========================================================================
 * NODE MANAGEMENT
 * ======================================================================== */

static hk_node_state_t* find_node(uint8_t node_id)
{
    for (int i = 0; i < g_state.num_nodes; i++) {
        if (g_state.nodes[i].node_id == node_id) {
            return &g_state.nodes[i];
        }
    }
    return NULL;
}

static hk_node_state_t* add_node(uint8_t node_id, hk_node_type_t type)
{
    if (g_state.num_nodes >= HK_MAX_NODES) return NULL;
    
    hk_node_state_t *node = &g_state.nodes[g_state.num_nodes++];
    memset(node, 0, sizeof(hk_node_state_t));
    node->node_id = node_id;
    node->type = type;
    node->online = true;
    node->last_seen_ms = k_uptime_get_32();
    node->tdma_slot = hk_slot_for_node(node_id);
    
    LOG_INF("Node added: ID=0x%02X type=%s slot=%d", 
            node_id, hk_type_str((hk_packet_type_t)type), node->tdma_slot);
    return node;
}

/* ========================================================================
 * PROCESS RECEIVED PACKETS
 * ======================================================================== */

static void process_radar_data(uint8_t src_id, const hk_radar_data_t *radar)
{
    hk_node_state_t *node = find_node(src_id);
    if (!node) return;
    
    node->radar = *radar;
    node->last_seen_ms = k_uptime_get_32();
    
    LOG_INF("Room 0x%02X: presence=%d pos=%s fall_prob=%.2f move=%.2f",
            src_id, radar->presence_count,
            hk_position_str(radar->position_class),
            radar->fall_probability, radar->movement_index);
    
    /* Check for fall */
    if (radar->fall_probability > HK_FALL_PROBABILITY_THRESHOLD &&
        (radar->position_class == HK_POS_FALLING || 
         radar->position_class == HK_POS_FALLEN)) {
        
        LOG_WRN("FALL DETECTED in room 0x%02X! Probability: %.2f",
                src_id, radar->fall_probability);
        
        /* Start alert escalation */
        g_state.emergency_active = true;
        g_state.emergency_room = src_id;
        g_state.emergency_start_ms = k_uptime_get_32();
        g_state.alert_level = HK_ALERT_URGENT;
        g_state.alert_room = src_id;
        g_state.alerts_fall++;
        
        hk_alert_level_t level = HK_ALERT_URGENT;
        k_msgq_put(&alert_queue, &level, K_NO_WAIT);
    }
}

static void process_bed_vitals(uint8_t src_id, const hk_bed_vitals_t *vitals)
{
    hk_node_state_t *node = find_node(src_id);
    if (!node) return;
    
    node->vitals = *vitals;
    node->last_seen_ms = k_uptime_get_32();
    
    LOG_INF("Bed vitals: HR=%.1f BR=%.1f in_bed=%d phase=%d",
            vitals->heart_rate_bpm, vitals->breathing_rate,
            vitals->in_bed, vitals->sleep_phase);
    
    /* Check for abnormal vitals */
    if (vitals->in_bed) {
        if (vitals->heart_rate_bpm > 0 && 
            (vitals->heart_rate_bpm < HK_HEART_RATE_LOW || 
             vitals->heart_rate_bpm > HK_HEART_RATE_HIGH)) {
            LOG_WRN("Abnormal heart rate: %.1f BPM", vitals->heart_rate_bpm);
        }
        if (vitals->breathing_rate > 0 &&
            (vitals->breathing_rate < HK_BREATHING_RATE_LOW || 
             vitals->breathing_rate > HK_BREATHING_RATE_HIGH)) {
            LOG_WRN("Abnormal breathing rate: %.1f rpm", vitals->breathing_rate);
        }
    }
}

static void process_packet(const hk_packet_t *pkt)
{
    g_state.packets_rx++;
    
    /* Find or add node */
    hk_node_state_t *node = find_node(pkt->src_id);
    if (!node && pkt->src_id != HK_NODE_ID_HUB) {
        /* Auto-register new node */
        node = add_node(pkt->src_id, (hk_node_type_t)pkt->type);
    }
    if (node) {
        node->online = true;
        node->last_seen_ms = k_uptime_get_32();
    }
    
    /* Dispatch by type */
    switch ((hk_packet_type_t)pkt->type) {
        case HK_TYPE_RADAR_DATA: {
            hk_radar_data_t radar;
            memcpy(&radar, pkt->payload, sizeof(radar));
            process_radar_data(pkt->src_id, &radar);
            break;
        }
        case HK_TYPE_ENV_DATA: {
            hk_env_data_t env;
            memcpy(&env, pkt->payload, sizeof(env));
            LOG_INF("Room 0x%02X: %.1f°C %.0f%%RH %.0flux",
                    pkt->src_id, env.temperature_c, env.humidity_pct, env.light_lux);
            if (node) node->env = env;
            break;
        }
        case HK_TYPE_BED_VITALS: {
            hk_bed_vitals_t vitals;
            memcpy(&vitals, pkt->payload, sizeof(vitals));
            process_bed_vitals(pkt->src_id, &vitals);
            break;
        }
        case HK_TYPE_FALL_ALERT: {
            hk_fall_alert_t fall;
            memcpy(&fall, pkt->payload, sizeof(fall));
            LOG_WRN("FALL ALERT from room 0x%02X! Prob: %.2f Impact: %.2f m/s",
                    fall.room_id, fall.fall_probability, fall.impact_velocity);
            g_state.emergency_active = true;
            g_state.emergency_room = fall.room_id;
            g_state.alert_level = HK_ALERT_EMERGENCY;
            g_state.alert_room = fall.room_id;
            g_state.alerts_fall++;
            hk_alert_level_t level = HK_ALERT_EMERGENCY;
            k_msgq_put(&alert_queue, &level, K_NO_WAIT);
            break;
        }
        case HK_TYPE_PANIC_ALERT: {
            hk_panic_alert_t panic;
            memcpy(&panic, pkt->payload, sizeof(panic));
            LOG_WRN("PANIC ALERT from tag 0x%02X!", panic.tag_id);
            g_state.alerts_panic++;
            hk_alert_level_t level = HK_ALERT_EMERGENCY;
            k_msgq_put(&alert_queue, &level, K_NO_WAIT);
            break;
        }
        case HK_TYPE_HEARTBEAT: {
            hk_heartbeat_t hb;
            memcpy(&hb, pkt->payload, sizeof(hb));
            LOG_INF("Heartbeat from 0x%02X: bat=%d%% rssi=%d uptime=%dm",
                    pkt->src_id, hb.battery_pct, hb.signal_rssi, hb.uptime_min);
            if (node) {
                node->battery_pct = hb.battery_pct;
                node->rssi = hb.signal_rssi;
                
                /* Check low battery */
                if (hb.battery_pct <= HK_BATTERY_LOW_PCT) {
                    LOG_WRN("Low battery on node 0x%02X: %d%%",
                           pkt->src_id, hb.battery_pct);
                }
            }
            break;
        }
        default:
            LOG_INF("Unhandled packet type: %s", hk_type_str(pkt->type));
            break;
    }
}

/* ========================================================================
 * RADIO THREAD — Receive and process Sub-GHz packets
 * ======================================================================== */

static void radio_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Radio thread started");
    
    hk_packet_t rx_pkt;
    uint8_t rx_buf[HK_MAX_PACKET_LEN + 10];
    uint16_t rx_len;
    
    while (1) {
        /* Receive packet from radio */
        int ret = radio_receive(&g_radio, rx_buf, &rx_len, 100);
        
        if (ret == 0 && rx_len > 0) {
            /* Deserialize and validate */
            if (hk_packet_deserialize(rx_buf, rx_len, &rx_pkt)) {
                process_packet(&rx_pkt);
            }
        }
        
        /* Check TX queue */
        hk_packet_t tx_pkt;
        if (k_msgq_get(&radio_tx_queue, &tx_pkt, K_NO_WAIT) == 0) {
            uint8_t tx_buf[HK_MAX_PACKET_LEN];
            uint16_t tx_len = hk_packet_serialize(&tx_pkt, tx_buf, sizeof(tx_buf));
            if (tx_len > 0) {
                radio_transmit(&g_radio, tx_buf, tx_len);
            }
        }
        
        /* Small delay to prevent busy loop */
        k_msleep(1);
    }
}

/* ========================================================================
 * ALERT ESCALATION THREAD
 * ======================================================================== */

static void alert_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Alert thread started");
    
    while (1) {
        hk_alert_level_t level;
        
        /* Wait for alert */
        if (k_msgq_get(&alert_queue, &level, K_FOREVER) != 0) {
            continue;
        }
        
        LOG_WRN("Alert escalation: level=%s", hk_alert_str(level));
        
        /* Tier 1: Local verification (0-30 seconds) */
        if (level >= HK_ALERT_URGENT) {
            /* Play gentle voice prompt */
            LOG_INF("Playing voice prompt: 'It looks like you may have fallen. Are you okay?'");
            g_state.speaker_muted = false;
            
            /* Light LED pattern: slow pulse red */
            for (int i = 0; i < 30; i++) {
                gpio_pin_set_dt(&led_r, 1);
                k_msleep(500);
                gpio_pin_set_dt(&led_r, 0);
                k_msleep(500);
                
                /* Check if cancelled (via wearable tag "I'm OK" or hub button) */
                if (!g_state.emergency_active) {
                    LOG_INF("Alert cancelled by user");
                    goto alert_done;
                }
            }
        }
        
        /* Tier 2: Caregiver notification (30-120 seconds) */
        if (level >= HK_ALERT_URGENT) {
            LOG_WRN("Escalating to caregiver notification");
            
            /* Send push notification via MQTT */
            /* In a real implementation, this would publish to the MQTT broker */
            LOG_INF("MQTT: Publishing fall alert to caregiver app");
            
            /* Wait for caregiver response */
            for (int i = 0; i < 90; i++) {
                k_msleep(1000);
                if (!g_state.emergency_active) {
                    LOG_INF("Alert cancelled by caregiver");
                    goto alert_done;
                }
            }
        }
        
        /* Tier 3: Emergency escalation (2+ minutes) */
        if (level >= HK_ALERT_EMERGENCY) {
            LOG_WRN("ESCALATING TO EMERGENCY SERVICES");
            
            /* Auto-dial caregivers */
            LOG_INF("Auto-dialing primary caregiver");
            k_msleep(30000);
            
            if (g_state.emergency_active) {
                LOG_INF("Auto-dialing secondary caregiver");
                k_msleep(30000);
            }
            
            if (g_state.emergency_active) {
                LOG_WRN("CALLING EMERGENCY SERVICES (911)");
                /* In real implementation, this would trigger a call through VoIP gateway */
            }
        }
        
alert_done:
        /* Reset alert state */
        g_state.emergency_active = false;
        g_state.alert_level = HK_ALERT_NONE;
        
        /* Turn off alarm LEDs */
        gpio_pin_set_dt(&led_r, 0);
    }
}

/* ========================================================================
 * DISPLAY THREAD — TFT dashboard rendering
 * ======================================================================== */

static void display_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("Display thread started");
    
    while (1) {
        /* Render dashboard every 1 second */
        
        /* In a real implementation, this would:
         * 1. Clear TFT display
         * 2. Draw room status cards (occupied/empty/bed)
         * 3. Draw vital signs (heart rate, breathing)
         * 4. Draw alert status
         * 5. Draw connection status (WiFi, mesh nodes)
         * 6. Update TFT via SPI
         */
        
        /* Update status LED */
        if (g_state.emergency_active) {
            /* Red flashing */
            gpio_pin_set_dt(&led_r, (k_uptime_get_32() / 250) % 2);
            gpio_pin_set_dt(&led_g, 0);
            gpio_pin_set_dt(&led_b, 0);
        } else if (g_state.mqtt_connected) {
            /* Solid green */
            gpio_pin_set_dt(&led_r, 0);
            gpio_pin_set_dt(&led_g, 1);
            gpio_pin_set_dt(&led_b, 0);
        } else {
            /* Yellow (WiFi not connected) */
            gpio_pin_set_dt(&led_r, 1);
            gpio_pin_set_dt(&led_g, 1);
            gpio_pin_set_dt(&led_b, 0);
        }
        
        /* Update zone LEDs based on room occupancy */
        for (int i = 0; i < g_state.num_nodes && i < 4; i++) {
            const struct gpio_dt_spec *zone_leds[] = {&zone1, &zone2, &zone3, &zone4};
            if (g_state.nodes[i].online) {
                gpio_pin_set_dt(zone_leds[i], 
                    g_state.nodes[i].radar.presence_count > 0);
            }
        }
        
        k_msleep(1000);
    }
}

/* ========================================================================
 * MQTT THREAD — WiFi uplink to cloud
 * ======================================================================== */

static void mqtt_thread_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("MQTT thread started");
    
    /* In a real implementation, this would:
     * 1. Connect to WiFi via ESP32-C6 UART bridge
     * 2. Connect to MQTT broker (TLS, QoS 1)
     * 3. Publish sensor data to hearethkeep/sensors/<node_id>/
     * 4. Subscribe to hearethkeep/commands/<hub_id>/
     * 5. Handle OTA update commands
     * 6. Publish alerts to hearethkeep/alerts/<hub_id>/
     * 7. Reconnect on failure
     */
    
    while (1) {
        /* Publish aggregated data every 30 seconds */
        k_msleep(30000);
        
        if (!g_state.wifi_connected) {
            LOG_WRN("WiFi not connected, skipping MQTT publish");
            continue;
        }
        
        /* Publish all node data */
        for (int i = 0; i < g_state.num_nodes; i++) {
            hk_node_state_t *node = &g_state.nodes[i];
            
            char topic[64];
            char payload[256];
            
            switch (node->type) {
                case HK_NODE_ROOM_MONITOR:
                    snprintf(topic, sizeof(topic), 
                            "hearethkeep/sensors/radar/%02X", node->node_id);
                    snprintf(payload, sizeof(payload),
                            "{\"room\":\"0x%02X\",\"presence\":%d,\"position\":\"%s\","
                            "\"fall_prob\":%.3f,\"movement\":%.3f,\"distance\":%.2f,"
                            "\"temp\":%.1f,\"humidity\":%.1f,\"iaq\":%.1f,\"lux\":%.0f}",
                            node->node_id, node->radar.presence_count,
                            hk_position_str(node->radar.position_class),
                            node->radar.fall_probability, node->radar.movement_index,
                            node->radar.distance_m,
                            node->env.temperature_c, node->env.humidity_pct,
                            node->env.iaq_index, node->env.light_lux);
                    /* mqtt_publish(topic, payload, QOS_1); */
                    break;
                    
                case HK_NODE_BED_MAT:
                    snprintf(topic, sizeof(topic),
                            "hearethkeep/sensors/vitals/%02X", node->node_id);
                    snprintf(payload, sizeof(payload),
                            "{\"hr\":%.1f,\"br\":%.1f,\"movement\":%.3f,"
                            "\"in_bed\":%d,\"sleep_phase\":%d,"
                            "\"hr_conf\":%.2f,\"br_conf\":%.2f,\"temp\":%.1f}",
                            node->vitals.heart_rate_bpm, node->vitals.breathing_rate,
                            node->vitals.movement_index,
                            node->vitals.in_bed, node->vitals.sleep_phase,
                            node->vitals.hr_confidence, node->vitals.br_confidence,
                            node->vitals.mattress_temp_c);
                    /* mqtt_publish(topic, payload, QOS_1); */
                    break;
                    
                default:
                    break;
            }
        }
    }
}

/* ========================================================================
 * BLE THREAD — Wearable tag and mobile app connectivity
 * ======================================================================== */

static void mqtt_thread_fn_placeholder(void *p1, void *p2, void *p3)
{
    /* Placeholder - same as mqtt_thread_fn, renamed to avoid conflict */
}

/* ========================================================================
 * HEARTBEAT TIMER — Send periodic heartbeats
 * ======================================================================== */

static void heartbeat_timer_handler(struct k_timer *timer)
{
    /* Send heartbeat every 60 seconds */
    hk_packet_t pkt;
    hk_packet_init(&pkt, g_state.hub_id, HK_NODE_ID_BROADCAST, HK_TYPE_HEARTBEAT);
    
    hk_heartbeat_t hb;
    hb.node_type = HK_NODE_HUB;
    hb.battery_pct = 100;
    hb.signal_rssi = 0;
    hb.uptime_min = k_uptime_get_32() / 60000;
    hb.fault_flags = 0;
    hb.firmware_version = 1;
    
    memcpy(pkt.payload, &hb, sizeof(hb));
    pkt.length = HK_HEADER_LEN + sizeof(hb) + HK_CRC_LEN;
    
    k_msgq_put(&radio_tx_queue, &pkt, K_NO_WAIT);
}

/* ========================================================================
 * MAIN ENTRY POINT
 * ======================================================================== */

int main(void)
{
    LOG_INF("=== HearthKeep Hub Node Starting ===");
    
    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.hub_id = HK_NODE_ID_HUB;
    
    /* Initialize GPIOs */
    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&zone1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&zone2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&zone3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&zone4, GPIO_OUTPUT_INACTIVE);
    
    /* Configure emergency button */
    gpio_pin_configure_dt(&emergency_btn, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&emergency_btn, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&emergency_btn_cb, emergency_btn_handler, BIT(emergency_btn.pin));
    gpio_add_callback(emergency_btn.port, &emergency_btn_cb);
    
    /* Initialize radio */
    if (radio_init(&g_radio) != 0) {
        LOG_ERR("Failed to initialize radio!");
        /* Flash red LED rapidly to indicate error */
        while (1) {
            gpio_pin_set_dt(&led_r, 1);
            k_msleep(100);
            gpio_pin_set_dt(&led_r, 0);
            k_msleep(100);
        }
    }
    
    /* Start TDMA coordinator */
    tdma_start();
    
    /* Start heartbeat timer (every 60 seconds) */
    k_timer_start(&heartbeat_timer, K_SECONDS(60), K_SECONDS(60));
    
    /* Start worker threads */
    k_thread_create(&radio_thread, radio_stack, STACK_SIZE,
                    radio_thread_fn, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    
    k_thread_create(&mqtt_thread, mqtt_stack, STACK_SIZE,
                    mqtt_thread_fn, NULL, NULL, NULL,
                    3, 0, K_NO_WAIT);
    
    k_thread_create(&display_thread, display_stack, STACK_SIZE,
                    display_thread_fn, NULL, NULL, NULL,
                    2, 0, K_NO_WAIT);
    
    k_thread_create(&alert_thread, alert_stack, STACK_SIZE,
                    alert_thread_fn, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);  /* Highest priority */
    
    LOG_INF("HearthKeep Hub Node running. Hub ID: 0x%02X", g_state.hub_id);
    
    /* Main loop - monitor system health */
    while (1) {
        g_state.uptime_ms = k_uptime_get_32();
        
        /* Check for offline nodes */
        for (int i = 0; i < g_state.num_nodes; i++) {
            uint32_t age = g_state.uptime_ms - g_state.nodes[i].last_seen_ms;
            if (age > 120000) {  /* 2 minutes without heartbeat */
                if (g_state.nodes[i].online) {
                    LOG_WRN("Node 0x%02X went offline", g_state.nodes[i].node_id);
                    g_state.nodes[i].online = false;
                }
            }
        }
        
        k_msleep(5000);
    }
    
    return 0;
}
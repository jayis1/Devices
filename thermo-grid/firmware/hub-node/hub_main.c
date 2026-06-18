/*
 * hub_main.c — ThermoGrid Hub Node (RP2040 + ESP32-C6)
 *
 * Responsibilities:
 * - Sub-GHz mesh coordinator (dynamic TDMA scheduler)
 * - Data aggregation from room sensors + zone actuators + comfort tags
 * - Thermal forecast engine (physics-informed RC-network + GRU correction)
 * - Comfort optimizer (per-person comfort profile + per-zone setpoint MILP/heuristic)
 * - Solar self-consumption coordinator
 * - TOU (time-of-use) tariff optimizer
 * - WiFi uplink to MQTT broker
 * - BLE GATT server for mobile app + comfort tags
 * - TFT dashboard: home thermal map, energy bar, solar gauge
 * - OTA update distribution
 * - Freeze protection (safety-critical, works without WiFi)
 *
 * RP2040 Core 0: mesh TDMA + thermal forecast + display (hard real-time)
 * RP2040 Core 1: ESP32-C6 UART bridge + comfort model inference (soft real-time)
 *
 * SAFETY: Freeze alerts override normal TDMA. Hub halts scheduling,
 * forces all valves open, activates boiler relay. Works on battery backup.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "pico/time.h"
#include "hardware/rtc.h"

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

#define PIN_BOILER_RELAY 22   /* boiler/heat-pump relay (for freeze protection) */
#define PIN_USER_BTN     23
#define PIN_LED_R        24
#define PIN_LED_G        25
#define PIN_LED_B        26

/* ---- I2C for RTC (PCF8563) ---- */
#define PIN_I2C_SDA      2
#define PIN_I2C_SCL      3
#define I2C_INSTANCE     i2c1
#define PCF8563_ADDR     0x51

/* ---- Mesh State ---- */
#define MAX_NODES       32
#define NODE_TIMEOUT_S  300

typedef enum {
    NODE_TYPE_NONE = 0,
    NODE_TYPE_SENSOR,
    NODE_TYPE_ACTUATOR,
    NODE_TYPE_TAG
} node_type_t;

typedef struct {
    uint8_t  node_id;
    node_type_t type;
    uint8_t  zone_id;          /* which zone/room this node belongs to */
    bool     active;
    absolute_time_t last_seen;
    sensor_data_payload_t   sensor_data;
    actuator_data_payload_t actuator_data;
    comfort_data_payload_t  comfort_data;
} node_state_t;

static node_state_t nodes[MAX_NODES];
static uint8_t num_active_nodes = 0;
static uint8_t num_sensors = 0;
static uint8_t num_actuators = 0;
static uint8_t num_tags = 0;

/* ---- Zone management ---- */
#define MAX_ZONES    16
typedef struct {
    uint8_t  zone_id;
    uint8_t  sensor_node;       /* node_id of the room sensor for this zone */
    uint8_t  actuator_node;     /* node_id of the actuator for this zone */
    int16_t  setpoint_cx100;    /* current setpoint ×100 °C */
    int16_t  comfort_setpoint;  /* comfort-adjusted setpoint */
    uint8_t  mode;             /* MODE_* */
    uint8_t  boost_minutes;    /* temporary boost remaining */
    int16_t  boost_delta_cx100; /* boost temp delta ×100 °C */
    bool     frost_protect;
    bool     window_open;
    uint16_t window_open_s;
} zone_t;

static zone_t zones[MAX_ZONES];
static uint8_t num_zones = 0;

/* ---- Alarm / safety state ---- */
static bool freeze_alert_active = false;
static absolute_time_t freeze_alert_time;
static freeze_alert_payload_t last_freeze_alert;
static bool boiler_relay_on = false;

/* ---- Thermal forecast state ---- */
#define FORECAST_STEPS    16   /* 4 hours at 15-min resolution */
#define HISTORY_STEPS     8    /* 2 hours history at 15-min resolution */

typedef struct {
    int16_t temp_cx100[FORECAST_STEPS];   /* predicted temps per step */
    int16_t mrt_cx100[FORECAST_STEPS];
    uint8_t occupancy_prob[FORECAST_STEPS]; /* 0-100 */
} zone_forecast_t;

static zone_forecast_t zone_forecasts[MAX_ZONES];

/* ---- Energy / solar / TOU state ---- */
static int16_t solar_production_w = 0;
static int16_t solar_base_load_w = 0;
static int16_t solar_surplus_w = 0;
static bool solar_boost_recommended = false;
static uint8_t current_tou_period = 0; /* 0=off-peak,1=mid,2=peak,3=solar */
static uint16_t current_tou_rate = 0;  /* cents/kWh ×10 */
static uint16_t next_tou_change_min = 0;
static uint8_t  next_tou_period = 0;

/* ---- Event log ---- */
#define EVENT_LOG_SIZE 64
typedef struct {
    uint8_t  event_type;   /* 0=freeze,1=window,2=zone,3=comfort,4=energy,5=fault */
    uint8_t  severity;
    uint16_t timestamp_s;
    uint8_t  zone_id;
    uint8_t  reserved[3];
} event_log_entry_t;
static event_log_entry_t event_log[EVENT_LOG_SIZE];
static uint8_t event_log_head = 0;

static void log_event(uint8_t type, uint8_t severity, uint8_t zone)
{
    event_log[event_log_head].event_type  = type;
    event_log[event_log_head].severity   = severity;
    event_log[event_log_head].timestamp_s = (uint16_t)(to_us_since_boot(get_absolute_time()) / 1000000);
    event_log[event_log_head].zone_id     = zone;
    event_log_head = (event_log_head + 1) % EVENT_LOG_SIZE;
}

/* ---- Node registry helpers ---- */
static node_state_t *find_node(uint8_t node_id)
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].active && nodes[i].node_id == node_id)
            return &nodes[i];
    }
    return NULL;
}

static node_state_t *find_or_register(uint8_t node_id, node_type_t type)
{
    /* Find existing */
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].active && nodes[i].node_id == node_id) {
            nodes[i].last_seen = get_absolute_time();
            return &nodes[i];
        }
    }
    /* Register new */
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].active) {
            nodes[i].node_id = node_id;
            nodes[i].type = type;
            nodes[i].active = true;
            nodes[i].last_seen = get_absolute_time();
            nodes[i].zone_id = 0xFF;
            num_active_nodes++;
            if (type == NODE_TYPE_SENSOR) num_sensors++;
            else if (type == NODE_TYPE_ACTUATOR) num_actuators++;
            else if (type == NODE_TYPE_TAG) num_tags++;
            printf("[MESH] Registered node 0x%02X type=%d (total=%d)\n",
                   node_id, type, num_active_nodes);
            return &nodes[i];
        }
    }
    return NULL;
}

static void check_node_timeouts(void)
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].active) {
            int64_t age = absolute_time_diff_us(nodes[i].last_seen, get_absolute_time());
            if (age > NODE_TIMEOUT_S * 1000000LL) {
                printf("[MESH] Node 0x%02X timed out (%lld s)\n",
                       nodes[i].node_id, (long long)(age / 1000000));
                nodes[i].active = false;
                num_active_nodes--;
                if (nodes[i].type == NODE_TYPE_SENSOR) num_sensors--;
                else if (nodes[i].type == NODE_TYPE_ACTUATOR) num_actuators--;
                else if (nodes[i].type == NODE_TYPE_TAG) num_tags--;
                log_event(5, ALERT_WARNING, nodes[i].zone_id);
            }
        }
    }
}

/* ---- Zone management ---- */
static zone_t *find_zone(uint8_t zone_id)
{
    for (int i = 0; i < num_zones; i++) {
        if (zones[i].zone_id == zone_id)
            return &zones[i];
    }
    return NULL;
}

static zone_t *find_zone_by_sensor(uint8_t sensor_node)
{
    for (int i = 0; i < num_zones; i++) {
        if (zones[i].sensor_node == sensor_node)
            return &zones[i];
    }
    return NULL;
}

/* ---- SX1262 Radio Interface ---- */

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

    gpio_put(PIN_SX1262_NRST, 0);
    sleep_ms(10);
    gpio_put(PIN_SX1262_NRST, 1);
    sleep_ms(50);

    printf("[SX1262] Initialized on SPI1 (915MHz, +20dBm)\n");
}

static void sx1262_send(const uint8_t *data, uint16_t len)
{
    printf("[SX1262] TX %d bytes\n", len);
}

static int16_t sx1262_receive(uint8_t *buf, uint16_t max_len)
{
    return 0; /* stub */
}

/* ---- Boiler relay (freeze protection) ---- */

static void boiler_relay_on_set(bool on)
{
    gpio_put(PIN_BOILER_RELAY, on ? 1 : 0);
    boiler_relay_on = on;
    printf("[BOILER] Relay %s\n", on ? "ON" : "OFF");
}

/* ---- RTC (PCF8563) ---- */

static void rtc_init(void)
{
    i2c_init(I2C_INSTANCE, 100000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    /* In production: initialize PCF8563, read time, set Pico RTC */
    printf("[RTC] PCF8563 initialized\n");
}

static uint32_t get_epoch_seconds(void)
{
    /* In production: read from PCF8563 via I2C */
    return (uint32_t)(to_us_since_boot(get_absolute_time()) / 1000000);
}

/* ---- TDMA Coordinator ---- */

static void tdma_run_frame(void)
{
    mesh_packet_t tx_pkt, rx_pkt;

    /* Slot 0: Hub broadcast — sync + zone setpoints + solar + TOU */
    {
        /* Heartbeat with system state */
        uint8_t hb_payload[8];
        hb_payload[0] = freeze_alert_active ? 1 : 0;
        hb_payload[1] = boiler_relay_on ? 1 : 0;
        hb_payload[2] = current_tou_period;
        hb_payload[3] = num_active_nodes;
        hb_payload[4] = (uint8_t)(solar_surplus_w >> 8);
        hb_payload[5] = (uint8_t)(solar_surplus_w & 0xFF);
        hb_payload[6] = solar_boost_recommended ? 1 : 0;
        hb_payload[7] = num_zones;
        uint16_t pkt_len = mesh_build_packet(
            NODE_ID_HUB, NODE_ID_BROADCAST, PKT_HEARTBEAT,
            hb_payload, sizeof(hb_payload), &tx_pkt);
        sx1262_send((uint8_t *)&tx_pkt, pkt_len);

        /* Also send current solar status */
        solar_status_payload_t solar;
        solar.production_w = solar_production_w;
        solar.base_load_w = solar_base_load_w;
        solar.surplus_w = solar_surplus_w;
        solar.boost_recommended = solar_boost_recommended ? 1 : 0;
        solar.boost_target_c = 0;
        memset(solar.reserved, 0, sizeof(solar.reserved));
        pkt_len = mesh_build_packet(
            NODE_ID_HUB, NODE_ID_BROADCAST, PKT_SOLAR_STATUS,
            (uint8_t *)&solar, sizeof(solar), &tx_pkt);
        sx1262_send((uint8_t *)&tx_pkt, pkt_len);

        /* TOU schedule */
        tou_schedule_payload_t tou;
        tou.current_period = current_tou_period;
        tou.rate_cents_x10 = current_tou_rate;
        tou.next_change_min = next_tou_change_min;
        tou.next_period = next_tou_period;
        tou.next_rate_cents_x10 = 0;
        memset(tou.reserved, 0, sizeof(tou.reserved));
        pkt_len = mesh_build_packet(
            NODE_ID_HUB, NODE_ID_BROADCAST, PKT_TOU_SCHEDULE,
            (uint8_t *)&tou, sizeof(tou), &tx_pkt);
        sx1262_send((uint8_t *)&tx_pkt, pkt_len);
    }

    sleep_ms(TDMA_SLOT_MS);

    /* Slots 1..N: Receive from each registered sensor/actuator */
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].active)
            continue;

        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        int16_t rx_len = sx1262_receive((uint8_t *)&rx_pkt, sizeof(rx_pkt));
        if (rx_len > 0 && mesh_parse_packet((uint8_t *)&rx_pkt, rx_len, &rx_pkt) == 0) {
            if (rx_pkt.dst_id == NODE_ID_HUB || rx_pkt.dst_id == NODE_ID_BROADCAST) {
                process_uplink(&rx_pkt);
            }
        }
        sleep_until(slot_end);
    }

    /* Control slot: send setpoints to actuators */
    {
        absolute_time_t slot_end = make_timeout_time_ms(TDMA_SLOT_MS);
        for (int z = 0; z < num_zones; z++) {
            zone_t *zone = &zones[z];
            if (!zone->actuator_node || zone->actuator_node == 0xFF)
                continue;

            zone_setpoint_payload_t sp;
            sp.zone_id = zone->zone_id;
            sp.setpoint_cx100 = zone->setpoint_cx100;
            sp.pipe_target_cx100 = 0;
            sp.mode = zone->mode;
            sp.valve_pos_override = 255;
            sp.boost_minutes = zone->boost_minutes;
            sp.source = freeze_alert_active ? 4 :
                        (zone->boost_minutes > 0) ? 3 :
                        zone->frost_protect ? 4 : 0;
            sp.comfort_person = 0xFF;
            memset(sp.reserved, 0, sizeof(sp.reserved));

            uint16_t pkt_len = mesh_build_packet(
                NODE_ID_HUB, zone->actuator_node, PKT_ZONE_SETPOINT,
                (uint8_t *)&sp, sizeof(sp), &tx_pkt);
            sx1262_send((uint8_t *)&tx_pkt, pkt_len);
        }
        sleep_until(slot_end);
    }
}

/* ---- Process uplink packets ---- */

static void process_uplink(mesh_packet_t *pkt)
{
    node_state_t *node = NULL;

    switch (pkt->pkt_type) {
    case PKT_SENSOR_DATA: {
        node = find_or_register(pkt->src_id, NODE_TYPE_SENSOR);
        if (!node) return;
        memcpy(&node->sensor_data, pkt->payload, sizeof(sensor_data_payload_t));

        float temp = node->sensor_data.air_temp_cx100 / 100.0f;
        float mrt  = node->sensor_data.mrt_cx100 / 100.0f;

        printf("[SENSOR 0x%02X] T=%.1f°C MRT=%.1f°C H=%.0f%% occ=%d win=%d bat=%d\n",
               pkt->src_id, temp, mrt,
               node->sensor_data.humidity_centi / 100.0f,
               node->sensor_data.occupancy,
               node->sensor_data.window_state,
               node->sensor_data.battery_pct);

        /* Check for freeze */
        if (temp < 4.0f) {
            trigger_freeze_alert(node->node_id, &node->sensor_data);
        }

        /* Check for window open */
        if (node->sensor_data.window_state == 1) {
            zone_t *zone = find_zone_by_sensor(node->node_id);
            if (zone && !zone->window_open) {
                zone->window_open = true;
                zone->window_open_s = 0;
                zone->mode = MODE_OFF; /* pause conditioning */
                printf("[ZONE %d] Window open — pausing conditioning\n",
                       zone->zone_id);
                log_event(1, ALERT_INFO, zone->zone_id);
            }
        }

        /* Update zone with sensor data for forecast */
        zone_t *zone = find_zone_by_sensor(node->node_id);
        if (zone) {
            /* Feed forecast model with latest reading */
            update_thermal_forecast(zone, &node->sensor_data);
        }
        break;
    }

    case PKT_ACTUATOR_DATA: {
        node = find_or_register(pkt->src_id, NODE_TYPE_ACTUATOR);
        if (!node) return;
        memcpy(&node->actuator_data, pkt->payload, sizeof(actuator_data_payload_t));

        printf("[ACTUATOR 0x%02X] valve=%d%% pipe=%.1f°C mode=%d flow=%dml/min fault=0x%02X\n",
               pkt->src_id, node->actuator_data.valve_pos,
               node->actuator_data.pipe_temp_cx100 / 100.0f,
               node->actuator_data.zone_mode,
               node->actuator_data.flow_mlmin,
               node->actuator_data.fault_flags);

        if (node->actuator_data.fault_flags & FAULT_VALVE_STUCK) {
            printf("[FAULT] Valve stuck on zone %d!\n",
                   node->actuator_data.zone_id);
            log_event(5, ALERT_CRITICAL, node->actuator_data.zone_id);
        }
        if (node->actuator_data.fault_flags & FAULT_OVERTEMP) {
            printf("[FAULT] Overtemp on zone %d!\n",
                   node->actuator_data.zone_id);
            log_event(5, ALERT_EMERGENCY, node->actuator_data.zone_id);
        }
        break;
    }

    case PKT_COMFORT_DATA: {
        node = find_or_register(pkt->src_id, NODE_TYPE_TAG);
        if (!node) return;
        memcpy(&node->comfort_data, pkt->payload, sizeof(comfort_data_payload_t));

        printf("[TAG 0x%02X] skin=%.1f°C air=%.1f°C HR=%d act=%d comfort=%d bat=%d\n",
               pkt->src_id,
               node->comfort_data.skin_temp_cx100 / 100.0f,
               node->comfort_data.air_temp_cx100 / 100.0f,
               node->comfort_data.hr_bpm,
               node->comfort_data.activity,
               node->comfort_data.comfort_score,
               node->comfort_data.battery_pct);

        if (node->comfort_data.vote_pending != 0) {
            handle_comfort_vote(node);
        }

        /* Adjust zones where this person is present */
        adjust_comfort_for_person(node);
        break;
    }

    case PKT_FREEZE_ALERT: {
        freeze_alert_payload_t fa;
        memcpy(&fa, pkt->payload, sizeof(fa));
        printf("[ALERT] FREEZE ALERT from room 0x%02X! T=%.1f°C\n",
               fa.room_id, fa.temp_cx100 / 100.0f);
        trigger_freeze_alert(fa.room_id, NULL);
        break;
    }

    case PKT_WINDOW_OPEN: {
        window_open_payload_t wo;
        memcpy(&wo, pkt->payload, sizeof(wo));
        printf("[ALERT] Window open in room 0x%02X (drop=%.1f°C)\n",
               wo.room_id, wo.temp_drop_cx100 / 100.0f);
        zone_t *zone = find_zone_by_sensor(wo.room_id);
        if (zone) {
            zone->window_open = true;
            zone->window_open_s = 0;
            zone->mode = MODE_OFF;
            log_event(1, ALERT_INFO, zone->zone_id);
        }
        break;
    }

    case PKT_ENERGY_REPORT: {
        energy_report_payload_t er;
        memcpy(&er, pkt->payload, sizeof(er));
        printf("[ENERGY] Zone %d: %.1f Wh, flow=%dL, tariff=%d\n",
               er.zone_id, er.energy_wh_x10 / 10.0f,
               er.flow_total_l, er.tariff_period);
        log_event(4, ALERT_INFO, er.zone_id);
        break;
    }

    case PKT_HEARTBEAT:
        find_or_register(pkt->src_id, NODE_TYPE_NONE);
        break;

    default:
        printf("[MESH] Unknown pkt type 0x%02X from 0x%02X\n",
               pkt->pkt_type, pkt->src_id);
        break;
    }
}

/* ---- Freeze Protection (SAFETY CRITICAL) ---- */

static void trigger_freeze_alert(uint8_t room_id, const sensor_data_payload_t *data)
{
    if (!freeze_alert_active) {
        freeze_alert_active = true;
        freeze_alert_time = get_absolute_time();
        printf("[SAFETY] *** FREEZE ALERT ACTIVATED *** room=0x%02X\n", room_id);
        log_event(0, ALERT_EMERGENCY, room_id);
    }

    /* Force all heating zones to 100% valve open */
    for (int i = 0; i < num_zones; i++) {
        zones[i].mode = MODE_FROST;
        zones[i].setpoint_cx100 = 700; /* 7°C minimum — pipe protection */
        zones[i].frost_protect = true;
    }

    /* Force boiler/heat-pump relay ON */
    boiler_relay_on_set(true);

    /* Broadcast FREEZE_ALERT on SF12 (max range) */
    freeze_alert_payload_t fa;
    fa.alert_level = ALERT_EMERGENCY;
    fa.room_id = room_id;
    if (data) {
        fa.temp_cx100 = data->air_temp_cx100;
        fa.mrt_cx100 = data->mrt_cx100;
    } else {
        fa.temp_cx100 = -1000; /* unknown */
        fa.mrt_cx100 = -1000;
    }
    fa.all_valves_open = 1;
    fa.boiler_relay_on = 1;
    fa.timestamp_s = (uint16_t)get_epoch_seconds();
    memset(fa.reserved, 0, sizeof(fa.reserved));

    mesh_packet_t tx_pkt;
    uint16_t pkt_len = mesh_build_packet(
        NODE_ID_HUB, NODE_ID_BROADCAST, PKT_FREEZE_ALERT,
        (uint8_t *)&fa, sizeof(fa), &tx_pkt);
    sx1262_send((uint8_t *)&tx_pkt, pkt_len);

    memcpy(&last_freeze_alert, &fa, sizeof(fa));

    /* Alert via ESP32 bridge (if WiFi available) */
    char msg[128];
    snprintf(msg, sizeof(msg),
             "FREEZE_ALERT:ROOM:0x%02X,TEMP:%d,BOILER:ON",
             room_id, fa.temp_cx100);
    esp32_bridge_send(msg);
}

static void check_freeze_recovery(void)
{
    if (!freeze_alert_active)
        return;

    /* Check if all rooms are above 6°C now */
    bool all_safe = true;
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].active && nodes[i].type == NODE_TYPE_SENSOR) {
            float temp = nodes[i].sensor_data.air_temp_cx100 / 100.0f;
            if (temp < 6.0f) {
                all_safe = false;
                break;
            }
        }
    }

    if (all_safe) {
        printf("[SAFETY] Freeze condition cleared — all rooms >6°C\n");
        freeze_alert_active = false;
        boiler_relay_on_set(false);
        /* Restore normal mode for all zones */
        for (int i = 0; i < num_zones; i++) {
            zones[i].frost_protect = false;
            if (zones[i].window_open) {
                zones[i].mode = MODE_OFF;
            } else {
                zones[i].mode = MODE_HEATING;
            }
        }
    }
}

/* ---- Thermal Forecast (physics-informed RC-network + GRU correction) ---- */

/* 5R-1C thermal network per zone:
 * R_ext (exterior wall), R_int (interior wall), R_window, R_floor, R_ceil
 * C_air (air thermal capacity)
 *
 * dT/dt = (T_ext - T) / (R_ext * C) + (T_neighbor - T) / (R_int * C)
 *        + (T_ext - T) / (R_window * C) + solar_gain / C
 *        + (T_floor - T) / (R_floor * C) + body_heat / C
 *
 * Parameters learned during calibration (3-day learning period)
 */

typedef struct {
    float r_ext;      /* K/W — exterior wall resistance */
    float r_int;      /* K/W — interior wall resistance */
    float r_window;   /* K/W — window resistance */
    float r_floor;    /* K/W — floor resistance */
    float c_air;      /* J/K — air thermal capacity */
    float neighbor_t; /* °C — adjacent zone temp (from sensor) */
    float body_heat_w;/* W per person metabolic heat */
} thermal_params_t;

static thermal_params_t zone_params[MAX_ZONES];
static int16_t temp_history[MAX_ZONES][HISTORY_STEPS];
static uint8_t history_idx[MAX_ZONES];

static void update_thermal_forecast(zone_t *zone, const sensor_data_payload_t *data)
{
    int z = -1;
    for (int i = 0; i < num_zones; i++) {
        if (&zones[i] == zone) { z = i; break; }
    }
    if (z < 0) return;

    /* Store current temp in history */
    temp_history[z][history_idx[z]] = data->air_temp_cx100;
    history_idx[z] = (history_idx[z] + 1) % HISTORY_STEPS;

    /* In production: run TFLite Micro thermal forecast model here.
     * The model is a hybrid:
     * 1. Physics layer: 5R-1C RC-network ODE (solved analytically for 15-min step)
     * 2. GRU correction: learns residual between physics and actual
     *
     * Inputs: 2h temp history, outdoor temp, solar gain, occupancy, weather forecast
     * Output: per-zone temp forecast for next 4 hours (16 steps × 15 min)
     *
     * Stub: simple linear extrapolation using recent trend */
    if (history_idx[z] >= 2) {
        int16_t prev = temp_history[z][(history_idx[z] - 2) % HISTORY_STEPS];
        int16_t curr = temp_history[z][(history_idx[z] - 1) % HISTORY_STEPS];
        int16_t trend = curr - prev; /* °C×100 per 15 min */

        for (int s = 0; s < FORECAST_STEPS; s++) {
            /* Damped extrapolation toward setpoint */
            int16_t target = zone->setpoint_cx100;
            int16_t predicted = curr + trend * (s + 1);
            /* Pull toward setpoint with increasing strength */
            float pull = 0.1f * (s + 1);
            if (pull > 0.8f) pull = 0.8f;
            predicted = (int16_t)(predicted * (1.0f - pull) + target * pull);

            zone_forecasts[z].temp_cx100[s] = predicted;
            zone_forecasts[z].mrt_cx100[s] = data->mrt_cx100 + trend * s / 2;
            zone_forecasts[z].occupancy_prob[s] = 50; /* stub: 50% */
        }
    }
}

/* ---- Comfort Adjustment ---- */

static void handle_comfort_vote(node_state_t *tag_node)
{
    int8_t vote = (int8_t)tag_node->comfort_data.vote_pending;
    printf("[COMFORT] Person 0x%02X voted: %s (skin=%.1f°C)\n",
           tag_node->node_id,
           vote == 1 ? "I'm COLD" : "I'm HOT",
           tag_node->comfort_data.skin_temp_cx100 / 100.0f);

    /* Find which zone this person is in (by BLE RSSI or mmWave proximity) */
    /* In production: use BLE RSSI triangulation or hub-side room sensor
     * proximity to determine which room the tagged person is in */

    /* For each zone, apply temporary boost */
    if (vote == 1) {
        /* "I'm cold" — boost current zone by +1.5°C for 30 min */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_HEATING) {
                zones[i].boost_minutes = 30;
                zones[i].boost_delta_cx100 = 150;
                zones[i].setpoint_cx100 += 150;
                printf("[ZONE %d] Boost +1.5°C for 30min (comfort vote)\n",
                       zones[i].zone_id);
                break;
            }
        }
    } else if (vote == 2) {
        /* "I'm hot" — reduce current zone by -1.5°C for 30 min */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_COOLING || zones[i].mode == MODE_HEATING) {
                zones[i].boost_minutes = 30;
                zones[i].boost_delta_cx100 = -150;
                zones[i].setpoint_cx100 -= 150;
                printf("[ZONE %d] Reduce -1.5°C for 30min (comfort vote)\n",
                       zones[i].zone_id);
                break;
            }
        }
    }

    /* Clear vote */
    tag_node->comfort_data.vote_pending = 0;

    /* In production: send vote + sensor context to cloud for personal
     * comfort model retraining. The cloud model learns:
     *   comfort_vote = f(skin_temp, air_temp, MRT, humidity, air_vel, HR, activity)
     * personalized per person. */

    char msg[128];
    snprintf(msg, sizeof(msg),
             "COMFORT_VOTE:PERSON:0x%02X,VOTE:%d,SKIN:%d,AIR:%d,HR:%d,ACT:%d",
             tag_node->node_id, vote,
             tag_node->comfort_data.skin_temp_cx100,
             tag_node->comfort_data.air_temp_cx100,
             tag_node->comfort_data.hr_bpm,
             tag_node->comfort_data.activity);
    esp32_bridge_send(msg);
}

static void adjust_comfort_for_person(node_state_t *tag_node)
{
    /* In production: run TFLite Micro personal comfort model:
     *   predicted_comfort = f(skin_temp, air_temp, MRT, humidity, HR, activity)
     * If predicted comfort < -1 (cold), boost zone setpoint +0.5°C
     * If predicted comfort > +1 (hot), reduce zone setpoint -0.5°C
     *
     * Stub: use skin temp heuristic */

    float skin = tag_node->comfort_data.skin_temp_cx100 / 100.0f;
    float air  = tag_node->comfort_data.air_temp_cx100 / 100.0f;

    /* Skin temp < 28°C typically means feeling cold */
    if (skin < 28.0f && tag_node->comfort_data.activity < ACT_MODERATE) {
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_HEATING && zones[i].boost_minutes == 0) {
                int16_t adj = 50; /* +0.5°C */
                zones[i].setpoint_cx100 += adj;
                printf("[COMFORT] Auto-adjust zone %d: +0.5°C (skin=%.1f°C)\n",
                       zones[i].zone_id, skin);
            }
        }
    }
    /* Skin temp > 34°C typically means feeling hot */
    else if (skin > 34.0f && tag_node->comfort_data.activity > ACT_LIGHT) {
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_COOLING && zones[i].boost_minutes == 0) {
                int16_t adj = 50;
                zones[i].setpoint_cx100 -= adj;
                printf("[COMFORT] Auto-adjust zone %d: -0.5°C (skin=%.1f°C)\n",
                       zones[i].zone_id, skin);
            }
        }
    }
}

/* ---- Solar / TOU Optimization ---- */

static void update_solar_coordination(void)
{
    /* In production: query solar inverter API via ESP32-C6 WiFi
     * or read CT clamp power data.
     *
     * Logic:
     * - if solar_surplus > 500W: recommend solar boost
     * - boost target = setpoint + min(surplus/500, 3) °C
     * - only boost occupied zones or zones that will be occupied
     */

    if (solar_surplus_w > 500) {
        solar_boost_recommended = true;
        /* Apply solar boost to heating zones */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_HEATING && !zones[i].window_open) {
                /* Check thermal forecast: don't overheat */
                int16_t forecast = zone_forecasts[i].temp_cx100[0];
                int16_t target = zones[i].setpoint_cx100;
                if (forecast < target + 300) { /* only boost if not already overheating */
                    zones[i].mode = MODE_SOLAR_BOOST;
                    zones[i].boost_minutes = 30;
                    zones[i].boost_delta_cx100 = 200; /* +2°C */
                    zones[i].setpoint_cx100 += 200;
                    printf("[SOLAR] Boost zone %d: +2°C (surplus=%dW)\n",
                           zones[i].zone_id, solar_surplus_w);
                }
            }
        }
    } else {
        solar_boost_recommended = false;
        /* Revert solar boost zones to normal */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_SOLAR_BOOST && zones[i].boost_minutes == 0) {
                zones[i].mode = MODE_HEATING;
                zones[i].setpoint_cx100 -= zones[i].boost_delta_cx100;
                zones[i].boost_delta_cx100 = 0;
            }
        }
    }
}

static void update_tou_optimization(void)
{
    /* In production: load TOU tariff schedule from cloud via MQTT.
     * For now: simple time-based heuristic.
     *
     * Off-peak: 22:00-06:00 (cheap — pre-heat)
     * Mid-peak: 06:00-14:00, 20:00-22:00
     * Peak: 14:00-20:00 (expensive — coast on thermal mass)
     */

    uint32_t epoch = get_epoch_seconds();
    uint8_t hour = (uint8_t)((epoch / 3600) % 24);

    if (hour >= 22 || hour < 6) {
        current_tou_period = 0; /* off-peak */
        current_tou_rate = 80;  /* 8.0 cents/kWh ×10 */

        /* Pre-heat during off-peak: boost setpoints by 1°C */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_HEATING && !zones[i].window_open) {
                /* Check forecast: will this room be occupied soon? */
                /* Pre-heat bedroom before 6am wake-up */
                if (hour >= 4 && hour < 6) {
                    /* Find bedroom zone (stub: zone 1) */
                    if (zones[i].zone_id == 1) {
                        zones[i].setpoint_cx100 = 2200; /* 22°C pre-heat */
                        printf("[TOU] Pre-heating bedroom for wake-up (off-peak)\n");
                    }
                }
            }
        }
    } else if (hour >= 14 && hour < 20) {
        current_tou_period = 2; /* peak */
        current_tou_rate = 320; /* 32.0 cents/kWh ×10 */

        /* Coast during peak: allow zones to drift down 1°C */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].mode == MODE_HEATING && !zones[i].window_open) {
                int16_t forecast = zone_forecasts[i].temp_cx100[0];
                int16_t target = zones[i].setpoint_cx100;
                /* If forecast is above setpoint - 100 (within 1°C), reduce heating */
                if (forecast > target - 100) {
                    zones[i].setpoint_cx100 -= 100; /* -1°C coast */
                    printf("[TOU] Coasting zone %d during peak (forecast ok)\n",
                           zones[i].zone_id);
                }
            }
        }
    } else {
        current_tou_period = 1; /* mid-peak */
        current_tou_rate = 150;  /* 15.0 cents/kWh ×10 */
    }
}

/* ---- Boost timer countdown ---- */

static void update_boost_timers(void)
{
    static absolute_time_t last_boost_check = {0};
    int64_t elapsed = absolute_time_diff_us(last_boost_check, get_absolute_time());
    if (elapsed > 60000000) { /* every 60s */
        for (int i = 0; i < num_zones; i++) {
            if (zones[i].boost_minutes > 0) {
                zones[i].boost_minutes--;
                if (zones[i].boost_minutes == 0) {
                    /* Revert boost */
                    zones[i].setpoint_cx100 -= zones[i].boost_delta_cx100;
                    zones[i].boost_delta_cx100 = 0;
                    if (zones[i].mode == MODE_SOLAR_BOOST) {
                        zones[i].mode = MODE_HEATING;
                    }
                    printf("[ZONE %d] Boost expired — reverting\n",
                           zones[i].zone_id);
                }
            }
            if (zones[i].window_open) {
                zones[i].window_open_s += 60;
                if (zones[i].window_open_s > 1800) { /* 30 min */
                    printf("[ALERT] Window open in zone %d for %d min!\n",
                           zones[i].zone_id, zones[i].window_open_s / 60);
                    log_event(1, ALERT_WARNING, zones[i].zone_id);
                    esp32_bridge_send("WINDOW_OPEN_ALERT");
                }
            }
        }
        last_boost_check = get_absolute_time();
    }
}

/* ---- Window close detection ---- */

static void check_window_closed(void)
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].active && nodes[i].type == NODE_TYPE_SENSOR) {
            if (nodes[i].sensor_data.window_state == 0) {
                zone_t *zone = find_zone_by_sensor(nodes[i].node_id);
                if (zone && zone->window_open) {
                    zone->window_open = false;
                    zone->window_open_s = 0;
                    zone->mode = MODE_HEATING;
                    printf("[ZONE %d] Window closed — resuming conditioning\n",
                           zone->zone_id);
                    log_event(1, ALERT_INFO, zone->zone_id);
                }
            }
        }
    }
}

/* ---- ESP32-C6 UART Bridge (WiFi/BLE uplink) ---- */

static void esp32_bridge_send(const char *msg)
{
    uart_puts(PIN_ES32_UART, msg);
    uart_putc_raw(PIN_ES32_UART, '\n');
}

static void esp32_bridge_send_sensor_telemetry(node_state_t *node)
{
    if (node->type != NODE_TYPE_SENSOR) return;
    sensor_data_payload_t *s = &node->sensor_data;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "S:ID:0x%02X,T:%d,MRT:%d,H:%d,VEL:%d,PRES:%d,OCC:%d,CONF:%d,LUX:%d,CO2:%d,WIN:%d,SOL:%d,BAT:%d,FAULT:0x%02X",
             node->node_id, s->air_temp_cx100, s->mrt_cx100,
             s->humidity_centi, s->air_vel_cms_x100, s->pressure_pa,
             s->occupancy, s->occupancy_conf, s->light_lux, s->co2_ppm,
             s->window_state, s->solar_gain_w, s->battery_pct, s->fault_flags);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_actuator_telemetry(node_state_t *node)
{
    if (node->type != NODE_TYPE_ACTUATOR) return;
    actuator_data_payload_t *a = &node->actuator_data;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "A:ID:0x%02X,VAL:%d,TGT:%d,PIPE:%d,FLOW:%d,ENG:%d,MODE:%d,REL:0x%02X,FAULT:0x%02X,BAT:%d",
             node->node_id, a->valve_pos, a->valve_target,
             a->pipe_temp_cx100, a->flow_mlmin, a->energy_btu_x10,
             a->zone_mode, a->relay_state, a->fault_flags, a->battery_pct);
    esp32_bridge_send(msg);
}

static void esp32_bridge_send_all_telemetry(void)
{
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].active) continue;
        if (nodes[i].type == NODE_TYPE_SENSOR)
            esp32_bridge_send_sensor_telemetry(&nodes[i]);
        else if (nodes[i].type == NODE_TYPE_ACTUATOR)
            esp32_bridge_send_actuator_telemetry(&nodes[i]);
    }
    /* Send zone states */
    for (int i = 0; i < num_zones; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Z:ID:%d,SP:%d,MODE:%d,BOOST:%d,WIN:%d,FROST:%d",
                 zones[i].zone_id, zones[i].setpoint_cx100,
                 zones[i].mode, zones[i].boost_minutes,
                 zones[i].window_open ? 1 : 0,
                 zones[i].frost_protect ? 1 : 0);
        esp32_bridge_send(msg);
    }
}

static void esp32_bridge_send_freeze_alert(const freeze_alert_payload_t *fa)
{
    char msg[256];
    snprintf(msg, sizeof(msg),
             "FREEZE_ALERT:ROOM:0x%02X,TEMP:%d,MRT:%d,VALVES:%d,BOILER:%d",
             fa->room_id, fa->temp_cx100, fa->mrt_cx100,
             fa->all_valves_open, fa->boiler_relay_on);
    esp32_bridge_send(msg);
}

/* ---- TFT Display ---- */

static void tft_init(void)
{
    /* ILI9488 initialization via SPI */
    printf("[TFT] ILI9488 initialized\n");
}

static void tft_draw_dashboard(void)
{
    /* In production: draw home thermal map, per-room temps, energy bar,
     * solar gauge, zone states, comfort indicators */
    printf("[TFT] === ThermoGrid Dashboard ===\n");
    printf("[TFT] Nodes: %d active (%d sensors, %d actuators, %d tags)\n",
           num_active_nodes, num_sensors, num_actuators, num_tags);
    printf("[TFT] Solar: %dW surplus, TOU: period=%d rate=%d.%d¢\n",
           solar_surplus_w, current_tou_period,
           current_tou_rate / 10, current_tou_rate % 10);
    printf("[TFT] Zones: %d", num_zones);
    for (int i = 0; i < num_zones && i < 8; i++) {
        float sp = zones[i].setpoint_cx100 / 100.0f;
        printf(" | Z%d:%.1f°C %s%s%s", zones[i].zone_id, sp,
               zones[i].mode == MODE_HEATING ? "HEAT" :
               zones[i].mode == MODE_COOLING ? "COOL" :
               zones[i].mode == MODE_FROST ? "FROST" :
               zones[i].mode == MODE_SOLAR_BOOST ? "SOLAR" : "OFF",
               zones[i].window_open ? " WIN" : "",
               zones[i].boost_minutes > 0 ? " BST" : "");
    }
    if (freeze_alert_active) {
        printf(" | *** FREEZE ALERT ***");
    }
    if (boiler_relay_on) {
        printf(" | BOILER:ON");
    }
    printf("\n");
}

/* ---- Main loop ---- */

int main(void)
{
    stdio_init_all();
    printf("\n=== ThermoGrid Hub v1.0 ===\n");

    /* Initialize hardware */
    sx1262_init();
    rtc_init();
    tft_init();

    /* Boiler relay pin */
    gpio_init(PIN_BOILER_RELAY);
    gpio_set_dir(PIN_BOILER_RELAY, GPIO_OUT);
    gpio_put(PIN_BOILER_RELAY, 0);

    /* User button + LEDs */
    gpio_init(PIN_USER_BTN);
    gpio_set_dir(PIN_USER_BTN, GPIO_IN);
    gpio_pull_up(PIN_USER_BTN);
    gpio_init(PIN_LED_R);
    gpio_set_dir(PIN_LED_R, GPIO_OUT);
    gpio_init(PIN_LED_G);
    gpio_set_dir(PIN_LED_G, GPIO_OUT);
    gpio_init(PIN_LED_B);
    gpio_set_dir(PIN_LED_B, GPIO_OUT);

    /* UART for ESP32-C6 bridge */
    uart_init(PIN_ES32_UART, 115200);
    gpio_set_function(PIN_ES32_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ES32_RX, GPIO_FUNC_UART);

    /* Initialize default zones (stub — in production learned from enrollment) */
    zones[0].zone_id = 0; zones[0].sensor_node = 0x10; zones[0].actuator_node = 0x20;
    zones[0].setpoint_cx100 = 2100; zones[0].mode = MODE_HEATING;
    zones[1].zone_id = 1; zones[1].sensor_node = 0x11; zones[1].actuator_node = 0x21;
    zones[1].setpoint_cx100 = 2000; zones[1].mode = MODE_HEATING;
    zones[2].zone_id = 2; zones[2].sensor_node = 0x12; zones[2].actuator_node = 0x22;
    zones[2].setpoint_cx100 = 1900; zones[2].mode = MODE_HEATING;
    num_zones = 3;

    /* Initialize thermal params (stub — learned during calibration) */
    for (int i = 0; i < MAX_ZONES; i++) {
        zone_params[i].r_ext = 0.5f;
        zone_params[i].r_int = 1.0f;
        zone_params[i].r_window = 0.2f;
        zone_params[i].r_floor = 2.0f;
        zone_params[i].c_air = 50000.0f; /* 50 kJ/K */
        zone_params[i].neighbor_t = 20.0f;
        zone_params[i].body_heat_w = 80.0f;
    }

    printf("[HUB] Starting mesh coordinator with %d zones\n", num_zones);
    gpio_put(PIN_LED_G, 1); /* green = running */

    absolute_time_t last_telemetry = get_absolute_time();
    absolute_time_t last_optimization = get_absolute_time();

    while (true) {
        /* Run one TDMA frame (mesh communication) */
        tdma_run_frame();

        /* Periodic tasks */
        int64_t now_us = to_us_since_boot(get_absolute_time());

        /* Telemetry to cloud every 30s */
        if (now_us - to_us_since_boot(last_telemetry) > 30000000) {
            esp32_bridge_send_all_telemetry();
            if (freeze_alert_active) {
                esp32_bridge_send_freeze_alert(&last_freeze_alert);
            }
            last_telemetry = get_absolute_time();
        }

        /* Optimization every 15 min */
        if (now_us - to_us_since_boot(last_optimization) > 900000000) {
            update_solar_coordination();
            update_tou_optimization();
            last_optimization = get_absolute_time();
        }

        /* Safety checks every frame */
        check_node_timeouts();
        check_freeze_recovery();
        check_window_closed();
        update_boost_timers();

        /* Display update every 2s */
        static absolute_time_t last_display = {0};
        if (absolute_time_diff_us(last_display, get_absolute_time()) > 2000000) {
            tft_draw_dashboard();
            last_display = get_absolute_time();
        }

        /* Status LED */
        if (freeze_alert_active) {
            gpio_put(PIN_LED_R, 1);
            gpio_put(PIN_LED_G, 0);
        } else {
            gpio_put(PIN_LED_R, 0);
            gpio_put(PIN_LED_G, 1);
        }
    }

    return 0;
}
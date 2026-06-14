/**
 * UrbanHarvest - Hub Node Firmware
 * nRF5340 + ESP32-C6
 *
 * Central coordinator: mesh network, ML inference, voice alerts,
 * WiFi uplink, BLE for mobile app, TFT garden dashboard.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/display.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========== CONSTANTS ========== */
#define MESH_MAX_NODES          24
#define MESH_SLOT_DURATION_MS   100
#define MESH_FRAME_DURATION_MS  2600
#define MESH_NUM_SLOTS          26
#define MESH_ALERT_SLOT         25

#define WIFI_MQTT_BROKER        "mqtt://urbanharvest.local:8883"
#define WIFI_MQTT_TOPIC_SENSORS "urbanharvest/sensors/{plant_id}"
#define WIFI_MQTT_TOPIC_ALERTS  "urbanharvest/alerts"
#define WIFI_MQTT_TOPIC_GROWPOD "urbanharvest/growpod/cmd"

#define HEALTH_THRIVING         0
#define HEALTH_GOOD             1
#define HEALTH_STRESSED         2
#define HEALTH_CRITICAL         3
#define HEALTH_DEAD             4

#define ALERT_SOIL_DRY          15      /* % moisture */
#define ALERT_SOIL_WET          85      /* % moisture */
#define ALERT_EC_HIGH           3.0f    /* mS/cm */
#define ALERT_SOIL_TEMP_LOW     10.0f   /* °C */
#define ALERT_SOIL_TEMP_HIGH    40.0f   /* °C */
#define ALERT_PUMP_FAILURE     0       /* flow when pump off */
#define ALERT_WIND_DANGER       50.0f   /* km/h */

/* ========== DATA STRUCTURES ========== */

typedef struct __attribute__((packed)) {
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[48];
    uint16_t crc16;
} mesh_packet_t;

typedef struct {
    uint8_t  node_id;
    uint8_t  node_type;  /* 0=plant_sensor, 1=grow_pod, 2=weather_station */
    uint8_t  slot;
    uint32_t last_seen_ms;
    uint8_t  active;
} mesh_node_info_t;

typedef struct {
    /* Soil and environmental readings */
    float    soil_moisture_pct;
    float    soil_ec_ms_cm;
    float    soil_temp_c;
    float    par_umol_m2s;
    uint16_t light_lux;
    float    leaf_wetness_pct;
    float    leaf_wet_hours;
    uint8_t  health_index;     /* 0-100 */
    uint8_t  health_category;  /* 0-4 */
    uint32_t timestamp_ms;
} plant_reading_t;

typedef struct {
    /* Grow pod controller state */
    uint8_t  pump_running;
    float    nutrient_a_ml;
    float    nutrient_b_ml;
    float    ph_dose_ml;
    uint8_t  fan_speed_pct;
    uint8_t  heater_on;
    uint8_t  humidifier_on;
    uint8_t  light_on;
    uint8_t  red_pwm;
    uint8_t  blue_pwm;
    uint8_t  white_pwm;
    uint8_t  far_red_pwm;
    float    water_temp_c;
    float    flow_rate_ml_s;
    uint8_t  disease_alert;     /* 0=none, 1-5=disease class */
    float    disease_confidence;
} growpod_state_t;

typedef struct {
    /* Weather station readings */
    float    temperature_c;
    float    humidity_pct;
    float    pressure_hpa;
    float    wind_speed_kmh;
    uint8_t  wind_direction;   /* 0-7: N,NE,E,SE,S,SW,W,NW */
    float    rain_mm;
    float    uv_index;
    uint16_t light_lux;
    float    solar_voltage;
    float    battery_voltage;
    uint8_t  battery_soc_pct;
} weather_data_t;

typedef struct {
    /* Irrigation schedule for a plant */
    uint8_t  plant_id;
    uint16_t volume_ml;
    uint16_t interval_min;
    uint8_t  skip_rain;        /* 1 = skip if rain detected/forecast */
    uint8_t  active;
} irrigation_schedule_t;

typedef struct {
    /* Harvest prediction */
    uint8_t  plant_id;
    uint8_t  plant_type;       /* 0=unknown, 1=tomato, 2=basil, 3=lettuce, ... */
    uint16_t days_to_harvest;
    float    estimated_yield_g;
    uint8_t  confidence_pct;
} harvest_prediction_t;

/* ========== GLOBALS ========== */

static mesh_node_info_t mesh_nodes[MESH_MAX_NODES];
static uint8_t mesh_num_nodes = 0;
static uint16_t mesh_seq = 0;

static plant_reading_t plant_data[MESH_MAX_NODES];
static growpod_state_t growpod_state;
static weather_data_t weather;

static irrigation_schedule_t irrigation_sched[24];
static harvest_prediction_t harvest_pred[24];

static uint8_t hub_health_avg = 0;
static uint8_t hub_alerts_count = 0;
static uint8_t hub_harvest_ready = 0;

/* TFT display buffers */
static char display_line1[32];
static char display_line2[32];
static char display_line3[32];
static char display_line4[32];

/* ========== SX1262 SUB-GHZ RADIO DRIVER ========== */

#define SX1262_SPI      DT_NODELABEL(spi1)
#define SX1262_NSS      DT_GPIO_PIN(DT_NODELABEL(sx1262_nss), gpios)
#define SX1262_BUSY     DT_GPIO_PIN(DT_NODELABEL(sx1262_busy), gpios)
#define SX1262_IRQ      DT_GPIO_PIN(DT_NODELABEL(sx1262_irq), gpios)
#define SX1262_NRST     DT_GPIO_PIN(DT_NODELABEL(sx1262_nrst), gpios)

static const struct device *spi_dev;
static struct gpio_dt_spec sx1262_nss;
static struct gpio_dt_spec sx1262_busy;
static struct gpio_dt_spec sx1262_irq;
static struct gpio_dt_spec sx1262_nrst;

static uint8_t sx1262_tx_buf[64];
static uint8_t sx1262_rx_buf[64];

/**
 * sx1262_wait_busy - Wait until BUSY pin goes low
 */
static void sx1262_wait_busy(void)
{
    while (gpio_pin_get_dt(&sx1262_busy)) {
        k_msleep(1);
    }
}

/**
 * sx1262_spi_transfer - Send/receive data via SPI
 */
static void sx1262_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    gpio_pin_set_dt(&sx1262_nss, 0);
    sx1262_wait_busy();

    struct spi_buf tx_buf = { .buf = (void *)tx, .len = len };
    struct spi_buf rx_buf = { .buf = rx, .len = len };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    spi_transceive_dt(spi_dev, &tx_set, &rx_set);

    gpio_pin_set_dt(&sx1262_nss, 1);
}

/**
 * sx1262_write_command - Send command to SX1262
 */
static void sx1262_write_command(uint8_t cmd, const uint8_t *data, uint16_t len)
{
    sx1262_tx_buf[0] = cmd;
    memcpy(&sx1262_tx_buf[1], data, len);
    sx1262_spi_transfer(sx1262_tx_buf, NULL, len + 1);
}

/**
 * sx1262_init - Initialize SX1262 radio
 */
static int sx1262_init(void)
{
    /* Reset */
    gpio_pin_set_dt(&sx1262_nrst, 0);
    k_msleep(10);
    gpio_pin_set_dt(&sx1262_nrst, 1);
    k_msleep(50);

    /* Set to standby RC oscillator */
    uint8_t standby_cmd[] = { 0x00 };
    sx1262_write_command(0x80, standby_cmd, 1);
    k_msleep(10);

    /* Set regulator mode to DC-DC */
    uint8_t reg_mode[] = { 0x01 };
    sx1262_write_command(0x96, reg_mode, 1);

    /* Set DIO2 as RF switch control */
    uint8_t dio2_cfg[] = { 0x01 };
    sx1262_write_command(0x9D, dio2_cfg, 1);

    /* Set modem to LoRa */
    uint8_t packet_type[] = { 0x01 };
    sx1262_write_command(0x8A, packet_type, 1);
    k_msleep(10);

    /* Configure LoRa: SF7, 125kHz BW, CR 4/5 */
    uint8_t mod_params[] = {
        0x04,  /* SF7 */
        0x07,  /* BW 125kHz */
        0x01,  /* CR 4/5 */
        0x00   /* Low DR opt off */
    };
    sx1262_write_command(0x8E, mod_params, 4);

    /* Set frequency: 868.0 MHz */
    uint32_t freq = 868000000 / 61;  /* PLL step = 61.035 Hz */
    uint8_t freq_cmd[] = {
        (freq >> 16) & 0xFF,
        (freq >> 8) & 0xFF,
        freq & 0xFF
    };
    sx1262_write_command(0x86, freq_cmd, 3);

    /* Set TX power: +14 dBm */
    uint8_t tx_params[] = { 0x0E, 0x02 };
    sx1262_write_command(0x8E, tx_params, 2);

    /* Set sync word for UrbanHarvest mesh */
    uint8_t sync_word[] = { 0xUH, 0x01 };
    sx1262_write_command(0x8E + 4, sync_word, 2);

    /* Set preamble length: 4 symbols */
    uint8_t preamble[] = { 0x00, 0x04 };
    sx1262_write_command(0x8E + 6, preamble, 2);

    return 0;
}

/**
 * sx1262_transmit - Send mesh packet
 */
static int sx1262_transmit(const mesh_packet_t *pkt)
{
    uint8_t buf[60];
    uint16_t idx = 0;

    buf[idx++] = 0xAA;
    buf[idx++] = 0xAA;
    buf[idx++] = 0x55;
    buf[idx++] = 0x55;
    buf[idx++] = sizeof(mesh_packet_t) + 2;
    buf[idx++] = pkt->src_id;
    buf[idx++] = pkt->dst_id;
    buf[idx++] = pkt->msg_type;
    buf[idx++] = (pkt->seq_num >> 8) & 0xFF;
    buf[idx++] = pkt->seq_num & 0xFF;
    memcpy(&buf[idx], pkt->payload, 48);
    idx += 48;

    /* CRC16-CCITT */
    uint16_t crc = 0;
    for (int i = 6; i < idx; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    buf[idx++] = (crc >> 8) & 0xFF;
    buf[idx++] = crc & 0xFF;

    /* Set buffer and transmit */
    uint8_t set_buf_cmd[] = { 0x00, 0x00, (idx >> 8) & 0xFF, idx & 0xFF };
    sx1262_write_command(0x92, set_buf_cmd, 4);

    uint8_t tx_cmd[] = { 0x00, 0x00, 0x00, 0x00 };
    sx1262_write_command(0x83, tx_cmd, 4);

    return 0;
}

/**
 * sx1262_receive - Set radio to receive mode
 */
static void sx1262_receive(uint32_t timeout_ms)
{
    uint8_t rx_cmd[] = {
        (timeout_ms >> 24) & 0xFF,
        (timeout_ms >> 16) & 0xFF,
        (timeout_ms >> 8) & 0xFF,
        timeout_ms & 0xFF
    };
    sx1262_write_command(0x82, rx_cmd, 4);
}

/* ========== MESH TDMA SCHEDULER ========== */

static void mesh_assign_slots(void)
{
    uint8_t slot = 1;
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (mesh_nodes[i].active) {
            mesh_nodes[i].slot = slot++;
        }
    }
}

static int mesh_register_node(uint8_t node_id, uint8_t node_type)
{
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (!mesh_nodes[i].active) {
            mesh_nodes[i].node_id = node_id;
            mesh_nodes[i].node_type = node_type;
            mesh_nodes[i].active = 1;
            mesh_nodes[i].last_seen_ms = k_uptime_get_32();
            mesh_num_nodes++;
            mesh_assign_slots();
            return i;
        }
    }
    return -1;
}

/**
 * mesh_send_sync - Hub broadcasts sync packet in slot 0
 */
static void mesh_send_sync(void)
{
    mesh_packet_t sync;
    sync.src_id = 0;
    sync.dst_id = 0xFF;
    sync.msg_type = 0x04;  /* COMMAND/SYNC */
    sync.seq_num = mesh_seq++;

    sync.payload[0] = mesh_num_nodes;
    for (int i = 0; i < mesh_num_nodes && i < 16; i++) {
        sync.payload[1 + i * 2] = mesh_nodes[i].node_id;
        sync.payload[2 + i * 2] = mesh_nodes[i].slot;
    }
    sync.payload[47] = hub_health_avg;
    sync.payload[48] = hub_alerts_count;

    sx1262_transmit(&sync);
}

/* ========== PLANT HEALTH CALCULATION ========== */

/**
 * calculate_plant_health - Compute plant health index from sensor readings
 * Returns 0-100: 0=dead, 50=stressed, 100=thriving
 * Different target ranges per plant type (simplified, extensible)
 */
static uint8_t calculate_plant_health(const plant_reading_t *pr, uint8_t plant_type)
{
    float health = 100.0f;

    /* Soil moisture scoring — most vegetables want 40-65% */
    float moisture_target_low = 40.0f;
    float moisture_target_high = 65.0f;
    if (plant_type == 2) { /* Lettuce likes it wetter */
        moisture_target_low = 50.0f;
        moisture_target_high = 75.0f;
    }

    if (pr->soil_moisture_pct < moisture_target_low) {
        float deficit = moisture_target_low - pr->soil_moisture_pct;
        health -= deficit * 1.5f;  /* 1.5 health points per % below target */
    } else if (pr->soil_moisture_pct > moisture_target_high) {
        float excess = pr->soil_moisture_pct - moisture_target_high;
        health -= excess * 1.2f;  /* Overwatering penalty slightly less */
    }

    /* Soil EC scoring — most vegetables want 1.0-2.5 mS/cm */
    if (pr->soil_ec_ms_cm < 0.5f) {
        health -= 20.0f;  /* Very low nutrients */
    } else if (pr->soil_ec_ms_cm > 2.5f) {
        float over = pr->soil_ec_ms_cm - 2.5f;
        health -= over * 25.0f;  /* Nutrient burn penalty */
    }

    /* Soil temperature scoring — optimal 18-26°C for most */
    if (pr->soil_temp_c < 10.0f) {
        health -= 30.0f;
    } else if (pr->soil_temp_c < 18.0f) {
        health -= (18.0f - pr->soil_temp_c) * 2.0f;
    } else if (pr->soil_temp_c > 35.0f) {
        health -= 30.0f;
    } else if (pr->soil_temp_c > 26.0f) {
        health -= (pr->soil_temp_c - 26.0f) * 1.5f;
    }

    /* Light (PAR) scoring — most veggies want 200-800 µmol/m²/s */
    if (pr->par_umol_m2s < 50.0f) {
        health -= 40.0f;  /* Severely light-deprived */
    } else if (pr->par_umol_m2s < 200.0f) {
        health -= (200.0f - pr->par_umol_m2s) * 0.15f;
    }

    /* Leaf wetness duration — fungal disease risk if > 6 hours */
    if (pr->leaf_wet_hours > 6.0f) {
        health -= (pr->leaf_wet_hours - 6.0f) * 5.0f;
    }

    if (health > 100.0f) health = 100.0f;
    if (health < 0.0f) health = 0.0f;

    return (uint8_t)health;
}

/**
 * get_health_category - Classify health index into category
 */
static uint8_t get_health_category(uint8_t health)
{
    if (health >= 80) return HEALTH_THRIVING;
    if (health >= 60) return HEALTH_GOOD;
    if (health >= 35) return HEALTH_STRESSED;
    if (health >= 10) return HEALTH_CRITICAL;
    return HEALTH_DEAD;
}

/* ========== ALERT ENGINE ========== */

/**
 * check_plant_alerts - Evaluate alert conditions for a plant
 * Returns alert level: 0=none, 1=info, 2=warning, 3=danger, 4=critical
 */
static int check_plant_alerts(const plant_reading_t *pr, uint8_t plant_id)
{
    int alert_level = 0;

    /* Critically dry soil */
    if (pr->soil_moisture_pct < ALERT_SOIL_DRY) {
        alert_level = 4;
        snprintf(display_line1, sizeof(display_line1), "CRITICAL: Plant %d dry!", plant_id);
        snprintf(display_line2, sizeof(display_line2), "Moisture: %.0f%%", pr->soil_moisture_pct);
    }
    /* Waterlogged */
    else if (pr->soil_moisture_pct > ALERT_SOIL_WET) {
        if (alert_level < 3) alert_level = 3;
        snprintf(display_line1, sizeof(display_line1), "WARNING: Plant %d waterlogged", plant_id);
    }

    /* Nutrient burn risk */
    if (pr->soil_ec_ms_cm > ALERT_EC_HIGH) {
        if (alert_level < 3) alert_level = 3;
        snprintf(display_line3, sizeof(display_line3), "EC high: %.1f mS/cm", pr->soil_ec_ms_cm);
    }

    /* Temperature extremes */
    if (pr->soil_temp_c < ALERT_SOIL_TEMP_LOW || pr->soil_temp_c > ALERT_SOIL_TEMP_HIGH) {
        if (alert_level < 3) alert_level = 3;
    }

    /* Fungal disease risk from leaf wetness */
    if (pr->leaf_wet_hours > 8.0f) {
        if (alert_level < 2) alert_level = 2;
        snprintf(display_line4, sizeof(display_line4), "Fungal risk: %.1fh wet", pr->leaf_wet_hours);
    }

    return alert_level;
}

/**
 * check_weather_alerts - Evaluate weather-based risks for outdoor plants
 */
static int check_weather_alerts(void)
{
    int alert_level = 0;

    /* Wind danger */
    if (weather.wind_speed_kmh > ALERT_WIND_DANGER) {
        alert_level = 4;
        snprintf(display_line1, sizeof(display_line1), "WIND ALERT: %.0f km/h", weather.wind_speed_kmh);
        snprintf(display_line2, sizeof(display_line2), "Move potted plants inside!");
    }

    /* Frost warning */
    if (weather.temperature_c < 2.0f) {
        if (alert_level < 3) alert_level = 3;
    }

    /* Extreme UV */
    if (weather.uv_index > 10.0f) {
        if (alert_level < 2) alert_level = 2;
    }

    return alert_level;
}

/* ========== IRRIGATION ENGINE ========== */

/**
 * should_skip_irrigation - Check if we should skip watering due to weather
 */
static int should_skip_irrigation(uint8_t plant_id)
{
    /* Skip if it has rained recently */
    if (weather.rain_mm > 5.0f) {
        return 1;
    }

    /* Skip if rain is forecast (detected by falling barometric pressure) */
    if (weather.pressure_hpa < 1005.0f && weather.humidity_pct > 80.0f) {
        return 1;
    }

    /* Don't skip for indoor/grow pod plants (they don't get rain) */
    if (plant_id >= 16) {  /* IDs 16+ are grow pod plants */
        return 0;
    }

    return 0;
}

/**
 * calculate_irrigation_volume - Determine how much water a plant needs
 * Based on pot size, plant type, current moisture deficit, and weather
 */
static uint16_t calculate_irrigation_volume(uint8_t plant_id,
                                             float moisture_deficit_pct,
                                             uint8_t plant_type)
{
    /* Base volume: bring moisture back to 60% from current level */
    /* Assumes ~1ml per 1% moisture per liter of soil */
    float pot_liters = 10.0f;  /* Default 10L pot, configurable per plant */

    if (plant_type == 3) { /* Lettuce in small containers */
        pot_liters = 5.0f;
    } else if (plant_type == 1) { /* Tomatoes in large pots */
        pot_liters = 15.0f;
    }

    float volume_ml = moisture_deficit_pct * pot_liters * 10.0f;

    /* Adjust for hot/windy weather (higher transpiration) */
    if (weather.temperature_c > 30.0f) {
        volume_ml *= 1.3f;
    }
    if (weather.wind_speed_kmh > 20.0f) {
        volume_ml *= 1.15f;
    }

    /* Cap at 500ml per watering event (safety) */
    if (volume_ml > 500.0f) volume_ml = 500.0f;

    return (uint16_t)volume_ml;
}

/**
 * send_irrigation_command - Send watering command to grow pod via mesh
 */
static void send_irrigation_command(uint8_t plant_id, uint16_t volume_ml)
{
    mesh_packet_t cmd;
    cmd.src_id = 0;  /* Hub */
    cmd.dst_id = 0x01;  /* Grow pod (node 1 by convention) */
    cmd.msg_type = 0x06;  /* IRRIGATION_CMD */
    cmd.seq_num = mesh_seq++;

    /* Payload: plant_id(1) + volume_ml(2) + duration_s(2) */
    cmd.payload[0] = plant_id;
    cmd.payload[1] = (volume_ml >> 8) & 0xFF;
    cmd.payload[2] = volume_ml & 0xFF;
    uint16_t duration_s = volume_ml / 10;  /* ~10ml/s flow rate */
    cmd.payload[3] = (duration_s >> 8) & 0xFF;
    cmd.payload[4] = duration_s & 0xFF;

    sx1262_transmit(&cmd);
}

/* ========== TFT DASHBOARD RENDERER ========== */

/**
 * render_dashboard - Draw garden status on TFT display
 * Shows: overall health, per-plant status, weather, alerts
 */
static void render_dashboard(void)
{
    /* Line 1: Garden summary */
    snprintf(display_line1, sizeof(display_line1),
             "Garden: %d plants | Health: %d%%",
             mesh_num_nodes, hub_health_avg);

    /* Line 2: Harvest countdown (next ready plant) */
    int soonest = -1;
    uint16_t min_days = 999;
    for (int i = 0; i < 24; i++) {
        if (harvest_pred[i].days_to_harvest > 0 &&
            harvest_pred[i].days_to_harvest < min_days) {
            min_days = harvest_pred[i].days_to_harvest;
            soonest = i;
        }
    }
    if (soonest >= 0) {
        snprintf(display_line2, sizeof(display_line2),
                 "Next harvest: %d days", min_days);
    } else {
        snprintf(display_line2, sizeof(display_line2), "No harvest data yet");
    }

    /* Line 3: Weather summary */
    snprintf(display_line3, sizeof(display_line3),
             "Out: %.1fC %.0f%%RH %s%.0fkm/h",
             weather.temperature_c, weather.humidity_pct,
             weather.wind_speed_kmh > 20 ? "!" : "",
             weather.wind_speed_kmh);

    /* Line 4: Alerts or all-clear */
    if (hub_alerts_count > 0) {
        snprintf(display_line4, sizeof(display_line4),
                 "ALERTS: %d active", hub_alerts_count);
    } else {
        snprintf(display_line4, sizeof(display_line4), "All plants healthy :)");
    }

    /* TODO: Push display_line1-4 to ILI9341 via SPI */
}

/* ========== MAIN LOOP ========== */

/**
 * hub_mesh_cycle - Run one TDMA frame cycle
 * Slot 0: hub transmits sync + commands
 * Slots 1-24: hub receives from plant sensors, grow pod, weather station
 * Slot 25: alert/retransmit
 */
static void hub_mesh_cycle(void)
{
    /* Slot 0: Hub sync broadcast */
    mesh_send_sync();

    /* Slots 1-25: Listen for node data */
    sx1262_receive(MESH_SLOT_DURATION_MS * MESH_NUM_SLOTS);

    /* Process received packets (handled by radio IRQ callback) */

    /* After full cycle: run irrigation engine */
    for (int i = 0; i < mesh_num_nodes; i++) {
        if (mesh_nodes[i].node_type == 0) {  /* Plant sensor */
            uint8_t pid = mesh_nodes[i].node_id;
            plant_reading_t *pr = &plant_data[pid];

            /* Check if irrigation needed */
            float target_moisture = 60.0f;
            if (pr->soil_moisture_pct < target_moisture - 10.0f) {
                if (!should_skip_irrigation(pid)) {
                    float deficit = target_moisture - pr->soil_moisture_pct;
                    uint16_t vol = calculate_irrigation_volume(pid, deficit, 0);
                    send_irrigation_command(pid, vol);
                }
            }
        }
    }

    /* Update dashboard */
    render_dashboard();
}

void main(void)
{
    printk("UrbanHarvest Hub Node starting...\n");

    /* Initialize SX1262 radio */
    if (sx1262_init() != 0) {
        printk("FATAL: Radio init failed\n");
        return;
    }
    printk("SX1262 radio initialized on 868.0 MHz\n");

    /* Initialize ESP32-C6 WiFi bridge via UART */
    /* TODO: WiFi + MQTT connection */

    /* Initialize BLE */
    /* TODO: GATT server for mobile app */

    /* Initialize display */
    /* TODO: ILI9341 TFT init */

    /* Main loop: run mesh TDMA cycle continuously */
    while (1) {
        hub_mesh_cycle();
        k_msleep(MESH_FRAME_DURATION_MS);
    }
}
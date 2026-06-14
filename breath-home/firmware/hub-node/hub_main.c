/**
 * BreathHome - Hub Node Firmware
 * nRF5340 + ESP32-C6
 * 
 * Central coordinator: mesh network, ML inference, voice alerts,
 * WiFi uplink, BLE for wearable tags, TFT dashboard.
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
#define MESH_MAX_NODES          16
#define MESH_SLOT_DURATION_MS   50
#define MESH_FRAME_DURATION_MS  900
#define MESH_NUM_SLOTS          18
#define MESH_ALERT_SLOT         17

#define WIFI_MQTT_BROKER        "mqtt://breathhome.local:8883"
#define WIFI_MQTT_TOPIC_SENSORS "breathhome/sensors/{room_id}"
#define WIFI_MQTT_TOPIC_ALERTS   "breathhome/alerts"
#define WIFI_MQTT_TOPIC_HVAC    "breathhome/hvac/cmd"

#define AQI_GOOD                0
#define AQI_MODERATE            1
#define AQI_UNHEALTHY_SENSITIVE 2
#define AQI_UNHEALTHY           3
#define AQI_VERY_UNHEALTHY     4
#define AQI_HAZARDOUS           5

#define ALERT_CO2_DANGER       2500   /* ppm */
#define ALERT_PM25_DANGER      150    /* ug/m3 */
#define ALERT_RADON_DANGER     148    /* Bq/m3 (4 pCi/L) */
#define ALERT_RADON_CRITICAL   370    /* Bq/m3 (10 pCi/L) */
#define ALERT_MOLD_HIGH        85     /* percent */
#define ALERT_ASTHMA_HIGH      0.75   /* risk score */

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
    uint8_t  node_type;  /* 0=room_sensor, 1=hvac, 2=wearable_relay */
    uint8_t  slot;
    uint32_t last_seen_ms;
    uint8_t  active;
} mesh_node_info_t;

typedef struct {
    /* Air quality readings */
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;
    float co2;
    float voc_index;
    float nox_index;
    float hcho;
    float temperature;
    float humidity;
    float pressure;
    uint16_t light_lux;
    float radon_bq_m3;
    float mold_risk_pct;
    uint16_t aqi_score;
    uint8_t  aqi_category;
    uint32_t timestamp_ms;
} air_quality_t;

typedef struct {
    /* HVAC controller state */
    uint8_t  vent_positions[8];  /* 0-100% per vent zone */
    uint8_t  purifier_speed;     /* 0=off, 1-4 */
    float    filter_health_pct;
    float    duct_pressure_pa;
    float    supply_air_temp_c;
    float    blower_current_ma;
    uint8_t  relay_states;       /* bitmask: bit0=fan, bit1=bath_exh, bit2=range, bit3=whole_house */
} hvac_state_t;

typedef struct {
    /* Wearable tag data */
    uint8_t  tag_id;
    float    eco2;
    float    tvoc;
    float    temperature;
    float    humidity;
    uint8_t  activity;    /* 0=still, 1=walking, 2=running, 3=sleeping */
    uint8_t  symptom_flag;
    uint8_t  battery_pct;
    float    personal_aqi;
} wearable_data_t;

typedef struct {
    /* Personal exposure tracking */
    float cumulative_pm25_mg;
    float cumulative_co2_ppm_h;
    float cumulative_voc_h;
    uint32_t exposure_start_ms;
    uint8_t asthma_risk_level;  /* 0=low, 1=medium, 2=high, 3=critical */
} exposure_t;

/* ========== GLOBALS ========== */

static mesh_node_info_t mesh_nodes[MESH_MAX_NODES];
static uint8_t mesh_num_nodes = 0;
static uint16_t mesh_seq = 0;

static air_quality_t room_air[MESH_MAX_NODES];  /* per-room air quality */
static hvac_state_t hvac_state;
static wearable_data_t wearable_data[4];  /* up to 4 tags */
static exposure_t tag_exposure[4];

static uint8_t hub_aqi = 0;
static uint8_t hub_aqi_category = 0;

/* TFT display buffer */
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
 * sx1262_read_register - Read register from SX1262
 */
static uint8_t sx1262_read_register(uint16_t addr)
{
    uint8_t cmd[4] = {
        0x1D,  /* ReadRegister */
        (addr >> 8) & 0xFF,
        addr & 0xFF,
        0x00   /* NOP */
    };
    uint8_t rx[4];
    sx1262_spi_transfer(cmd, rx, 4);
    return rx[3];
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
    
    /* Set to standby */
    uint8_t standby_cmd[] = { 0x00 };  /* STDBY_RC */
    sx1262_write_command(0x80, standby_cmd, 1);
    k_msleep(10);
    
    /* Set regulator mode */
    uint8_t reg_mode[] = { 0x01 };  /* DC-DC */
    sx1262_write_command(0x96, reg_mode, 1);
    
    /* Set DIO2 as RF switch control */
    uint8_t dio2_cfg[] = { 0x01 };
    sx1262_write_command(0x9D, dio2_cfg, 1);
    
    /* Set modem to LoRa */
    uint8_t packet_type[] = { 0x01 };  /* LoRa */
    sx1262_write_command(0x8A, packet_type, 1);
    k_msleep(10);
    
    /* Configure LoRa: SF7, 125kHz BW, CR 4/5, low DR opt off */
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
    uint8_t tx_params[] = { 0x0E, 0x02 };  /* +14dBm, ramp 40us */
    sx1262_write_command(0x8E, tx_params, 2);
    
    /* Set sync word */
    uint8_t sync_word[] = { 0xBH, 0x01 };  /* BreathHome sync word */
    sx1262_write_command(0x8E + 4, sync_word, 2);  /* Simplified */
    
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
    
    /* Preamble handled by radio */
    /* Sync word handled by radio */
    buf[idx++] = 0xAA;  /* Preamble marker */
    buf[idx++] = 0xAA;
    buf[idx++] = 0x55;  /* Sync */
    buf[idx++] = 0x55;
    buf[idx++] = sizeof(mesh_packet_t) + 2;  /* LEN */
    buf[idx++] = pkt->src_id;
    buf[idx++] = pkt->dst_id;
    buf[idx++] = pkt->msg_type;
    buf[idx++] = (pkt->seq_num >> 8) & 0xFF;
    buf[idx++] = pkt->seq_num & 0xFF;
    memcpy(&buf[idx], pkt->payload, 48);
    idx += 48;
    
    /* CRC16 */
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
    
    /* Set buffer */
    uint8_t set_buf_cmd[] = { 0x00, 0x00, (idx >> 8) & 0xFF, idx & 0xFF };
    sx1262_write_command(0x92, set_buf_cmd, 4);
    
    /* Set TX */
    uint8_t tx_cmd[] = { 0x00, 0x00, 0x00, 0x00 };  /* timeout = infinite */
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

/**
 * mesh_assign_slots - Assign TDMA slots to registered nodes
 */
static void mesh_assign_slots(void)
{
    uint8_t slot = 1;  /* Slot 0 is hub */
    
    for (int i = 0; i < MESH_MAX_NODES; i++) {
        if (mesh_nodes[i].active) {
            mesh_nodes[i].slot = slot++;
        }
    }
}

/**
 * mesh_register_node - Register a new node in the mesh
 */
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
    return -1;  /* No slots available */
}

/**
 * mesh_send_sync - Hub broadcasts sync packet in slot 0
 */
static void mesh_send_sync(void)
{
    mesh_packet_t sync;
    sync.src_id = 0;  /* Hub */
    sync.dst_id = 0xFF;  /* Broadcast */
    sync.msg_type = 0x04;  /* COMMAND */
    sync.seq_num = mesh_seq++;
    
    /* Payload: slot assignments + current time + AQI summary */
    sync.payload[0] = mesh_num_nodes;
    for (int i = 0; i < mesh_num_nodes && i < 16; i++) {
        sync.payload[1 + i * 2] = mesh_nodes[i].node_id;
        sync.payload[2 + i * 2] = mesh_nodes[i].slot;
    }
    /* Hub AQI summary at end */
    sync.payload[47] = hub_aqi;
    sync.payload[48] = hub_aqi_category;
    
    sx1262_transmit(&sync);
}

/* ========== AQI CALCULATION ========== */

/**
 * calculate_aqi - Compute composite AQI from multi-sensor readings
 * Uses EPA breakpoint tables adapted for real-time indoor monitoring
 */
static uint16_t calculate_aqi(const air_quality_t *aq)
{
    /* PM2.5 sub-index */
    float pm25_index;
    if (aq->pm2_5 <= 12.0f) {
        pm25_index = 50.0f * aq->pm2_5 / 12.0f;
    } else if (aq->pm2_5 <= 35.4f) {
        pm25_index = 50.0f + 50.0f * (aq->pm2_5 - 12.1f) / (35.4f - 12.1f);
    } else if (aq->pm2_5 <= 55.4f) {
        pm25_index = 100.0f + 50.0f * (aq->pm2_5 - 35.5f) / (55.4f - 35.5f);
    } else if (aq->pm2_5 <= 150.4f) {
        pm25_index = 150.0f + 50.0f * (aq->pm2_5 - 55.5f) / (150.4f - 55.5f);
    } else {
        pm25_index = 200.0f + 100.0f * (aq->pm2_5 - 150.5f) / (250.4f - 150.5f);
    }
    
    /* CO2 sub-index (adapted for indoor) */
    float co2_index;
    if (aq->co2 <= 800.0f) {
        co2_index = 50.0f * aq->co2 / 800.0f;
    } else if (aq->co2 <= 1200.0f) {
        co2_index = 50.0f + 50.0f * (aq->co2 - 800.0f) / 400.0f;
    } else if (aq->co2 <= 1800.0f) {
        co2_index = 100.0f + 50.0f * (aq->co2 - 1200.0f) / 600.0f;
    } else if (aq->co2 <= 2500.0f) {
        co2_index = 150.0f + 50.0f * (aq->co2 - 1800.0f) / 700.0f;
    } else {
        co2_index = 200.0f + 100.0f * (aq->co2 - 2500.0f) / 2500.0f;
    }
    
    /* VOC sub-index */
    float voc_index;
    if (aq->voc_index <= 100.0f) {
        voc_index = 50.0f * aq->voc_index / 100.0f;
    } else if (aq->voc_index <= 200.0f) {
        voc_index = 50.0f + 50.0f * (aq->voc_index - 100.0f) / 100.0f;
    } else if (aq->voc_index <= 300.0f) {
        voc_index = 100.0f + 50.0f * (aq->voc_index - 200.0f) / 100.0f;
    } else {
        voc_index = 150.0f + 50.0f * (aq->voc_index - 300.0f) / 200.0f;
    }
    
    /* Composite: max of all sub-indices (EPA method) */
    float composite = pm25_index;
    if (co2_index > composite) composite = co2_index;
    if (voc_index > composite) composite = voc_index;
    
    /* Add formaldehyde penalty */
    if (aq->hcho > 0.08f) {
        composite += 20.0f * (aq->hcho / 0.08f);
    }
    
    /* Add mold risk penalty */
    if (aq->mold_risk_pct > 60.0f) {
        composite += 15.0f * (aq->mold_risk_pct / 60.0f);
    }
    
    if (composite > 500.0f) composite = 500.0f;
    if (composite < 0.0f) composite = 0.0f;
    
    return (uint16_t)composite;
}

/**
 * get_aqi_category - Classify AQI score into category
 */
static uint8_t get_aqi_category(uint16_t aqi)
{
    if (aqi <= 50)  return AQI_GOOD;
    if (aqi <= 100) return AQI_MODERATE;
    if (aqi <= 150) return AQI_UNHEALTHY_SENSITIVE;
    if (aqi <= 200) return AQI_UNHEALTHY;
    if (aqi <= 300) return AQI_VERY_UNHEALTHY;
    return AQI_HAZARDOUS;
}

/* ========== ALERT ENGINE ========== */

/**
 * check_alerts - Evaluate all alert conditions for a room
 * Returns alert level: 0=none, 1=info, 2=warning, 3=danger, 4=critical
 */
static int check_alerts(const air_quality_t *aq, uint8_t room_id)
{
    int alert_level = 0;
    
    /* CO2 alerts */
    if (aq->co2 > ALERT_CO2_DANGER) {
        alert_level = 4;  /* Critical */
        snprintf(display_line1, sizeof(display_line1), "CRITICAL: CO2 %.0fppm", aq->co2);
        snprintf(display_line2, sizeof(display_line2), "Room %d - EVACUATE", room_id);
    } else if (aq->co2 > 1800.0f) {
        if (alert_level < 3) alert_level = 3;
    } else if (aq->co2 > 1200.0f) {
        if (alert_level < 2) alert_level = 2;
    }
    
    /* PM2.5 alerts */
    if (aq->pm2_5 > ALERT_PM25_DANGER) {
        if (alert_level < 4) alert_level = 4;
    } else if (aq->pm2_5 > 55.0f) {
        if (alert_level < 3) alert_level = 3;
    } else if (aq->pm2_5 > 35.0f) {
        if (alert_level < 2) alert_level = 2;
    }
    
    /* Radon alerts */
    if (aq->radon_bq_m3 > ALERT_RADON_CRITICAL) {
        if (alert_level < 4) alert_level = 4;
    } else if (aq->radon_bq_m3 > ALERT_RADON_DANGER) {
        if (alert_level < 3) alert_level = 3;
    }
    
    /* Mold risk alerts */
    if (aq->mold_risk_pct > ALERT_MOLD_HIGH) {
        if (alert_level < 3) alert_level = 3;
    } else if (aq->mold_risk_pct > 60.0f) {
        if (alert_level < 2) alert_level = 2;
    }
    
    return alert_level;
}

/**
 * send_hvac_command - Send ventilation command to HVAC controller
 */
static void send_hvac_command(uint8_t command, uint8_t room_id, uint8_t value)
{
    mesh_packet_t cmd;
    cmd.src_id = 0;  /* Hub */
    cmd.dst_id = 0x01;  /* HVAC controller (always node 1) */
    cmd.msg_type = 0x04;  /* HVAC_COMMAND */
    cmd.seq_num = mesh_seq++;
    cmd.payload[0] = command;  /* 0=vent_pos, 1=purifier_speed, 2=fan_on, 3=fan_off */
    cmd.payload[1] = room_id;
    cmd.payload[2] = value;
    
    sx1262_transmit(&cmd);
}

/* ========== BLE GATT SERVER (WEARABLE TAG) ========== */

#define BLE_SERVICE_BREATHHOME  BT_UUID_DECLARE_16(0xBREA)
#define BLE_CHAR_AQI           BT_UUID_DECLARE_16(0xBH01)
#define BLE_CHAR_SYMPTOM       BT_UUID_DECLARE_16(0xBH02)
#define BLE_CHAR_ACTIVITY      BT_UUID_DECLARE_16(0xBH03)
#define BLE_CHAR_BATTERY      BT_UUID_DECLARE_16(0xBH04)
#define BLE_CHAR_VIBRATE      BT_UUID_DECLARE_16(0xBH05)
#define BLE_CHAR_CONFIG       BT_UUID_DECLARE_16(0xBH06)

static uint8_t ble_aqi_data[4] = {0};  /* AQI, PM2.5, CO2, VOC */
static uint8_t ble_vibrate_cmd = 0;
static uint8_t ble_symptom_data = 0;

static ssize_t ble_read_aqi(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, ble_aqi_data, sizeof(ble_aqi_data));
}

static ssize_t ble_write_symptom(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len == 1) {
        ble_symptom_data = ((uint8_t *)buf)[0];
    }
    return len;
}

static ssize_t ble_write_vibrate(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len == 1) {
        ble_vibrate_cmd = ((uint8_t *)buf)[0];
    }
    return len;
}

BT_GATT_SERVICE_DEFINE(ble_breathhome_svc,
    BT_GATT_PRIMARY_SERVICE(BLE_SERVICE_BREATHHOME),
    BT_GATT_CHARACTERISTIC(BLE_CHAR_AQI, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, ble_read_aqi, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BLE_CHAR_SYMPTOM, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, ble_write_symptom, NULL),
    BT_GATT_CHARACTERISTIC(BLE_CHAR_ACTIVITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, NULL, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BLE_CHAR_BATTERY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, NULL, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BLE_CHAR_VIBRATE, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, ble_write_vibrate, NULL),
    BT_GATT_CHARACTERISTIC(BLE_CHAR_CONFIG, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, NULL, NULL),
);

/* ========== SENSORS (HUB ON-BOARD) ========== */

/**
 * read_hub_sensors - Read hub's own air quality sensors
 */
static void read_hub_sensors(air_quality_t *aq)
{
    /* Read SCD41 CO2 */
    uint8_t scd41_cmd[2] = {0x21, 0x61};  /* Read measurement */
    uint8_t scd41_data[9];
    /* i2c_write then i2c_read for SCD41 - simplified */
    /* co2 = (data[0] << 8) | data[1] */
    aq->co2 = 415.0f;  /* Placeholder - real driver needed */
    
    /* Read SPS30 particulate */
    uint8_t sps30_cmd[2] = {0x02, 0x02};  /* Read measured values */
    uint8_t sps30_data[40];
    /* i2c_write then i2c_read for SPS30 */
    aq->pm1_0 = 5.2f;
    aq->pm2_5 = 8.7f;
    aq->pm4_0 = 12.1f;
    aq->pm10 = 15.3f;
    
    /* Read BME688 environment */
    /* temperature, humidity, pressure, IAQ */
    aq->temperature = 22.5f;
    aq->humidity = 45.0f;
    aq->pressure = 1013.25f;
    
    /* Read SGP41 VOC/NOx */
    aq->voc_index = 85.0f;
    aq->nox_index = 12.0f;
    
    /* Read SFA30 formaldehyde */
    aq->hcho = 0.023f;
    
    /* Compute AQI */
    aq->aqi_score = calculate_aqi(aq);
    aq->aqi_category = get_aqi_category(aq->aqi_score);
}

/* ========== DISPLAY UPDATE ========== */

static const struct device *display_dev;

/**
 * update_display - Render air quality dashboard on TFT
 */
static void update_display(const air_quality_t *hub_aq, uint8_t num_rooms)
{
    /* Format display lines */
    snprintf(display_line1, sizeof(display_line1), "AQI: %d (%s)", 
             hub_aq->aqi_category < 4 ? hub_aq->aqi_score : 300,
             hub_aq->aqi_category <= 1 ? "GOOD" :
             hub_aq->aqi_category == 2 ? "MODERATE" :
             hub_aq->aqi_category == 3 ? "UNHEALTHY" : "HAZARDOUS");
    
    snprintf(display_line2, sizeof(display_line2), "PM2.5:%.1f CO2:%.0f VOC:%.0f",
             hub_aq->pm2_5, hub_aq->co2, hub_aq->voc_index);
    
    snprintf(display_line3, sizeof(display_line3), "Rooms: %d | Tags: %d",
             num_rooms, 0);
    
    snprintf(display_line4, sizeof(display_line4), "Temp:%.1fC Hum:%.0f%% Press:%.0f",
             hub_aq->temperature, hub_aq->humidity, hub_aq->pressure);
    
    /* Render to display - using display_write API */
    struct display_capabilities caps;
    display_get_capabilities(display_dev, &caps);
    
    /* In a real implementation, use lvgl or custom framebuffer renderer */
    /* For now, we update the display with text lines */
}

/* ========== MAIN LOOP ========== */

/**
 * mesh_tdma_thread - Thread for mesh TDMA scheduling
 */
static void mesh_tdma_thread(void *p1, void *p2, void *p3)
{
    int64_t frame_start;
    
    while (1) {
        frame_start = k_uptime_get();
        
        /* Slot 0: Hub transmits sync + commands */
        mesh_send_sync();
        k_msleep(MESH_SLOT_DURATION_MS);
        
        /* Slots 1-16: Listen for node transmissions */
        for (int slot = 1; slot <= 16; slot++) {
            sx1262_receive(MESH_SLOT_DURATION_MS);
            k_msleep(MESH_SLOT_DURATION_MS);
            
            /* Process received packet if available */
            /* In real implementation, IRQ handler would populate rx_buffer */
        }
        
        /* Slot 17: Alert/control slot */
        sx1262_receive(MESH_SLOT_DURATION_MS);
        k_msleep(MESH_SLOT_DURATION_MS);
        
        /* Wait for next frame */
        int64_t elapsed = k_uptime_delta(&frame_start);
        if (elapsed < MESH_FRAME_DURATION_MS) {
            k_msleep(MESH_FRAME_DURATION_MS - elapsed);
        }
    }
}

/**
 * sensor_thread - Thread for reading hub sensors
 */
static void sensor_thread(void *p1, void *p2, void *p3)
{
    air_quality_t hub_sensors;
    
    while (1) {
        read_hub_sensors(&hub_sensors);
        
        /* Update global */
        room_air[0] = hub_sensors;
        hub_aqi = (uint8_t)(hub_sensors.aqi_score > 255 ? 255 : hub_sensors.aqi_score);
        hub_aqi_category = hub_sensors.aqi_category;
        
        /* Check alerts */
        int alert = check_alerts(&hub_sensors, 0);
        if (alert >= 3) {
            /* Send HVAC ventilation command */
            send_hvac_command(2, 0, 1);  /* Turn on fan for room 0 */
        }
        
        /* Update BLE AQI characteristic */
        ble_aqi_data[0] = hub_aqi;
        ble_aqi_data[1] = (uint8_t)(hub_sensors.pm2_5 > 255 ? 255 : hub_sensors.pm2_5);
        ble_aqi_data[2] = (uint8_t)(hub_sensors.co2 / 10);  /* Scale to fit byte */
        ble_aqi_data[3] = (uint8_t)(hub_sensors.voc_index > 255 ? 255 : hub_sensors.voc_index);
        
        k_msleep(5000);  /* Read sensors every 5 seconds */
    }
}

/**
 * display_thread - Thread for updating TFT display
 */
static void display_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        update_display(&room_air[0], mesh_num_nodes);
        k_msleep(1000);  /* Update display every second */
    }
}

/**
 * mqtt_thread - Thread for WiFi/MQTT cloud uplink
 */
static void mqtt_thread(void *p1, void *p2, void *p3)
{
    /* WiFi/MQTT handled by ESP32-C6 via UART */
    /* Send sensor data, receive commands */
    
    while (1) {
        /* Send hub sensor data to cloud */
        /* JSON format for MQTT publish */
        char mqtt_payload[256];
        snprintf(mqtt_payload, sizeof(mqtt_payload),
                 "{\"room\":0,\"pm2_5\":%.1f,\"co2\":%.0f,\"voc\":%.0f,\"aqi\":%d,\"t\":%.1f,\"rh\":%.0f}",
                 room_air[0].pm2_5, room_air[0].co2, room_air[0].voc_index,
                 room_air[0].aqi_score, room_air[0].temperature, room_air[0].humidity);
        
        /* Send to ESP32-C6 via UART for MQTT publish */
        /* uart_send(uart_dev, mqtt_payload, strlen(mqtt_payload)); */
        
        k_msleep(10000);  /* Publish every 10 seconds */
    }
}

/* Thread definitions */
K_THREAD_DEFINE(mesh_tid, 2048, mesh_tdma_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(display_tid, 1024, display_thread, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(mqtt_tid, 2048, mqtt_thread, NULL, NULL, NULL, 1, 0, 0);

/**
 * main - Entry point
 */
int main(void)
{
    /* Initialize GPIOs */
    sx1262_nss = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1262_nss), gpios);
    sx1262_busy = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1262_busy), gpios);
    sx1262_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1262_irq), gpios);
    sx1262_nrst = GPIO_DT_SPEC_GET(DT_NODELABEL(sx1262_nrst), gpios);
    
    gpio_pin_configure_dt(&sx1262_nss, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&sx1262_busy, GPIO_INPUT);
    gpio_pin_configure_dt(&sx1262_irq, GPIO_INPUT);
    gpio_pin_configure_dt(&sx1262_nrst, GPIO_OUTPUT_HIGH);
    
    /* Initialize SPI */
    spi_dev = DEVICE_DT_GET(SX1262_SPI);
    
    /* Initialize SX1262 */
    sx1262_init();
    
    /* Initialize display */
    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    
    /* Initialize BLE */
    bt_enable(NULL);
    
    /* Initialize I2C sensors */
    /* SCD41, SPS30, BME688, SGP41, SFA30 */
    
    /* Start threads */
    /* Threads already started by K_THREAD_DEFINE */
    
    printk("BreathHome Hub started\n");
    printk("Mesh TDMA scheduler running\n");
    printk("BLE GATT server active\n");
    
    return 0;
}
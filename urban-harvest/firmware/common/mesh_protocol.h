/**
 * UrbanHarvest - Shared Mesh Protocol
 * Common definitions and utilities for Sub-GHz LoRa mesh
 * Used by all nodes: Hub, Plant Sensor, Grow Pod, Weather Station
 */

#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ========== CONSTANTS ========== */

#define MESH_MAX_NODES          24
#define MESH_SLOT_DURATION_MS   100
#define MESH_FRAME_DURATION_MS  2600
#define MESH_NUM_SLOTS          26
#define MESH_ALERT_SLOT         25

/* Sync word for UrbanHarvest mesh network */
#define MESH_SYNC_WORD          0xUH01

/* Maximum payload size */
#define MESH_PAYLOAD_SIZE       48

/* Node types */
#define NODE_TYPE_HUB           0x00
#define NODE_TYPE_PLANT_SENSOR  0x01
#define NODE_TYPE_GROW_POD     0x02
#define NODE_TYPE_WEATHER       0x03

/* ========== PACKET TYPES ========== */

#define MSG_SOIL_DATA           0x01
#define MSG_LIGHT_DATA          0x02
#define MSG_LEAF_WETNESS        0x03
#define MSG_GROW_POD_STATUS     0x04
#define MSG_WEATHER_DATA        0x05
#define MSG_IRRIGATION_CMD      0x06
#define MSG_NUTRIENT_CMD        0x07
#define MSG_LIGHT_CMD           0x08
#define MSG_DISEASE_ALERT       0x09
#define MSG_ACK                 0x0A
#define MSG_OTA_BLOCK           0x0B
#define MSG_HARVEST_PREDICT     0x0C
#define MSG_HEARTBEAT          0x0D
#define MSG_CALIBRATION         0x0E
#define MSG_DANGER_ALERT        0x0F
#define MSG_CAMERA_READY        0x10

/* ========== ALERT TYPES ========== */

#define ALERT_SOIL_DRY          0x00
#define ALERT_SOIL_WET          0x01
#define ALERT_EC_HIGH           0x02
#define ALERT_TEMP_LOW          0x03
#define ALERT_TEMP_HIGH         0x04
#define ALERT_DISEASE          0x05
#define ALERT_PUMP_FAILURE     0x06
#define ALERT_WIND              0x07
#define ALERT_FROST             0x08
#define ALERT_LEAK              0x09

/* ========== IRRIGATION COMMANDS ========== */

#define IRR_CMD_WATER_PLANT     0x00
#define IRR_CMD_STOP_PUMP       0x01
#define IRR_CMD_SET_SCHEDULE    0x02

/* ========== LIGHT COMMANDS ========== */

#define LIGHT_CMD_SET_SPECTRUM  0x00
#define LIGHT_CMD_SET_SCHEDULE  0x01
#define LIGHT_CMD_VEGETATIVE    0x02
#define LIGHT_CMD_FLOWERING     0x03
#define LIGHT_CMD_SEEDLING      0x04

/* ========== HEALTH CATEGORIES ========== */

#define HEALTH_THRIVING         0
#define HEALTH_GOOD             1
#define HEALTH_STRESSED         2
#define HEALTH_CRITICAL         3
#define HEALTH_DEAD             4

/* ========== DISEASE CLASSES ========== */

#define DISEASE_HEALTHY         0
#define DISEASE_POWDERY_MILDEW  1
#define DISEASE_DOWNY_MILDEW   2
#define DISEASE_LEAF_SPOT_BACT  3
#define DISEASE_LEAF_SPOT_FUNG  4
#define DISEASE_NUTRIENT_DEF   5

/* ========== PLANT TYPES ========== */

#define PLANT_GENERIC           0x00
#define PLANT_TOMATO            0x01
#define PLANT_BASIL             0x02
#define PLANT_LETTUCE           0x03
#define PLANT_PEPPER            0x04
#define PLANT_MINT              0x05
#define PLANT_MICROGREENS       0x06
#define PLANT_HERB_PROP        0x07
#define PLANT_CUCUMBER          0x08
#define PLANT_STRAWBERRY        0x09
#define PLANT_SPINACH           0x0A

/* ========== BLE SERVICE UUIDS ========== */

#define BLE_UUID_URBANHARVEST    0xUH01
#define BLE_UUID_GARDEN_SUMMARY  0xUH11
#define BLE_UUID_PLANT_DETAIL   0xUH12
#define BLE_UUID_IRRIGATION     0xUH13
#define BLE_UUID_LIGHT_CTRL     0xUH14
#define BLE_UUID_ALERT_CONFIG   0xUH15
#define BLE_UUID_WEATHER        0xUH16
#define BLE_UUID_VOICE_CMD      0xUH17

/* ========== PACKET STRUCTURE ========== */

typedef struct __attribute__((packed)) {
    uint8_t  preamble[2];     /* 0xAA, 0x55 */
    uint8_t  len;             /* Total packet length */
    uint8_t  src_id;          /* Source node ID (0=hub, 1-24=plant sensors, 0x40=grow pod, 0x80=weather) */
    uint8_t  dst_id;          /* Destination (0xFF=broadcast) */
    uint8_t  msg_type;        /* Message type */
    uint16_t seq_num;         /* Sequence number */
    uint8_t  payload[MESH_PAYLOAD_SIZE]; /* Payload */
    uint16_t crc16;           /* CRC16-CCITT */
} urbanharvest_packet_t;

/* ========== SOIL DATA PAYLOAD ========== */

typedef struct __attribute__((packed)) {
    uint8_t  moisture_pct;     /* 0-100% */
    uint8_t  ec_x10_hi;       /* EC mS/cm × 10, high byte */
    uint8_t  ec_x10_lo;       /* EC mS/cm × 10, low byte */
    uint8_t  temp_c_offset;   /* °C + 40 (to fit -40 to +85 in uint8) */
    uint8_t  par_x10_hi;      /* PAR µmol/m²/s × 10, high byte */
    uint8_t  par_x10_lo;      /* PAR µmol/m²/s × 10, low byte */
    uint8_t  health_index;    /* 0-100 */
    uint8_t  leaf_wet_pct;    /* 0-100% */
    uint8_t  battery_x20;    /* Voltage × 20 */
    uint8_t  health_category; /* 0-4 */
    uint8_t  leaf_wet_h_x10_hi; /* Leaf wetness hours × 10, high byte */
    uint8_t  leaf_wet_h_x10_lo; /* Leaf wetness hours × 10, low byte */
} soil_data_payload_t;

/* ========== WEATHER DATA PAYLOAD ========== */

typedef struct __attribute__((packed)) {
    int16_t  temp_x10;        /* °C × 10 */
    uint16_t rh_x10;          /* %RH × 10 */
    uint16_t pressure_x10;    /* hPa × 10 */
    uint16_t wind_x10;        /* km/h × 10 */
    uint8_t  wind_dir;        /* 0-7 */
    uint16_t rain_x100;       /* mm × 100 */
    uint16_t uv_x10;          /* UV index × 10 */
    uint16_t light_lux;       /* lux */
    uint8_t  solar_v_x20;     /* V × 20 */
    uint8_t  bat_v_x20;      /* V × 20 */
    uint8_t  bat_soc;         /* 0-100% */
    uint8_t  pressure_trend;  /* 1=rising, 2=steady, 3=falling */
    uint8_t  rain_predicted;  /* 0/1 */
} weather_data_payload_t;

/* ========== GROW POD STATUS PAYLOAD ========== */

typedef struct __attribute__((packed)) {
    uint8_t  pump_running;
    uint16_t nutrient_a_ml_x10;
    uint16_t nutrient_b_ml_x10;
    uint16_t ph_dose_ml_x10;
    uint8_t  fan_speed_pct;
    uint8_t  heater_on;
    uint8_t  humidifier_on;
    uint8_t  light_on;
    uint8_t  red_pwm;
    uint8_t  blue_pwm;
    uint8_t  white_pwm;
    uint8_t  far_red_pwm;
    uint8_t  water_temp_c_offset; /* °C + 40 */
    uint8_t  disease_class;       /* 0-5 */
    uint8_t  disease_conf_pct;   /* 0-100% */
} growpod_status_payload_t;

/* ========== IRRIGATION COMMAND PAYLOAD ========== */

typedef struct __attribute__((packed)) {
    uint8_t  plant_id;
    uint16_t volume_ml;
    uint16_t duration_s;
} irrigation_cmd_payload_t;

/* ========== DISEASE ALERT PAYLOAD ========== */

typedef struct __attribute__((packed)) {
    uint8_t  plant_id;
    uint8_t  disease_class;
    uint8_t  confidence_pct;
    uint16_t image_id;
} disease_alert_payload_t;

/* ========== DANGER ALERT PAYLOAD ========== */

typedef struct __attribute__((packed)) {
    uint8_t  alert_type;
    uint8_t  node_id;
    float    value;
} danger_alert_payload_t;

/* ========== CRC16-CCITT ========== */

/**
 * crc16_ccitt - Calculate CRC16-CCITT for packet integrity
 * Polynomial: 0x1021, Init: 0xFFFF
 */
static inline uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * mesh_build_packet - Build a mesh packet with CRC
 */
static inline void mesh_build_packet(urbanharvest_packet_t *pkt,
                                      uint8_t src_id, uint8_t dst_id,
                                      uint8_t msg_type, uint16_t seq_num,
                                      const uint8_t *payload, uint8_t payload_len)
{
    pkt->preamble[0] = 0xAA;
    pkt->preamble[1] = 0x55;
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->msg_type = msg_type;
    pkt->seq_num = seq_num;

    memset(pkt->payload, 0, MESH_PAYLOAD_SIZE);
    if (payload_len > 0 && payload_len <= MESH_PAYLOAD_SIZE) {
        memcpy(pkt->payload, payload, payload_len);
    }

    pkt->len = sizeof(urbanharvest_packet_t);
    pkt->crc16 = crc16_ccitt((const uint8_t *)&pkt->src_id,
                              sizeof(urbanharvest_packet_t) - sizeof(pkt->preamble) - sizeof(pkt->crc16));
}

/**
 * mesh_verify_packet - Verify packet CRC
 */
static inline int mesh_verify_packet(const urbanharvest_packet_t *pkt)
{
    if (pkt->preamble[0] != 0xAA || pkt->preamble[1] != 0x55) {
        return -1;
    }

    uint16_t computed_crc = crc16_ccitt(
        (const uint8_t *)&pkt->src_id,
        sizeof(urbanharvest_packet_t) - sizeof(pkt->preamble) - sizeof(pkt->crc16)
    );

    return (computed_crc == pkt->crc16) ? 0 : -2;
}

/* ========== PLANT HEALTH CALCULATION (shared) ========== */

/**
 * calculate_plant_health_from_readings - Compute plant health index
 * Returns 0-100: 0=dead, 50=stressed, 100=thriving
 * Uses plant-type-specific thresholds
 */
static inline uint8_t calculate_plant_health_from_readings(
    float moisture_pct, float ec_ms_cm, float temp_c,
    float par_umol, float leaf_wet_hours, uint8_t plant_type)
{
    float health = 100.0f;

    /* Moisture targets per plant type */
    float moist_low = 40.0f, moist_high = 65.0f;
    if (plant_type == PLANT_LETTUCE || plant_type == PLANT_SPINACH) {
        moist_low = 50.0f; moist_high = 75.0f;
    } else if (plant_type == PLANT_BASIL || plant_type == PLANT_MINT) {
        moist_low = 40.0f; moist_high = 70.0f;
    } else if (plant_type == PLANT_TOMATO || plant_type == PLANT_PEPPER) {
        moist_low = 35.0f; moist_high = 65.0f;
    }

    /* Moisture penalty */
    if (moisture_pct < moist_low) {
        health -= (moist_low - moisture_pct) * 1.5f;
    } else if (moisture_pct > moist_high) {
        health -= (moisture_pct - moist_high) * 1.2f;
    }

    /* EC penalty */
    if (ec_ms_cm < 0.5f) health -= 20.0f;
    else if (ec_ms_cm > 2.5f) health -= (ec_ms_cm - 2.5f) * 25.0f;

    /* Temperature penalty */
    if (temp_c < 10.0f) health -= 30.0f;
    else if (temp_c < 18.0f) health -= (18.0f - temp_c) * 2.0f;
    else if (temp_c > 35.0f) health -= 30.0f;
    else if (temp_c > 26.0f) health -= (temp_c - 26.0f) * 1.5f;

    /* Light penalty */
    if (par_umol < 50.0f) health -= 40.0f;
    else if (par_umol < 200.0f) health -= (200.0f - par_umol) * 0.15f;

    /* Leaf wetness (fungal risk) */
    if (leaf_wet_hours > 6.0f) {
        health -= (leaf_wet_hours - 6.0f) * 5.0f;
    }

    if (health > 100.0f) health = 100.0f;
    if (health < 0.0f) health = 0.0f;

    return (uint8_t)health;
}

/**
 * get_health_category - Classify health index into category
 */
static inline uint8_t get_health_category(uint8_t health)
{
    if (health >= 80) return HEALTH_THRIVING;
    if (health >= 60) return HEALTH_GOOD;
    if (health >= 35) return HEALTH_STRESSED;
    if (health >= 10) return HEALTH_CRITICAL;
    return HEALTH_DEAD;
}

/**
 * get_health_label - Get human-readable health category label
 */
static inline const char *get_health_label(uint8_t category)
{
    switch (category) {
        case HEALTH_THRIVING:  return "Thriving";
        case HEALTH_GOOD:      return "Good";
        case HEALTH_STRESSED:  return "Stressed";
        case HEALTH_CRITICAL:  return "Critical";
        case HEALTH_DEAD:      return "Dead";
        default:              return "Unknown";
    }
}

/**
 * get_disease_label - Get human-readable disease class label
 */
static inline const char *get_disease_label(uint8_t disease_class)
{
    switch (disease_class) {
        case DISEASE_HEALTHY:          return "Healthy";
        case DISEASE_POWDERY_MILDEW:   return "Powdery Mildew";
        case DISEASE_DOWNY_MILDEW:     return "Downy Mildew";
        case DISEASE_LEAF_SPOT_BACT:   return "Bacterial Leaf Spot";
        case DISEASE_LEAF_SPOT_FUNG:   return "Fungal Leaf Spot";
        case DISEASE_NUTRIENT_DEF:     return "Nutrient Deficiency";
        default:                      return "Unknown";
    }
}

/**
 * get_plant_type_label - Get human-readable plant type label
 */
static inline const char *get_plant_type_label(uint8_t plant_type)
{
    switch (plant_type) {
        case PLANT_GENERIC:     return "Generic";
        case PLANT_TOMATO:      return "Tomato";
        case PLANT_BASIL:       return "Basil";
        case PLANT_LETTUCE:     return "Lettuce";
        case PLANT_PEPPER:      return "Pepper";
        case PLANT_MINT:        return "Mint";
        case PLANT_MICROGREENS: return "Microgreens";
        case PLANT_HERB_PROP:   return "Herb Propagation";
        case PLANT_CUCUMBER:    return "Cucumber";
        case PLANT_STRAWBERRY:  return "Strawberry";
        case PLANT_SPINACH:     return "Spinach";
        default:               return "Unknown";
    }
}

/**
 * get_wind_dir_label - Get human-readable wind direction
 */
static inline const char *get_wind_dir_label(uint8_t dir)
{
    static const char *dirs[8] = {"N","NE","E","SE","S","SW","W","NW"};
    if (dir > 7) return "?";
    return dirs[dir];
}

/* ========== RAIN PREDICTION (shared) ========== */

/**
 * predict_rain_simple - Simple barometric rain prediction
 * Returns 1 if rain likely, 0 if not
 * Based on: falling pressure + high humidity + low pressure
 */
static inline int predict_rain_simple(float pressure_hpa, int8_t pressure_trend,
                                       float humidity_pct)
{
    if (pressure_trend < 0 && pressure_hpa < 1010.0f && humidity_pct > 75.0f) {
        return 1;
    }
    return 0;
}

#endif /* MESH_PROTOCOL_H */
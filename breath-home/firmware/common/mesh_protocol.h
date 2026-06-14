/**
 * BreathHome - Shared Mesh Protocol
 * Common definitions and utilities for Sub-GHz LoRa mesh
 * Used by all nodes: Hub, Room Sensor, HVAC Controller
 */

#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ========== CONSTANTS ========== */

#define MESH_MAX_NODES          16
#define MESH_SLOT_DURATION_MS   50
#define MESH_FRAME_DURATION_MS  900
#define MESH_NUM_SLOTS          18
#define MESH_ALERT_SLOT         17

/* Sync word for BreathHome mesh network */
#define MESH_SYNC_WORD          0xBHEA

/* Maximum payload size */
#define MESH_PAYLOAD_SIZE       48

/* Node types */
#define NODE_TYPE_HUB           0x00
#define NODE_TYPE_ROOM_SENSOR   0x01
#define NODE_TYPE_HVAC_CTRL    0x02
#define NODE_TYPE_WEARABLE_RELAY 0x03

/* ========== PACKET TYPES ========== */

#define MSG_AIR_QUALITY         0x01
#define MSG_RADON_DATA         0x02
#define MSG_MOLD_RISK          0x03
#define MSG_HVAC_COMMAND       0x04
#define MSG_HVAC_STATUS        0x05
#define MSG_FILTER_ALERT       0x06
#define MSG_ACK                0x07
#define MSG_OTA_BLOCK          0x08
#define MSG_DANGER_ALERT       0x09
#define MSG_CALIBRATION        0x0A
#define MSG_HEARTBEAT          0x0B
#define MSG_EXPOSURE_DATA      0x0C

/* ========== ALERT TYPES ========== */

#define ALERT_PM25              0x00
#define ALERT_CO2               0x01
#define ALERT_VOC               0x02
#define ALERT_HCHO              0x03
#define ALERT_RADON             0x04
#define ALERT_MOLD              0x05

/* ========== HVAC COMMANDS ========== */

#define HVAC_CMD_VENT_POSITION  0x00
#define HVAC_CMD_PURIFIER_SPEED 0x01
#define HVAC_CMD_FAN_ON         0x02
#define HVAC_CMD_FAN_OFF        0x03
#define HVAC_CMD_RANGE_HOOD_ON  0x04
#define HVAC_CMD_RANGE_HOOD_OFF 0x05
#define HVAC_CMD_BATH_EXH_ON   0x06
#define HVAC_CMD_BATH_EXH_OFF  0x07

/* ========== AQI CATEGORIES ========== */

#define AQI_GOOD                0
#define AQI_MODERATE            1
#define AQI_UNHEALTHY_SENSITIVE 2
#define AQI_UNHEALTHY           3
#define AQI_VERY_UNHEALTHY     4
#define AQI_HAZARDOUS           5

/* ========== BLE SERVICE UUIDS ========== */

#define BLE_UUID_BREATHHOME     0xBREA
#define BLE_UUID_AQI             0xBH01
#define BLE_UUID_SYMPTOM        0xBH02
#define BLE_UUID_ACTIVITY       0xBH03
#define BLE_UUID_BATTERY        0xBH04
#define BLE_UUID_VIBRATE        0xBH05
#define BLE_UUID_CONFIG         0xBH06

/* ========== WEARABLE TAG DEFINITIONS ========== */

#define TAG_SYMPTOM_NONE        0x00
#define TAG_SYMPTOM_WHEEZE      0x01
#define TAG_SYMPTOM_COUGH       0x02
#define TAG_SYMPTOM_SOBOE       0x03
#define TAG_SYMPTOM_THROAT      0x04

#define TAG_ACTIVITY_STILL      0x00
#define TAG_ACTIVITY_WALKING    0x01
#define TAG_ACTIVITY_RUNNING    0x02
#define TAG_ACTIVITY_SLEEPING   0x03

/* ========== PACKET STRUCTURE ========== */

typedef struct __attribute__((packed)) {
    uint8_t  preamble[2];    /* 0xAA, 0x55 */
    uint8_t  len;            /* Total packet length */
    uint8_t  src_id;          /* Source node ID (0=hub, 1-16=sensors) */
    uint8_t  dst_id;          /* Destination (0xFF=broadcast) */
    uint8_t  msg_type;        /* Message type */
    uint16_t seq_num;         /* Sequence number */
    uint8_t  payload[MESH_PAYLOAD_SIZE]; /* Payload */
    uint16_t crc16;           /* CRC16-CCITT */
} breathhome_packet_t;

/* ========== AIR QUALITY DATA STRUCTURES ========== */

typedef struct __attribute__((packed)) {
    float    pm2_5;
    float    pm10;
    float    co2;
    float    voc_index;
    float    hcho;
    float    temperature;
    float    humidity;
    float    pressure;
    uint16_t aqi_score;
    uint8_t  aqi_category;
    float    mold_risk_pct;
    uint16_t light_lux;
    float    radon_bq_m3;
} air_quality_payload_t;

typedef struct __attribute__((packed)) {
    float    eco2;
    float    tvoc;
    float    temperature;
    float    humidity;
    uint8_t  activity;
    uint8_t  symptom_flag;
    uint8_t  battery_pct;
    float    personal_aqi;
} exposure_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  vent_positions[8];
    uint8_t  purifier_speed;
    float    filter_health_pct;
    float    duct_pressure_pa;
    float    supply_air_temp_c;
    float    blower_current_ma;
    uint8_t  relay_states;
} hvac_status_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  alert_type;
    float    value;
    uint8_t  aqi_category;
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
static inline void mesh_build_packet(breathhome_packet_t *pkt,
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
    
    pkt->len = sizeof(breathhome_packet_t);
    pkt->crc16 = crc16_ccitt((const uint8_t *)&pkt->src_id, 
                              sizeof(breathhome_packet_t) - sizeof(pkt->preamble) - sizeof(pkt->crc16));
}

/**
 * mesh_verify_packet - Verify packet CRC
 */
static inline int mesh_verify_packet(const breathhome_packet_t *pkt)
{
    /* Check preamble */
    if (pkt->preamble[0] != 0xAA || pkt->preamble[1] != 0x55) {
        return -1;
    }
    
    /* Verify CRC */
    uint16_t computed_crc = crc16_ccitt(
        (const uint8_t *)&pkt->src_id,
        sizeof(breathhome_packet_t) - sizeof(pkt->preamble) - sizeof(pkt->crc16)
    );
    
    return (computed_crc == pkt->crc16) ? 0 : -2;
}

/* ========== AQI CALCULATION (shared) ========== */

/**
 * calculate_aqi_from_readings - Compute composite AQI
 * Uses EPA-style breakpoint tables adapted for indoor air quality
 */
static inline uint16_t calculate_aqi_from_readings(
    float pm25, float co2, float voc_index, float hcho, float mold_risk)
{
    /* PM2.5 sub-index */
    float pm25_idx;
    if (pm25 <= 12.0f)        pm25_idx = 50.0f * pm25 / 12.0f;
    else if (pm25 <= 35.4f)   pm25_idx = 50.0f + 50.0f * (pm25 - 12.1f) / 23.3f;
    else if (pm25 <= 55.4f)   pm25_idx = 100.0f + 50.0f * (pm25 - 35.5f) / 19.9f;
    else if (pm25 <= 150.4f)  pm25_idx = 150.0f + 50.0f * (pm25 - 55.5f) / 94.9f;
    else                      pm25_idx = 200.0f + 100.0f * (pm25 - 150.5f) / 99.9f;
    
    /* CO2 sub-index (indoor-adapted) */
    float co2_idx;
    if (co2 <= 800.0f)        co2_idx = 50.0f * co2 / 800.0f;
    else if (co2 <= 1200.0f)  co2_idx = 50.0f + 50.0f * (co2 - 800.0f) / 400.0f;
    else if (co2 <= 1800.0f)  co2_idx = 100.0f + 50.0f * (co2 - 1200.0f) / 600.0f;
    else if (co2 <= 2500.0f)  co2_idx = 150.0f + 50.0f * (co2 - 1800.0f) / 700.0f;
    else                      co2_idx = 200.0f + 100.0f * (co2 - 2500.0f) / 2500.0f;
    
    /* VOC sub-index */
    float voc_idx;
    if (voc_index <= 100.0f)   voc_idx = 50.0f * voc_index / 100.0f;
    else if (voc_index <= 200.0f) voc_idx = 50.0f + 50.0f * (voc_index - 100.0f) / 100.0f;
    else if (voc_index <= 300.0f) voc_idx = 100.0f + 50.0f * (voc_index - 200.0f) / 100.0f;
    else                          voc_idx = 150.0f + 50.0f * (voc_index - 300.0f) / 200.0f;
    
    /* Composite: max of sub-indices (EPA method) */
    float composite = pm25_idx;
    if (co2_idx > composite) composite = co2_idx;
    if (voc_idx > composite) composite = voc_idx;
    
    /* Formaldehyde penalty */
    if (hcho > 0.08f) {
        composite += 20.0f * (hcho / 0.08f);
    }
    
    /* Mold risk penalty */
    if (mold_risk > 60.0f) {
        composite += 15.0f * (mold_risk / 60.0f);
    }
    
    if (composite > 500.0f) composite = 500.0f;
    if (composite < 0.0f) composite = 0.0f;
    
    return (uint16_t)composite;
}

/**
 * get_aqi_category - Classify AQI score into category
 */
static inline uint8_t get_aqi_category(uint16_t aqi)
{
    if (aqi <= 50)  return AQI_GOOD;
    if (aqi <= 100) return AQI_MODERATE;
    if (aqi <= 150) return AQI_UNHEALTHY_SENSITIVE;
    if (aqi <= 200) return AQI_UNHEALTHY;
    if (aqi <= 300) return AQI_VERY_UNHEALTHY;
    return AQI_HAZARDOUS;
}

/**
 * get_aqi_label - Get human-readable AQI category label
 */
static inline const char *get_aqi_label(uint8_t category)
{
    switch (category) {
        case AQI_GOOD:               return "Good";
        case AQI_MODERATE:           return "Moderate";
        case AQI_UNHEALTHY_SENSITIVE: return "Unhealthy (Sensitive)";
        case AQI_UNHEALTHY:          return "Unhealthy";
        case AQI_VERY_UNHEALTHY:     return "Very Unhealthy";
        case AQI_HAZARDOUS:         return "Hazardous";
        default:                    return "Unknown";
    }
}

/* ========== MOLD RISK CALCULATION (shared) ========== */

/**
 * calculate_mold_risk - Estimate mold growth risk from environmental data
 * Returns risk percentage (0-100%)
 */
static inline float calculate_mold_risk(float humidity, float temperature, 
                                         float voc_index, float dew_point,
                                         float wet_hours_24h)
{
    float risk = 0.0f;
    
    /* Humidity factor */
    if (humidity > 80.0f)       risk += 40.0f;
    else if (humidity > 70.0f)  risk += 30.0f * (humidity - 70.0f) / 10.0f;
    else if (humidity > 60.0f) risk += 20.0f * (humidity - 60.0f) / 10.0f;
    
    /* Temperature factor (mold grows 10-35°C, optimal 25-30°C) */
    float temp_factor = 0.0f;
    if (temperature >= 10.0f && temperature <= 35.0f) {
        float optimal_dist = fabsf(temperature - 27.5f);
        temp_factor = 30.0f * (1.0f - optimal_dist / 17.5f);
        if (temp_factor < 0) temp_factor = 0;
    }
    risk += temp_factor;
    
    /* Wet surface hours factor */
    risk += 20.0f * (wet_hours_24h / 24.0f);
    
    /* VOC factor (biological off-gassing) */
    if (voc_index > 200.0f) {
        risk += 10.0f * (voc_index - 200.0f) / 100.0f;
    }
    
    if (risk > 100.0f) risk = 100.0f;
    if (risk < 0.0f) risk = 0.0f;
    
    return risk;
}

/* ========== DEW POINT CALCULATION ========== */

/**
 * calculate_dew_point - Calculate dew point from temperature and humidity
 */
static inline float calculate_dew_point(float temperature_c, float humidity_pct)
{
    const float a = 17.27f;
    const float b = 237.7f;
    
    float alpha = (a * temperature_c) / (b + temperature_c) + logf(humidity_pct / 100.0f);
    return (b * alpha) / (a - alpha);
}

#endif /* MESH_PROTOCOL_H */
/**
 * BreathHome - Shared Mesh Protocol Implementation
 * CRC16, packet building, and protocol utilities
 */

#include "mesh_protocol.h"

/**
 * mesh_init_packet - Initialize a mesh packet with defaults
 */
void mesh_init_packet(breathhome_packet_t *pkt)
{
    memset(pkt, 0, sizeof(breathhome_packet_t));
    pkt->preamble[0] = 0xAA;
    pkt->preamble[1] = 0x55;
    pkt->len = sizeof(breathhome_packet_t);
}

/**
 * mesh_build_air_quality_packet - Build an air quality report packet
 */
void mesh_build_air_quality_packet(breathhome_packet_t *pkt, uint8_t src_id,
                                    const air_quality_payload_t *data)
{
    mesh_build_packet(pkt, src_id, 0x00, MSG_AIR_QUALITY, 0,
                      (const uint8_t *)data, sizeof(air_quality_payload_t));
}

/**
 * mesh_build_hvac_command_packet - Build an HVAC command packet
 */
void mesh_build_hvac_command_packet(breathhome_packet_t *pkt, uint8_t dst_id,
                                     uint8_t command, uint8_t room_id, uint8_t value)
{
    uint8_t payload[3] = { command, room_id, value };
    mesh_build_packet(pkt, 0x00, dst_id, MSG_HVAC_COMMAND, 0,
                      payload, sizeof(payload));
}

/**
 * mesh_build_danger_alert_packet - Build a danger alert packet
 */
void mesh_build_danger_alert_packet(breathhome_packet_t *pkt, uint8_t src_id,
                                     uint8_t alert_type, float value, uint8_t aqi_category)
{
    danger_alert_payload_t alert;
    alert.alert_type = alert_type;
    alert.value = value;
    alert.aqi_category = aqi_category;
    
    mesh_build_packet(pkt, src_id, 0xFF, MSG_DANGER_ALERT, 0,
                      (const uint8_t *)&alert, sizeof(danger_alert_payload_t));
}

/**
 * mesh_build_heartbeat_packet - Build a heartbeat packet
 */
void mesh_build_heartbeat_packet(breathhome_packet_t *pkt, uint8_t src_id, uint8_t node_type)
{
    uint8_t payload[8] = {
        node_type,     /* Node type */
        0x01,          /* Protocol version */
        0x00, 0x00,    /* Uptime (seconds, little-endian) */
        0x64,          /* Battery % (placeholder) */
        0x00, 0x00, 0x00 /* Reserved */
    };
    
    mesh_build_packet(pkt, src_id, 0xFF, MSG_HEARTBEAT, 0,
                      payload, sizeof(payload));
}

/**
 * mesh_parse_air_quality - Parse air quality payload from packet
 */
int mesh_parse_air_quality(const breathhome_packet_t *pkt, air_quality_payload_t *data)
{
    if (pkt->msg_type != MSG_AIR_QUALITY) return -1;
    if (sizeof(air_quality_payload_t) > MESH_PAYLOAD_SIZE) return -2;
    
    memcpy(data, pkt->payload, sizeof(air_quality_payload_t));
    return 0;
}

/**
 * mesh_parse_hvac_status - Parse HVAC status payload from packet
 */
int mesh_parse_hvac_status(const breathhome_packet_t *pkt, hvac_status_payload_t *data)
{
    if (pkt->msg_type != MSG_HVAC_STATUS) return -1;
    if (sizeof(hvac_status_payload_t) > MESH_PAYLOAD_SIZE) return -2;
    
    memcpy(data, pkt->payload, sizeof(hvac_status_payload_t));
    return 0;
}

/**
 * mesh_parse_danger_alert - Parse danger alert payload from packet
 */
int mesh_parse_danger_alert(const breathhome_packet_t *pkt, danger_alert_payload_t *data)
{
    if (pkt->msg_type != MSG_DANGER_ALERT) return -1;
    if (sizeof(danger_alert_payload_t) > MESH_PAYLOAD_SIZE) return -2;
    
    memcpy(data, pkt->payload, sizeof(danger_alert_payload_t));
    return 0;
}

/**
 * get_alert_type_string - Get human-readable alert type name
 */
const char *get_alert_type_string(uint8_t alert_type)
{
    switch (alert_type) {
        case ALERT_PM25:  return "PM2.5";
        case ALERT_CO2:   return "CO2";
        case ALERT_VOC:   return "VOC";
        case ALERT_HCHO:  return "Formaldehyde";
        case ALERT_RADON: return "Radon";
        case ALERT_MOLD:  return "Mold Risk";
        default:          return "Unknown";
    }
}
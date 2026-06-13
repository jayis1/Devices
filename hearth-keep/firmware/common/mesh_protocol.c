/**
 * HearthKeep Mesh Protocol - Common Implementation
 * 
 * Implements packet construction, serialization, CRC, and utility functions
 * for the Sub-GHz LoRa mesh protocol.
 */

#include "mesh_protocol.h"
#include <string.h>

/* --- CRC-16/CCITT (polynomial 0x1021, init 0x1D0F) --- */
static const uint16_t hk_crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x918F, 0x81AE, 0xB1CD, 0xA1EC, 0xD10B, 0xC12A, 0xF149, 0xE168,
    0x10F7, 0x00D6, 0x3095, 0x20B4, 0x5053, 0x4072, 0x7031, 0x6010,
    0x83C9, 0x93E8, 0xA38B, 0xB3AA, 0xC34D, 0xD36C, 0xE30F, 0xF32E,
    0x02C1, 0x12E0, 0x2283, 0x32A2, 0x4245, 0x5264, 0x6207, 0x7226,
    0xB3C7, 0xA3E6, 0x9385, 0x83A4, 0xF343, 0xE362, 0xD301, 0xC320,
    0x32DF, 0x22FE, 0x129D, 0x02BC, 0x725B, 0x627A, 0x5219, 0x4238,
    0xA5C4, 0xB5E5, 0x8586, 0x95A7, 0xE540, 0xF561, 0xC502, 0xD523,
    0x24CC, 0x34ED, 0x048E, 0x14AF, 0x6448, 0x7469, 0x440A, 0x542B,
    0x9653, 0x8672, 0xB611, 0xA630, 0xD6D7, 0xC6F6, 0xF695, 0xE6B4,
    0x17AB, 0x078A, 0x37E9, 0x27C8, 0x772F, 0x674E, 0x572D, 0x470C,
    0xAFCA, 0xBFEB, 0x8F88, 0x9FA9, 0xEF4E, 0xFF6F, 0xCF0C, 0xDF2D,
    0x2EC2, 0x3EE3, 0x0E80, 0x1EA1, 0x6E46, 0x7E67, 0x4E04, 0x5E25,
    0xB9D9, 0xA9F8, 0x999B, 0x89BA, 0xF95D, 0xE97C, 0xD91F, 0xC93E,
    0x38D1, 0x28F0, 0x1893, 0x08B2, 0x7855, 0x6874, 0x5817, 0x4836,
    0xCA0F, 0xDA2E, 0xEA4D, 0xFA6C, 0x8A8B, 0x9AAA, 0xAAC9, 0xBAE8,
    0x4B07, 0x5B26, 0x6B45, 0x7B64, 0x0B83, 0x1BA2, 0x2BC1, 0x3BE0,
};

uint16_t hk_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0x1D0F;
    for (uint16_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ hk_crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

/* --- Packet Initialization --- */
void hk_packet_init(hk_packet_t *pkt, uint8_t src_id, uint8_t dst_id, hk_packet_type_t type)
{
    memset(pkt, 0, sizeof(hk_packet_t));
    
    /* Preamble: alternating 0x48 0x4B (HK) */
    pkt->preamble[0] = 0x48;
    pkt->preamble[1] = 0x4B;
    pkt->preamble[2] = 0x48;
    pkt->preamble[3] = 0x4B;
    
    pkt->sync_word = HK_SYNC_WORD;
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->type = (uint8_t)type;
    pkt->seq_num = 0;
    pkt->length = HK_HEADER_LEN + HK_CRC_LEN;  /* Minimum: header + CRC */
}

/* --- Packet Serialization --- */
uint16_t hk_packet_serialize(const hk_packet_t *pkt, uint8_t *buf, uint16_t buf_len)
{
    uint16_t total_len = pkt->length;
    
    if (buf_len < total_len) {
        return 0;  /* Buffer too small */
    }
    
    uint16_t idx = 0;
    
    /* Preamble */
    memcpy(&buf[idx], pkt->preamble, HK_PREAMBLE_LEN);
    idx += HK_PREAMBLE_LEN;
    
    /* Sync word */
    buf[idx++] = (uint8_t)(pkt->sync_word >> 8);
    buf[idx++] = (uint8_t)(pkt->sync_word & 0xFF);
    
    /* Length */
    buf[idx++] = pkt->length;
    
    /* Source ID */
    buf[idx++] = pkt->src_id;
    
    /* Destination ID */
    buf[idx++] = pkt->dst_id;
    
    /* Type */
    buf[idx++] = pkt->type;
    
    /* Sequence number */
    buf[idx++] = (uint8_t)(pkt->seq_num >> 8);
    buf[idx++] = (uint8_t)(pkt->seq_num & 0xFF);
    
    /* Payload */
    uint16_t payload_len = pkt->length - HK_HEADER_LEN - HK_CRC_LEN;
    if (payload_len > 0) {
        memcpy(&buf[idx], pkt->payload, payload_len);
        idx += payload_len;
    }
    
    /* CRC-16 over everything except preamble and CRC itself */
    uint16_t crc = hk_crc16(&buf[HK_PREAMBLE_LEN], idx - HK_PREAMBLE_LEN);
    buf[idx++] = (uint8_t)(crc >> 8);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    
    return idx;
}

/* --- Packet Deserialization --- */
bool hk_packet_deserialize(const uint8_t *buf, uint16_t len, hk_packet_t *pkt)
{
    if (len < HK_HEADER_LEN + HK_CRC_LEN + HK_PREAMBLE_LEN) {
        return false;  /* Too short */
    }
    
    memset(pkt, 0, sizeof(hk_packet_t));
    
    uint16_t idx = 0;
    
    /* Preamble */
    memcpy(pkt->preamble, &buf[idx], HK_PREAMBLE_LEN);
    idx += HK_PREAMBLE_LEN;
    
    /* Verify preamble */
    if (pkt->preamble[0] != 0x48 || pkt->preamble[1] != 0x4B ||
        pkt->preamble[2] != 0x48 || pkt->preamble[3] != 0x4B) {
        return false;
    }
    
    /* Sync word */
    pkt->sync_word = ((uint16_t)buf[idx] << 8) | buf[idx + 1];
    idx += 2;
    
    if (pkt->sync_word != HK_SYNC_WORD) {
        return false;
    }
    
    /* Length */
    pkt->length = buf[idx++];
    
    /* Validate length */
    if (pkt->length > HK_MAX_PACKET_LEN || pkt->length > len) {
        return false;
    }
    
    /* Source ID */
    pkt->src_id = buf[idx++];
    
    /* Destination ID */
    pkt->dst_id = buf[idx++];
    
    /* Type */
    pkt->type = buf[idx++];
    
    /* Sequence number */
    pkt->seq_num = ((uint16_t)buf[idx] << 8) | buf[idx + 1];
    idx += 2;
    
    /* Payload */
    uint16_t payload_len = pkt->length - HK_HEADER_LEN - HK_CRC_LEN;
    if (payload_len > HK_MAX_PAYLOAD) {
        return false;
    }
    if (payload_len > 0) {
        memcpy(pkt->payload, &buf[idx], payload_len);
    }
    idx += payload_len;
    
    /* CRC verification */
    uint16_t received_crc = ((uint16_t)buf[idx] << 8) | buf[idx + 1];
    uint16_t computed_crc = hk_crc16(&buf[HK_PREAMBLE_LEN], idx - HK_PREAMBLE_LEN);
    
    if (received_crc != computed_crc) {
        return false;  /* CRC mismatch */
    }
    
    pkt->crc16 = received_crc;
    return true;
}

/* --- Packet Validation --- */
bool hk_packet_validate(const hk_packet_t *pkt)
{
    /* Check preamble */
    if (pkt->preamble[0] != 0x48 || pkt->preamble[1] != 0x4B ||
        pkt->preamble[2] != 0x48 || pkt->preamble[3] != 0x4B) {
        return false;
    }
    
    /* Check sync word */
    if (pkt->sync_word != HK_SYNC_WORD) {
        return false;
    }
    
    /* Check length */
    if (pkt->length < HK_HEADER_LEN + HK_CRC_LEN ||
        pkt->length > HK_MAX_PACKET_LEN) {
        return false;
    }
    
    /* Check type */
    if (pkt->type < HK_TYPE_RADAR_DATA || pkt->type > HK_TYPE_LOW_BATTERY) {
        return false;
    }
    
    return true;
}

/* --- Slot Assignment --- */
uint8_t hk_slot_for_node(uint8_t node_id)
{
    /* Hub is always slot 0 */
    if (node_id == HK_NODE_ID_HUB) return HK_SLOT_HUB;
    
    /* Bed mat is always slot 9 */
    if (node_id == 0x0A) return HK_SLOT_BED_MAT;
    
    /* Room monitors get slots 1-8 and 10-15 based on their ID */
    if (node_id >= 0x01 && node_id <= 0x08) {
        return node_id;  /* Slots 1-8 */
    }
    if (node_id >= 0x09 && node_id <= 0x0E) {
        return node_id + 1;  /* Slots 10-15 */
    }
    
    /* Unknown node */
    return 0xFF;
}

uint8_t hk_node_for_slot(uint8_t slot)
{
    switch (slot) {
        case 0:  return HK_NODE_ID_HUB;  /* Hub */
        case 9:  return 0x0A;             /* Bed mat */
        case 17: return 0xFF;             /* Control slot */
        default:
            if (slot >= 1 && slot <= 8) return slot;
            if (slot >= 10 && slot <= 15) return slot - 1;
            return 0xFF;
    }
}

/* --- Debug Strings --- */
const char* hk_type_str(hk_packet_type_t type)
{
    switch (type) {
        case HK_TYPE_RADAR_DATA:  return "RADAR_DATA";
        case HK_TYPE_ENV_DATA:    return "ENV_DATA";
        case HK_TYPE_BED_VITALS:  return "BED_VITALS";
        case HK_TYPE_COMMAND:     return "COMMAND";
        case HK_TYPE_ACK:         return "ACK";
        case HK_TYPE_OTA_BLOCK:   return "OTA_BLOCK";
        case HK_TYPE_FALL_ALERT:  return "FALL_ALERT";
        case HK_TYPE_PANIC_ALERT: return "PANIC_ALERT";
        case HK_TYPE_HEARTBEAT:  return "HEARTBEAT";
        case HK_TYPE_CALIBRATION: return "CALIBRATION";
        case HK_TYPE_LOW_BATTERY: return "LOW_BATTERY";
        default:                  return "UNKNOWN";
    }
}

const char* hk_position_str(hk_position_class_t pos)
{
    switch (pos) {
        case HK_POS_UNKNOWN:  return "UNKNOWN";
        case HK_POS_STANDING: return "STANDING";
        case HK_POS_SITTING:  return "SITTING";
        case HK_POS_LYING:    return "LYING";
        case HK_POS_FALLING:  return "FALLING";
        case HK_POS_FALLEN:   return "FALLEN";
        case HK_POS_ABSENT:   return "ABSENT";
        default:              return "INVALID";
    }
}

const char* hk_alert_str(hk_alert_level_t level)
{
    switch (level) {
        case HK_ALERT_NONE:      return "NONE";
        case HK_ALERT_INFO:      return "INFO";
        case HK_ALERT_WARNING:   return "WARNING";
        case HK_ALERT_URGENT:    return "URGENT";
        case HK_ALERT_EMERGENCY: return "EMERGENCY";
        default:                 return "INVALID";
    }
}
/**
 * @file mesh_packet.c
 * @brief SoundNest Sub-GHz mesh packet encode/decode implementation.
 */

#include "mesh_packet.h"
#include <string.h>

/* ── CRC16-CCITT ───────────────────────────────────────────────────── */

static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
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

/* ── Packet Encode ─────────────────────────────────────────────────── */

int mesh_packet_encode(const mesh_header_t *header,
                       const uint8_t *payload, uint8_t payload_len,
                       uint8_t *out_buf, size_t out_buf_len)
{
    if (!header || !out_buf) return -1;
    if (payload_len > MESH_MAX_PAYLOAD) return -2;

    size_t total_len = MESH_HEADER_LEN + payload_len + MESH_MIC_LEN;
    if (total_len > out_buf_len) return -3;

    /* Preamble */
    memset(out_buf, 0xAA, MESH_PREAMBLE_LEN);

    /* Header */
    mesh_header_t *hdr = (mesh_header_t *)(out_buf + MESH_PREAMBLE_LEN);
    memcpy(hdr, header, sizeof(mesh_header_t));
    hdr->sync_word = MESH_SYNC_WORD;
    hdr->length = (uint8_t)total_len;

    /* Payload */
    if (payload && payload_len > 0) {
        memcpy(out_buf + MESH_PREAMBLE_LEN + MESH_HEADER_LEN,
               payload, payload_len);
    }

    /* MIC placeholder (encryption module fills this in) */
    memset(out_buf + MESH_PREAMBLE_LEN + MESH_HEADER_LEN + payload_len,
           0, MESH_MIC_LEN);

    return (int)(MESH_PREAMBLE_LEN + total_len);
}

/* ── Packet Decode ─────────────────────────────────────────────────── */

int mesh_packet_decode(const uint8_t *buf, size_t buf_len,
                       mesh_packet_t *packet)
{
    if (!buf || !packet || buf_len < MESH_PREAMBLE_LEN + MESH_HEADER_LEN) {
        return -1;
    }

    /* Find preamble */
    size_t offset = 0;
    bool found = false;
    for (size_t i = 0; i <= buf_len - MESH_PREAMBLE_LEN; i++) {
        if (buf[i] == 0xAA && buf[i+1] == 0xAA &&
            buf[i+2] == 0xAA && buf[i+3] == 0xAA) {
            offset = i + MESH_PREAMBLE_LEN;
            found = true;
            break;
        }
    }
    if (!found) return -2;

    /* Parse header */
    if (offset + MESH_HEADER_LEN > buf_len) return -3;
    memcpy(&packet->header, buf + offset, sizeof(mesh_header_t));

    /* Validate sync word */
    if (packet->header.sync_word != MESH_SYNC_WORD) return -4;

    /* Validate length */
    uint8_t total_len = packet->header.length;
    if (total_len < MESH_HEADER_LEN + MESH_MIC_LEN) return -5;
    uint8_t payload_len = total_len - MESH_HEADER_LEN - MESH_MIC_LEN;
    if (payload_len > MESH_MAX_PAYLOAD) return -6;

    /* Copy payload */
    if (offset + MESH_HEADER_LEN + payload_len > buf_len) return -7;
    memcpy(packet->payload, buf + offset + MESH_HEADER_LEN, payload_len);

    /* Copy MIC */
    if (offset + total_len > buf_len) return -8;
    memcpy(packet->mic, buf + offset + MESH_HEADER_LEN + payload_len,
           MESH_MIC_LEN);

    return 0;
}

/* ── Helper: Build Event Report ────────────────────────────────────── */

int mesh_build_event_report(uint16_t src_addr, uint16_t dst_addr,
                            uint16_t seq_num,
                            const event_report_payload_t *event,
                            uint8_t *out_buf, size_t out_buf_len)
{
    mesh_header_t header = {
        .sync_word = MESH_SYNC_WORD,
        .length = 0,  /* filled by encode */
        .src_addr = src_addr,
        .dst_addr = dst_addr,
        .msg_type = MSG_TYPE_EVENT_REPORT,
        .seq_num = seq_num,
    };

    return mesh_packet_encode(&header, (const uint8_t *)event,
                              sizeof(event_report_payload_t),
                              out_buf, out_buf_len);
}

/* ── Helper: Build SPL Report ──────────────────────────────────────── */

int mesh_build_spl_report(uint16_t src_addr, uint16_t dst_addr,
                           uint16_t seq_num,
                           const spl_report_payload_t *spl,
                           uint8_t *out_buf, size_t out_buf_len)
{
    mesh_header_t header = {
        .sync_word = MESH_SYNC_WORD,
        .length = 0,
        .src_addr = src_addr,
        .dst_addr = dst_addr,
        .msg_type = MSG_TYPE_SPL_REPORT,
        .seq_num = seq_num,
    };

    return mesh_packet_encode(&header, (const uint8_t *)spl,
                              sizeof(spl_report_payload_t),
                              out_buf, out_buf_len);
}

/* ── Helper: Build Masking Command ─────────────────────────────────── */

int mesh_build_masking_cmd(uint16_t src_addr, uint16_t dst_addr,
                           uint16_t seq_num,
                           const masking_cmd_payload_t *cmd,
                           uint8_t *out_buf, size_t out_buf_len)
{
    mesh_header_t header = {
        .sync_word = MESH_SYNC_WORD,
        .length = 0,
        .src_addr = src_addr,
        .dst_addr = dst_addr,
        .msg_type = MSG_TYPE_MASKING_CMD,
        .seq_num = seq_num,
    };

    return mesh_packet_encode(&header, (const uint8_t *)cmd,
                              sizeof(masking_cmd_payload_t),
                              out_buf, out_buf_len);
}

/* ── Helper: Build Alert Command ───────────────────────────────────── */

int mesh_build_alert_cmd(uint16_t src_addr, uint16_t dst_addr,
                         uint16_t seq_num,
                         const alert_cmd_payload_t *alert,
                         uint8_t *out_buf, size_t out_buf_len)
{
    mesh_header_t header = {
        .sync_word = MESH_SYNC_WORD,
        .length = 0,
        .src_addr = src_addr,
        .dst_addr = dst_addr,
        .msg_type = MSG_TYPE_ALERT_CMD,
        .seq_num = seq_num,
    };

    return mesh_packet_encode(&header, (const uint8_t *)alert,
                              sizeof(alert_cmd_payload_t),
                              out_buf, out_buf_len);
}

/* ── Helper: Build Heartbeat ───────────────────────────────────────── */

int mesh_build_heartbeat(uint16_t src_addr, uint16_t dst_addr,
                         uint16_t seq_num,
                         const heartbeat_payload_t *hb,
                         uint8_t *out_buf, size_t out_buf_len)
{
    mesh_header_t header = {
        .sync_word = MESH_SYNC_WORD,
        .length = 0,
        .src_addr = src_addr,
        .dst_addr = dst_addr,
        .msg_type = MSG_TYPE_HEARTBEAT,
        .seq_num = seq_num,
    };

    return mesh_packet_encode(&header, (const uint8_t *)hb,
                              sizeof(heartbeat_payload_t),
                              out_buf, out_buf_len);
}

/* ── Sound Event Name Lookup ───────────────────────────────────────── */

const char *mesh_sound_event_name(uint8_t sound_class)
{
    switch (sound_class) {
    case SOUND_SMOKE_ALARM:    return "Smoke Alarm";
    case SOUND_CO_ALARM:       return "CO Alarm";
    case SOUND_BURGLAR_ALARM:  return "Burglar Alarm";
    case SOUND_CAR_ALARM:      return "Car Alarm";
    case SOUND_TIMER_ALARM:    return "Timer Alarm";
    case SOUND_DOORBELL:       return "Doorbell";
    case SOUND_DOOR_KNOCK:     return "Door Knock";
    case SOUND_DOOR_OPEN:      return "Door Open";
    case SOUND_DOOR_CLOSE:     return "Door Close";
    case SOUND_SPEECH:         return "Speech";
    case SOUND_CRYING_BABY:   return "Crying Baby";
    case SOUND_COUGH:          return "Cough";
    case SOUND_SNEEZE:         return "Sneeze";
    case SOUND_LAUGH:          return "Laugh";
    case SOUND_SHOUT:          return "Shout";
    case SOUND_DOG_BARK:       return "Dog Bark";
    case SOUND_CAT_MEOW:       return "Cat Meow";
    case SOUND_BIRD_CHIRP:     return "Bird Chirp";
    case SOUND_MICROWAVE:      return "Microwave";
    case SOUND_BLENDER:        return "Blender";
    case SOUND_DISHWASHER:     return "Dishwasher";
    case SOUND_KETTLE:         return "Kettle";
    case SOUND_FAUCET:          return "Faucet";
    case SOUND_VACUUM:         return "Vacuum";
    case SOUND_WASHER:         return "Washer";
    case SOUND_DRYER:          return "Dryer";
    case SOUND_FAN:            return "Fan";
    case SOUND_AC_UNIT:        return "AC Unit";
    case SOUND_TV:             return "TV";
    case SOUND_MUSIC:          return "Music";
    case SOUND_CAR_HORN:       return "Car Horn";
    case SOUND_SIREN:          return "Siren";
    case SOUND_ENGINE:         return "Engine";
    case SOUND_MOTORCYCLE:     return "Motorcycle";
    case SOUND_BICYCLE_BELL:   return "Bicycle Bell";
    case SOUND_RAIN:           return "Rain";
    case SOUND_THUNDER:        return "Thunder";
    case SOUND_WIND:           return "Wind";
    case SOUND_RUNNING_WATER:  return "Running Water";
    case SOUND_PHONE_RING:     return "Phone Ring";
    case SOUND_NOTIFICATION:   return "Notification";
    case SOUND_KEYBOARD:       return "Keyboard";
    case SOUND_GLASS_BREAK:    return "Glass Break";
    case SOUND_CRASH:          return "Crash";
    case SOUND_GUNSHOT:        return "Gunshot";
    case SOUND_SILENCE:        return "Silence";
    case SOUND_UNKNOWN:        return "Unknown";
    default:                   return "Invalid";
    }
}

/* ── Alert Priority Name Lookup ────────────────────────────────────── */

const char *mesh_alert_priority_name(uint8_t priority)
{
    switch (priority) {
    case ALERT_PRIORITY_INFO:      return "Info";
    case ALERT_PRIORITY_LOW:       return "Low";
    case ALERT_PRIORITY_MEDIUM:    return "Medium";
    case ALERT_PRIORITY_HIGH:      return "High";
    case ALERT_PRIORITY_CRITICAL:  return "Critical";
    default:                       return "Invalid";
    }
}

/* ── Masking Mode Name Lookup ───────────────────────────────────────── */

const char *mesh_masking_mode_name(uint8_t mode)
{
    switch (mode) {
    case MASKING_OFF:            return "Off";
    case MASKING_WHITE_NOISE:   return "White Noise";
    case MASKING_PINK_NOISE:   return "Pink Noise";
    case MASKING_BROWN_NOISE:  return "Brown Noise";
    case MASKING_NATURE_RAIN:  return "Rain";
    case MASKING_NATURE_STREAM:return "Stream";
    case MASKING_NATURE_FOREST:return "Forest";
    case MASKING_NATURE_OCEAN: return "Ocean";
    case MASKING_TINNITUS:     return "Tinnitus";
    case MASKING_PRIVACY:       return "Privacy";
    case MASKING_CUSTOM:        return "Custom";
    default:                    return "Invalid";
    }
}
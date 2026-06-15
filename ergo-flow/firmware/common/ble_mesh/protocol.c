/*
 * ErgoFlow — Protocol Serialization/Deserialization
 * Pack and unpack BLE mesh message structures
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "protocol.h"
#include <string.h>

/* ── Pack functions ──────────────────────────────────────────────── */

int ergo_pack_pressure_map(const ergo_pressure_map_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_pressure_map_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_imu_orientation(const ergo_imu_orientation_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_imu_orientation_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_heart_rate(const ergo_heart_rate_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_heart_rate_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_desk_command(const ergo_desk_command_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_desk_command_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_desk_status(const ergo_desk_status_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_desk_status_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_ambient_reading(const ergo_ambient_reading_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_ambient_reading_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_posture_score(const ergo_posture_score_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_posture_score_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_break_reminder(const ergo_break_reminder_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_break_reminder_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_lighting_cmd(const ergo_lighting_cmd_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_lighting_cmd_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_monitor_tilt(const ergo_monitor_tilt_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_monitor_tilt_t);
    memcpy(buf, msg, *len);
    return 0;
}

int ergo_pack_node_heartbeat(const ergo_node_heartbeat_t *msg, uint8_t *buf, uint16_t *len)
{
    *len = sizeof(ergo_node_heartbeat_t);
    memcpy(buf, msg, *len);
    return 0;
}

/* ── Unpack functions ────────────────────────────────────────────── */

int ergo_unpack_pressure_map(const uint8_t *buf, uint16_t len, ergo_pressure_map_t *msg)
{
    if (len < sizeof(ergo_pressure_map_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_pressure_map_t));
    return 0;
}

int ergo_unpack_imu_orientation(const uint8_t *buf, uint16_t len, ergo_imu_orientation_t *msg)
{
    if (len < sizeof(ergo_imu_orientation_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_imu_orientation_t));
    return 0;
}

int ergo_unpack_heart_rate(const uint8_t *buf, uint16_t len, ergo_heart_rate_t *msg)
{
    if (len < sizeof(ergo_heart_rate_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_heart_rate_t));
    return 0;
}

int ergo_unpack_desk_command(const uint8_t *buf, uint16_t len, ergo_desk_command_t *msg)
{
    if (len < sizeof(ergo_desk_command_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_desk_command_t));
    return 0;
}

int ergo_unpack_desk_status(const uint8_t *buf, uint16_t len, ergo_desk_status_t *msg)
{
    if (len < sizeof(ergo_desk_status_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_desk_status_t));
    return 0;
}

int ergo_unpack_ambient_reading(const uint8_t *buf, uint16_t len, ergo_ambient_reading_t *msg)
{
    if (len < sizeof(ergo_ambient_reading_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_ambient_reading_t));
    return 0;
}

int ergo_unpack_posture_score(const uint8_t *buf, uint16_t len, ergo_posture_score_t *msg)
{
    if (len < sizeof(ergo_posture_score_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_posture_score_t));
    return 0;
}

int ergo_unpack_break_reminder(const uint8_t *buf, uint16_t len, ergo_break_reminder_t *msg)
{
    if (len < sizeof(ergo_break_reminder_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_break_reminder_t));
    return 0;
}

int ergo_unpack_lighting_cmd(const uint8_t *buf, uint16_t len, ergo_lighting_cmd_t *msg)
{
    if (len < sizeof(ergo_lighting_cmd_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_lighting_cmd_t));
    return 0;
}

int ergo_unpack_monitor_tilt(const uint8_t *buf, uint16_t len, ergo_monitor_tilt_t *msg)
{
    if (len < sizeof(ergo_monitor_tilt_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_monitor_tilt_t));
    return 0;
}

int ergo_unpack_node_heartbeat(const uint8_t *buf, uint16_t len, ergo_node_heartbeat_t *msg)
{
    if (len < sizeof(ergo_node_heartbeat_t)) return -1;
    memcpy(msg, buf, sizeof(ergo_node_heartbeat_t));
    return 0;
}
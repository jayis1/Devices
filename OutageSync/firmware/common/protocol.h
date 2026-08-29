#ifndef OUTAGESYNC_PROTOCOL_H
#define OUTAGESYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OS_NODE_HUB                0x0001
#define OS_NODE_PANEL              0x0002
#define OS_NODE_COLD_BASE          0x0100
#define OS_NODE_OUTLET_BASE        0x0200
#define OS_NODE_FUEL_SENTINEL      0x0005
#define OS_NODE_BROADCAST          0xFFFF

#define OS_MSG_JOIN_REQ            0x01
#define OS_MSG_JOIN_ACK            0x02
#define OS_MSG_HEARTBEAT           0x03
#define OS_MSG_PANEL_STATUS        0x04
#define OS_MSG_COLD_STATUS         0x05
#define OS_MSG_OUTLET_STATUS       0x06
#define OS_MSG_FUEL_STATUS         0x07
#define OS_MSG_COMMAND             0x08
#define OS_MSG_ALERT               0x09
#define OS_MSG_POLICY              0x0A

#define OS_CMD_NONE                0x00
#define OS_CMD_SHED_LOAD           0x01
#define OS_CMD_ENABLE_LOAD         0x02
#define OS_CMD_PREPARE_GENERATOR   0x03
#define OS_CMD_INHIBIT_GENERATOR   0x04
#define OS_CMD_STAGE_RESTORE       0x05

#define OS_PAYLOAD_MAX             56
#define OS_SYNC_0                  0x4A
#define OS_SYNC_1                  0x91

#pragma pack(push, 1)
typedef struct {
    uint8_t preamble[4];
    uint8_t sync[2];
    uint8_t length;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t msg_type;
    uint16_t seq;
    uint32_t session_nonce;
    uint8_t payload[OS_PAYLOAD_MAX];
    uint16_t crc;
} os_frame_t;

typedef struct {
    uint16_t vrms;
    uint16_t irms_x10;
    uint16_t freq_x100;
    uint16_t thd_x100;
    uint8_t grid_present;
    uint8_t backup_mode;
    uint16_t battery_soc_x10;
    uint16_t reserve_minutes;
} os_panel_status_t;

typedef struct {
    int16_t product_temp_c_x100;
    int16_t ambient_temp_c_x100;
    uint16_t rh_x100;
    uint16_t hold_minutes_remaining;
    uint16_t door_open_seconds;
    uint8_t appliance_type;
    uint8_t risk_level;
} os_cold_status_t;

typedef struct {
    uint16_t watts;
    uint16_t watt_hours_today;
    uint16_t line_vrms;
    uint8_t priority;
    uint8_t relay_enabled;
    uint8_t on_backup;
    uint8_t manual_override;
} os_outlet_status_t;

typedef struct {
    uint16_t co2_ppm;
    uint16_t fuel_level_pct;
    uint16_t enclosure_temp_c_x100;
    uint16_t vibration_rms;
    uint8_t co_alarm;
    uint8_t start_inhibited;
    uint16_t runtime_minutes;
} os_fuel_status_t;

typedef struct {
    uint8_t command;
    uint8_t arg0;
    uint16_t arg1;
    uint32_t arg2;
} os_command_t;
#pragma pack(pop)

uint16_t os_crc16(const uint8_t *data, size_t len);
void os_build_frame(os_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t nonce,
                    const uint8_t *payload,
                    uint8_t payload_len);
bool os_validate_frame(const os_frame_t *frame, uint8_t payload_len);

#endif

#ifndef DRAINSYNC_PROTOCOL_H
#define DRAINSYNC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define DS_PROTO_VERSION 1u
#define DS_MAX_PAYLOAD 32u
#define DS_BROADCAST_ID 0xFFFFu

typedef enum {
    DS_MSG_TELEMETRY = 0x01,
    DS_MSG_ACK = 0x02,
    DS_MSG_COMMAND = 0x03,
    DS_MSG_ALERT = 0x04,
    DS_MSG_JOIN_REQ = 0x05,
    DS_MSG_JOIN_RESP = 0x06
} ds_msg_type_t;

typedef enum {
    DS_NODE_HUB = 0x10,
    DS_NODE_UNDER_SINK = 0x11,
    DS_NODE_FLOOR_DRAIN = 0x12,
    DS_NODE_MAIN_STACK = 0x13,
    DS_NODE_ACTUATOR = 0x14
} ds_node_type_t;

typedef enum {
    DS_CMD_NOP = 0x00,
    DS_CMD_PRIME_TRAP = 0x01,
    DS_CMD_VALVE_OPEN = 0x02,
    DS_CMD_VALVE_CLOSE = 0x03,
    DS_CMD_ENTER_SERVICE = 0x04
} ds_command_t;

typedef struct {
    uint8_t version;
    uint8_t msg_type;
    uint16_t src;
    uint16_t dst;
    uint32_t nonce;
    uint16_t seq;
    uint8_t payload[DS_MAX_PAYLOAD];
    uint16_t crc;
} ds_frame_t;

typedef struct {
    uint16_t duration_ms;
    uint16_t turbulence;
    uint16_t vibration_rms;
    uint16_t gas_index;
    uint8_t leak_flags;
    uint8_t temperature_c;
} ds_flow_summary_t;

typedef struct {
    uint16_t trap_depth_raw;
    uint16_t h2s_ppb;
    uint16_t humidity_rh_x10;
    uint8_t primer_cycles;
    uint8_t water_present;
} ds_trap_status_t;

typedef struct {
    uint16_t level_mm;
    int16_t diff_pressure_pa;
    uint16_t surge_count;
    uint16_t battery_mv;
} ds_stack_status_t;

typedef struct {
    uint8_t target_position;
    uint8_t measured_position;
    uint16_t motor_current_ma;
    uint16_t travel_time_ms;
    uint16_t fault_bits;
} ds_actuator_status_t;

uint16_t ds_crc16_ccitt(const uint8_t *data, uint16_t len);
void ds_init_frame(ds_frame_t *frame, uint8_t type, uint16_t src, uint16_t dst, uint32_t nonce, uint16_t seq);
bool ds_encode_flow_summary(ds_frame_t *frame, const ds_flow_summary_t *summary);
bool ds_encode_trap_status(ds_frame_t *frame, const ds_trap_status_t *status);
bool ds_encode_stack_status(ds_frame_t *frame, const ds_stack_status_t *status);
bool ds_encode_actuator_status(ds_frame_t *frame, const ds_actuator_status_t *status);

#endif

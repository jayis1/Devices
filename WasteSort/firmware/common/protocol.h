#ifndef WASTESORT_PROTOCOL_H
#define WASTESORT_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WS_NODE_HUB               0x0001
#define WS_NODE_SORTER            0x0002
#define WS_NODE_BIN_DOCK_BASE     0x0100
#define WS_NODE_PICKUP_BEACON     0x0004
#define WS_NODE_BROADCAST         0xFFFF

#define WS_MSG_JOIN_REQ           0x01
#define WS_MSG_JOIN_ACK           0x02
#define WS_MSG_HEARTBEAT          0x03
#define WS_MSG_SORT_EVENT         0x04
#define WS_MSG_BIN_TELEMETRY      0x05
#define WS_MSG_PICKUP_EVENT       0x06
#define WS_MSG_COMMAND            0x07
#define WS_MSG_CONFIG             0x08
#define WS_MSG_OTA_CHUNK          0x09
#define WS_MSG_OTA_STATUS         0x0A
#define WS_MSG_ALERT              0x0B

#define WS_STREAM_UNKNOWN         0
#define WS_STREAM_RECYCLE         1
#define WS_STREAM_COMPOST         2
#define WS_STREAM_LANDFILL        3
#define WS_STREAM_GLASS           4
#define WS_STREAM_DEPOSIT         5
#define WS_STREAM_SPECIAL         6

#define WS_EVENT_CURB_PLACED      1
#define WS_EVENT_PICKUP_CONFIRMED 2
#define WS_EVENT_MISSED_PICKUP    3

#define WS_FRAME_PAYLOAD_MAX      48
#define WS_SYNC_0                 0x37
#define WS_SYNC_1                 0xC9

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
    uint8_t payload[WS_FRAME_PAYLOAD_MAX];
    uint16_t crc;
} ws_frame_t;

typedef struct {
    uint32_t item_id;
    uint8_t recommended_stream;
    uint8_t material_class;
    uint16_t confidence_q15;
    uint8_t contamination_risk_pct;
    uint8_t barcode_present;
    uint16_t mass_grams;
    uint16_t reserved;
} ws_sort_event_t;

typedef struct {
    uint8_t stream;
    uint8_t fill_pct;
    uint16_t mass_grams;
    uint16_t voc_index;
    int16_t temp_c_x100;
    uint16_t rh_pct_x100;
    uint16_t lid_open_count;
    uint16_t battery_mv;
    uint8_t deodorizer_active;
    uint8_t reserved[3];
} ws_bin_telemetry_t;

typedef struct {
    uint8_t event_type;
    int16_t tilt_mdps;
    uint16_t lift_peak_mg;
    uint16_t solar_mv;
    uint16_t battery_mv;
    uint32_t next_pickup_epoch;
    uint8_t reserved[4];
} ws_pickup_event_t;
#pragma pack(pop)

uint16_t ws_crc16(const uint8_t *data, size_t len);
void ws_build_frame(ws_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t nonce,
                    const uint8_t *payload,
                    uint8_t payload_len);
bool ws_validate_frame(const ws_frame_t *frame, uint8_t payload_len);

#endif

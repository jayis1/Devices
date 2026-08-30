#include "protocol.h"
#include <string.h>

static void put_u16(uint8_t *buf, uint16_t value) {
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFFu);
}

uint16_t ds_crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void ds_init_frame(ds_frame_t *frame, uint8_t type, uint16_t src, uint16_t dst, uint32_t nonce, uint16_t seq) {
    memset(frame, 0, sizeof(*frame));
    frame->version = DS_PROTO_VERSION;
    frame->msg_type = type;
    frame->src = src;
    frame->dst = dst;
    frame->nonce = nonce;
    frame->seq = seq;
}

static bool finalize_frame(ds_frame_t *frame, uint8_t payload_len) {
    if (payload_len > DS_MAX_PAYLOAD) {
        return false;
    }
    frame->crc = ds_crc16_ccitt((const uint8_t *)frame, (uint16_t)(10u + payload_len));
    return true;
}

bool ds_encode_flow_summary(ds_frame_t *frame, const ds_flow_summary_t *summary) {
    put_u16(&frame->payload[0], summary->duration_ms);
    put_u16(&frame->payload[2], summary->turbulence);
    put_u16(&frame->payload[4], summary->vibration_rms);
    put_u16(&frame->payload[6], summary->gas_index);
    frame->payload[8] = summary->leak_flags;
    frame->payload[9] = summary->temperature_c;
    return finalize_frame(frame, 10u);
}

bool ds_encode_trap_status(ds_frame_t *frame, const ds_trap_status_t *status) {
    put_u16(&frame->payload[0], status->trap_depth_raw);
    put_u16(&frame->payload[2], status->h2s_ppb);
    put_u16(&frame->payload[4], status->humidity_rh_x10);
    frame->payload[6] = status->primer_cycles;
    frame->payload[7] = status->water_present;
    return finalize_frame(frame, 8u);
}

bool ds_encode_stack_status(ds_frame_t *frame, const ds_stack_status_t *status) {
    put_u16(&frame->payload[0], status->level_mm);
    put_u16(&frame->payload[2], (uint16_t)status->diff_pressure_pa);
    put_u16(&frame->payload[4], status->surge_count);
    put_u16(&frame->payload[6], status->battery_mv);
    return finalize_frame(frame, 8u);
}

bool ds_encode_actuator_status(ds_frame_t *frame, const ds_actuator_status_t *status) {
    frame->payload[0] = status->target_position;
    frame->payload[1] = status->measured_position;
    put_u16(&frame->payload[2], status->motor_current_ma);
    put_u16(&frame->payload[4], status->travel_time_ms);
    put_u16(&frame->payload[6], status->fault_bits);
    return finalize_frame(frame, 8u);
}

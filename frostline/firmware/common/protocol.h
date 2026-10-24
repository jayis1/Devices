#ifndef FROSTLINE_PROTOCOL_H
#define FROSTLINE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define FL_VERSION 1u
#define FL_FRAME_LEN 23u
typedef struct {
    uint8_t version, message_type;
    uint16_t node_id, sequence;
    uint32_t unix_time_s;
    int16_t surface_temp_c_centi, air_temp_c_centi;
    uint16_t rh_permille, wetness_u16, battery_mv;
    uint8_t flags;
} fl_telemetry_t;
uint16_t fl_crc16(const uint8_t *data, size_t length);
int fl_encode(const fl_telemetry_t *in, uint8_t out[FL_FRAME_LEN]);
int fl_decode(const uint8_t *frame, size_t length, fl_telemetry_t *out);
#endif

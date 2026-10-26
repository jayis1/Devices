#include <stdint.h>
#ifndef SPINSHARE_PROTOCOL_H
#define SPINSHARE_PROTOCOL_H
#define SS_FRAME_SIZE 16u
#define SS_VERSION 1u
#define SS_TYPE_TELEMETRY 1u
typedef struct { uint8_t version,type,flags; uint16_t node_id,sequence; uint32_t uptime_s; uint16_t vibration_rms_mg,battery_mv; } ss_record_t;
int ss_encode(const ss_record_t *r, uint8_t out[SS_FRAME_SIZE]);
int ss_decode(const uint8_t *in, unsigned len, ss_record_t *r);
#endif

#include <stdint.h>
#define RR_FRAME_SIZE 16u
#define RR_VERSION 1u
struct rr_frame { uint8_t version, type; uint16_t node_id, sequence; uint32_t uptime_s; int16_t value_a, value_b; uint16_t crc; };
uint16_t rr_crc16(const uint8_t *data, uint32_t len);
int rr_encode(const struct rr_frame *f, uint8_t out[RR_FRAME_SIZE]);
int rr_decode(const uint8_t in[RR_FRAME_SIZE], struct rr_frame *f);

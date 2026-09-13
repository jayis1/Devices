#ifndef MAINTAINSYNC_PROTOCOL_H
#define MAINTAINSYNC_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define MS_MAX_PAYLOAD 96u
#define MS_TAG_LEN 16u
typedef enum { MS_HEARTBEAT=1, MS_TELEMETRY=2, MS_INSPECTION=3, MS_COMMAND=16, MS_ACK=17, MS_FAULT=18, MS_TIME=32 } ms_type_t;
typedef struct __attribute__((packed)) { uint8_t version,type; uint32_t node_id,seq,epoch; uint8_t len; uint8_t payload[MS_MAX_PAYLOAD]; uint32_t crc32c; uint8_t tag[MS_TAG_LEN]; } ms_frame_t;
uint32_t ms_crc32c(const uint8_t *p, uint16_t n);
bool ms_validate(const ms_frame_t *f, uint16_t n, uint32_t last_seq);
#endif

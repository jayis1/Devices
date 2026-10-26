#ifndef AEDREADY_PROTOCOL_H
#define AEDREADY_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define AEDREADY_VERSION 1u
#define AEDREADY_FRAME_SIZE 24u
#define AEDREADY_KIND_STATUS 1u
#define AEDREADY_FLAG_DOOR_OPEN 0x0001u
#define AEDREADY_FLAG_SENSOR_VALID 0x0002u
#define AEDREADY_FLAG_LOW_BATTERY 0x0004u
/* Serialized explicitly little-endian; do not cast wire bytes to this struct. */
typedef struct {
    uint8_t version, kind;
    uint16_t node_id;
    uint32_t sequence, uptime_s;
    int16_t temperature_centi_c;
    uint16_t battery_mv, flags, crc16;
} aedready_status_t;
uint16_t aedready_crc16(const uint8_t *data, size_t len);
size_t aedready_encode(const aedready_status_t *msg, uint8_t *out, size_t capacity);
int aedready_decode(const uint8_t *data, size_t len, aedready_status_t *out);
#endif

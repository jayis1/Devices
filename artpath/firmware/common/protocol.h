#ifndef ARTPATH_PROTOCOL_H
#define ARTPATH_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define ARTPATH_VERSION 1u
#define ARTPATH_MAX_FRAME 512u
/* Length-delimited byte strings; always validate lengths before use. */
typedef struct {
    uint8_t version;
    uint8_t type;
    uint32_t sequence;
    uint8_t event_id[16];
    uint16_t payload_len;
} artpath_header_t;
uint16_t artpath_crc16(const uint8_t *data, size_t len);
int artpath_frame_valid(const uint8_t *frame, size_t len);
#endif

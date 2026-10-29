#include <stdint.h>
#include <stddef.h>

#define COOLSTOP_PROTOCOL_VERSION 1u
#define COOLSTOP_MAX_FRAME 1024u

/* Portable bounded frame header. Payload is UTF-8 JSON defined in docs/protocol.md. */
typedef struct {
    uint8_t version;
    uint8_t kind;
    uint16_t node_id;
    uint32_t sequence;
    uint16_t payload_length;
} coolstop_frame_header_t;

typedef enum { COOLSTOP_CLIMATE = 1, COOLSTOP_SUPPLY = 2 } coolstop_kind_t;

/* Return 1 only for a complete bounded header+payload buffer. */
static inline int coolstop_frame_valid(const uint8_t *buffer, size_t length) {
    if (buffer == NULL || length < sizeof(coolstop_frame_header_t) || length > COOLSTOP_MAX_FRAME) return 0;
    const coolstop_frame_header_t *header = (const coolstop_frame_header_t *)buffer;
    return header->version == COOLSTOP_PROTOCOL_VERSION &&
           header->payload_length <= COOLSTOP_MAX_FRAME - sizeof(*header) &&
           length == sizeof(*header) + header->payload_length;
}

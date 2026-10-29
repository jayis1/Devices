#include <stdint.h>
#include <stddef.h>

#define TT_VERSION 1u
#define TT_MAX_NODE 20u
#define TT_MAX_FRAME 64u

typedef struct {
    uint8_t version;
    uint16_t node_id;
    uint16_t sequence;
    uint8_t kind;
    int16_t value_x100;
    uint32_t timestamp_s;
} tt_observation;

uint16_t tt_crc16(const uint8_t *data, size_t len);
size_t tt_encode(const tt_observation *obs, uint8_t *out, size_t cap);
int tt_decode(const uint8_t *data, size_t len, tt_observation *out);

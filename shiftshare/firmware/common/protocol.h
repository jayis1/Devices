#include <stddef.h>
#include <stdint.h>

enum { SHIFTSHARE_PROTOCOL_VERSION = 1, SHIFTSHARE_MAX_PAYLOAD = 64 };
typedef struct {
    uint8_t version;
    uint8_t kind;
    uint32_t sequence;
    uint16_t value;
} shiftshare_frame_t;

/* Application payload only; transport framing and authentication are not implemented. */
int shiftshare_frame_valid(const shiftshare_frame_t *frame);

#include "protocol.h"

int shiftshare_frame_valid(const shiftshare_frame_t *frame) {
    return frame != NULL && frame->version == SHIFTSHARE_PROTOCOL_VERSION && frame->kind <= 3;
}

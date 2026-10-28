#include "../common/quietpath.h"
/* Board-specific ESP-IDF acquisition and SX1262 driver integration required. */
int sentinel_report(uint16_t id, uint16_t seq, uint32_t minute, uint16_t leq, uint16_t peak, uint8_t flags, uint8_t out[QP_FRAME_LEN]) {
    qp_report_t r = {id, seq, minute, leq, peak, flags};
    return qp_encode(&r, out);
}

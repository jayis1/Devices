#include "../common/protocol.h"

/* Board SDK integration intentionally not implemented; zero is unknown, not empty. */
int supply_dock_publish(uint32_t seq, uint16_t estimated_count) {
    const shiftshare_frame_t frame = { SHIFTSHARE_PROTOCOL_VERSION, 2, seq, estimated_count };
    return shiftshare_frame_valid(&frame);
}

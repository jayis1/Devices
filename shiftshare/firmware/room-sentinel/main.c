#include "../common/protocol.h"

/* Board SDK integration intentionally not implemented; sensor faults must publish unknown. */
int room_sentinel_publish(uint32_t seq, uint16_t relative_sound_level) {
    const shiftshare_frame_t frame = { SHIFTSHARE_PROTOCOL_VERSION, 1, seq, relative_sound_level };
    return shiftshare_frame_valid(&frame);
}

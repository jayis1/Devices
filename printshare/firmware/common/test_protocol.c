#include <assert.h>
#include "protocol.h"
int main(void) {
    uint8_t frame[PS_FRAME_SIZE];
    assert(ps_encode(frame, PS_TYPE_FILAMENT, 42, 65535, 1234, 1, 5500, 2150, 3300) == 0);
    assert(ps_validate(frame, sizeof frame));
    frame[10] ^= 1;
    assert(!ps_validate(frame, sizeof frame));
    assert(!ps_validate(frame, sizeof frame - 1));
    assert(ps_encode(frame, 9, 1, 1, 0, 0, 0, 0, 0) < 0);
    return 0;
}

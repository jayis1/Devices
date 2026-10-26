#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "protocol.h"
int main(void) {
    aedready_status_t in = {1,1,42,9,120,2150,2900,3,0}, out = {0};
    uint8_t frame[AEDREADY_FRAME_SIZE];
    assert(aedready_encode(&in, frame, sizeof frame) == AEDREADY_FRAME_SIZE);
    assert(aedready_decode(frame, sizeof frame, &out));
    assert(out.node_id == in.node_id && out.sequence == in.sequence);
    assert(out.temperature_centi_c == in.temperature_centi_c && out.flags == in.flags);
    frame[7] ^= 1;
    assert(!aedready_decode(frame, sizeof frame, &out));
    assert(!aedready_decode(frame, sizeof frame - 1, &out));
    return 0;
}

#include "protocol.h"
#include <assert.h>
int main(void) {
    static const uint8_t input[] = "123456789";
    assert(artpath_crc16(input, 9u) == 0x29b1u);
    assert(!artpath_frame_valid(NULL, 0u));
    uint8_t frame[26] = {1, 1};
    frame[23]=0; /* empty payload */
    uint16_t crc=artpath_crc16(frame,24u); frame[24]=(uint8_t)(crc>>8); frame[25]=(uint8_t)crc;
    assert(artpath_frame_valid(frame,sizeof frame));
    frame[0]=2; assert(!artpath_frame_valid(frame,sizeof frame));
    return 0;
}

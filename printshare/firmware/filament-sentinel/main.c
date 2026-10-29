/* Board-port TODO: nRF Connect SDK SHT31 acquisition and authenticated BLE transport. */
#include "../common/protocol.h"
int main(void) {
    uint8_t frame[PS_FRAME_SIZE];
    (void)ps_encode(frame, PS_TYPE_FILAMENT, 2, 0, 0, 0, 0, 0, 3300);
    /* Read SHT31 at 0x44; sensor errors must map to unknown, never dry/ready. */
    return 0;
}

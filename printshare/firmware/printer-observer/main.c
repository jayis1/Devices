/* Board-port TODO: nRF Connect SDK ADC/GPIO sampling and authenticated BLE transport. */
#include "../common/protocol.h"
int main(void) {
    uint8_t frame[PS_FRAME_SIZE];
    (void)ps_encode(frame, PS_TYPE_PRINTER, 1, 0, 0, 0, 0, 0, 3300);
    /* Sample optical status and advertise/send only after per-site provisioning. */
    return 0;
}

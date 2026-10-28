#include "../common/protocol.h"
/* Board adapter: NAU7802/load-cell sampling and BLE advertisement are platform-specific. */
int main(void){pp_frame_t f={PP_VERSION,2,3,0,0,0,0};uint8_t packet[PP_FRAME_LEN];return pp_encode(&f,packet);}

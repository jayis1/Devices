#include "../common/protocol.h"
/* Board adapter: VL53L1X ranging and BLE advertisement are platform-specific. */
int main(void){pp_frame_t f={PP_VERSION,1,2,0,0,0,0};uint8_t packet[PP_FRAME_LEN];return pp_encode(&f,packet);}

#include "../common/protocol.h"
/* Board adapter supplies validated BLE parsing and MQTT/TLS publisher. */
int main(void){pp_frame_t f={PP_VERSION,1,1,0,0,0,0};uint8_t packet[PP_FRAME_LEN];return pp_encode(&f,packet);}

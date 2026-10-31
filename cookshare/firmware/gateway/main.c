/* Platform-neutral gateway validation; ESP-IDF radio/MQTT binding TODO. */
#include "../common/protocol.h"
int gateway_accept(const uint8_t*data,size_t len,cs_frame_t*out){if(cs_decode(data,len,out)||!out->node_id)return-1;return 0;}

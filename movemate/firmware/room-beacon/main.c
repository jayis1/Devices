#include "../common/protocol.h"
/* ESP-IDF integration TODO: BLE advertisements and explicit room confirmation. */
int movemate_room_beacon_protocol_ready(void){mm_message m={.version=1,.type=1,.length=0,.sequence=1};uint8_t frame[140];size_t n=0;return mm_encode(&m,frame,sizeof frame,&n);}

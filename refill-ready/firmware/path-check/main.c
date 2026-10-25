#include "../common/protocol.h"
#include "hal.h"
void path_check_sample(uint16_t node,uint16_t seq){uint8_t b[RR_FRAME_SIZE];struct rr_frame f={RR_VERSION,2,node,seq,hal_uptime_seconds(),(int16_t)hal_check_code(),(int16_t)hal_battery_mv(),0};if(rr_encode(&f,b)==0)hal_ble_send(b,RR_FRAME_SIZE);}

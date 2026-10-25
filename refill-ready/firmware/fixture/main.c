#include "../common/protocol.h"
#include "hal.h"
void fixture_sample(uint16_t node,uint16_t seq){uint8_t b[RR_FRAME_SIZE];struct rr_frame f={RR_VERSION,1,node,seq,hal_uptime_seconds(),(int16_t)hal_flow_pulses_10s(),(int16_t)hal_leak_active(),0};if(rr_encode(&f,b)==0)hal_ble_send(b,RR_FRAME_SIZE);}

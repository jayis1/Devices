#include "../common/protocol.h"
/* HAL adapter supplies GPIO, ADC and radio APIs. No actuator outputs. */
int shade_node_sample(int reed_closed, int tilt_tampered, uint16_t battery_mv, uint8_t *frame, uint32_t id, uint16_t seq) { return ss_encode(frame,id,seq,1,(int16_t)((reed_closed?1:0)|(tilt_tampered?2:0)),battery_mv); }

#include "../common/protocol.h"
/* ESP-IDF integration pending: human button event; never enables an actuator. */
int pickup_confirm_frame(uint32_t node,uint32_t seq,uint8_t out[RR_FRAME_SIZE]) { rr_message_t m={RR_STAFF_CONFIRM,node,seq,0,1};return rr_encode(&m,out); }

/* Gravity-fed demand observer scaffold; calibration is installation-specific. */
#include "../common/protocol.h"
int main(void){rb_packet_t p={0};p.type=4;p.node_id=3;p.payload_len=4;return p.payload_len>RB_MAX_PAYLOAD;}

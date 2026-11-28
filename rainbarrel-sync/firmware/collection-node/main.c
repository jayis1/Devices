/* Count debounced tipping-bucket reed pulses in the board driver. */
#include "../common/protocol.h"
int main(void){rb_packet_t p={0};p.type=3;p.node_id=2;p.payload_len=4;return p.payload_len>RB_MAX_PAYLOAD;}

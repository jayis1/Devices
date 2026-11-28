/* Hardware scaffold: connect to selected SX1262 driver and bounded local queue. */
#include "../common/protocol.h"
int main(void){rb_packet_t p={0};p.type=1;p.node_id=0;p.boot_id=1;p.sequence=1;return p.payload_len>RB_MAX_PAYLOAD;}

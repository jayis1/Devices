/* Tank-node scaffold; replace with calibrated sensor driver. */
#include "../common/protocol.h"
static int read_distance_mm(uint16_t*mm){(void)mm;return 0;}
int main(void){uint16_t mm=0;rb_packet_t p={0};p.type=2;p.node_id=1;p.payload_len=3;if(read_distance_mm(&mm)){p.payload[0]=(uint8_t)mm;p.payload[1]=(uint8_t)(mm>>8);p.payload[2]=0;}return (int)p.node_id-1;}

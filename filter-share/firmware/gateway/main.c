#include "../common/protocol.h"
/* ESP-IDF adapter required: authenticated peer receive, stale tracking, MQTT/TLS uplink. */
#define STATUS_LED_GPIO 2
int gateway_accept(const uint8_t *frame,size_t length,uint32_t *last_seq,uint32_t *node) {
 uint8_t type; uint32_t seq,up; int32_t a,b;
 if(fs_decode(frame,length,&type,node,&seq,&up,&a,&b)) return -1;
 if(*last_seq==seq) return -2;
 *last_seq=seq; return 0;
}

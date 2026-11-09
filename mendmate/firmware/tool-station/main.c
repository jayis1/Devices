#include "../common/protocol.h"
/* Adapter boundary: convert calibrated HX711 grams to centigrams, drawer=0/1. */
int station_report(mm_packet_t *p, uint32_t sequence, uint32_t thread_cg, uint8_t drawer_open) {
    if (!p || drawer_open > 1) return 0;
    p->node=2; p->seq=sequence; p->kind=2; p->length=5;
    for(unsigned i=0;i<4;++i) p->payload[i]=(uint8_t)(thread_cg>>(8u*i));
    p->payload[4]=drawer_open; return 1;
}

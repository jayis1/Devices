#include "../common/protocol.h"
/* Adapter boundary: sample AS7341 via I2C, normalize channels, enqueue kind=1 report. */
int swatch_report(mm_packet_t *p, uint32_t sequence, const uint16_t channels[11]) {
    if (!p || !channels) return 0;
    p->node=1; p->seq=sequence; p->kind=1; p->length=22;
    for (unsigned i=0;i<11;++i) { p->payload[2*i]=(uint8_t)channels[i]; p->payload[2*i+1]=(uint8_t)(channels[i]>>8); }
    return 1;
}

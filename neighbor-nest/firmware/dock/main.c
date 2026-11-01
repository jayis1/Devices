#include "../common/protocol.h"
/* Platform adapter: render requested care task, advertise explicit visit event. */
int dock_make_visit(uint16_t id, uint16_t seq, uint8_t button, uint8_t *out, size_t cap, size_t *len) {
    uint8_t payload[2]={button,0};
    return nn_encode(out,cap,id,seq,2,payload,sizeof payload,len);
}

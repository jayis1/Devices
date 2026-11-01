#include "../common/protocol.h"
/* Platform adapter: sample sensor, build a frame, advertise via BLE. */
int probe_make_frame(uint16_t id, uint16_t seq, uint16_t adc, uint8_t *out, size_t cap, size_t *len) {
    uint8_t payload[2]={(uint8_t)adc,(uint8_t)(adc>>8)};
    return nn_encode(out,cap,id,seq,1,payload,sizeof payload,len);
}

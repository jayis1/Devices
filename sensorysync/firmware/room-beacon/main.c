/* ESP32-C6 reference: GPIO6/7 I2C, GPIO2/3/4 I2S, authored by jayis1. */
#include <stdint.h>
#include "../common/sensorysync_protocol.h"
int main(void) { ss_frame_t f={0}; f.version=1; f.node_id=16; f.type=1; f.payload_len=6; f.payload[0]=1; return ss_validate(&f,0,0) ? 0 : 1; }

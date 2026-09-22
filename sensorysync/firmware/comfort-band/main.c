/* nRF52840 reference: P0.02 EDA ADC, P0.13 button, authored by jayis1. */
#include <stdint.h>
#include "../common/sensorysync_protocol.h"
static uint8_t local_stop_pressed(uint8_t button) { return button != 0u; }
int main(void) { ss_frame_t f={0}; f.version=1; f.node_id=32; f.type=2; f.payload_len=1; return local_stop_pressed(0u) ? 1 : (ss_validate(&f,0,0) ? 0 : 1); }

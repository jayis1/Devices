/* Sink Sentinel portable state reference. Author: jayis1. */
#include "../common/hygieneguard_protocol.h"
HGTelemetry sink_sample(uint32_t flow_ml, uint16_t soap_g, int16_t temp_c_x100, uint16_t leak_flag) {
  HGTelemetry sample = { flow_ml, soap_g, temp_c_x100, leak_flag ? 1u : 0u };
  return sample;
}
int main(void) { return 0; }

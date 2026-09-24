/* Smart Dispenser portable state reference. Author: jayis1. */
#include "../common/hygieneguard_protocol.h"
uint8_t dispenser_refill_state(uint16_t soap_g, uint16_t low_threshold_g) {
  return soap_g <= low_threshold_g ? 1u : 0u;
}
int main(void) { return 0; }

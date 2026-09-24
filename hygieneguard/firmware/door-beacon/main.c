/* Door Beacon portable state reference. Author: jayis1. */
#include "../common/hygieneguard_protocol.h"
int beacon_accept_display_command(uint32_t now, uint32_t expires, uint8_t command) {
  return command == 1u && expires >= now && expires - now <= 30u;
}
int main(void) { return 0; }

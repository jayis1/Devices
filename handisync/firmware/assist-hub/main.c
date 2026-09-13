#include "../common/protocol.h"
#include <stdbool.h>
// Platform adapters must implement radio_send, dock_is_safe, and emergency_latched.
bool authorize_command(const hs_frame_t *f, bool zone_match, bool confirmed) {
  if (!hs_validate(f, 18+f->len+HS_TAG_LEN, 0) || emergency_latched()) return false;
  return zone_match && confirmed && dock_is_safe(f->node_id);
}
void app_main(void) { /* poll BLE grips, TDMA radio, local policy and MQTT spool */ }

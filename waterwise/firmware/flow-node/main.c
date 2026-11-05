#include "../common/protocol.h"
/* Board adapter TODO: sample validated UART sensor, increment seq, advertise BLE. */
int main(void) {
  ww_telemetry_t sample = {WW_PROTOCOL_VERSION,"flow-01",1,0,0.0f,"flow","L/min"};
  return ww_valid(&sample) ? 0 : 1;
}

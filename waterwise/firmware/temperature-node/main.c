#include "../common/protocol.h"
/* Board adapter TODO: sample 1-Wire probes and advertise BLE telemetry. */
int main(void) {
  ww_telemetry_t sample = {WW_PROTOCOL_VERSION,"temp-01",1,0,21.0f,"pipe_temp","C"};
  return ww_valid(&sample) ? 0 : 1;
}

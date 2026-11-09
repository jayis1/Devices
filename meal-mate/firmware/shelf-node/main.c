#include "../common/protocol.h"
/* ESP-IDF application scaffold. Bind NAU7802 driver, BLE authenticated GATT and calibration store. */
int app_main(void) {
  /* Safe default: report unavailable until bridge calibration and bonded gateway. */
  mm_event_t event = { .version=MM_VERSION, .kind=MM_WEIGHT, .device_id=1u };
  (void)event;
  return 0;
}

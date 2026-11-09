#include "../common/protocol.h"
/* ESP-IDF application scaffold. Add SHT31 and reed input drivers; no food-safety alarm. */
int app_main(void) {
  mm_event_t event = { .version=MM_VERSION, .kind=MM_CLIMATE, .device_id=2u };
  (void)event;
  return 0;
}

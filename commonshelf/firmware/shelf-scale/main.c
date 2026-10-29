#include <stdint.h>
#include "../common/protocol.h"
/* Board SDK hooks supplied by target integration. */
extern int hx711_read_grams(int32_t *grams);
extern int ble_send_json(const cs_sample_t *sample);
void app_main(void) {
 uint32_t seq = 0;
 for (;;) {
  int32_t grams = 0;
  if (hx711_read_grams(&grams) == 0) {
   cs_sample_t s = { .seq=seq++, .timestamp_s=0, .value=grams, .battery_mv=0, .kind=CS_MASS_G };
   if (cs_valid(&s)) (void)ble_send_json(&s);
  }
 }
}

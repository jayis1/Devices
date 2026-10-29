#include <stdint.h>
#include "../common/protocol.h"
extern int tmp117_read_centi_c(int32_t *value);
extern int reed_is_open(void);
extern int ble_send_json(const cs_sample_t *sample);
void app_main(void) {
 uint32_t seq=0;
 for (;;) {
  int32_t temp;
  if (tmp117_read_centi_c(&temp)==0) {
   cs_sample_t s={.seq=seq++,.timestamp_s=0,.value=temp,.battery_mv=0,.kind=CS_TEMP_C_X100};
   if(cs_valid(&s)) (void)ble_send_json(&s);
  }
  cs_sample_t door={.seq=seq++,.timestamp_s=0,.value=!!reed_is_open(),.battery_mv=0,.kind=CS_DOOR};
  (void)ble_send_json(&door);
 }
}

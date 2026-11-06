/* Board adapter sketch: count rain-tip and anemometer reed pulses per interval. */
#include <stdint.h>
#include "../common/protocol.h"
#define NODE_ID 2
static uint32_t seq;
void weather_publish(int32_t rain_tips, int32_t wind_mm_s){uint8_t frame[LW_FRAME_LEN];if(lw_encode(frame,2,NODE_ID,seq++,rain_tips)==0){/* TODO: ESP-IDF BLE adv; send second kind=3 for wind. */}(void)wind_mm_s;}

/* Board adapter sketch: sample HX711 every 2 s; encode scaled load grams. */
#include <stdint.h>
#include "../common/protocol.h"
#define NODE_ID 1
static uint32_t seq;
void line_sample_publish(int32_t grams){uint8_t frame[LW_FRAME_LEN];if(lw_encode(frame,1,NODE_ID,seq++,grams)==0){/* TODO: Zephyr BLE advertising adapter sends frame. */}}

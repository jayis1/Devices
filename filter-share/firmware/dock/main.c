#include "../common/protocol.h"
/* ESP-IDF adapter required: debounce GPIO inputs and send state over ESP-NOW. */
#define PRESENT_A_GPIO 3
#define PRESENT_B_GPIO 4
#define CONFIRM_GPIO 5
#define SERVICE_GPIO 6
#define STATUS_LED_GPIO 7
int dock_make_frame(uint8_t *out,uint32_t seq,uint32_t uptime_ms,int32_t presence_bits,int32_t service_bits) {
 return fs_encode(out,2,0x444f4301u,seq,uptime_ms,presence_bits,service_bits);
}

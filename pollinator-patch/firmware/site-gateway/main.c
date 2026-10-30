#include <stdint.h>
#include "../common/protocol.h"
#define PP_STATUS_LED_GPIO 8
#define PP_ENROLL_BUTTON_GPIO 9
int main(void) { uint8_t frame[PP_FRAME_OVERHEAD]={0}; (void)pp_frame_valid(frame,sizeof frame); /* BLE/Wi-Fi adapters and bounded offline queue are not implemented */ return PP_STATUS_LED_GPIO==PP_ENROLL_BUTTON_GPIO; }

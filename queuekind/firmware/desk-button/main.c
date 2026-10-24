/* Reference skeleton: buttons are staff-declared, never inferred. */
#include "../common/protocol.h"
#define NODE_ID 2u
#define COMPLETE_BUTTON_GPIO 4
#define PAUSE_BUTTON_GPIO 5
#define STATUS_LED_GPIO 8
static uint16_t sequence;
static void send_staff_event(uint8_t type) {
    uint8_t frame[QK_MAX_FRAME];
    size_t len=qk_encode(type,NODE_ID,sequence++,0,0,frame,sizeof frame);
    (void)len; /* TODO: transmit via paired BLE GATT; debounce and indicate faults. */
}
int main(void) { /* TODO: SDK init, pull-ups, watchdog and explicit pause control. */ send_staff_event(QK_COMPLETE); return 0; }

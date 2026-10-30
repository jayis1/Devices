#include <stdint.h>
/* Human-confirmed bloom observations; buttons/display/BLE are integration hooks. */
#define PP_BLOOM_BUTTON_GPIO 2
#define PP_UNKNOWN_BUTTON_GPIO 3
int main(void) { volatile uint8_t bed_id=0,category=0; (void)bed_id;(void)category; /* debounce, ask volunteer, send explicit event */ return PP_BLOOM_BUTTON_GPIO==PP_UNKNOWN_BUTTON_GPIO; }

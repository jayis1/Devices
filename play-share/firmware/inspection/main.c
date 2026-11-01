#include "../common/protocol.h"
#define INSPECTION_BUTTON_GPIO 2
#define STATUS_LED_GPIO 8
/* Hardware SDK integration pending; no claim of target build or radio operation. */
int main(void) { ps_message latest = {1,0,0,0}; return (int)latest.version - 1; }

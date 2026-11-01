#include "../common/protocol.h"
#define HX711_SCK_GPIO 6
#define HX711_DOUT_GPIO 7
/* Hardware SDK integration pending; no claim of target build or radio operation. */
int main(void) { ps_message latest = {1,0,0,0}; return (int)latest.version - 1; }

#include "../common/protocol.h"
#define I2C_SDA_GPIO 4
#define I2C_SCL_GPIO 5
#define TOF_XSHUT_GPIO 6
/* Hardware SDK integration pending; no claim of target build or radio operation. */
int main(void) { ps_message latest = {1,0,0,0}; return (int)latest.version - 1; }

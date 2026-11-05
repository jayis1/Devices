#include <stdint.h>
#define I2C_SDA 6
#define I2C_SCL 7
#define DOOR_GPIO 4
#define NODE_ID 2
/* TMP117 at 0x48; invalid values must be surfaced as sensor faults. */
int valid_temp_centi(int16_t t){return t>=-4000&&t<=8500;}

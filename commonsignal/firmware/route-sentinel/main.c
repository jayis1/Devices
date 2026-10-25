#include "../common/protocol.h"
/* ESP-IDF integration point: configure GPIO6/7 I2C and VL53L1X; publish only classified, calibrated observations. */
int main(void) { return CS_ROUTE == 2 ? 0 : 1; }

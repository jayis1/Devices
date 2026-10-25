#include "../common/protocol.h"
/* ESP-IDF integration point: configure GPIO6/7 I2C and SHT31-D; report sensor faults. */
int main(void) { return CS_CLIMATE == 4 ? 0 : 1; }

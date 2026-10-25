#include "../common/protocol.h"
/* Zephyr integration point: configure P0.11 contact input and BLE advertiser. */
int main(void) { return CS_EQUIPMENT == 3 ? 0 : 1; }

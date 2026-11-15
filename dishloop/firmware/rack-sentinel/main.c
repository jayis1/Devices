#include "../common/protocol.h"
/* Platform adapter supplies sensor reads and BLE advertising. */
int main(void) { dishloop_event e = {1, 1, 0.0}; return dishloop_valid(&e, 0) ? 0 : 1; }

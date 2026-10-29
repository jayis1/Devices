#include <stdint.h>
#include "../common/protocol.h"
/* ESP-IDF integration stub: trigger ultrasonic range sample and publish ponding_mm. */
int main(void) { return rg_valid_value(0.0f, 0.0f, 4000.0f) ? 0 : 1; }

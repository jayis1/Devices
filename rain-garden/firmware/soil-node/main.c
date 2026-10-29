#include <stdint.h>
#include "../common/protocol.h"
/* ESP-IDF integration stub: sample ADC1 soil probe, calibrate locally, publish v1 JSON. */
int main(void) { return rg_valid_value(50.0f, 0.0f, 100.0f) ? 0 : 1; }

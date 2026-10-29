#include <stdint.h>
#include "../common/protocol.h"
/* ESP-IDF integration stub: debounce reed-switch tips and publish cumulative rain_mm. */
int main(void) { return rg_valid_value(0.0f, 0.0f, 100000.0f) ? 0 : 1; }

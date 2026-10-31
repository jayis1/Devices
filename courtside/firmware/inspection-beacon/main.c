#include <stdint.h>
#include "nrf.h"

/* nRF5 SDK/Zephyr board integration required; only records a debounced user inspection. */
int main(void) {
    SystemCoreClockUpdate();
    for (;;) {
        /* TODO: configure P0.11 pull-up + debounce; advertise signed inspection event. */
        __WFE();
    }
}

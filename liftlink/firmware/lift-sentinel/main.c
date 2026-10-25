/* Hardware integration scaffold; read-only sensor node, no lift-control wiring. */
#include <stdint.h>
#include "../common/protocol.h"
int main(void){ll_event_t event={LL_VERSION,1,0,LL_LIFT_SAMPLE,LL_UNKNOWN,0};(void)event;/* TODO: initialize board HAL, sample reed/accelerometer, advertise authenticated GATT. */for(;;){} return 0;}

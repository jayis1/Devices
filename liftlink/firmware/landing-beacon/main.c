/* Hardware integration scaffold; button reports only, no lift-control wiring. */
#include <stdint.h>
#include "../common/protocol.h"
int main(void){ll_event_t event={LL_VERSION,2,0,LL_USER_REPORT,LL_UNKNOWN,0};(void)event;/* TODO: debounce switch and send authenticated GATT report. */for(;;){} return 0;}

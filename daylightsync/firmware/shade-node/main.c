#include <stdint.h>
/* Platform loop must sample AS5600 and honor normally-closed limit switches.
 * Motor output remains off on boot, fault, stale command, or manual override. */
typedef enum { SHADE_IDLE, SHADE_MOVING, SHADE_FAULT } shade_state;
int shade_should_stop(shade_state state, int limit_hit, int manual, int expired, int overcurrent) {
    return state == SHADE_FAULT || limit_hit || manual || expired || overcurrent;
}

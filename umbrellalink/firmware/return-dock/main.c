#include <stdint.h>
/* Same hardware profile; distinct node ID is assigned during site enrollment. */
int return_free_slots(const uint8_t occupied[4]) {
    int n = 0;
    for (int i=0;i<4;i++) n += occupied[i] ? 0 : 1;
    return n;
}

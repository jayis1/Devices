#include <assert.h>
#include <math.h>
#include "../common/protocol.h"
int main(void) {
    bl_event e = {1,"tray-01",BL_SLOT_MASS_G,24.6,"g"}; assert(bl_valid(&e));
    e.value = 501; assert(!bl_valid(&e)); e.value = NAN; assert(!bl_valid(&e));
    e.value = 10; e.version = 2; assert(!bl_valid(&e));
    e.version = 1; e.kind = (bl_kind)9; assert(!bl_valid(&e));
    e.kind = BL_BAY_PRESENT; e.unit = "bool"; e.value = 1; assert(bl_valid(&e));
    e.value = 0.5; assert(!bl_valid(&e)); return 0;
}

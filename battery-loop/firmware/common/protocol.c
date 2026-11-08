#include "protocol.h"
#include <math.h>
#include <string.h>

int bl_valid(const bl_event *e) {
    if (!e || e->version != 1 || !e->node || !e->kind || !e->unit) return 0;
    if (strlen(e->node) == 0 || strlen(e->node) > 31) return 0;
    if (e->kind == BL_SLOT_MASS_G)
        return strcmp(e->unit, "g") == 0 && isfinite(e->value) && e->value >= 0.0 && e->value <= 500.0;
    if (e->kind == BL_BAY_PRESENT || e->kind == BL_RETURN_PRESSED)
        return strcmp(e->unit, "bool") == 0 && (e->value == 0.0 || e->value == 1.0);
    return 0;
}

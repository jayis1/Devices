#ifndef BATTERYLOOP_PROTOCOL_H
#define BATTERYLOOP_PROTOCOL_H

typedef enum { BL_SLOT_MASS_G = 1, BL_BAY_PRESENT = 2, BL_RETURN_PRESSED = 3 } bl_kind;
typedef struct {
    unsigned version;
    const char *node;
    bl_kind kind;
    double value;
    const char *unit;
} bl_event;
int bl_valid(const bl_event *event);
#endif

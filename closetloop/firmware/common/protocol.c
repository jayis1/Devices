#include "protocol.h"
#include <string.h>
static bool allowed(const char *s) {
    static const char *k[] = {"worn","needs_repair","washed","drying_complete","mended","retired","door_open","vibration_cycle"};
    for (size_t i=0;i<sizeof(k)/sizeof(k[0]);i++) if (!strcmp(s,k[i])) return true;
    return false;
}
bool cl_event_valid(const cl_event_t *e) {
    if (!e || e->version != CL_VERSION || !e->node_id[0] || !e->event_id[0] || !allowed(e->kind)) return false;
    if (!memchr(e->node_id,0,sizeof e->node_id) || !memchr(e->event_id,0,sizeof e->event_id) || !memchr(e->kind,0,sizeof e->kind) || !memchr(e->item_id,0,sizeof e->item_id)) return false;
    if ((strcmp(e->kind,"door_open") && strcmp(e->kind,"vibration_cycle")) && !e->item_id[0]) return false;
    return true;
}

#include "../common/protocol.h"
/* Reminder acknowledge event producer; e-paper/BLE drivers are not implemented here. */
int make_ack(rt_event *out, const char *node, uint16_t seq, uint32_t now) {
 if (!out || !node || strlen(node)>=sizeof out->node) return 0;
 *out=(rt_event){.version=RT_VERSION,.kind=4,.seq=seq,.timestamp=now}; strcpy(out->node,node); return rt_valid(out);
}

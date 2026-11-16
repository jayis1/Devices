#include "../common/protocol.h"
/* BLE advertisement driver and secure provisioning intentionally remain board-specific. */
int make_checkin(rt_event *out, const char *node, const char *book, uint16_t seq, uint32_t now) {
 if (!out || !node || !book || strlen(node)>=sizeof out->node || strlen(book)>=sizeof out->book) return 0;
 *out=(rt_event){.version=RT_VERSION,.kind=3,.seq=seq,.timestamp=now};
 strcpy(out->node,node); strcpy(out->book,book); return rt_valid(out);
}

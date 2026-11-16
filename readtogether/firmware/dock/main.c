#include "../common/protocol.h"
/* Hardware adapter TODO: PN532 read and button debouncing are platform-specific. */
int make_session_event(rt_event *out, const char *node, const char *book, uint16_t seq, uint32_t now, int starting) {
 if (!out || !node || !book || strlen(node)>=sizeof out->node || strlen(book)>=sizeof out->book) return 0;
 *out=(rt_event){.version=RT_VERSION,.kind=(uint8_t)(starting?1:2),.seq=seq,.timestamp=now};
 strcpy(out->node,node); strcpy(out->book,book); return rt_valid(out);
}

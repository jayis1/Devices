#include "event.h"
#include <stdio.h>
#include <string.h>
size_t dr_encode(const dr_event_t *e, char *out, size_t cap) {
 if (!e || !out || !cap || e->version != 1 || !memchr(e->node, '\0', sizeof e->node) || !memchr(e->kind, '\0', sizeof e->kind)) return 0;
 int n=snprintf(out,cap,"{\"v\":%u,\"node\":\"%s\",\"seq\":%u,\"kind\":\"%s\",\"ready\":%s,\"uptime_s\":%u}",e->version,e->node,e->seq,e->kind,e->ready?"true":"false",e->uptime_s);
 return n<0 || (size_t)n>=cap ? 0u : (size_t)n;
}

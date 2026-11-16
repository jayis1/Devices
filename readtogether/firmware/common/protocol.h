#ifndef READTOGETHER_PROTOCOL_H
#define READTOGETHER_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define RT_MAX_FRAME 512
#define RT_VERSION 1
/* Compact binary event envelope; strings are NUL-terminated and bounded. */
typedef struct { uint8_t version; uint8_t kind; uint16_t seq; uint32_t timestamp; char node[24]; char book[40]; } rt_event;
static inline int rt_valid(const rt_event *e) {
 if (!e || e->version != RT_VERSION || e->kind < 1 || e->kind > 4) return 0;
 if (!memchr(e->node, '\0', sizeof e->node) || e->node[0] == '\0') return 0;
 if (!memchr(e->book, '\0', sizeof e->book)) return 0;
 return 1;
}
#endif

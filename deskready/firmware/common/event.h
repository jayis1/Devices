#ifndef DESKREADY_EVENT_H
#define DESKREADY_EVENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DR_NODE_MAX 24
#define DR_KIND_MAX 16
typedef struct { uint8_t version; char node[DR_NODE_MAX]; uint32_t seq; char kind[DR_KIND_MAX]; bool ready; uint32_t uptime_s; } dr_event_t;
/* Bounded, NUL-terminated JSON subset. Returns 0 on invalid input/capacity. */
size_t dr_encode(const dr_event_t *e, char *out, size_t cap);
#endif

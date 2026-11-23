#ifndef CLOSETLOOP_PROTOCOL_H
#define CLOSETLOOP_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CL_VERSION 1
#define CL_MAX_PAYLOAD 512
#define CL_ID_MAX 40
typedef struct { uint8_t version; char node_id[CL_ID_MAX]; uint32_t seq; char event_id[37]; char kind[24]; char item_id[CL_ID_MAX]; uint64_t utc_ms; uint8_t quality; } cl_event_t;
bool cl_event_valid(const cl_event_t *e);
#endif

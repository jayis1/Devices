#include <stdint.h>
/* Event UUIDs and ticket IDs are gateway-generated; never use visitor identity. */
typedef struct { uint32_t sequence; uint8_t category; } ticket_request;
int gateway_request_valid(const ticket_request *r) { return r && r->category < 16; }

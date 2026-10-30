#ifndef BOOKDROP_PROTOCOL_H
#define BOOKDROP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define BD_PROTOCOL_VERSION 1u
#define BD_MAX_PAYLOAD 160u
int bd_payload_valid(const uint8_t *payload, size_t length);
#endif

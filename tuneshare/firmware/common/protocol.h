#ifndef TUNESHARE_PROTOCOL_H
#define TUNESHARE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define TS_VERSION 1u
#define TS_MAX_PAYLOAD 256u
#define TS_KIND_CLIMATE 1u
#define TS_KIND_RACK 2u
#define TS_KIND_CHECKIN 3u
/* Wire representation is specified in docs/protocol.md; this helper validates bounds only. */
int ts_payload_valid(const uint8_t *payload, size_t length);
#endif

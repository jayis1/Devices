#ifndef PLAYSHARE_PROTOCOL_H
#define PLAYSHARE_PROTOCOL_H
#include <stdint.h>
#define PS_MAX_PAYLOAD 160

typedef struct { uint32_t version, seq; uint64_t ts_ms; double value; } ps_message;
int ps_validate(const char *json, ps_message *out);
#endif

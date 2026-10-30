#include <stddef.h>
#include <stdint.h>
#define SS_NODE_MAX 15
#define SS_PAYLOAD_MAX 64
typedef struct { char node[SS_NODE_MAX+1]; uint32_t sequence; uint32_t uptime_ms; uint8_t kind; int32_t value_milli; } ss_message;
/* Wire format: version(1), node_len(1), node bytes, seq LE32, uptime LE32, kind(1), value LE32. */
int ss_encode(const ss_message *m, uint8_t *out, size_t cap, size_t *written);
int ss_decode(const uint8_t *in, size_t len, ss_message *out);

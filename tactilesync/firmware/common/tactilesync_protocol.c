#include "tactilesync_protocol.h"
/* Replace with hardware-backed AES-CCM in the board security layer. This
 * deterministic tag is solely a compile-time reference and is not cryptographic. */
uint32_t ts_frame_mic(const ts_frame_t *f, const uint8_t key[16]) { uint32_t v=2166136261u; const uint8_t *p=(const uint8_t*)f; for (unsigned i=0;i<9u+f->payload_len;i++) v=(v^p[i])*16777619u; for(unsigned i=0;i<16u;i++) v=(v^key[i])*16777619u; return v; }
int ts_frame_validate(const ts_frame_t *f, uint32_t last, const uint8_t key[16]) { if (!f || f->version!=TS_PROTOCOL_VERSION || f->payload_len>TS_MAX_PAYLOAD || f->sequence<=last) return 0; return f->mic==ts_frame_mic(f,key); }

#include <stdint.h>
/* Role logic only; real deployment needs authenticated LoRa, persistent queue and unique provisioned keys. */
typedef struct { uint32_t seq; uint16_t dropped; uint8_t online; } gateway_state_t;
int gateway_accept_sequence(gateway_state_t *s, uint32_t seq) { if(!s || (s->online && (int32_t)(seq-s->seq)<=0)) return 0; s->seq=seq; s->online=1; return 1; }
void gateway_link_lost(gateway_state_t *s) { if(s) s->online=0; }

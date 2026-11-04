#include "../common/protocol.h"
/* Portable logic example; ESP-IDF GPIO/BLE driver and persistent sequence storage are TODO. */
static uint32_t sequence;
rc_event_t ticket_created(uint8_t node_id, uint16_t random_ticket) {
  rc_event_t e = {RC_VERSION, RC_TICKET_CREATED, node_id, 0, ++sequence, random_ticket, 0};
  return e; /* TODO serialize little-endian, compute CRC, enqueue with bounded retry */
}

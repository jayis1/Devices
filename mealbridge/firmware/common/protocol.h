#include <stdint.h>
#include <stddef.h>
#define MB_VERSION 1
#define MB_MAX_PAYLOAD 64
/* Transport-independent bounded frame. CRC-16/CCITT-FALSE over header+payload. */
typedef struct { uint8_t version, kind; uint16_t node; uint32_t seq; uint8_t length; uint8_t payload[MB_MAX_PAYLOAD]; uint16_t crc; } mb_frame_t;
uint16_t mb_crc16(const uint8_t *data, size_t len);
int mb_encode(const mb_frame_t *f, uint8_t *out, size_t cap, size_t *used);
int mb_decode(const uint8_t *in, size_t len, mb_frame_t *out);

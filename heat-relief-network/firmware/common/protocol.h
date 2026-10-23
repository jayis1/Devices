/* jayis1 — portable application frame helpers; BLE/HAL integration not included. */
#ifndef HR_PROTOCOL_H
#define HR_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define HR_MAX_PAYLOAD 32u
#define HR_HEADER_SIZE 14u
#define HR_CRC_SIZE 2u
uint16_t hr_crc16(const uint8_t *data, size_t length);
int hr_encode(uint8_t *out, size_t capacity, uint8_t type, uint32_t node_id,
              uint32_t sequence, const uint8_t *payload, uint16_t payload_len,
              size_t *written);
int hr_validate(const uint8_t *frame, size_t length);
#endif

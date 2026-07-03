#ifndef DRIVESYNC_CRC8_H
#define DRIVESYNC_CRC8_H

#include <stdint.h>

/**
 * CRC-8 with polynomial 0x07 (CRC-8-CCITT).
 * Used for payload integrity verification on top of the header XOR checksum.
 */

uint8_t crc8_compute(const uint8_t *data, uint8_t len);

bool crc8_verify(const uint8_t *data, uint8_t len, uint8_t expected);

#endif /* DRIVESYNC_CRC8_H */
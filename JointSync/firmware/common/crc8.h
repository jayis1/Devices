#ifndef JOINTSYNC_CRC8_H
#define JOINTSYNC_CRC8_H

#include <stdint.h>

/**
 * CRC-8 (Maxim/Dallas polynomial 0x31)
 * Used for Sub-GHz packet integrity.
 */
uint8_t crc8(const uint8_t *data, uint8_t len);

#endif /* JOINTSYNC_CRC8_H */
#ifndef SIGHTSYNC_CRC8_H
#define SIGHTSYNC_CRC8_H

#include <stdint.h>

/* CRC-8 polynomial 0x07 (used for Sub-GHz packet integrity) */

uint8_t sightsync_crc8(const uint8_t *data, uint8_t len);

#endif /* SIGHTSYNC_CRC8_H */
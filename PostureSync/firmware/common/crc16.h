#ifndef POSTURESYNC_CRC16_H
#define POSTURESYNC_CRC16_H

#include <stdint.h>

uint16_t crc16_ccitt(const uint8_t *data, uint16_t len);

#endif
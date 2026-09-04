#ifndef WSYNC_CRC16_H
#define WSYNC_CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t wsync_crc16(const uint8_t *data, size_t len);

#endif

#ifndef OUTAGESYNC_CRC16_H
#define OUTAGESYNC_CRC16_H
#include <stddef.h>
#include <stdint.h>
uint16_t os_crc16_ccitt(const uint8_t *data, size_t len);
#endif

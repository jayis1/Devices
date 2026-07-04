#ifndef GLUCOSYNC_CRC8_H
#define GLUCOSYNC_CRC8_H

#include <stdint.h>

uint8_t crc8_compute(const uint8_t *data, uint8_t len);
bool crc8_verify(const uint8_t *data, uint8_t len, uint8_t expected);

#endif
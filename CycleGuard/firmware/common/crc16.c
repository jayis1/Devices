/*
 * CRC-16/CCITT-FALSE — convenience wrapper
 */
#include "crc16.h"
#include "protocol.h"  /* uses the same table */

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    return protocol_crc16(data, len);
}
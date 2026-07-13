/*
 * crc16.c — CRC16-CCITT implementation for CardioSync
 * License: MIT
 */
#include "cardiosync_protocol.h"

/* Implementation is inlined in header; this file is for
 * compilation units that need a non-inline version, or for
 * testing purposes.
 */
uint16_t cs_crc16_external(const uint8_t *data, size_t len)
{
    return cs_crc16(data, len);
}
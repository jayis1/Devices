#ifndef PRINTSHARE_PROTOCOL_H
#define PRINTSHARE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define PS_FRAME_SIZE 18u
#define PS_VERSION 1u
enum { PS_TYPE_PRINTER=1, PS_TYPE_FILAMENT=2 };
uint16_t ps_crc16(const uint8_t *data, size_t len);
int ps_encode(uint8_t out[PS_FRAME_SIZE], uint8_t type, uint16_t node_id,
              uint16_t sequence, uint32_t uptime_s, uint8_t state,
              uint16_t humidity_centi_rh, int16_t temperature_centi_c,
              uint16_t battery_mv);
int ps_validate(const uint8_t *frame, size_t len);
#endif

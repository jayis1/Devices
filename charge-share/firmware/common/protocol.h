#pragma once
#include <stddef.h>
#include <stdint.h>
#define CHARGESHARE_FRAME_LEN 20u
int charges_encode(uint8_t *out, size_t cap, uint32_t node, uint32_t seq,
                   uint32_t uptime_ms, uint16_t mv, uint16_t ma);
int charges_validate(const uint8_t *frame, size_t len);

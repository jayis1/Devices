/*
 * SeizureSync — TDMA timing helper
 * SPDX-License-Identifier: MIT
 */
#ifndef SZ_TDMA_H
#define SZ_TDMA_H

#include <stdint.h>
#include "protocol.h"

void sz_tdma_get_slot_timing(uint8_t my_slot, uint32_t *offset_ms,
                              uint32_t *duration_ms);
int  sz_parse(const uint8_t *buf, size_t len, sz_header_t *out_h,
              uint8_t *out_payload, size_t *out_plen);

#endif
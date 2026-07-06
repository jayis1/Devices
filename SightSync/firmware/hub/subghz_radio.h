/**
 * SightSync Vision Hub — Sub-GHz Radio (CC1101 868 MHz TDMA)
 * License: MIT
 */

#ifndef SUBGHZ_RADIO_H
#define SUBGHZ_RADIO_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

typedef void (*subghz_rx_cb_t)(const sightsync_header_t *hdr,
                                const uint8_t *payload);

void subghz_radio_init(subghz_rx_cb_t rx_cb);
void subghz_radio_send(uint16_t dest_id, const uint8_t *data, uint8_t len);
bool subghz_radio_is_ready(void);

#endif /* SUBGHZ_RADIO_H */
/**
 * SightSync Desk Sentinel — Sub-GHz Radio (CC1101) Stub
 *
 * Same CC1101 driver as hub; shared implementation.
 * License: MIT
 */

#ifndef DESK_SUBGHZ_H
#define DESK_SUBGHZ_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

typedef void (*desk_subghz_rx_cb_t)(const sightsync_header_t *hdr,
                                      const uint8_t *payload);

void subghz_radio_init(desk_subghz_rx_cb_t rx_cb);
void subghz_radio_send(uint16_t dest_id, const uint8_t *data, uint8_t len);
bool subghz_radio_is_ready(void);

#endif /* DESK_SUBGHZ_H */
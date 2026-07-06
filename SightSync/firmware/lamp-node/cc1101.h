/**
 * SightSync Smart Lamp Node — CC1101 Sub-GHz Radio
 * License: MIT
 */

#ifndef LAMP_CC1101_H
#define LAMP_CC1101_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

typedef void (*lamp_rx_cb_t)(const sightsync_header_t *hdr,
                              const uint8_t *payload);

void cc1101_init(uint8_t cs, uint8_t sck, uint8_t miso, uint8_t mosi,
                 uint8_t gdo0, lamp_rx_cb_t rx_cb);
void cc1101_send(uint16_t dest_id, const uint8_t *data, uint8_t len);
bool cc1101_is_ready(void);

#endif /* LAMP_CC1101_H */
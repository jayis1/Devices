/**
 * JointSync Hub — Sub-GHz Coordinator Interface
 *
 * License: MIT
 */

#ifndef SUBGHZ_COORD_H
#define SUBGHZ_COORD_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*subghz_data_cb_t)(uint16_t sender_id, const uint8_t *data, uint8_t len);

void subghz_coord_init(subghz_data_cb_t callback);
void subghz_coord_start(void);
void subghz_coord_stop(void);
void subghz_send_packet(uint8_t *data, uint8_t len);

#endif /* SUBGHZ_COORD_H */
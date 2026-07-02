/**
 * JointSync Compression Sleeve — Sub-GHz Node Interface
 *
 * License: MIT
 */

#ifndef SUBGHZ_NODE_H
#define SUBGHZ_NODE_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*subghz_data_cb_t)(uint16_t sender_id, const uint8_t *data, uint8_t len);

void subghz_node_init(subghz_data_cb_t callback);
void subghz_node_start_rx(void);
void subghz_node_stop(void);
void subghz_node_send(uint8_t *data, uint8_t len);

#endif /* SUBGHZ_NODE_H */
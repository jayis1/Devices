#ifndef UROSYNC_MESH_H
#define UROSYNC_MESH_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define US_TX_QUEUE_LEN 8

typedef struct {
    us_frame_t frames[US_TX_QUEUE_LEN];
    uint8_t payload_len[US_TX_QUEUE_LEN];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} us_mesh_queue_t;

void us_mesh_init(us_mesh_queue_t *queue);
bool us_mesh_enqueue(us_mesh_queue_t *queue, const us_frame_t *frame, uint8_t payload_len);
bool us_mesh_dequeue(us_mesh_queue_t *queue, us_frame_t *frame, uint8_t *payload_len);

#endif

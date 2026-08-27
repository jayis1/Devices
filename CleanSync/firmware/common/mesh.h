#ifndef CLEANSYNC_MESH_H
#define CLEANSYNC_MESH_H

#include "protocol.h"
#include <stdbool.h>
#include <stdint.h>

#define CS_MESH_QUEUE_DEPTH 8u

typedef struct {
    cs_frame_t frames[CS_MESH_QUEUE_DEPTH];
    uint8_t lengths[CS_MESH_QUEUE_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} cs_mesh_queue_t;

void cs_mesh_init(cs_mesh_queue_t *queue);
bool cs_mesh_enqueue(cs_mesh_queue_t *queue, const cs_frame_t *frame, uint8_t payload_len);
bool cs_mesh_dequeue(cs_mesh_queue_t *queue, cs_frame_t *frame, uint8_t *payload_len);

#endif

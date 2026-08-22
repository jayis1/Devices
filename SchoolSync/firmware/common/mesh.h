#ifndef SCHOOLSYNC_MESH_H
#define SCHOOLSYNC_MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "protocol.h"

#define SS_MESH_QUEUE_DEPTH 8

typedef struct {
    ss_frame_t frames[SS_MESH_QUEUE_DEPTH];
    uint8_t lengths[SS_MESH_QUEUE_DEPTH];
    size_t head;
    size_t tail;
    size_t count;
} ss_mesh_queue_t;

void ss_mesh_init(ss_mesh_queue_t *queue);
bool ss_mesh_enqueue(ss_mesh_queue_t *queue, const ss_frame_t *frame, uint8_t payload_len);
bool ss_mesh_dequeue(ss_mesh_queue_t *queue, ss_frame_t *frame, uint8_t *payload_len);

#endif

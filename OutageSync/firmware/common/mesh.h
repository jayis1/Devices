#ifndef OUTAGESYNC_MESH_H
#define OUTAGESYNC_MESH_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define OS_MESH_QUEUE_DEPTH 16

typedef struct {
    os_frame_t frames[OS_MESH_QUEUE_DEPTH];
    uint8_t payload_lengths[OS_MESH_QUEUE_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} os_mesh_queue_t;

void os_mesh_init(os_mesh_queue_t *queue);
bool os_mesh_enqueue(os_mesh_queue_t *queue, const os_frame_t *frame, uint8_t payload_len);
bool os_mesh_dequeue(os_mesh_queue_t *queue, os_frame_t *frame, uint8_t *payload_len);

#endif

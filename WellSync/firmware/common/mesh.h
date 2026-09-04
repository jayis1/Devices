#ifndef WSYNC_MESH_H
#define WSYNC_MESH_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define WSYNC_QUEUE_CAPACITY 8

typedef struct {
    wsync_frame_t frames[WSYNC_QUEUE_CAPACITY];
    uint8_t lengths[WSYNC_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} wsync_mesh_queue_t;

void wsync_mesh_init(wsync_mesh_queue_t *queue);
bool wsync_mesh_enqueue(wsync_mesh_queue_t *queue, const wsync_frame_t *frame, uint8_t payload_len);
bool wsync_mesh_dequeue(wsync_mesh_queue_t *queue, wsync_frame_t *frame, uint8_t *payload_len);

#endif

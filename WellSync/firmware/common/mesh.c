#include <string.h>
#include "mesh.h"

void wsync_mesh_init(wsync_mesh_queue_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

bool wsync_mesh_enqueue(wsync_mesh_queue_t *queue, const wsync_frame_t *frame, uint8_t payload_len) {
    if (queue->count >= WSYNC_QUEUE_CAPACITY) {
        return false;
    }
    queue->frames[queue->tail] = *frame;
    queue->lengths[queue->tail] = payload_len;
    queue->tail = (uint8_t)((queue->tail + 1u) % WSYNC_QUEUE_CAPACITY);
    queue->count++;
    return true;
}

bool wsync_mesh_dequeue(wsync_mesh_queue_t *queue, wsync_frame_t *frame, uint8_t *payload_len) {
    if (queue->count == 0u) {
        return false;
    }
    *frame = queue->frames[queue->head];
    *payload_len = queue->lengths[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % WSYNC_QUEUE_CAPACITY);
    queue->count--;
    return true;
}

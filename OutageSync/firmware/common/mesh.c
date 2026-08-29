#include <string.h>
#include "mesh.h"

void os_mesh_init(os_mesh_queue_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

bool os_mesh_enqueue(os_mesh_queue_t *queue, const os_frame_t *frame, uint8_t payload_len) {
    if (queue->count >= OS_MESH_QUEUE_DEPTH) return false;
    queue->frames[queue->tail] = *frame;
    queue->payload_lengths[queue->tail] = payload_len;
    queue->tail = (uint8_t)((queue->tail + 1u) % OS_MESH_QUEUE_DEPTH);
    queue->count++;
    return true;
}

bool os_mesh_dequeue(os_mesh_queue_t *queue, os_frame_t *frame, uint8_t *payload_len) {
    if (queue->count == 0) return false;
    *frame = queue->frames[queue->head];
    *payload_len = queue->payload_lengths[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % OS_MESH_QUEUE_DEPTH);
    queue->count--;
    return true;
}

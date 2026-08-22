#include "mesh.h"
#include <string.h>

void ss_mesh_init(ss_mesh_queue_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

bool ss_mesh_enqueue(ss_mesh_queue_t *queue, const ss_frame_t *frame, uint8_t payload_len) {
    if (queue->count >= SS_MESH_QUEUE_DEPTH) {
        return false;
    }
    queue->frames[queue->tail] = *frame;
    queue->lengths[queue->tail] = payload_len;
    queue->tail = (queue->tail + 1u) % SS_MESH_QUEUE_DEPTH;
    queue->count++;
    return true;
}

bool ss_mesh_dequeue(ss_mesh_queue_t *queue, ss_frame_t *frame, uint8_t *payload_len) {
    if (queue->count == 0) {
        return false;
    }
    *frame = queue->frames[queue->head];
    *payload_len = queue->lengths[queue->head];
    queue->head = (queue->head + 1u) % SS_MESH_QUEUE_DEPTH;
    queue->count--;
    return true;
}

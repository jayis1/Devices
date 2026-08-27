#include "mesh.h"
#include <string.h>

void cs_mesh_init(cs_mesh_queue_t *queue) {
    if (queue == NULL) return;
    memset(queue, 0, sizeof(*queue));
}

bool cs_mesh_enqueue(cs_mesh_queue_t *queue, const cs_frame_t *frame, uint8_t payload_len) {
    if (queue == NULL || frame == NULL || queue->count >= CS_MESH_QUEUE_DEPTH) {
        return false;
    }
    queue->frames[queue->tail] = *frame;
    queue->lengths[queue->tail] = payload_len;
    queue->tail = (uint8_t)((queue->tail + 1u) % CS_MESH_QUEUE_DEPTH);
    queue->count++;
    return true;
}

bool cs_mesh_dequeue(cs_mesh_queue_t *queue, cs_frame_t *frame, uint8_t *payload_len) {
    if (queue == NULL || frame == NULL || payload_len == NULL || queue->count == 0u) {
        return false;
    }
    *frame = queue->frames[queue->head];
    *payload_len = queue->lengths[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % CS_MESH_QUEUE_DEPTH);
    queue->count--;
    return true;
}

#include "mesh.h"
#include <string.h>

void ws_mesh_init(ws_mesh_queue_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

bool ws_mesh_enqueue(ws_mesh_queue_t *queue, const ws_frame_t *frame, uint8_t payload_len) {
    if (queue->count >= WS_TX_QUEUE_LEN) {
        return false;
    }
    queue->frames[queue->tail] = *frame;
    queue->payload_len[queue->tail] = payload_len;
    queue->tail = (uint8_t)((queue->tail + 1u) % WS_TX_QUEUE_LEN);
    queue->count++;
    return true;
}

bool ws_mesh_dequeue(ws_mesh_queue_t *queue, ws_frame_t *frame, uint8_t *payload_len) {
    if (queue->count == 0) {
        return false;
    }
    *frame = queue->frames[queue->head];
    *payload_len = queue->payload_len[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % WS_TX_QUEUE_LEN);
    queue->count--;
    return true;
}

#include "mesh.h"
#include <string.h>

void us_mesh_init(us_mesh_queue_t *queue) {
    memset(queue, 0, sizeof(*queue));
}

bool us_mesh_enqueue(us_mesh_queue_t *queue, const us_frame_t *frame, uint8_t payload_len) {
    if (queue->count >= US_TX_QUEUE_LEN) {
        return false;
    }
    queue->frames[queue->tail] = *frame;
    queue->payload_len[queue->tail] = payload_len;
    queue->tail = (uint8_t)((queue->tail + 1u) % US_TX_QUEUE_LEN);
    queue->count++;
    return true;
}

bool us_mesh_dequeue(us_mesh_queue_t *queue, us_frame_t *frame, uint8_t *payload_len) {
    if (queue->count == 0) {
        return false;
    }
    *frame = queue->frames[queue->head];
    *payload_len = queue->payload_len[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % US_TX_QUEUE_LEN);
    queue->count--;
    return true;
}

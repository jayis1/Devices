#ifndef WASTESORT_MESH_H
#define WASTESORT_MESH_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

#define WS_TX_QUEUE_LEN 8

typedef struct {
    ws_frame_t frames[WS_TX_QUEUE_LEN];
    uint8_t payload_len[WS_TX_QUEUE_LEN];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} ws_mesh_queue_t;

void ws_mesh_init(ws_mesh_queue_t *queue);
bool ws_mesh_enqueue(ws_mesh_queue_t *queue, const ws_frame_t *frame, uint8_t payload_len);
bool ws_mesh_dequeue(ws_mesh_queue_t *queue, ws_frame_t *frame, uint8_t *payload_len);

#endif
